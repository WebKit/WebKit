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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#if !USE(CFNETWORK)

#import "AuthenticationChallenge.h"
#import "AuthenticationMac.h"
#import "BlockExceptions.h"
#import "CookieStorage.h"
#import "CredentialStorage.h"
#import "CachedResourceLoader.h"
#import "EmptyProtocolDefinitions.h"
#import "FormDataStreamMac.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "Logging.h"
#import "MIMETypeRegistry.h"
#import "NetworkingContext.h"
#import "Page.h"
#import "ResourceError.h"
#import "ResourceResponse.h"
#import "Settings.h"
#import "SharedBuffer.h"
#import "SubresourceLoader.h"
#import "WebCoreSystemInterface.h"
#import "WebCoreURLResponse.h"
#import <wtf/SchedulePair.h>
#import <wtf/UnusedParam.h>
#import <wtf/text/Base64.h>
#import <wtf/text/CString.h>

using namespace WebCore;

@interface WebCoreResourceHandleAsDelegate : NSObject <NSURLConnectionDelegate> {
    ResourceHandle* m_handle;
}
- (id)initWithHandle:(ResourceHandle*)handle;
- (void)detachHandle;
@end

// WebCoreNSURLConnectionDelegateProxy exists so that we can cast m_proxy to it in order
// to disambiguate the argument type in the -setDelegate: call.  This avoids a spurious
// warning that the compiler would otherwise emit.
@interface WebCoreNSURLConnectionDelegateProxy : NSObject <NSURLConnectionDelegate>
- (void)setDelegate:(id<NSURLConnectionDelegate>)delegate;
@end

@interface NSURLConnection (Details)
-(id)_initWithRequest:(NSURLRequest *)request delegate:(id)delegate usesCache:(BOOL)usesCacheFlag maxContentLength:(long long)maxContentLength startImmediately:(BOOL)startImmediately connectionProperties:(NSDictionary *)connectionProperties;
@end

@interface NSURLRequest (Details)
- (id)_propertyForKey:(NSString *)key;
@end

class WebCoreSynchronousLoaderClient : public ResourceHandleClient {
public:
    static PassOwnPtr<WebCoreSynchronousLoaderClient> create()
    {
        return adoptPtr(new WebCoreSynchronousLoaderClient);
    }

    virtual ~WebCoreSynchronousLoaderClient();

    void setAllowStoredCredentials(bool allow) { m_allowStoredCredentials = allow; }
    NSURLResponse *response() { return m_response; }
    NSMutableData *data() { return m_data; }
    NSError *error() { return m_error; }
    bool isDone() { return m_isDone; }

private:
    WebCoreSynchronousLoaderClient()
        : m_allowStoredCredentials(false)
        , m_response(0)
        , m_data(0)
        , m_error(0)
        , m_isDone(false)
    {
    }

    virtual void willSendRequest(ResourceHandle*, ResourceRequest&, const ResourceResponse& /*redirectResponse*/);
    virtual bool shouldUseCredentialStorage(ResourceHandle*);
    virtual void didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge&);
    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int /*encodedDataLength*/);
    virtual void didFinishLoading(ResourceHandle*, double /*finishTime*/);
    virtual void didFail(ResourceHandle*, const ResourceError&);
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(ResourceHandle*, const ProtectionSpace&);
#endif

    bool m_allowStoredCredentials;
    NSURLResponse *m_response;
    NSMutableData *m_data;
    NSError *m_error;
    bool m_isDone;
};

namespace WebCore {

static void applyBasicAuthorizationHeader(ResourceRequest& request, const Credential& credential)
{
    String authenticationHeader = "Basic " + base64Encode(String(credential.user() + ":" + credential.password()).utf8());
    request.clearHTTPAuthorization(); // FIXME: Should addHTTPHeaderField be smart enough to not build comma-separated lists in headers like Authorization?
    request.addHTTPHeaderField("Authorization", authenticationHeader);
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

void ResourceHandle::createNSURLConnection(id delegate, bool shouldUseCredentialStorage, bool shouldContentSniff)
{
    // Credentials for ftp can only be passed in URL, the connection:didReceiveAuthenticationChallenge: delegate call won't be made.
    if ((!d->m_user.isEmpty() || !d->m_pass.isEmpty()) && !firstRequest().url().protocolIsInHTTPFamily()) {
        KURL urlWithCredentials(firstRequest().url());
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

    NSMutableDictionary *streamProperties = [NSMutableDictionary dictionary];

    if (!shouldUseCredentialStorage)
        [streamProperties setObject:@"WebKitPrivateSession" forKey:@"_kCFURLConnectionSessionID"];

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    RetainPtr<CFDataRef> sourceApplicationAuditData = d->m_context->sourceApplicationAuditData();
    if (sourceApplicationAuditData)
        [streamProperties setObject:(NSData *)sourceApplicationAuditData.get() forKey:@"kCFStreamPropertySourceApplication"];
#endif

    NSDictionary *propertyDictionary = [NSDictionary dictionaryWithObject:streamProperties forKey:@"kCFURLConnectionSocketStreamProperties"];
    d->m_connection.adoptNS([[NSURLConnection alloc] _initWithRequest:nsRequest delegate:delegate usesCache:YES maxContentLength:0 startImmediately:NO connectionProperties:propertyDictionary]);
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

    ASSERT(!d->m_proxy);
    d->m_proxy.adoptNS(wkCreateNSURLConnectionDelegateProxy());
    [static_cast<WebCoreNSURLConnectionDelegateProxy*>(d->m_proxy.get()) setDelegate:ResourceHandle::delegate()];

    bool shouldUseCredentialStorage = !client() || client()->shouldUseCredentialStorage(this);

    d->m_needsSiteSpecificQuirks = d->m_context->needsSiteSpecificQuirks();

    createNSURLConnection(
        d->m_proxy.get(),
        shouldUseCredentialStorage,
        d->m_shouldContentSniff || d->m_context->localFileContentSniffingEnabled());

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

    if (NSOperationQueue *operationQueue = d->m_context->scheduledOperationQueue()) {
        ASSERT(!scheduled);
        [connection() setDelegateQueue:operationQueue];
        scheduled = true;
    }

    // Start the connection if we did schedule with at least one runloop.
    // We can't start the connection until we have one runloop scheduled.
    if (scheduled)
        [connection() start];
    else
        d->m_startWhenScheduled = true;

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

void ResourceHandle::schedule(SchedulePair* pair)
{
    NSRunLoop *runLoop = pair->nsRunLoop();
    if (!runLoop)
        return;
    [d->m_connection.get() scheduleInRunLoop:runLoop forMode:(NSString *)pair->mode()];
    if (d->m_startWhenScheduled) {
        [d->m_connection.get() start];
        d->m_startWhenScheduled = false;
    }
}

void ResourceHandle::unschedule(SchedulePair* pair)
{
    if (NSRunLoop *runLoop = pair->nsRunLoop())
        [d->m_connection.get() unscheduleFromRunLoop:runLoop forMode:(NSString *)pair->mode()];
}

WebCoreResourceHandleAsDelegate *ResourceHandle::delegate()
{
    if (!d->m_delegate) {
        WebCoreResourceHandleAsDelegate *delegate = [[WebCoreResourceHandleAsDelegate alloc] initWithHandle:this];
        d->m_delegate = delegate;
        [delegate release];
    }
    return d->m_delegate.get();
}

void ResourceHandle::releaseDelegate()
{
    if (!d->m_delegate)
        return;
    if (d->m_proxy)
        [d->m_proxy.get() setDelegate:nil];
    [d->m_delegate.get() detachHandle];
    d->m_delegate = nil;
}

id ResourceHandle::releaseProxy()
{
    id proxy = [[d->m_proxy.get() retain] autorelease];
    d->m_proxy = nil;
    [proxy setDelegate:nil];
    return proxy;
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

    NSError *nsError = nil;
    NSURLResponse *nsURLResponse = nil;
    NSData *result = nil;

    ASSERT(!request.isEmpty());
    
    OwnPtr<WebCoreSynchronousLoaderClient> client = WebCoreSynchronousLoaderClient::create();
    client->setAllowStoredCredentials(storedCredentials == AllowStoredCredentials);

    RefPtr<ResourceHandle> handle = adoptRef(new ResourceHandle(context, request, client.get(), false /*defersLoading*/, true /*shouldContentSniff*/));

    handle->d->m_storageSession = context->storageSession().platformSession();

    if (context && handle->d->m_scheduledFailureType != NoFailure) {
        error = context->blockedError(request);
        return;
    }

    handle->createNSURLConnection(
        handle->delegate(), // A synchronous request cannot turn into a download, so there is no need to proxy the delegate.
        storedCredentials == AllowStoredCredentials,
        handle->shouldContentSniff() || context->localFileContentSniffingEnabled());

    [handle->connection() scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:(NSString *)synchronousLoadRunLoopMode()];
    [handle->connection() start];
    
    while (!client->isDone())
        [[NSRunLoop currentRunLoop] runMode:(NSString *)synchronousLoadRunLoopMode() beforeDate:[NSDate distantFuture]];

    result = client->data();
    nsURLResponse = client->response();
    nsError = client->error();
    
    [handle->connection() cancel];


    if (!nsError)
        response = nsURLResponse;
    else {
        response = ResourceResponse(request.url(), String(), 0, String(), String());
        if ([nsError domain] == NSURLErrorDomain)
            switch ([nsError code]) {
                case NSURLErrorUserCancelledAuthentication:
                    // FIXME: we should really return the actual HTTP response, but sendSynchronousRequest doesn't provide us with one.
                    response.setHTTPStatusCode(401);
                    break;
                default:
                    response.setHTTPStatusCode([nsError code]);
            }
        else
            response.setHTTPStatusCode(404);
    }
    
    data.resize([result length]);
    memcpy(data.data(), [result bytes], [result length]);
    
    error = nsError;
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
                request.setHTTPHeaderField("Content-Type", originalContentType);
        }
    }

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (!request.url().protocolIs("https") && protocolIs(request.httpReferrer(), "https") && d->m_context->shouldClearReferrerOnHTTPSToHTTPRedirect())
        request.clearHTTPReferrer();

    const KURL& url = request.url();
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

    RefPtr<ResourceHandle> protect(this);
    client()->willSendRequest(this, request, redirectResponse);

    // Client call may not preserve the session, especially if the request is sent over IPC.
    if (!request.isNull())
        request.setStorageSession(d->m_storageSession.get());
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    if (client())
        return client()->shouldUseCredentialStorage(this);

    return false;
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
    if (client())
        return client()->canAuthenticateAgainstProtectionSpace(this, protectionSpace);
        
    return false;
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
        KURL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = challenge.failureResponse().url();
        CredentialStorage::set(webCredential, core([d->m_currentMacChallenge protectionSpace]), urlToStore);
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

} // namespace WebCore

@implementation WebCoreResourceHandleAsDelegate

- (id)initWithHandle:(ResourceHandle*)handle
{
    self = [self init];
    if (!self)
        return nil;
    m_handle = handle;
    return self;
}

- (void)detachHandle
{
    m_handle = 0;
}

- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    UNUSED_PARAM(connection);

    if (!m_handle || !m_handle->client())
        return nil;
    
    // See <rdar://problem/5380697>. This is a workaround for a behavior change in CFNetwork where willSendRequest gets called more often.
    if (!redirectResponse)
        return newRequest;

#if !LOG_DISABLED
    if ([redirectResponse isKindOfClass:[NSHTTPURLResponse class]])
        LOG(Network, "Handle %p delegate connection:%p willSendRequest:%@ redirectResponse:%d, Location:<%@>", m_handle, connection, [newRequest description], static_cast<int>([(id)redirectResponse statusCode]), [[(id)redirectResponse allHeaderFields] objectForKey:@"Location"]);
    else
        LOG(Network, "Handle %p delegate connection:%p willSendRequest:%@ redirectResponse:non-HTTP", m_handle, connection, [newRequest description]); 
#endif

    ResourceRequest request = newRequest;

    m_handle->willSendRequest(request, redirectResponse);

    return request.nsURLRequest(UpdateHTTPBody);
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connectionShouldUseCredentialStorage:%p", m_handle, connection);

    if (!m_handle)
        return NO;

    return m_handle->shouldUseCredentialStorage();
}

- (void)connection:(NSURLConnection *)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didReceiveAuthenticationChallenge:%p", m_handle, connection, challenge);

    if (!m_handle) {
        [[challenge sender] cancelAuthenticationChallenge:challenge];
        return;
    }
    m_handle->didReceiveAuthenticationChallenge(core(challenge));
}

- (void)connection:(NSURLConnection *)connection didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    // FIXME: We probably don't need to implement this (see <rdar://problem/8960124>).

    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didCancelAuthenticationChallenge:%p", m_handle, connection, challenge);

    if (!m_handle)
        return;
    m_handle->didCancelAuthenticationChallenge(core(challenge));
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
- (BOOL)connection:(NSURLConnection *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p canAuthenticateAgainstProtectionSpace:%@://%@:%u realm:%@ method:%@ %@%@", m_handle, connection, [protectionSpace protocol], [protectionSpace host], [protectionSpace port], [protectionSpace realm], [protectionSpace authenticationMethod], [protectionSpace isProxy] ? @"proxy:" : @"", [protectionSpace isProxy] ? [protectionSpace proxyType] : @"");

    if (!m_handle)
        return NO;

    return m_handle->canAuthenticateAgainstProtectionSpace(core(protectionSpace));
}
#endif

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)r
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didReceiveResponse:%p (HTTP status %d, reported MIMEType '%s')", m_handle, connection, r, [r respondsToSelector:@selector(statusCode)] ? [(id)r statusCode] : 0, [[r MIMEType] UTF8String]);

    if (!m_handle || !m_handle->client())
        return;

    // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
    int statusCode = [r respondsToSelector:@selector(statusCode)] ? [(id)r statusCode] : 0;
    if (statusCode != 304)
        adjustMIMETypeIfNecessary([r _CFURLResponse]);

    if ([m_handle->firstRequest().nsURLRequest(DoNotUpdateHTTPBody) _propertyForKey:@"ForceHTMLMIMEType"])
        [r _setMIMEType:@"text/html"];

    m_handle->client()->didReceiveResponse(m_handle, r);
}

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
- (void)connection:(NSURLConnection *)connection didReceiveDataArray:(NSArray *)dataArray
{
    UNUSED_PARAM(connection);
    LOG(Network, "Handle %p delegate connection:%p didReceiveDataArray:%p arraySize:%d", m_handle, connection, dataArray, [dataArray count]);

    if (!dataArray)
        return;

    if (!m_handle || !m_handle->client())
        return;

    m_handle->handleDataArray(reinterpret_cast<CFArrayRef>(dataArray));
    // The call to didReceiveData above can cancel a load, and if so, the delegate (self) could have been deallocated by this point.
}
#endif

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    UNUSED_PARAM(connection);
    UNUSED_PARAM(lengthReceived);

    LOG(Network, "Handle %p delegate connection:%p didReceiveData:%p lengthReceived:%lld", m_handle, connection, data, lengthReceived);

    if (!m_handle || !m_handle->client())
        return;
    // FIXME: If we get more than 2B bytes in a single chunk, this code won't do the right thing.
    // However, with today's computers and networking speeds, this won't happen in practice.
    // Could be an issue with a giant local file.

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=19793
    // -1 means we do not provide any data about transfer size to inspector so it would use
    // Content-Length headers or content size to show transfer size.
    m_handle->client()->didReceiveBuffer(m_handle, SharedBuffer::wrapNSData(data), -1);
}

- (void)connection:(NSURLConnection *)connection willStopBufferingData:(NSData *)data
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p willStopBufferingData:%p", m_handle, connection, data);

    if (!m_handle || !m_handle->client())
        return;
    // FIXME: If we get a resource with more than 2B bytes, this code won't do the right thing.
    // However, with today's computers and networking speeds, this won't happen in practice.
    // Could be an issue with a giant local file.
    m_handle->client()->willStopBufferingData(m_handle, (const char*)[data bytes], static_cast<int>([data length]));
}

- (void)connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
    UNUSED_PARAM(connection);
    UNUSED_PARAM(bytesWritten);

    LOG(Network, "Handle %p delegate connection:%p didSendBodyData:%d totalBytesWritten:%d totalBytesExpectedToWrite:%d", m_handle, connection, bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);

    if (!m_handle || !m_handle->client())
        return;
    m_handle->client()->didSendData(m_handle, totalBytesWritten, totalBytesExpectedToWrite);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connectionDidFinishLoading:%p", m_handle, connection);

    if (!m_handle || !m_handle->client())
        return;

    m_handle->client()->didFinishLoading(m_handle, 0);
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didFailWithError:%@", m_handle, connection, error);

    if (!m_handle || !m_handle->client())
        return;

    m_handle->client()->didFail(m_handle, error);
}


- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
    LOG(Network, "Handle %p delegate connection:%p willCacheResponse:%p", m_handle, connection, cachedResponse);

    UNUSED_PARAM(connection);

    if (!m_handle || !m_handle->client())
        return nil;

    // Workaround for <rdar://problem/6300990> Caching does not respect Vary HTTP header.
    // FIXME: WebCore cache has issues with Vary, too (bug 58797, bug 71509).
    if ([[cachedResponse response] isKindOfClass:[NSHTTPURLResponse class]]
        && [[(NSHTTPURLResponse *)[cachedResponse response] allHeaderFields] objectForKey:@"Vary"])
        return nil;

    NSCachedURLResponse *newResponse = m_handle->client()->willCacheResponse(m_handle, cachedResponse);
    if (newResponse != cachedResponse)
        return newResponse;
    
    return newResponse;
}

@end


WebCoreSynchronousLoaderClient::~WebCoreSynchronousLoaderClient()
{
    [m_response release];
    [m_data release];
    [m_error release];
}

void WebCoreSynchronousLoaderClient::willSendRequest(ResourceHandle* handle, ResourceRequest& request, const ResourceResponse& /*redirectResponse*/)
{
    // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
    if (!protocolHostAndPortAreEqual(handle->firstRequest().url(), request.url())) {
        ASSERT(!m_error);
        m_error = [[NSError alloc] initWithDomain:NSURLErrorDomain code:NSURLErrorBadServerResponse userInfo:nil];
        m_isDone = true;
        request = 0;
        return;
    }
}

bool WebCoreSynchronousLoaderClient::shouldUseCredentialStorage(ResourceHandle*)
{
    // FIXME: We should ask FrameLoaderClient whether using credential storage is globally forbidden.
    return m_allowStoredCredentials;
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool WebCoreSynchronousLoaderClient::canAuthenticateAgainstProtectionSpace(ResourceHandle*, const ProtectionSpace&)
{
    // FIXME: We should ask FrameLoaderClient. <http://webkit.org/b/65196>
    return true;
}
#endif

void WebCoreSynchronousLoaderClient::didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge& challenge)
{
    // FIXME: The user should be asked for credentials, as in async case.
    [challenge.sender() continueWithoutCredentialForAuthenticationChallenge:challenge.nsURLAuthenticationChallenge()];
}

void WebCoreSynchronousLoaderClient::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    [m_response release];
    m_response = [response.nsURLResponse() copy];
}

void WebCoreSynchronousLoaderClient::didReceiveData(ResourceHandle*, const char* data, int length, int /*encodedDataLength*/)
{
    if (!m_data)
        m_data = [[NSMutableData alloc] init];
    [m_data appendBytes:data length:length];
}

void WebCoreSynchronousLoaderClient::didFinishLoading(ResourceHandle*, double)
{
    m_isDone = true;
}

void WebCoreSynchronousLoaderClient::didFail(ResourceHandle*, const ResourceError& error)
{
    ASSERT(!m_error);

    m_error = [error copy];
    m_isDone = true;
}


#endif // !USE(CFNETWORK)
