/*
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#import "BlockExceptions.h"
#import "CookieStorage.h"
#import "CredentialStorage.h"
#import "CachedResourceLoader.h"
#import "FormDataStreamMac.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "HTTPHeaderNames.h"
#import "Logging.h"
#import "MIMETypeRegistry.h"
#import "NetworkingContext.h"
#import "Page.h"
#import "ResourceError.h"
#import "ResourceResponse.h"
#import "Settings.h"
#import "SharedBuffer.h"
#import "SubresourceLoader.h"
#import "WebCoreResourceHandleAsDelegate.h"
#import "WebCoreResourceHandleAsOperationQueueDelegate.h"
#import "SynchronousLoaderClient.h"
#import "WebCoreSystemInterface.h"
#import "WebCoreURLResponse.h"
#import <wtf/Ref.h>
#import <wtf/SchedulePair.h>
#import <wtf/text/Base64.h>
#import <wtf/text/CString.h>

#if USE(CFNETWORK)
#if __has_include(<CFNetwork/CFURLConnectionPriv.h>)
#import <CFNetwork/CFURLConnectionPriv.h>
#endif
typedef struct _CFURLConnection* CFURLConnectionRef;
extern "C" {
CFDictionaryRef _CFURLConnectionCopyTimingData(CFURLConnectionRef);
}
#endif // USE(CFNETWORK)

#if __has_include(<Foundation/NSURLConnectionPrivate.h>)
#import <Foundation/NSURLConnectionPrivate.h>
#else
@interface NSURLConnection (TimingData)
+ (void)_setCollectsTimingData:(BOOL)collect;
- (NSDictionary *)_timingData;
@end
#endif

#if PLATFORM(IOS)
#import <CFNetwork/CFURLRequest.h>

#import "RuntimeApplicationChecksIOS.h"
#import "WebCoreThreadRun.h"

@interface NSURLRequest (iOSDetails)
- (CFURLRequestRef) _CFURLRequest;
@end
#endif

#if USE(QUICK_LOOK)
#import "QuickLook.h"
#endif

using namespace WebCore;

@interface NSURLConnection (Details)
-(id)_initWithRequest:(NSURLRequest *)request delegate:(id)delegate usesCache:(BOOL)usesCacheFlag maxContentLength:(long long)maxContentLength startImmediately:(BOOL)startImmediately connectionProperties:(NSDictionary *)connectionProperties;
@end

namespace WebCore {
    
#if !USE(CFNETWORK)
    
static void applyBasicAuthorizationHeader(ResourceRequest& request, const Credential& credential)
{
    String authenticationHeader = "Basic " + base64Encode(String(credential.user() + ":" + credential.password()).utf8());

    request.setHTTPHeaderField(HTTPHeaderName::Authorization, authenticationHeader);
}

static NSOperationQueue *operationQueueForAsyncClients()
{
    static NSOperationQueue *queue;
    if (!queue) {
        queue = [[NSOperationQueue alloc] init];
        // Default concurrent operation count depends on current system workload, but delegate methods are mostly idling in IPC, so we can run as many as needed.
        [queue setMaxConcurrentOperationCount:NSOperationQueueDefaultMaxConcurrentOperationCount];
    }
    return queue;
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

#if PLATFORM(IOS)
static bool synchronousWillSendRequestEnabled()
{
    static bool disabled = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitDisableSynchronousWillSendRequestPreferenceKey"];
    return !disabled;
}
#endif

#if !PLATFORM(IOS)
void ResourceHandle::createNSURLConnection(id delegate, bool shouldUseCredentialStorage, bool shouldContentSniff, SchedulingBehavior schedulingBehavior)
#else
void ResourceHandle::createNSURLConnection(id delegate, bool shouldUseCredentialStorage, bool shouldContentSniff, SchedulingBehavior schedulingBehavior, NSDictionary *connectionProperties)
#endif
{
    // Credentials for ftp can only be passed in URL, the connection:didReceiveAuthenticationChallenge: delegate call won't be made.
    if ((!d->m_user.isEmpty() || !d->m_pass.isEmpty()) && !firstRequest().url().protocolIsInHTTPFamily()) {
        URL urlWithCredentials(firstRequest().url());
        urlWithCredentials.setUser(d->m_user);
        urlWithCredentials.setPass(d->m_pass);
        firstRequest().setURL(urlWithCredentials);
    }

    if (shouldUseCredentialStorage && firstRequest().url().protocolIsInHTTPFamily()) {
        if (d->m_user.isEmpty() && d->m_pass.isEmpty()) {
            // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication, 
            // try and reuse the credential preemptively, as allowed by RFC 2617.
            d->m_initialCredential = CredentialStorage::get(firstRequest().url());
        } else {
            // If there is already a protection space known for the URL, update stored credentials before sending a request.
            // This makes it possible to implement logout by sending an XMLHttpRequest with known incorrect credentials, and aborting it immediately
            // (so that an authentication dialog doesn't pop up).
            CredentialStorage::set(Credential(d->m_user, d->m_pass, CredentialPersistenceNone), firstRequest().url());
        }
    }
        
    if (!d->m_initialCredential.isEmpty()) {
        // FIXME: Support Digest authentication, and Proxy-Authorization.
        applyBasicAuthorizationHeader(firstRequest(), d->m_initialCredential);
    }

    NSURLRequest *nsRequest = firstRequest().nsURLRequest(UpdateHTTPBody);
    if (!shouldContentSniff) {
        NSMutableURLRequest *mutableRequest = [[nsRequest mutableCopy] autorelease];
        wkSetNSURLRequestShouldContentSniff(mutableRequest, NO);
        nsRequest = mutableRequest;
    }

    if (d->m_storageSession)
        nsRequest = [wkCopyRequestWithStorageSession(d->m_storageSession.get(), nsRequest) autorelease];

    ASSERT([NSURLConnection instancesRespondToSelector:@selector(_initWithRequest:delegate:usesCache:maxContentLength:startImmediately:connectionProperties:)]);

#if PLATFORM(IOS)
    // FIXME: This code is different from iOS code in ResourceHandleCFNet.cpp in that here we respect stream properties that were present in client properties.
    NSDictionary *streamPropertiesFromClient = [connectionProperties objectForKey:@"kCFURLConnectionSocketStreamProperties"];
    NSMutableDictionary *streamProperties = streamPropertiesFromClient ? [[streamPropertiesFromClient mutableCopy] autorelease] : [NSMutableDictionary dictionary];
#else
    NSMutableDictionary *streamProperties = [NSMutableDictionary dictionary];
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

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    RetainPtr<CFDataRef> sourceApplicationAuditData = d->m_context->sourceApplicationAuditData();
    if (sourceApplicationAuditData)
        [streamProperties setObject:(NSData *)sourceApplicationAuditData.get() forKey:@"kCFStreamPropertySourceApplication"];
#endif

#if PLATFORM(IOS)
    NSMutableDictionary *propertyDictionary = [NSMutableDictionary dictionaryWithDictionary:connectionProperties];
    [propertyDictionary setObject:streamProperties forKey:@"kCFURLConnectionSocketStreamProperties"];
    const bool usesCache = false;
    if (synchronousWillSendRequestEnabled())
        CFURLRequestSetShouldStartSynchronously([nsRequest _CFURLRequest], 1);
#else
    NSDictionary *propertyDictionary = [NSDictionary dictionaryWithObject:streamProperties forKey:@"kCFURLConnectionSocketStreamProperties"];
    const bool usesCache = true;
#endif
    d->m_connection = adoptNS([[NSURLConnection alloc] _initWithRequest:nsRequest delegate:delegate usesCache:usesCache maxContentLength:0 startImmediately:NO connectionProperties:propertyDictionary]);
#if ENABLE(WEB_TIMING)
    [NSURLConnection _setCollectsTimingData:YES];
#endif
}

bool ResourceHandle::start()
{
    if (!d->m_context)
        return false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // If NetworkingContext is invalid then we are no longer attached to a Page,
    // this must be an attempted load from an unload event handler, so let's just block it.
    if (!d->m_context->isValid())
        return false;

    d->m_storageSession = d->m_context->storageSession().platformSession();

    // FIXME: Do not use the sync version of shouldUseCredentialStorage when the client returns true from usesAsyncCallbacks.
    bool shouldUseCredentialStorage = !client() || client()->shouldUseCredentialStorage(this);

    d->m_needsSiteSpecificQuirks = d->m_context->needsSiteSpecificQuirks();

#if !PLATFORM(IOS)
    createNSURLConnection(
        ResourceHandle::makeDelegate(shouldUseCredentialStorage),
        shouldUseCredentialStorage,
        d->m_shouldContentSniff || d->m_context->localFileContentSniffingEnabled(),
        SchedulingBehavior::Asynchronous);
#else
    createNSURLConnection(
        d->m_proxy.get(),
        shouldUseCredentialStorage,
        d->m_shouldContentSniff || d->m_context->localFileContentSniffingEnabled(),
        SchedulingBehavior::Asynchronous,
        (NSDictionary *)client()->connectionProperties(this));
#endif

#if PLATFORM(IOS)
    NSURLConnection *urlConnection = connection();
    [urlConnection scheduleInRunLoop:WebThreadNSRunLoop() forMode:NSDefaultRunLoopMode];
    [urlConnection start];
#else
    bool scheduled = false;
    if (SchedulePairHashSet* scheduledPairs = d->m_context->scheduledRunLoopPairs()) {
        SchedulePairHashSet::iterator end = scheduledPairs->end();
        for (SchedulePairHashSet::iterator it = scheduledPairs->begin(); it != end; ++it) {
            if (NSRunLoop *runLoop = (*it)->nsRunLoop()) {
                [connection() scheduleInRunLoop:runLoop forMode:(NSString *)(*it)->mode()];
                scheduled = true;
            }
        }
    }

    if (client() && client()->usesAsyncCallbacks()) {
        ASSERT(!scheduled);
        [connection() setDelegateQueue:operationQueueForAsyncClients()];
        scheduled = true;
    }

    // Start the connection if we did schedule with at least one runloop.
    // We can't start the connection until we have one runloop scheduled.
    if (scheduled)
        [connection() start];
    else
        d->m_startWhenScheduled = true;
#endif

    LOG(Network, "Handle %p starting connection %p for %@", this, connection(), firstRequest().nsURLRequest(DoNotUpdateHTTPBody));
    
    if (d->m_connection) {
        if (d->m_defersLoading)
            wkSetNSURLConnectionDefersCallbacks(connection(), YES);

        return true;
    }

    END_BLOCK_OBJC_EXCEPTIONS;

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
        wkSetNSURLConnectionDefersCallbacks(d->m_connection.get(), defers);
}

#if PLATFORM(MAC)

void ResourceHandle::schedule(SchedulePair& pair)
{
    NSRunLoop *runLoop = pair.nsRunLoop();
    if (!runLoop)
        return;
    [d->m_connection.get() scheduleInRunLoop:runLoop forMode:(NSString *)pair.mode()];
    if (d->m_startWhenScheduled) {
        [d->m_connection.get() start];
        d->m_startWhenScheduled = false;
    }
}

void ResourceHandle::unschedule(SchedulePair& pair)
{
    if (NSRunLoop *runLoop = pair.nsRunLoop())
        [d->m_connection.get() unscheduleFromRunLoop:runLoop forMode:(NSString *)pair.mode()];
}

#endif

id ResourceHandle::makeDelegate(bool shouldUseCredentialStorage)
{
    ASSERT(!d->m_delegate);

    id <NSURLConnectionDelegate> delegate;
    if (client() && client()->usesAsyncCallbacks()) {
        if (shouldUseCredentialStorage)
            delegate = [[WebCoreResourceHandleAsOperationQueueDelegate alloc] initWithHandle:this];
        else
            delegate = [[WebCoreResourceHandleWithCredentialStorageAsOperationQueueDelegate alloc] initWithHandle:this];
    } else
        delegate = [[WebCoreResourceHandleAsDelegate alloc] initWithHandle:this];

    d->m_delegate = delegate;
    [delegate release];

    return d->m_delegate.get();
}

id ResourceHandle::delegate()
{
    if (!d->m_delegate)
        return makeDelegate(false);
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
    
bool ResourceHandle::loadsBlocked()
{
    return false;
}

CFStringRef ResourceHandle::synchronousLoadRunLoopMode()
{
    return CFSTR("WebCoreSynchronousLoaderRunLoopMode");
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    LOG(Network, "ResourceHandle::platformLoadResourceSynchronously:%@ allowStoredCredentials:%u", request.nsURLRequest(DoNotUpdateHTTPBody), storedCredentials);

    ASSERT(!request.isEmpty());
    
    OwnPtr<SynchronousLoaderClient> client = SynchronousLoaderClient::create();
    client->setAllowStoredCredentials(storedCredentials == AllowStoredCredentials);

    RefPtr<ResourceHandle> handle = adoptRef(new ResourceHandle(context, request, client.get(), false /*defersLoading*/, true /*shouldContentSniff*/));

    handle->d->m_storageSession = context->storageSession().platformSession();

    if (context && handle->d->m_scheduledFailureType != NoFailure) {
        error = context->blockedError(request);
        return;
    }

#if !PLATFORM(IOS)
    bool shouldUseCredentialStorage = storedCredentials == AllowStoredCredentials;
    handle->createNSURLConnection(
        handle->makeDelegate(shouldUseCredentialStorage),
        shouldUseCredentialStorage,
        handle->shouldContentSniff() || context->localFileContentSniffingEnabled(),
        SchedulingBehavior::Synchronous);
#else
    handle->createNSURLConnection(
        handle->delegate(), // A synchronous request cannot turn into a download, so there is no need to proxy the delegate.
        storedCredentials == AllowStoredCredentials,
        handle->shouldContentSniff() || (context && context->localFileContentSniffingEnabled()),
        SchedulingBehavior::Synchronous,
        (NSDictionary *)handle->client()->connectionProperties(handle.get()));
#endif

    [handle->connection() scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:(NSString *)synchronousLoadRunLoopMode()];
    [handle->connection() start];
    
    while (!client->isDone())
        [[NSRunLoop currentRunLoop] runMode:(NSString *)synchronousLoadRunLoopMode() beforeDate:[NSDate distantFuture]];

    error = client->error();
    
    [handle->connection() cancel];

    if (error.isNull())
        response = client->response();

    data.swap(client->mutableData());
}

void ResourceHandle::willSendRequest(ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT(!redirectResponse.isNull());

    if (redirectResponse.httpStatusCode() == 307) {
        String lastHTTPMethod = d->m_lastHTTPMethod;
        if (!equalIgnoringCase(lastHTTPMethod, request.httpMethod())) {
            request.setHTTPMethod(lastHTTPMethod);
    
            FormData* body = d->m_firstRequest.httpBody();
            if (!equalIgnoringCase(lastHTTPMethod, "GET") && body && !body->isEmpty())
                request.setHTTPBody(body);

            String originalContentType = d->m_firstRequest.httpContentType();
            if (!originalContentType.isEmpty())
                request.setHTTPHeaderField(HTTPHeaderName::ContentType, originalContentType);
        }
    }

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (!request.url().protocolIs("https") && protocolIs(request.httpReferrer(), "https") && d->m_context->shouldClearReferrerOnHTTPSToHTTPRedirect())
        request.clearHTTPReferrer();

    const URL& url = request.url();
    d->m_user = url.user();
    d->m_pass = url.pass();
    d->m_lastHTTPMethod = request.httpMethod();
    request.removeCredentials();

    if (!protocolHostAndPortAreEqual(request.url(), redirectResponse.url())) {
        // If the network layer carries over authentication headers from the original request
        // in a cross-origin redirect, we want to clear those headers here.
        // As of Lion, CFNetwork no longer does this.
        request.clearHTTPAuthorization();
    } else {
        // Only consider applying authentication credentials if this is actually a redirect and the redirect
        // URL didn't include credentials of its own.
        if (d->m_user.isEmpty() && d->m_pass.isEmpty() && !redirectResponse.isNull()) {
            Credential credential = CredentialStorage::get(request.url());
            if (!credential.isEmpty()) {
                d->m_initialCredential = credential;
                
                // FIXME: Support Digest authentication, and Proxy-Authorization.
                applyBasicAuthorizationHeader(request, d->m_initialCredential);
            }
        }
    }

    if (client()->usesAsyncCallbacks()) {
        client()->willSendRequestAsync(this, request, redirectResponse);
    } else {
        Ref<ResourceHandle> protect(*this);
        client()->willSendRequest(this, request, redirectResponse);

        // Client call may not preserve the session, especially if the request is sent over IPC.
        if (!request.isNull())
            request.setStorageSession(d->m_storageSession.get());
    }
}

void ResourceHandle::continueWillSendRequest(const ResourceRequest& request)
{
    ASSERT(client());
    ASSERT(client()->usesAsyncCallbacks());

    // Client call may not preserve the session, especially if the request is sent over IPC.
    ResourceRequest newRequest = request;
    if (!newRequest.isNull())
        newRequest.setStorageSession(d->m_storageSession.get());
    [(id)delegate() continueWillSendRequest:newRequest.nsURLRequest(UpdateHTTPBody)];
}

void ResourceHandle::continueDidReceiveResponse()
{
    ASSERT(client());
    ASSERT(client()->usesAsyncCallbacks());

    [delegate() continueDidReceiveResponse];
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    ASSERT(!client()->usesAsyncCallbacks());
    return client() && client()->shouldUseCredentialStorage(this);
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

    if (!d->m_user.isNull() && !d->m_pass.isNull()) {
        NSURLCredential *credential = [[NSURLCredential alloc] initWithUser:d->m_user
                                                                   password:d->m_pass
                                                                persistence:NSURLCredentialPersistenceForSession];
        d->m_currentMacChallenge = challenge.nsURLAuthenticationChallenge();
        d->m_currentWebChallenge = challenge;
        receivedCredential(challenge, core(credential));
        [credential release];
        // FIXME: Per the specification, the user shouldn't be asked for credentials if there were incorrect ones provided explicitly.
        d->m_user = String();
        d->m_pass = String();
        return;
    }

    // FIXME: Do not use the sync version of shouldUseCredentialStorage when the client returns true from usesAsyncCallbacks.
    if (!client() || client()->shouldUseCredentialStorage(this)) {
        if (!d->m_initialCredential.isEmpty() || challenge.previousFailureCount()) {
            // The stored credential wasn't accepted, stop using it.
            // There is a race condition here, since a different credential might have already been stored by another ResourceHandle,
            // but the observable effect should be very minor, if any.
            CredentialStorage::remove(challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            Credential credential = CredentialStorage::get(challenge.protectionSpace());
            if (!credential.isEmpty() && credential != d->m_initialCredential) {
                ASSERT(credential.persistence() == CredentialPersistenceNone);
                if (challenge.failureResponse().httpStatusCode() == 401) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    CredentialStorage::set(credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }
                [challenge.sender() useCredential:mac(credential) forAuthenticationChallenge:mac(challenge)];
                return;
            }
        }
    }

#if PLATFORM(IOS)
    // If the challenge is for a proxy protection space, look for default credentials in
    // the keychain.  CFNetwork used to handle this until WebCore was changed to always
    // return NO to -connectionShouldUseCredentialStorage: for <rdar://problem/7704943>.
    if (!challenge.previousFailureCount() && challenge.protectionSpace().isProxy()) {
        NSURLAuthenticationChallenge *macChallenge = mac(challenge);
        if (NSURLCredential *credential = [[NSURLCredentialStorage sharedCredentialStorage] defaultCredentialForProtectionSpace:[macChallenge protectionSpace]]) {
            [challenge.sender() useCredential:credential forAuthenticationChallenge:macChallenge];
            return;
        }
    }
#endif // PLATFORM(IOS)

    d->m_currentMacChallenge = challenge.nsURLAuthenticationChallenge();
    d->m_currentWebChallenge = core(d->m_currentMacChallenge);
    d->m_currentWebChallenge.setAuthenticationClient(this);

    // FIXME: Several concurrent requests can return with the an authentication challenge for the same protection space.
    // We should avoid making additional client calls for the same protection space when already waiting for the user,
    // because typing the same credentials several times is annoying.
    if (client())
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
}

void ResourceHandle::didCancelAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    ASSERT(d->m_currentMacChallenge);
    ASSERT(d->m_currentMacChallenge == challenge.nsURLAuthenticationChallenge());
    ASSERT(!d->m_currentWebChallenge.isNull());

    if (client())
        client()->didCancelAuthenticationChallenge(this, challenge);
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool ResourceHandle::canAuthenticateAgainstProtectionSpace(const ProtectionSpace& protectionSpace)
{
    if (client()->usesAsyncCallbacks()) {
        if (client())
            client()->canAuthenticateAgainstProtectionSpaceAsync(this, protectionSpace);
        else
            continueCanAuthenticateAgainstProtectionSpace(false);
        return false; // Ignored by caller.
    } else
        return client() && client()->canAuthenticateAgainstProtectionSpace(this, protectionSpace);
}

void ResourceHandle::continueCanAuthenticateAgainstProtectionSpace(bool result)
{
    ASSERT(client());
    ASSERT(client()->usesAsyncCallbacks());

    [(id)delegate() continueCanAuthenticateAgainstProtectionSpace:result];
}
#endif

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    LOG(Network, "Handle %p receivedCredential", this);

    ASSERT(!challenge.isNull());
    if (challenge != d->m_currentWebChallenge)
        return;

    // FIXME: Support empty credentials. Currently, an empty credential cannot be stored in WebCore credential storage, as that's empty value for its map.
    if (credential.isEmpty()) {
        receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    if (credential.persistence() == CredentialPersistenceForSession && (!d->m_needsSiteSpecificQuirks || ![[[mac(challenge) protectionSpace] host] isEqualToString:@"gallery.me.com"])) {
        // Manage per-session credentials internally, because once NSURLCredentialPersistenceForSession is used, there is no way
        // to ignore it for a particular request (short of removing it altogether).
        // <rdar://problem/6867598> gallery.me.com is temporarily whitelisted, so that QuickTime plug-in could see the credentials.
        Credential webCredential(credential, CredentialPersistenceNone);
        URL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = challenge.failureResponse().url();
        CredentialStorage::set(webCredential, ProtectionSpace([d->m_currentMacChallenge protectionSpace]), urlToStore);
        [[d->m_currentMacChallenge sender] useCredential:mac(webCredential) forAuthenticationChallenge:d->m_currentMacChallenge];
    } else
        [[d->m_currentMacChallenge sender] useCredential:mac(credential) forAuthenticationChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    LOG(Network, "Handle %p receivedRequestToContinueWithoutCredential", this);

    ASSERT(!challenge.isNull());
    if (challenge != d->m_currentWebChallenge)
        return;

    [[d->m_currentMacChallenge sender] continueWithoutCredentialForAuthenticationChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    LOG(Network, "Handle %p receivedCancellation", this);

    if (challenge != d->m_currentWebChallenge)
        return;

    if (client())
        client()->receivedCancellation(this, challenge);
}

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge& challenge)
{
    LOG(Network, "Handle %p receivedRequestToPerformDefaultHandling", this);

    ASSERT(!challenge.isNull());
    if (challenge != d->m_currentWebChallenge)
        return;

    [[d->m_currentMacChallenge sender] performDefaultHandlingForAuthenticationChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge& challenge)
{
    LOG(Network, "Handle %p receivedChallengeRejection", this);

    ASSERT(!challenge.isNull());
    if (challenge != d->m_currentWebChallenge)
        return;

    [[d->m_currentMacChallenge sender] rejectProtectionSpaceAndContinueWithChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::continueWillCacheResponse(NSCachedURLResponse *response)
{
    ASSERT(client());
    ASSERT(client()->usesAsyncCallbacks());

    [(id)delegate() continueWillCacheResponse:response];
}
    
#endif // !USE(CFNETWORK)
    
#if ENABLE(WEB_TIMING)
    
void ResourceHandle::getConnectionTimingData(NSDictionary *timingData, ResourceLoadTiming& timing)
{
    if (!timingData)
        return;

    // This is not the navigationStart time in monotonic time, but the other times are relative to this time
    // and only the differences between times are stored.
    double referenceStart = [[timingData valueForKey:@"_kCFNTimingDataFetchStart"] doubleValue];
            
    double domainLookupStart = [[timingData valueForKey:@"_kCFNTimingDataDomainLookupStart"] doubleValue];
    double domainLookupEnd = [[timingData valueForKey:@"_kCFNTimingDataDomainLookupEnd"] doubleValue];
    double connectStart = [[timingData valueForKey:@"_kCFNTimingDataConnectStart"] doubleValue];
    double secureConnectionStart = [[timingData valueForKey:@"_kCFNTimingDataSecureConnectionStart"] doubleValue];
    double connectEnd = [[timingData valueForKey:@"_kCFNTimingDataConnectEnd"] doubleValue];
    double requestStart = [[timingData valueForKey:@"_kCFNTimingDataRequestStart"] doubleValue];
    double responseStart = [[timingData valueForKey:@"_kCFNTimingDataResponseStart"] doubleValue];
        
    timing.domainLookupStart = domainLookupStart <= 0 ? -1 : (domainLookupStart - referenceStart) * 1000;
    timing.domainLookupEnd = domainLookupEnd <= 0 ? -1 : (domainLookupEnd - referenceStart) * 1000;
    timing.connectStart = connectStart <= 0 ? -1 : (connectStart - referenceStart) * 1000;
    timing.secureConnectionStart = secureConnectionStart <= 0 ? -1 : (secureConnectionStart - referenceStart) * 1000;
    timing.connectEnd = connectEnd <= 0 ? -1 : (connectEnd - referenceStart) * 1000;
    timing.requestStart = requestStart <= 0 ? 0 : (requestStart - referenceStart) * 1000;
    timing.responseStart = responseStart <= 0 ? 0 : (responseStart - referenceStart) * 1000;
}
    
#if USE(CFNETWORK)
    
void ResourceHandle::setCollectsTimingData()
{
    [NSURLConnection _setCollectsTimingData:YES];
}
    
void ResourceHandle::getConnectionTimingData(CFURLConnectionRef connection, ResourceLoadTiming& timing)
{
    getConnectionTimingData((__bridge NSDictionary*)(adoptCF(_CFURLConnectionCopyTimingData(connection)).get()), timing);
}
    
#else
    
void ResourceHandle::getConnectionTimingData(NSURLConnection *connection, ResourceLoadTiming& timing)
{
    getConnectionTimingData([connection _timingData], timing);
}
    
#endif
    
#endif // ENABLE(WEB_TIMING)

} // namespace WebCore

