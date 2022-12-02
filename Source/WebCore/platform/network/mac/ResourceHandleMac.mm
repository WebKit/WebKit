/*
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "ResourceHandleInternal.h"

#import "AuthenticationChallenge.h"
#import "AuthenticationMac.h"
#import "CachedResourceLoader.h"
#import "CookieStorage.h"
#import "CredentialStorage.h"
#import "FormDataStreamMac.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "HTTPHeaderNames.h"
#import "Logging.h"
#import "MIMETypeRegistry.h"
#import "NetworkStorageSession.h"
#import "NetworkingContext.h"
#import "ResourceError.h"
#import "ResourceResponse.h"
#import "SharedBuffer.h"
#import "SubresourceLoader.h"
#import "SynchronousLoaderClient.h"
#import "WebCoreResourceHandleAsOperationQueueDelegate.h"
#import "WebCoreURLResponse.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NSURLConnectionSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/CompletionHandler.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/Ref.h>
#import <wtf/SchedulePair.h>
#import <wtf/text/Base64.h>
#import <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY)
#import "RuntimeApplicationChecks.h"
#import "WebCoreThreadRun.h"
#endif

using namespace WebCore;

namespace WebCore {
    
static void applyBasicAuthorizationHeader(ResourceRequest& request, const Credential& credential)
{
    request.setHTTPHeaderField(HTTPHeaderName::Authorization, credential.serializationForBasicAuthorizationHeader());
}

static NSOperationQueue *operationQueueForAsyncClients()
{
    static NeverDestroyed queue = [] {
        auto queue = adoptNS([[NSOperationQueue alloc] init]);
        // Default concurrent operation count depends on current system workload, but delegate methods are mostly idling in IPC, so we can run as many as needed.
        [queue setMaxConcurrentOperationCount:NSOperationQueueDefaultMaxConcurrentOperationCount];
        return queue;
    }();
    return queue.get().get();
}

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
    releaseDelegate();
    d->m_currentWebChallenge.setAuthenticationClient(0);

    LOG(Network, "Handle %p destroyed", this);
}

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)

static bool synchronousWillSendRequestEnabled()
{
    static bool disabled = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitDisableSynchronousWillSendRequestPreferenceKey"] || CocoaApplication::isIBooks();
    return !disabled;
}

#endif

#if PLATFORM(COCOA)

NSURLRequest *ResourceHandle::applySniffingPoliciesIfNeeded(NSURLRequest *request, bool shouldContentSniff, ContentEncodingSniffingPolicy contentEncodingSniffingPolicy)
{
#if !USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
    UNUSED_PARAM(contentEncodingSniffingPolicy);
#endif

    if (shouldContentSniff
#if USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
        && contentEncodingSniffingPolicy == ContentEncodingSniffingPolicy::Default
#endif
        )
        return request;

    auto mutableRequest = adoptNS([request mutableCopy]);

#if USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
    if (contentEncodingSniffingPolicy == ContentEncodingSniffingPolicy::Disable)
        [mutableRequest _setProperty:@YES forKey:(__bridge NSString *)kCFURLRequestContentDecoderSkipURLCheck];
#endif

    if (!shouldContentSniff)
        [mutableRequest _setProperty:@NO forKey:(__bridge NSString *)_kCFURLConnectionPropertyShouldSniff];

    return mutableRequest.autorelease();
}

#endif

#if !PLATFORM(IOS_FAMILY)
void ResourceHandle::createNSURLConnection(id delegate, bool shouldUseCredentialStorage, bool shouldContentSniff, ContentEncodingSniffingPolicy contentEncodingSniffingPolicy, SchedulingBehavior schedulingBehavior)
#else
void ResourceHandle::createNSURLConnection(id delegate, bool shouldUseCredentialStorage, bool shouldContentSniff, ContentEncodingSniffingPolicy contentEncodingSniffingPolicy, SchedulingBehavior schedulingBehavior, NSDictionary *connectionProperties)
#endif
{
    // Credentials for ftp can only be passed in URL, the connection:didReceiveAuthenticationChallenge: delegate call won't be made.
    if ((!d->m_user.isEmpty() || !d->m_password.isEmpty()) && !firstRequest().url().protocolIsInHTTPFamily()) {
        URL urlWithCredentials(firstRequest().url());
        urlWithCredentials.setUser(d->m_user);
        urlWithCredentials.setPassword(d->m_password);
        firstRequest().setURL(urlWithCredentials);
    }

    if (shouldUseCredentialStorage && firstRequest().url().protocolIsInHTTPFamily()) {
        if (d->m_user.isEmpty() && d->m_password.isEmpty()) {
            // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication, 
            // try and reuse the credential preemptively, as allowed by RFC 2617.
            if (auto* networkStorageSession = d->m_context->storageSession())
                d->m_initialCredential = networkStorageSession->credentialStorage().get(firstRequest().cachePartition(), firstRequest().url());
        } else {
            // If there is already a protection space known for the URL, update stored credentials before sending a request.
            // This makes it possible to implement logout by sending an XMLHttpRequest with known incorrect credentials, and aborting it immediately
            // (so that an authentication dialog doesn't pop up).
            if (auto* networkStorageSession = d->m_context->storageSession())
                networkStorageSession->credentialStorage().set(firstRequest().cachePartition(), Credential(d->m_user, d->m_password, CredentialPersistenceNone), firstRequest().url());
        }
    }
        
    if (!d->m_initialCredential.isEmpty() && !firstRequest().hasHTTPHeaderField(HTTPHeaderName::Authorization)) {
        // FIXME: Support Digest authentication, and Proxy-Authorization.
        applyBasicAuthorizationHeader(firstRequest(), d->m_initialCredential);
    }

    auto nsRequest = retainPtr(firstRequest().nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody));
    nsRequest = applySniffingPoliciesIfNeeded(nsRequest.get(), shouldContentSniff, contentEncodingSniffingPolicy);

    if (d->m_storageSession)
        nsRequest = copyRequestWithStorageSession(d->m_storageSession.get(), nsRequest.get());

    ASSERT([NSURLConnection instancesRespondToSelector:@selector(_initWithRequest:delegate:usesCache:maxContentLength:startImmediately:connectionProperties:)]);

#if PLATFORM(IOS_FAMILY)
    // FIXME: This code is different from iOS code in ResourceHandleCFNet.cpp in that here we respect stream properties that were present in client properties.
    NSDictionary *streamPropertiesFromClient = [connectionProperties objectForKey:@"kCFURLConnectionSocketStreamProperties"];
    RetainPtr<NSMutableDictionary> streamProperties;
    if (streamPropertiesFromClient)
        streamProperties = adoptNS([streamPropertiesFromClient mutableCopy]);
    else
        streamProperties = retainPtr([NSMutableDictionary dictionary]);
#else
    auto streamProperties = retainPtr([NSMutableDictionary dictionary]);
#endif

    if (!shouldUseCredentialStorage) {
        // Avoid using existing connections, because they may be already authenticated.
        [streamProperties setObject:@"WebKitPrivateSession" forKey:@"_kCFURLConnectionSessionID"];
    }

    if (schedulingBehavior == SchedulingBehavior::Synchronous) {
        // Synchronous requests should not be subject to regular connection count limit to avoid deadlocks.
        // If we are using all available connections for async requests, and make a sync request, then prior
        // requests may get stuck waiting for delegate calls while we are in nested run loop, and the sync
        // request won't start because there are no available connections.
        // Connections are grouped by their socket stream properties, with each group having a separate count.
        [streamProperties setObject:@TRUE forKey:@"_WebKitSynchronousRequest"];
    }

    RetainPtr<CFDataRef> sourceApplicationAuditData = d->m_context->sourceApplicationAuditData();
    if (sourceApplicationAuditData)
        [streamProperties setObject:(__bridge NSData *)sourceApplicationAuditData.get() forKey:@"kCFStreamPropertySourceApplication"];

#if PLATFORM(IOS_FAMILY)
    NSMutableDictionary *propertyDictionary = [NSMutableDictionary dictionaryWithDictionary:connectionProperties];
    [propertyDictionary setObject:streamProperties.get() forKey:@"kCFURLConnectionSocketStreamProperties"];
    const bool usesCache = false;
#if !PLATFORM(MACCATALYST)
    if (synchronousWillSendRequestEnabled())
        CFURLRequestSetShouldStartSynchronously([nsRequest _CFURLRequest], 1);
#endif
#else
    NSMutableDictionary *propertyDictionary = [NSMutableDictionary dictionaryWithObject:streamProperties.get() forKey:@"kCFURLConnectionSocketStreamProperties"];
    const bool usesCache = true;
#endif
    [propertyDictionary setObject:@{@"_kCFURLConnectionPropertyTimingDataOptions": @(_TimingDataOptionsEnableW3CNavigationTiming)} forKey:@"kCFURLConnectionURLConnectionProperties"];

    // This is used to signal that to CFNetwork that this connection should be considered
    // web content for purposes of App Transport Security.
    [propertyDictionary setObject:@{@"NSAllowsArbitraryLoadsInWebContent": @YES} forKey:@"_kCFURLConnectionPropertyATSFrameworkOverrides"];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    d->m_connection = adoptNS([[NSURLConnection alloc] _initWithRequest:nsRequest.get() delegate:delegate usesCache:usesCache maxContentLength:0 startImmediately:NO connectionProperties:propertyDictionary]);
ALLOW_DEPRECATED_DECLARATIONS_END
}

bool ResourceHandle::start()
{
    if (!d->m_context)
        return false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // If NetworkingContext is invalid then we are no longer attached to a Page,
    // this must be an attempted load from an unload event handler, so let's just block it.
    if (!d->m_context->isValid())
        return false;

    if (auto* networkStorageSession = d->m_context->storageSession())
        d->m_storageSession = networkStorageSession->platformSession();

    // FIXME: Do not use the sync version of shouldUseCredentialStorage when the client returns true from usesAsyncCallbacks.
    bool shouldUseCredentialStorage = !client() || client()->shouldUseCredentialStorage(this);

    SchedulingBehavior schedulingBehavior = client() && client()->loadingSynchronousXHR() ? SchedulingBehavior::Synchronous : SchedulingBehavior::Asynchronous;

#if !PLATFORM(IOS_FAMILY)
    createNSURLConnection(
        ResourceHandle::makeDelegate(shouldUseCredentialStorage, nullptr),
        shouldUseCredentialStorage,
        d->m_shouldContentSniff || d->m_context->localFileContentSniffingEnabled(),
        d->m_contentEncodingSniffingPolicy,
        schedulingBehavior);
#else
    createNSURLConnection(
        ResourceHandle::makeDelegate(shouldUseCredentialStorage, nullptr),
        shouldUseCredentialStorage,
        d->m_shouldContentSniff || d->m_context->localFileContentSniffingEnabled(),
        d->m_contentEncodingSniffingPolicy,
        schedulingBehavior,
        (NSDictionary *)client()->connectionProperties(this).get());
#endif

    [connection() setDelegateQueue:operationQueueForAsyncClients()];
    [connection() start];
    d->m_startTime = MonotonicTime::now();

    LOG(Network, "Handle %p starting connection %p for %@", this, connection(), firstRequest().nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody));
    
    if (d->m_connection) {
        if (d->m_defersLoading)
            connection().defersCallbacks = YES;

        return true;
    }

    END_BLOCK_OBJC_EXCEPTIONS

    return false;
}

void ResourceHandle::cancel()
{
    LOG(Network, "Handle %p cancel connection %p", this, d->m_connection.get());

    // Leaks were seen on HTTP tests without this; can be removed once <rdar://problem/6886937> is fixed.
    if (d->m_currentMacChallenge)
        [[d->m_currentMacChallenge sender] cancelAuthenticationChallenge:d->m_currentMacChallenge];

    [d->m_connection.get() cancel];
}

void ResourceHandle::platformSetDefersLoading(bool defers)
{
    if (d->m_connection)
        [d->m_connection setDefersCallbacks:defers];
}

void ResourceHandle::schedule(SchedulePair& pair)
{
    NSRunLoop *runLoop = pair.nsRunLoop();
    if (!runLoop)
        return;
    [d->m_connection.get() scheduleInRunLoop:runLoop forMode:(__bridge NSString *)pair.mode()];
}

void ResourceHandle::unschedule(SchedulePair& pair)
{
    if (NSRunLoop *runLoop = pair.nsRunLoop())
        [d->m_connection.get() unscheduleFromRunLoop:runLoop forMode:(__bridge NSString *)pair.mode()];
}

id ResourceHandle::makeDelegate(bool shouldUseCredentialStorage, RefPtr<SynchronousLoaderMessageQueue>&& queue)
{
    ASSERT(!d->m_delegate);

    if (shouldUseCredentialStorage)
        d->m_delegate = adoptNS([[WebCoreResourceHandleAsOperationQueueDelegate alloc] initWithHandle:this messageQueue:WTFMove(queue)]);
    else
        d->m_delegate = adoptNS([[WebCoreResourceHandleWithCredentialStorageAsOperationQueueDelegate alloc] initWithHandle:this messageQueue:WTFMove(queue)]);

    return d->m_delegate.get();
}

id ResourceHandle::delegate()
{
    if (!d->m_delegate)
        return makeDelegate(false, nullptr);
    return d->m_delegate.get();
}

void ResourceHandle::releaseDelegate()
{
    if (!d->m_delegate)
        return;
    [d->m_delegate.get() detachHandle];
    d->m_delegate = nil;
}

NSURLConnection *ResourceHandle::connection() const
{
    return d->m_connection.get();
}
    
CFStringRef ResourceHandle::synchronousLoadRunLoopMode()
{
    return CFSTR("WebCoreSynchronousLoaderRunLoopMode");
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentialsPolicy storedCredentialsPolicy, SecurityOrigin* sourceOrigin, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    LOG(Network, "ResourceHandle::platformLoadResourceSynchronously:%@ storedCredentialsPolicy:%u", request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody), static_cast<unsigned>(storedCredentialsPolicy));

    ASSERT(!request.isEmpty());
    
    SynchronousLoaderClient client;
    client.setAllowStoredCredentials(storedCredentialsPolicy == StoredCredentialsPolicy::Use);

    bool defersLoading = false;
    bool shouldContentSniff = true;
    RefPtr<ResourceHandle> handle = adoptRef(new ResourceHandle(context, request, &client, defersLoading, shouldContentSniff, ContentEncodingSniffingPolicy::Default, sourceOrigin, false));

    handle->d->m_storageSession = context->storageSession()->platformSession();

    if (context && handle->d->m_scheduledFailureType != NoFailure) {
        error = context->blockedError(request);
        return;
    }

    bool shouldUseCredentialStorage = storedCredentialsPolicy == StoredCredentialsPolicy::Use;
#if !PLATFORM(IOS_FAMILY)
    handle->createNSURLConnection(
        handle->makeDelegate(shouldUseCredentialStorage, &client.messageQueue()),
        shouldUseCredentialStorage,
        handle->shouldContentSniff() || context->localFileContentSniffingEnabled(),
        handle->contentEncodingSniffingPolicy(),
        SchedulingBehavior::Synchronous);
#else
    handle->createNSURLConnection(
        handle->makeDelegate(shouldUseCredentialStorage, &client.messageQueue()), // A synchronous request cannot turn into a download, so there is no need to proxy the delegate.
        shouldUseCredentialStorage,
        handle->shouldContentSniff() || (context && context->localFileContentSniffingEnabled()),
        handle->contentEncodingSniffingPolicy(),
        SchedulingBehavior::Synchronous,
        (NSDictionary *)handle->client()->connectionProperties(handle.get()).get());
#endif

    [handle->connection() setDelegateQueue:operationQueueForAsyncClients()];
    [handle->connection() start];
    
    do {
        if (auto task = client.messageQueue().waitForMessage())
            (*task)();
    } while (!client.messageQueue().killed());

    error = client.error();
    
    [handle->connection() cancel];

    if (error.isNull())
        response = client.response();

    data.swap(client.mutableData());
}

void ResourceHandle::willSendRequest(ResourceRequest&& request, ResourceResponse&& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT(!redirectResponse.isNull());

    if (redirectResponse.httpStatusCode() == 307) {
        String lastHTTPMethod = d->m_lastHTTPMethod;
        if (!equalIgnoringASCIICase(lastHTTPMethod, request.httpMethod())) {
            request.setHTTPMethod(lastHTTPMethod);
    
            FormData* body = d->m_firstRequest.httpBody();
            if (!equalLettersIgnoringASCIICase(lastHTTPMethod, "get"_s) && body && !body->isEmpty())
                request.setHTTPBody(body);

            String originalContentType = d->m_firstRequest.httpContentType();
            if (!originalContentType.isEmpty())
                request.setHTTPHeaderField(HTTPHeaderName::ContentType, originalContentType);
        }
    } else if (redirectResponse.httpStatusCode() == 303) { // FIXME: (rdar://problem/13706454).
        if (equalLettersIgnoringASCIICase(d->m_firstRequest.httpMethod(), "head"_s))
            request.setHTTPMethod("HEAD"_s);

        String originalContentType = d->m_firstRequest.httpContentType();
        if (!originalContentType.isEmpty())
            request.setHTTPHeaderField(HTTPHeaderName::ContentType, originalContentType);
    }

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (!request.url().protocolIs("https"_s) && protocolIs(request.httpReferrer(), "https"_s) && d->m_context->shouldClearReferrerOnHTTPSToHTTPRedirect())
        request.clearHTTPReferrer();

    const URL& url = request.url();
    d->m_user = url.user();
    d->m_password = url.password();
    d->m_lastHTTPMethod = request.httpMethod();
    request.removeCredentials();

    if (auto authorization = d->m_firstRequest.httpHeaderField(HTTPHeaderName::Authorization); !authorization.isNull()
        && protocolHostAndPortAreEqual(d->m_firstRequest.url(), request.url()))
        request.setHTTPHeaderField(HTTPHeaderName::Authorization, authorization);

    if (!protocolHostAndPortAreEqual(request.url(), redirectResponse.url())) {
        // The network layer might carry over some headers from the original request that
        // we want to strip here because the redirect is cross-origin.
        request.clearHTTPAuthorization();
        request.clearHTTPOrigin();
    } else {
        // Only consider applying authentication credentials if this is actually a redirect and the redirect
        // URL didn't include credentials of its own.
        if (d->m_user.isEmpty() && d->m_password.isEmpty() && !redirectResponse.isNull()) {
            Credential credential;
            if (auto* networkStorageSession = d->m_context->storageSession())
                credential = networkStorageSession->credentialStorage().get(request.cachePartition(), request.url());
            if (!credential.isEmpty()) {
                d->m_initialCredential = credential;
                
                // FIXME: Support Digest authentication, and Proxy-Authorization.
                applyBasicAuthorizationHeader(request, d->m_initialCredential);
            }
        }
    }

    client()->willSendRequestAsync(this, WTFMove(request), WTFMove(redirectResponse), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (ResourceRequest&& request) mutable {
        // Client call may not preserve the session, especially if the request is sent over IPC.
        if (!request.isNull())
            request.setStorageSession(d->m_storageSession.get());
        completionHandler(WTFMove(request));
    });
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    ASSERT(!d->m_currentMacChallenge);
    ASSERT(d->m_currentWebChallenge.isNull());
    // Since NSURLConnection networking relies on keeping a reference to the original NSURLAuthenticationChallenge,
    // we make sure that is actually present
    ASSERT(challenge.nsURLAuthenticationChallenge());

    // Proxy authentication is handled by CFNetwork internally. We can get here if the user cancels
    // CFNetwork authentication dialog, and we shouldn't ask the client to display another one in that case.
    if (challenge.protectionSpace().isProxy()) {
        // Cannot use receivedRequestToContinueWithoutCredential(), because current challenge is not yet set.
        [challenge.sender() continueWithoutCredentialForAuthenticationChallenge:challenge.nsURLAuthenticationChallenge()];
        return;
    }

    if (tryHandlePasswordBasedAuthentication(challenge))
        return;

#if PLATFORM(IOS_FAMILY)
    // If the challenge is for a proxy protection space, look for default credentials in
    // the keychain.  CFNetwork used to handle this until WebCore was changed to always
    // return NO to -connectionShouldUseCredentialStorage: for <rdar://problem/7704943>.
    if (!challenge.previousFailureCount() && challenge.protectionSpace().isProxy()) {
        RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));
        NSURLAuthenticationChallenge *macChallenge = mac(challenge);
        if (NSURLCredential *credential = [[NSURLCredentialStorage sharedCredentialStorage] defaultCredentialForProtectionSpace:[macChallenge protectionSpace]]) {
            [challenge.sender() useCredential:credential forAuthenticationChallenge:macChallenge];
            return;
        }
    }
#endif // PLATFORM(IOS_FAMILY)

    d->m_currentMacChallenge = challenge.nsURLAuthenticationChallenge();
    d->m_currentWebChallenge = core(d->m_currentMacChallenge);
    d->m_currentWebChallenge.setAuthenticationClient(this);

    // FIXME: Several concurrent requests can return with the an authentication challenge for the same protection space.
    // We should avoid making additional client calls for the same protection space when already waiting for the user,
    // because typing the same credentials several times is annoying.
    if (client())
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
    else {
        clearAuthentication();
        [challenge.sender() performDefaultHandlingForAuthenticationChallenge:challenge.nsURLAuthenticationChallenge()];
    }
}

bool ResourceHandle::tryHandlePasswordBasedAuthentication(const AuthenticationChallenge& challenge)
{
    if (!challenge.protectionSpace().isPasswordBased())
        return false;

    if (!d->m_user.isEmpty() || !d->m_password.isEmpty()) {
        auto credential = adoptNS([[NSURLCredential alloc] initWithUser:d->m_user
            password:d->m_password persistence:NSURLCredentialPersistenceForSession]);
        d->m_currentMacChallenge = challenge.nsURLAuthenticationChallenge();
        d->m_currentWebChallenge = challenge;
        receivedCredential(challenge, Credential(credential.get()));
        // FIXME: Per the specification, the user shouldn't be asked for credentials if there were incorrect ones provided explicitly.
        d->m_user = String();
        d->m_password = String();
        return true;
    }

    // FIXME: Do not use the sync version of shouldUseCredentialStorage when the client returns true from usesAsyncCallbacks.
    if (!client() || client()->shouldUseCredentialStorage(this)) {
        if (!d->m_initialCredential.isEmpty() || challenge.previousFailureCount()) {
            // The stored credential wasn't accepted, stop using it.
            // There is a race condition here, since a different credential might have already been stored by another ResourceHandle,
            // but the observable effect should be very minor, if any.
            if (auto* networkStorageSession = d->m_context->storageSession())
                networkStorageSession->credentialStorage().remove(d->m_partition, challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            Credential credential;
            if (auto* networkStorageSession = d->m_context->storageSession())
                credential = networkStorageSession->credentialStorage().get(d->m_partition, challenge.protectionSpace());
            if (!credential.isEmpty() && credential != d->m_initialCredential) {
                ASSERT(credential.persistence() == CredentialPersistenceNone);
                if (challenge.failureResponse().httpStatusCode() == 401) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    if (auto* networkStorageSession = d->m_context->storageSession())
                        networkStorageSession->credentialStorage().set(d->m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }
                [challenge.sender() useCredential:credential.nsCredential() forAuthenticationChallenge:mac(challenge)];
                return true;
            }
        }
    }

    return false;
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void ResourceHandle::canAuthenticateAgainstProtectionSpace(const ProtectionSpace& protectionSpace, CompletionHandler<void(bool)>&& completionHandler)
{
    if (ResourceHandleClient* client = this->client())
        client->canAuthenticateAgainstProtectionSpaceAsync(this, protectionSpace, WTFMove(completionHandler));
    else
        completionHandler(false);
}
#endif

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    LOG(Network, "Handle %p receivedCredential", this);

    ASSERT(!challenge.isNull());
    if (!AuthenticationChallengeBase::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge))
        return;

    // FIXME: Support empty credentials. Currently, an empty credential cannot be stored in WebCore credential storage, as that's empty value for its map.
    if (credential.isEmpty()) {
        receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    if (credential.persistence() == CredentialPersistenceForSession && challenge.protectionSpace().authenticationScheme() != ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested) {
        // Manage per-session credentials internally, because once NSURLCredentialPersistenceForSession is used, there is no way
        // to ignore it for a particular request (short of removing it altogether).
        Credential webCredential(credential, CredentialPersistenceNone);
        URL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = challenge.failureResponse().url();
        if (auto* networkStorageSession = d->m_context->storageSession())
            networkStorageSession->credentialStorage().set(d->m_partition, webCredential, ProtectionSpace([d->m_currentMacChallenge protectionSpace]), urlToStore);
        [[d->m_currentMacChallenge sender] useCredential:webCredential.nsCredential() forAuthenticationChallenge:d->m_currentMacChallenge];
    } else
        [[d->m_currentMacChallenge sender] useCredential:credential.nsCredential() forAuthenticationChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    LOG(Network, "Handle %p receivedRequestToContinueWithoutCredential", this);

    ASSERT(!challenge.isNull());
    if (!AuthenticationChallengeBase::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge))
        return;

    [[d->m_currentMacChallenge sender] continueWithoutCredentialForAuthenticationChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    LOG(Network, "Handle %p receivedCancellation", this);

    if (!AuthenticationChallengeBase::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge))
        return;

    if (client())
        client()->receivedCancellation(this, challenge);
}

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge& challenge)
{
    LOG(Network, "Handle %p receivedRequestToPerformDefaultHandling", this);

    ASSERT(!challenge.isNull());
    if (!AuthenticationChallengeBase::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge))
        return;

    [[d->m_currentMacChallenge sender] performDefaultHandlingForAuthenticationChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge& challenge)
{
    LOG(Network, "Handle %p receivedChallengeRejection", this);

    ASSERT(!challenge.isNull());
    if (!AuthenticationChallengeBase::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge))
        return;

    [[d->m_currentMacChallenge sender] rejectProtectionSpaceAndContinueWithChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

} // namespace WebCore
