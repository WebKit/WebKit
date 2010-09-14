/*
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#import "AuthenticationChallenge.h"
#import "AuthenticationMac.h"
#import "Base64.h"
#import "BlobRegistry.h"
#import "BlockExceptions.h"
#import "CredentialStorage.h"
#import "CachedResourceLoader.h"
#import "EmptyProtocolDefinitions.h"
#import "FormDataStreamMac.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "Logging.h"
#import "MIMETypeRegistry.h"
#import "Page.h"
#import "ResourceError.h"
#import "ResourceResponse.h"
#import "SchedulePair.h"
#import "Settings.h"
#import "SharedBuffer.h"
#import "SubresourceLoader.h"
#import "WebCoreSystemInterface.h"
#import "WebCoreURLResponse.h"
#import <wtf/text/CString.h>
#import <wtf/UnusedParam.h>

#ifdef BUILDING_ON_TIGER
typedef int NSInteger;
#endif

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

@interface NSURLConnection (NSURLConnectionTigerPrivate)
- (NSData *)_bufferedData;
@end

@interface NSURLConnection (Details)
-(id)_initWithRequest:(NSURLRequest *)request delegate:(id)delegate usesCache:(BOOL)usesCacheFlag maxContentLength:(long long)maxContentLength startImmediately:(BOOL)startImmediately connectionProperties:(NSDictionary *)connectionProperties;
@end

@interface NSURLRequest (Details)
- (id)_propertyForKey:(NSString *)key;
@end

#ifndef BUILDING_ON_TIGER

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
    virtual void didReceiveData(ResourceHandle*, const char*, int, int /*lengthReceived*/);
    virtual void didFinishLoading(ResourceHandle*);
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

static NSString *WebCoreSynchronousLoaderRunLoopMode = @"WebCoreSynchronousLoaderRunLoopMode";

#endif

namespace WebCore {

#ifdef BUILDING_ON_TIGER
static unsigned inNSURLConnectionCallback;
#endif

#ifndef NDEBUG
static bool isInitializingConnection;
#endif
    
class CallbackGuard {
public:
    CallbackGuard()
    {
#ifdef BUILDING_ON_TIGER
        ++inNSURLConnectionCallback;
#endif
    }
    ~CallbackGuard()
    {
#ifdef BUILDING_ON_TIGER
        ASSERT(inNSURLConnectionCallback > 0);
        --inNSURLConnectionCallback;
#endif
    }
};

#ifndef BUILDING_ON_TIGER
static String encodeBasicAuthorization(const String& user, const String& password)
{
    CString unencodedString = (user + ":" + password).utf8();
    Vector<char> unencoded(unencodedString.length());
    std::copy(unencodedString.data(), unencodedString.data() + unencodedString.length(), unencoded.begin());
    Vector<char> encoded;
    base64Encode(unencoded, encoded);
    return String(encoded.data(), encoded.size());
}
#endif

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
    releaseDelegate();
    d->m_currentWebChallenge.setAuthenticationClient(0);

    LOG(Network, "Handle %p destroyed", this);
}

static const double MaxFoundationVersionWithoutdidSendBodyDataDelegate = 677.21;
bool ResourceHandle::didSendBodyDataDelegateExists()
{
    return NSFoundationVersionNumber > MaxFoundationVersionWithoutdidSendBodyDataDelegate;
}

void ResourceHandle::createNSURLConnection(id delegate, bool shouldUseCredentialStorage, bool shouldContentSniff)
{
    // Credentials for ftp can only be passed in URL, the connection:didReceiveAuthenticationChallenge: delegate call won't be made.
    if ((!d->m_user.isEmpty() || !d->m_pass.isEmpty())
#ifndef BUILDING_ON_TIGER
     && !firstRequest().url().protocolInHTTPFamily() // On Tiger, always pass credentials in URL, so that they get stored even if the request gets cancelled right away.
#endif
    ) {
        KURL urlWithCredentials(firstRequest().url());
        urlWithCredentials.setUser(d->m_user);
        urlWithCredentials.setPass(d->m_pass);
        firstRequest().setURL(urlWithCredentials);
    }

    // If a URL already has cookies, then we'll relax the 3rd party cookie policy and accept new cookies.
    NSHTTPCookieStorage *sharedStorage = [NSHTTPCookieStorage sharedHTTPCookieStorage];
    if ([sharedStorage cookieAcceptPolicy] == NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain && [[sharedStorage cookiesForURL:firstRequest().url()] count])
        firstRequest().setFirstPartyForCookies(firstRequest().url());

#if !defined(BUILDING_ON_TIGER)
    if (shouldUseCredentialStorage && firstRequest().url().protocolInHTTPFamily()) {
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
        String authHeader = "Basic " + encodeBasicAuthorization(d->m_initialCredential.user(), d->m_initialCredential.password());
        firstRequest().addHTTPHeaderField("Authorization", authHeader);
    }

    NSURLRequest *nsRequest = firstRequest().nsURLRequest();
    if (!shouldContentSniff) {
        NSMutableURLRequest *mutableRequest = [[nsRequest copy] autorelease];
        wkSetNSURLRequestShouldContentSniff(mutableRequest, NO);
        nsRequest = mutableRequest;
    }

#if !defined(BUILDING_ON_LEOPARD)
    ASSERT([NSURLConnection instancesRespondToSelector:@selector(_initWithRequest:delegate:usesCache:maxContentLength:startImmediately:connectionProperties:)]);
    static bool supportsSettingConnectionProperties = true;
#else
    static bool supportsSettingConnectionProperties = [NSURLConnection instancesRespondToSelector:@selector(_initWithRequest:delegate:usesCache:maxContentLength:startImmediately:connectionProperties:)];
#endif

    if (supportsSettingConnectionProperties) {
        NSDictionary *sessionID = shouldUseCredentialStorage ? [NSDictionary dictionary] : [NSDictionary dictionaryWithObject:@"WebKitPrivateSession" forKey:@"_kCFURLConnectionSessionID"];
        NSDictionary *propertyDictionary = [NSDictionary dictionaryWithObject:sessionID forKey:@"kCFURLConnectionSocketStreamProperties"];
        d->m_connection.adoptNS([[NSURLConnection alloc] _initWithRequest:nsRequest delegate:delegate usesCache:YES maxContentLength:0 startImmediately:NO connectionProperties:propertyDictionary]);
        return;
    }

    d->m_connection.adoptNS([[NSURLConnection alloc] initWithRequest:nsRequest delegate:delegate startImmediately:NO]);
    return;

#else
    // Building on Tiger. Don't use WebCore credential storage, don't try to disable content sniffing.
    UNUSED_PARAM(shouldUseCredentialStorage);
    UNUSED_PARAM(shouldContentSniff);
    d->m_connection.adoptNS([[NSURLConnection alloc] initWithRequest:firstRequest().nsURLRequest() delegate:delegate]);
#endif
}

bool ResourceHandle::start(NetworkingContext* context)
{
    if (!context)
        return false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // If NetworkingContext is invalid then we are no longer attached to a Page,
    // this must be an attempted load from an unload event handler, so let's just block it.
    if (!context->isValid())
        return false;

#ifndef NDEBUG
    isInitializingConnection = YES;
#endif

    ASSERT(!d->m_proxy);
    d->m_proxy.adoptNS(wkCreateNSURLConnectionDelegateProxy());
    [static_cast<WebCoreNSURLConnectionDelegateProxy*>(d->m_proxy.get()) setDelegate:ResourceHandle::delegate()];

    bool shouldUseCredentialStorage = !client() || client()->shouldUseCredentialStorage(this);

    if (!ResourceHandle::didSendBodyDataDelegateExists())
        associateStreamWithResourceHandle([firstRequest().nsURLRequest() HTTPBodyStream], this);

#ifdef BUILDING_ON_TIGER
    // A conditional request sent by WebCore (e.g. to update appcache) can be for a resource that is not cacheable by NSURLConnection,
    // which can get confused and fail to load it in this case.
    if (firstRequest().isConditional())
        firstRequest().setCachePolicy(ReloadIgnoringCacheData);
#endif

    d->m_needsSiteSpecificQuirks = context->needsSiteSpecificQuirks();

    createNSURLConnection(
        d->m_proxy.get(),
        shouldUseCredentialStorage,
        d->m_shouldContentSniff || context->localFileContentSniffingEnabled());

#ifndef BUILDING_ON_TIGER
    bool scheduled = false;
    if (SchedulePairHashSet* scheduledPairs = context->scheduledRunLoopPairs()) {
        SchedulePairHashSet::iterator end = scheduledPairs->end();
        for (SchedulePairHashSet::iterator it = scheduledPairs->begin(); it != end; ++it) {
            if (NSRunLoop *runLoop = (*it)->nsRunLoop()) {
                [connection() scheduleInRunLoop:runLoop forMode:(NSString *)(*it)->mode()];
                scheduled = true;
            }
        }
    }

    // Start the connection if we did schedule with at least one runloop.
    // We can't start the connection until we have one runloop scheduled.
    if (scheduled)
        [connection() start];
    else
        d->m_startWhenScheduled = true;
#endif

#ifndef NDEBUG
    isInitializingConnection = NO;
#endif

    LOG(Network, "Handle %p starting connection %p for %@", this, connection(), firstRequest().nsURLRequest());
    
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

    if (!ResourceHandle::didSendBodyDataDelegateExists())
        disassociateStreamWithResourceHandle([firstRequest().nsURLRequest() HTTPBodyStream]);
    [d->m_connection.get() cancel];
}

void ResourceHandle::platformSetDefersLoading(bool defers)
{
    if (d->m_connection)
        wkSetNSURLConnectionDefersCallbacks(d->m_connection.get(), defers);
}

void ResourceHandle::schedule(SchedulePair* pair)
{
#ifndef BUILDING_ON_TIGER
    NSRunLoop *runLoop = pair->nsRunLoop();
    if (!runLoop)
        return;
    [d->m_connection.get() scheduleInRunLoop:runLoop forMode:(NSString *)pair->mode()];
    if (d->m_startWhenScheduled) {
        [d->m_connection.get() start];
        d->m_startWhenScheduled = false;
    }
#else
    UNUSED_PARAM(pair);
#endif
}

void ResourceHandle::unschedule(SchedulePair* pair)
{
#ifndef BUILDING_ON_TIGER
    if (NSRunLoop *runLoop = pair->nsRunLoop())
        [d->m_connection.get() unscheduleFromRunLoop:runLoop forMode:(NSString *)pair->mode()];
#else
    UNUSED_PARAM(pair);
#endif
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

bool ResourceHandle::supportsBufferedData()
{
    static bool supportsBufferedData = [NSURLConnection instancesRespondToSelector:@selector(_bufferedData)];
    return supportsBufferedData;
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    if (supportsBufferedData())
        return SharedBuffer::wrapNSData([d->m_connection.get() _bufferedData]);

    return 0;
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
#ifndef BUILDING_ON_TIGER
    return false;
#else
    // On Tiger, if we're in an NSURLConnection callback, that blocks all other NSURLConnection callbacks.
    // On Leopard and newer, it blocks only callbacks on that same NSURLConnection object, which is not
    // a problem in practice.
    return inNSURLConnectionCallback != 0;
#endif
}

bool ResourceHandle::willLoadFromCache(ResourceRequest& request, Frame*)
{
#ifndef BUILDING_ON_TIGER
    request.setCachePolicy(ReturnCacheDataDontLoad);
    NSURLResponse *nsURLResponse = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
   [NSURLConnection sendSynchronousRequest:request.nsURLRequest() returningResponse:&nsURLResponse error:nil];
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return nsURLResponse;
#else
    // <rdar://problem/6803217> - Re-enable after <rdar://problem/6786454> is resolved.
    UNUSED_PARAM(request);
    return false;
#endif
}

void ResourceHandle::loadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    LOG(Network, "ResourceHandle::loadResourceSynchronously:%@ allowStoredCredentials:%u", request.nsURLRequest(), storedCredentials);

#if ENABLE(BLOB)
    if (request.url().protocolIs("blob"))
        if (blobRegistry().loadResourceSynchronously(request, error, response, data))
            return;
#endif

    NSError *nsError = nil;
    NSURLResponse *nsURLResponse = nil;
    NSData *result = nil;

    ASSERT(!request.isEmpty());
    
#ifndef BUILDING_ON_TIGER
    OwnPtr<WebCoreSynchronousLoaderClient> client = WebCoreSynchronousLoaderClient::create();
    client->setAllowStoredCredentials(storedCredentials == AllowStoredCredentials);

    RefPtr<ResourceHandle> handle = adoptRef(new ResourceHandle(request, client.get(), false /*defersLoading*/, true /*shouldContentSniff*/));

    if (context && handle->d->m_scheduledFailureType != NoFailure) {
        error = context->blockedError(request);
        return;
    }

    handle->createNSURLConnection(
        handle->delegate(), // A synchronous request cannot turn into a download, so there is no need to proxy the delegate.
        storedCredentials == AllowStoredCredentials,
        handle->shouldContentSniff() || (context && context->localFileContentSniffingEnabled()));

    [handle->connection() scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:WebCoreSynchronousLoaderRunLoopMode];
    [handle->connection() start];
    
    while (!client->isDone())
        [[NSRunLoop currentRunLoop] runMode:WebCoreSynchronousLoaderRunLoopMode beforeDate:[NSDate distantFuture]];

    result = client->data();
    nsURLResponse = client->response();
    nsError = client->error();
    
    [handle->connection() cancel];

#else
    UNUSED_PARAM(storedCredentials);
    UNUSED_PARAM(context);
    NSURLRequest *firstRequest = request.nsURLRequest();

    // If a URL already has cookies, then we'll relax the 3rd party cookie policy and accept new cookies.
    NSHTTPCookieStorage *sharedStorage = [NSHTTPCookieStorage sharedHTTPCookieStorage];
    if ([sharedStorage cookieAcceptPolicy] == NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain && [[sharedStorage cookiesForURL:[firstRequest URL]] count]) {
        NSMutableURLRequest *mutableRequest = [[firstRequest mutableCopy] autorelease];
        [mutableRequest setMainDocumentURL:[mutableRequest URL]];
        firstRequest = mutableRequest;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    result = [NSURLConnection sendSynchronousRequest:firstRequest returningResponse:&nsURLResponse error:&nsError];
    END_BLOCK_OBJC_EXCEPTIONS;
#endif

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
    const KURL& url = request.url();
    d->m_user = url.user();
    d->m_pass = url.pass();
    d->m_lastHTTPMethod = request.httpMethod();
    request.removeCredentials();

    client()->willSendRequest(this, request, redirectResponse);
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

#ifndef BUILDING_ON_TIGER
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
                    CredentialStorage::set(credential, challenge.protectionSpace(), firstRequest().url());
                }
                [challenge.sender() useCredential:mac(credential) forAuthenticationChallenge:mac(challenge)];
                return;
            }
        }
    }
#endif

    d->m_currentMacChallenge = challenge.nsURLAuthenticationChallenge();
    d->m_currentWebChallenge = core(d->m_currentMacChallenge);
    d->m_currentWebChallenge.setAuthenticationClient(this);

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
    ASSERT(!challenge.isNull());
    if (challenge != d->m_currentWebChallenge)
        return;

    // FIXME: Support empty credentials. Currently, an empty credential cannot be stored in WebCore credential storage, as that's empty value for its map.
    if (credential.isEmpty()) {
        receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

#ifdef BUILDING_ON_TIGER
    if (credential.persistence() == CredentialPersistenceNone) {
        // NSURLCredentialPersistenceNone doesn't work on Tiger, so we have to use session persistence.
        Credential webCredential(credential.user(), credential.password(), CredentialPersistenceForSession);
        [[d->m_currentMacChallenge sender] useCredential:mac(webCredential) forAuthenticationChallenge:d->m_currentMacChallenge];
    } else
#else
    if (credential.persistence() == CredentialPersistenceForSession && (!d->m_needsSiteSpecificQuirks || ![[[mac(challenge) protectionSpace] host] isEqualToString:@"gallery.me.com"])) {
        // Manage per-session credentials internally, because once NSURLCredentialPersistenceForSession is used, there is no way
        // to ignore it for a particular request (short of removing it altogether).
        // <rdar://problem/6867598> gallery.me.com is temporarily whitelisted, so that QuickTime plug-in could see the credentials.
        Credential webCredential(credential, CredentialPersistenceNone);
        KURL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = firstRequest().url();
        CredentialStorage::set(webCredential, core([d->m_currentMacChallenge protectionSpace]), urlToStore);
        [[d->m_currentMacChallenge sender] useCredential:mac(webCredential) forAuthenticationChallenge:d->m_currentMacChallenge];
    } else
#endif
        [[d->m_currentMacChallenge sender] useCredential:mac(credential) forAuthenticationChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    ASSERT(!challenge.isNull());
    if (challenge != d->m_currentWebChallenge)
        return;

    [[d->m_currentMacChallenge sender] continueWithoutCredentialForAuthenticationChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
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

    // the willSendRequest call may cancel this load, in which case self could be deallocated
    RetainPtr<WebCoreResourceHandleAsDelegate> protect(self);

    if (!m_handle || !m_handle->client())
        return nil;
    
    // See <rdar://problem/5380697> .  This is a workaround for a behavior change in CFNetwork where willSendRequest gets called more often.
    if (!redirectResponse)
        return newRequest;

#if !LOG_DISABLED
    if ([redirectResponse isKindOfClass:[NSHTTPURLResponse class]])
        LOG(Network, "Handle %p delegate connection:%p willSendRequest:%@ redirectResponse:%d, Location:<%@>", m_handle, connection, [newRequest description], static_cast<int>([(id)redirectResponse statusCode]), [[(id)redirectResponse allHeaderFields] objectForKey:@"Location"]);
    else
        LOG(Network, "Handle %p delegate connection:%p willSendRequest:%@ redirectResponse:non-HTTP", m_handle, connection, [newRequest description]); 
#endif

    if ([redirectResponse isKindOfClass:[NSHTTPURLResponse class]] && [(NSHTTPURLResponse *)redirectResponse statusCode] == 307) {
        String lastHTTPMethod = m_handle->lastHTTPMethod();
        if (!equalIgnoringCase(lastHTTPMethod, String([newRequest HTTPMethod]))) {
            NSMutableURLRequest *mutableRequest = [newRequest mutableCopy];
            [mutableRequest setHTTPMethod:lastHTTPMethod];
    
            FormData* body = m_handle->firstRequest().httpBody();
            if (!equalIgnoringCase(lastHTTPMethod, "GET") && body && !body->isEmpty())
                WebCore::setHTTPBody(mutableRequest, body);

            String originalContentType = m_handle->firstRequest().httpContentType();
            if (!originalContentType.isEmpty())
                [mutableRequest setValue:originalContentType forHTTPHeaderField:@"Content-Type"];

            newRequest = [mutableRequest autorelease];
        }
    }

    CallbackGuard guard;
    ResourceRequest request = newRequest;

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (!request.url().protocolIs("https") && protocolIs(request.httpReferrer(), "https"))
        request.clearHTTPReferrer();

    m_handle->willSendRequest(request, redirectResponse);

    if (!ResourceHandle::didSendBodyDataDelegateExists()) {
        // The client may change the request's body stream, in which case we have to re-associate
        // the handle with the new stream so upload progress callbacks continue to work correctly.
        NSInputStream* oldBodyStream = [newRequest HTTPBodyStream];
        NSInputStream* newBodyStream = [request.nsURLRequest() HTTPBodyStream];
        if (oldBodyStream != newBodyStream) {
            disassociateStreamWithResourceHandle(oldBodyStream);
            associateStreamWithResourceHandle(newBodyStream, m_handle);
        }
    }

    return request.nsURLRequest();
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connectionShouldUseCredentialStorage:%p", m_handle, connection);

    if (!m_handle)
        return NO;

    CallbackGuard guard;
    return m_handle->shouldUseCredentialStorage();
}

- (void)connection:(NSURLConnection *)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didReceiveAuthenticationChallenge:%p", m_handle, connection, challenge);

    if (!m_handle)
        return;
    CallbackGuard guard;
    m_handle->didReceiveAuthenticationChallenge(core(challenge));
}

- (void)connection:(NSURLConnection *)connection didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didCancelAuthenticationChallenge:%p", m_handle, connection, challenge);

    if (!m_handle)
        return;
    CallbackGuard guard;
    m_handle->didCancelAuthenticationChallenge(core(challenge));
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
- (BOOL)connection:(NSURLConnection *)unusedConnection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
{
    UNUSED_PARAM(unusedConnection);
    
    if (!m_handle)
        return NO;
        
    CallbackGuard guard;
    return m_handle->canAuthenticateAgainstProtectionSpace(core(protectionSpace));
}
#endif

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)r
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didReceiveResponse:%p (HTTP status %d, reported MIMEType '%s')", m_handle, connection, r, [r respondsToSelector:@selector(statusCode)] ? [(id)r statusCode] : 0, [[r MIMEType] UTF8String]);

    if (!m_handle || !m_handle->client())
        return;
    CallbackGuard guard;

    // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
    int statusCode = [r respondsToSelector:@selector(statusCode)] ? [(id)r statusCode] : 0;
    if (statusCode != 304)
        [r adjustMIMETypeIfNecessary];

    if ([m_handle->firstRequest().nsURLRequest() _propertyForKey:@"ForceHTMLMIMEType"])
        [r _setMIMEType:@"text/html"];

#if ENABLE(WML)
    const KURL& url = [r URL];
    if (url.isLocalFile()) {
        // FIXME: Workaround for <rdar://problem/6917571>: The WML file extension ".wml" is not mapped to
        // the right MIME type, work around that CFNetwork problem, to unbreak WML support for local files.
        const String& path = url.path();
  
        DEFINE_STATIC_LOCAL(const String, wmlExt, (".wml"));
        if (path.endsWith(wmlExt, false)) {
            static NSString* defaultMIMETypeString = [(NSString*) defaultMIMEType() retain];
            if ([[r MIMEType] isEqualToString:defaultMIMETypeString])
                [r _setMIMEType:@"text/vnd.wap.wml"];
        }
    }
#endif

    m_handle->client()->didReceiveResponse(m_handle, r);
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didReceiveData:%p lengthReceived:%lld", m_handle, connection, data, lengthReceived);

    if (!m_handle || !m_handle->client())
        return;
    // FIXME: If we get more than 2B bytes in a single chunk, this code won't do the right thing.
    // However, with today's computers and networking speeds, this won't happen in practice.
    // Could be an issue with a giant local file.
    CallbackGuard guard;
    m_handle->client()->didReceiveData(m_handle, (const char*)[data bytes], [data length], static_cast<int>(lengthReceived));
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
    CallbackGuard guard;
    m_handle->client()->willStopBufferingData(m_handle, (const char*)[data bytes], static_cast<int>([data length]));
}

- (void)connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
    UNUSED_PARAM(connection);
    UNUSED_PARAM(bytesWritten);

    LOG(Network, "Handle %p delegate connection:%p didSendBodyData:%d totalBytesWritten:%d totalBytesExpectedToWrite:%d", m_handle, connection, bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);

    if (!m_handle || !m_handle->client())
        return;
    CallbackGuard guard;
    m_handle->client()->didSendData(m_handle, totalBytesWritten, totalBytesExpectedToWrite);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connectionDidFinishLoading:%p", m_handle, connection);

    if (!m_handle || !m_handle->client())
        return;
    CallbackGuard guard;

    if (!ResourceHandle::didSendBodyDataDelegateExists())
        disassociateStreamWithResourceHandle([m_handle->firstRequest().nsURLRequest() HTTPBodyStream]);

    m_handle->client()->didFinishLoading(m_handle);
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didFailWithError:%@", m_handle, connection, error);

    if (!m_handle || !m_handle->client())
        return;
    CallbackGuard guard;

    if (!ResourceHandle::didSendBodyDataDelegateExists())
        disassociateStreamWithResourceHandle([m_handle->firstRequest().nsURLRequest() HTTPBodyStream]);

    m_handle->client()->didFail(m_handle, error);
}

#ifdef BUILDING_ON_TIGER
- (void)_callConnectionWillCacheResponseWithInfo:(NSMutableDictionary *)info
{
    NSURLConnection *connection = [info objectForKey:@"connection"];
    NSCachedURLResponse *cachedResponse = [info objectForKey:@"cachedResponse"];
    NSCachedURLResponse *result = [self connection:connection willCacheResponse:cachedResponse];
    if (result)
        [info setObject:result forKey:@"result"];
}
#endif

- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
    LOG(Network, "Handle %p delegate connection:%p willCacheResponse:%p", m_handle, connection, cachedResponse);

#ifdef BUILDING_ON_TIGER
    // On Tiger CFURLConnection can sometimes call the connection:willCacheResponse: delegate method on
    // a secondary thread instead of the main thread. If this happens perform the work on the main thread.
    if (!pthread_main_np()) {
        NSMutableDictionary *info = [[NSMutableDictionary alloc] init];
        if (connection)
            [info setObject:connection forKey:@"connection"];
        if (cachedResponse)
            [info setObject:cachedResponse forKey:@"cachedResponse"];

        // Include synchronous url connection's mode as an acceptable run loopmode
        // <rdar://problem/5511842>
        NSArray *modes = [[NSArray alloc] initWithObjects:(NSString *)kCFRunLoopCommonModes, @"NSSynchronousURLConnection_PrivateMode", nil];        
        [self performSelectorOnMainThread:@selector(_callConnectionWillCacheResponseWithInfo:) withObject:info waitUntilDone:YES modes:modes];
        [modes release];

        NSCachedURLResponse *result = [[info valueForKey:@"result"] retain];
        [info release];

        return [result autorelease];
    }
#else
    UNUSED_PARAM(connection);
#endif

#ifndef NDEBUG
    if (isInitializingConnection)
        LOG_ERROR("connection:willCacheResponse: was called inside of [NSURLConnection initWithRequest:delegate:] (4067625)");
#endif

    if (!m_handle || !m_handle->client())
        return nil;

    CallbackGuard guard;
    
    NSCachedURLResponse *newResponse = m_handle->client()->willCacheResponse(m_handle, cachedResponse);
    if (newResponse != cachedResponse)
        return newResponse;
    
    CacheStoragePolicy policy = static_cast<CacheStoragePolicy>([newResponse storagePolicy]);
        
    m_handle->client()->willCacheResponse(m_handle, policy);

    if (static_cast<NSURLCacheStoragePolicy>(policy) != [newResponse storagePolicy])
        newResponse = [[[NSCachedURLResponse alloc] initWithResponse:[newResponse response]
                                                                data:[newResponse data]
                                                            userInfo:[newResponse userInfo]
                                                       storagePolicy:static_cast<NSURLCacheStoragePolicy>(policy)] autorelease];

    return newResponse;
}

@end

#ifndef BUILDING_ON_TIGER

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
    // FIXME: We should ask FrameLoaderClient.
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

void WebCoreSynchronousLoaderClient::didReceiveData(ResourceHandle*, const char* data, int length, int /*lengthReceived*/)
{
    if (!m_data)
        m_data = [[NSMutableData alloc] init];
    [m_data appendBytes:data length:length];
}

void WebCoreSynchronousLoaderClient::didFinishLoading(ResourceHandle*)
{
    m_isDone = true;
}

void WebCoreSynchronousLoaderClient::didFail(ResourceHandle*, const ResourceError& error)
{
    ASSERT(!m_error);

    m_error = [error copy];
    m_isDone = true;
}

#endif
