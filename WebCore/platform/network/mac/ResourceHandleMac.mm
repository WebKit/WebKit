/*
 * Copyright (C) 2004, 2006-2009 Apple Inc. All rights reserved.
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
#import "BlockExceptions.h"
#import "CString.h"
#import "CredentialStorage.h"
#import "DocLoader.h"
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
#import <wtf/UnusedParam.h>

#ifdef BUILDING_ON_TIGER
typedef int NSInteger;
#endif

using namespace WebCore;

@interface WebCoreResourceHandleAsDelegate : NSObject <NSURLAuthenticationChallengeSender>
{
    ResourceHandle* m_handle;
}
- (id)initWithHandle:(ResourceHandle*)handle;
- (void)detachHandle;
@end

@interface NSURLConnection (NSURLConnectionTigerPrivate)
- (NSData *)_bufferedData;
@end

@interface NSURLRequest (Details)
- (id)_propertyForKey:(NSString *)key;
@end

#ifndef BUILDING_ON_TIGER

@interface WebCoreSynchronousLoader : NSObject {
    NSURL *m_url;
    NSString *m_user;
    NSString *m_pass;
    // Store the preemptively used initial credential so that if we get an authentication challenge, we won't use the same one again.
    Credential m_initialCredential;
    BOOL m_allowStoredCredentials;
    NSURLResponse *m_response;
    NSMutableData *m_data;
    NSError *m_error;
    BOOL m_isDone;
}
+ (NSData *)loadRequest:(NSURLRequest *)request allowStoredCredentials:(BOOL)allowStoredCredentials returningResponse:(NSURLResponse **)response error:(NSError **)error;
@end

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

    LOG(Network, "Handle %p destroyed", this);
}

static const double MaxFoundationVersionWithoutdidSendBodyDataDelegate = 677.21;
bool ResourceHandle::didSendBodyDataDelegateExists()
{
    return NSFoundationVersionNumber > MaxFoundationVersionWithoutdidSendBodyDataDelegate;
}

bool ResourceHandle::start(Frame* frame)
{
    if (!frame)
        return false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    Page* page = frame->page();
    if (!page)
        return false;

#ifndef NDEBUG
    isInitializingConnection = YES;
#endif

    id delegate;
    
    if (d->m_mightDownloadFromHandle) {
        ASSERT(!d->m_proxy);
        d->m_proxy = wkCreateNSURLConnectionDelegateProxy();
        [d->m_proxy.get() setDelegate:ResourceHandle::delegate()];
        [d->m_proxy.get() release];
        
        delegate = d->m_proxy.get();
    } else 
        delegate = ResourceHandle::delegate();

    if ((!d->m_user.isEmpty() || !d->m_pass.isEmpty())
#ifndef BUILDING_ON_TIGER
     && !d->m_request.url().protocolInHTTPFamily() // On Tiger, always pass credentials in URL, so that they get stored even if the request gets cancelled right away.
#endif
    ) {
        // Credentials for ftp can only be passed in URL, the connection:didReceiveAuthenticationChallenge: delegate call won't be made.
        KURL urlWithCredentials(d->m_request.url());
        urlWithCredentials.setUser(d->m_user);
        urlWithCredentials.setPass(d->m_pass);
        d->m_request.setURL(urlWithCredentials);
    }

#ifndef BUILDING_ON_TIGER
    if ((!client() || client()->shouldUseCredentialStorage(this)) && d->m_request.url().protocolInHTTPFamily()) {
        if (d->m_user.isEmpty() && d->m_pass.isEmpty()) {
            // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication, 
            // try and reuse the credential preemptively, as allowed by RFC 2617.
            d->m_initialCredential = CredentialStorage::get(d->m_request.url());
        } else {
            // If there is already a protection space known for the URL, update stored credentials before sending a request.
            // This makes it possible to implement logout by sending an XMLHttpRequest with known incorrect credentials, and aborting it immediately
            // (so that an authentication dialog doesn't pop up).
            CredentialStorage::set(Credential(d->m_user, d->m_pass, CredentialPersistenceNone), d->m_request.url());
        }
    }
        
    if (!d->m_initialCredential.isEmpty()) {
        // FIXME: Support Digest authentication, and Proxy-Authorization.
        String authHeader = "Basic " + encodeBasicAuthorization(d->m_initialCredential.user(), d->m_initialCredential.password());
        d->m_request.addHTTPHeaderField("Authorization", authHeader);
    }
#endif

    if (!ResourceHandle::didSendBodyDataDelegateExists())
        associateStreamWithResourceHandle([d->m_request.nsURLRequest() HTTPBodyStream], this);

#ifdef BUILDING_ON_TIGER
    // A conditional request sent by WebCore (e.g. to update appcache) can be for a resource that is not cacheable by NSURLConnection,
    // which can get confused and fail to load it in this case.
    if (d->m_request.isConditional())
        d->m_request.setCachePolicy(ReloadIgnoringCacheData);
#endif

    d->m_needsSiteSpecificQuirks = frame->settings() && frame->settings()->needsSiteSpecificQuirks();

    NSURLConnection *connection;
    
    if (d->m_shouldContentSniff || frame->settings()->localFileContentSniffingEnabled()) 
#ifdef BUILDING_ON_TIGER
        connection = [[NSURLConnection alloc] initWithRequest:d->m_request.nsURLRequest() delegate:delegate];
#else
        connection = [[NSURLConnection alloc] initWithRequest:d->m_request.nsURLRequest() delegate:delegate startImmediately:NO];
#endif
    else {
        NSMutableURLRequest *request = [d->m_request.nsURLRequest() mutableCopy];
        wkSetNSURLRequestShouldContentSniff(request, NO);
#ifdef BUILDING_ON_TIGER
        connection = [[NSURLConnection alloc] initWithRequest:request delegate:delegate];
#else
        connection = [[NSURLConnection alloc] initWithRequest:request delegate:delegate startImmediately:NO];
#endif
        [request release];
    }

#ifndef BUILDING_ON_TIGER
    bool scheduled = false;
    if (SchedulePairHashSet* scheduledPairs = page->scheduledRunLoopPairs()) {
        SchedulePairHashSet::iterator end = scheduledPairs->end();
        for (SchedulePairHashSet::iterator it = scheduledPairs->begin(); it != end; ++it) {
            if (NSRunLoop *runLoop = (*it)->nsRunLoop()) {
                [connection scheduleInRunLoop:runLoop forMode:(NSString *)(*it)->mode()];
                scheduled = true;
            }
        }
    }

    // Start the connection if we did schedule with at least one runloop.
    // We can't start the connection until we have one runloop scheduled.
    if (scheduled)
        [connection start];
    else
        d->m_startWhenScheduled = true;
#endif

#ifndef NDEBUG
    isInitializingConnection = NO;
#endif

    LOG(Network, "Handle %p starting connection %p for %@", this, connection, d->m_request.nsURLRequest());
    
    d->m_connection = connection;

    if (d->m_connection) {
        [connection release];

        if (d->m_defersLoading)
            wkSetNSURLConnectionDefersCallbacks(d->m_connection.get(), YES);

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
        disassociateStreamWithResourceHandle([d->m_request.nsURLRequest() HTTPBodyStream]);
    [d->m_connection.get() cancel];
}

void ResourceHandle::setDefersLoading(bool defers)
{
    LOG(Network, "Handle %p setDefersLoading(%s)", this, defers ? "true" : "false");

    d->m_defersLoading = defers;
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
    if (ResourceHandle::supportsBufferedData())
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

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data, Frame*)
{
    NSError *nsError = nil;
    
    NSURLResponse *nsURLResponse = nil;
    NSData *result = nil;

    ASSERT(!request.isEmpty());
    
    NSURLRequest *nsRequest;
    if (!shouldContentSniffURL(request.url())) {
        NSMutableURLRequest *mutableRequest = [[request.nsURLRequest() mutableCopy] autorelease];
        wkSetNSURLRequestShouldContentSniff(mutableRequest, NO);
        nsRequest = mutableRequest;
    } else
        nsRequest = request.nsURLRequest();
            
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
#ifndef BUILDING_ON_TIGER
    result = [WebCoreSynchronousLoader loadRequest:nsRequest allowStoredCredentials:(storedCredentials == AllowStoredCredentials) returningResponse:&nsURLResponse error:&nsError];
#else
    UNUSED_PARAM(storedCredentials);
    result = [NSURLConnection sendSynchronousRequest:nsRequest returningResponse:&nsURLResponse error:&nsError];
#endif
    END_BLOCK_OBJC_EXCEPTIONS;

    if (nsError == nil)
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
    if (!challenge.previousFailureCount() && (!client() || client()->shouldUseCredentialStorage(this))) {
        Credential credential = CredentialStorage::get(challenge.protectionSpace());
        if (!credential.isEmpty() && credential != d->m_initialCredential) {
            ASSERT(credential.persistence() == CredentialPersistenceNone);
            [challenge.sender() useCredential:mac(credential) forAuthenticationChallenge:mac(challenge)];
            return;
        }
    }
#endif

    d->m_currentMacChallenge = challenge.nsURLAuthenticationChallenge();
    NSURLAuthenticationChallenge *webChallenge = [[NSURLAuthenticationChallenge alloc] initWithAuthenticationChallenge:d->m_currentMacChallenge 
                                                                                       sender:(id<NSURLAuthenticationChallengeSender>)delegate()];
    d->m_currentWebChallenge = core(webChallenge);
    [webChallenge release];

    if (client())
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
}

void ResourceHandle::didCancelAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    ASSERT(d->m_currentMacChallenge);
    ASSERT(!d->m_currentWebChallenge.isNull());
    ASSERT(d->m_currentWebChallenge == challenge);

    if (client())
        client()->didCancelAuthenticationChallenge(this, challenge);
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    ASSERT(!challenge.isNull());
    if (challenge != d->m_currentWebChallenge)
        return;

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
        Credential webCredential(credential.user(), credential.password(), CredentialPersistenceNone);
        KURL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = d->m_request.url();
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
    
    LOG(Network, "Handle %p delegate connection:%p willSendRequest:%@ redirectResponse:%p", m_handle, connection, [newRequest description], redirectResponse);

    if (redirectResponse && [redirectResponse isKindOfClass:[NSHTTPURLResponse class]] && [(NSHTTPURLResponse *)redirectResponse statusCode] == 307) {
        String originalMethod = m_handle->request().httpMethod();
        if (!equalIgnoringCase(originalMethod, String([newRequest HTTPMethod]))) {
            NSMutableURLRequest *mutableRequest = [newRequest mutableCopy];
            [mutableRequest setHTTPMethod:originalMethod];
    
            FormData* body = m_handle->request().httpBody();
            if (!equalIgnoringCase(originalMethod, "GET") && body && !body->isEmpty())
                WebCore::setHTTPBody(mutableRequest, body);

            String originalContentType = m_handle->request().httpContentType();
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

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)r
{
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didReceiveResponse:%p (HTTP status %d, reported MIMEType '%s')", m_handle, connection, r, [r respondsToSelector:@selector(statusCode)] ? [(id)r statusCode] : 0, [[r MIMEType] UTF8String]);

    if (!m_handle || !m_handle->client())
        return;
    CallbackGuard guard;

    [r adjustMIMETypeIfNecessary];

    if ([m_handle->request().nsURLRequest() _propertyForKey:@"ForceHTMLMIMEType"])
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
        disassociateStreamWithResourceHandle([m_handle->request().nsURLRequest() HTTPBodyStream]);

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
        disassociateStreamWithResourceHandle([m_handle->request().nsURLRequest() HTTPBodyStream]);

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

- (void)useCredential:(NSURLCredential *)credential forAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    m_handle->receivedCredential(core(challenge), core(credential));
}

- (void)continueWithoutCredentialForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    m_handle->receivedRequestToContinueWithoutCredential(core(challenge));
}

- (void)cancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    m_handle->receivedCancellation(core(challenge));
}

@end

#ifndef BUILDING_ON_TIGER

@implementation WebCoreSynchronousLoader

- (BOOL)_isDone
{
    return m_isDone;
}

- (void)dealloc
{
    [m_url release];
    [m_user release];
    [m_pass release];
    [m_response release];
    [m_data release];
    [m_error release];
    
    [super dealloc];
}

- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    UNUSED_PARAM(connection);

    LOG(Network, "WebCoreSynchronousLoader delegate connection:%p willSendRequest:%@ redirectResponse:%p", connection, [newRequest description], redirectResponse);

    // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
    if (m_url && !protocolHostAndPortAreEqual(m_url, [newRequest URL])) {
        m_error = [[NSError alloc] initWithDomain:NSURLErrorDomain code:NSURLErrorBadServerResponse userInfo:nil];
        m_isDone = YES;
        return nil;
    }

    NSURL *copy = [[newRequest URL] copy];
    [m_url release];
    m_url = copy;

    if (redirectResponse) {
        // Take user/pass out of the URL.
        [m_user release];
        [m_pass release];
        m_user = [[m_url user] copy];
        m_pass = [[m_url password] copy];
        if (m_user || m_pass) {
            ResourceRequest requestWithoutCredentials = newRequest;
            requestWithoutCredentials.removeCredentials();
            return requestWithoutCredentials.nsURLRequest();
        }
    }

    return newRequest;
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection
{
    UNUSED_PARAM(connection);

    LOG(Network, "WebCoreSynchronousLoader delegate connectionShouldUseCredentialStorage:%p", connection);

    // FIXME: We should ask FrameLoaderClient whether using credential storage is globally forbidden.
    return m_allowStoredCredentials;
}

- (void)connection:(NSURLConnection *)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    UNUSED_PARAM(connection);

    LOG(Network, "WebCoreSynchronousLoader delegate connection:%p didReceiveAuthenticationChallenge:%p", connection, challenge);

    if (m_user && m_pass) {
        NSURLCredential *credential = [[NSURLCredential alloc] initWithUser:m_user
                                                                   password:m_pass
                                                                persistence:NSURLCredentialPersistenceNone];
        KURL urlToStore;
        if ([[challenge failureResponse] isKindOfClass:[NSHTTPURLResponse class]] && [(NSHTTPURLResponse*)[challenge failureResponse] statusCode] == 401)
            urlToStore = m_url;
        CredentialStorage::set(core(credential), core([challenge protectionSpace]), urlToStore);
        
        [[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];
        [credential release];
        [m_user release];
        [m_pass release];
        m_user = 0;
        m_pass = 0;
        return;
    }
    if ([challenge previousFailureCount] == 0 && m_allowStoredCredentials) {
        Credential credential = CredentialStorage::get(core([challenge protectionSpace]));
        if (!credential.isEmpty() && credential != m_initialCredential) {
            ASSERT(credential.persistence() == CredentialPersistenceNone);
            [[challenge sender] useCredential:mac(credential) forAuthenticationChallenge:challenge];
            return;
        }
    }
    // FIXME: The user should be asked for credentials, as in async case.
    [[challenge sender] continueWithoutCredentialForAuthenticationChallenge:challenge];
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
    UNUSED_PARAM(connection);

    LOG(Network, "WebCoreSynchronousLoader delegate connection:%p didReceiveResponse:%p (HTTP status %d, reported MIMEType '%s')", connection, response, [response respondsToSelector:@selector(statusCode)] ? [(id)response statusCode] : 0, [[response MIMEType] UTF8String]);

    NSURLResponse *r = [response copy];
    
    [m_response release];
    m_response = r;
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    UNUSED_PARAM(connection);

    LOG(Network, "WebCoreSynchronousLoader delegate connection:%p didReceiveData:%p", connection, data);

    if (!m_data)
        m_data = [[NSMutableData alloc] init];
    
    [m_data appendData:data];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    UNUSED_PARAM(connection);

    LOG(Network, "WebCoreSynchronousLoader delegate connectionDidFinishLoading:%p", connection);

    m_isDone = YES;
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    UNUSED_PARAM(connection);

    LOG(Network, "WebCoreSynchronousLoader delegate connection:%p didFailWithError:%@", connection, error);

    ASSERT(!m_error);
    
    m_error = [error retain];
    m_isDone = YES;
}

- (NSData *)_data
{
    return [[m_data retain] autorelease];
}

- (NSURLResponse *)_response
{
    return [[m_response retain] autorelease];
}

- (NSError *)_error
{
    return [[m_error retain] autorelease];
}

+ (NSData *)loadRequest:(NSURLRequest *)request allowStoredCredentials:(BOOL)allowStoredCredentials returningResponse:(NSURLResponse **)response error:(NSError **)error
{
    LOG(Network, "WebCoreSynchronousLoader loadRequest:%@ allowStoredCredentials:%u", request, allowStoredCredentials);

    WebCoreSynchronousLoader *delegate = [[WebCoreSynchronousLoader alloc] init];

    KURL url([request URL]);
    delegate->m_user = [nsStringNilIfEmpty(url.user()) retain];
    delegate->m_pass = [nsStringNilIfEmpty(url.pass()) retain];
    delegate->m_allowStoredCredentials = allowStoredCredentials;

    NSURLConnection *connection;

    // Take user/pass out of the URL.
    // Credentials for ftp can only be passed in URL, the connection:didReceiveAuthenticationChallenge: delegate call won't be made.
    if ((delegate->m_user || delegate->m_pass) && url.protocolInHTTPFamily()) {
        ResourceRequest requestWithoutCredentials = request;
        requestWithoutCredentials.removeCredentials();
        connection = [[NSURLConnection alloc] initWithRequest:requestWithoutCredentials.nsURLRequest() delegate:delegate startImmediately:NO];
    } else {
        // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication, 
        // try and reuse the credential preemptively, as allowed by RFC 2617.
        ResourceRequest requestWithInitialCredentials = request;
        if (allowStoredCredentials && url.protocolInHTTPFamily())
            delegate->m_initialCredential = CredentialStorage::get(url);
            
        if (!delegate->m_initialCredential.isEmpty()) {
            String authHeader = "Basic " + encodeBasicAuthorization(delegate->m_initialCredential.user(), delegate->m_initialCredential.password());
            requestWithInitialCredentials.addHTTPHeaderField("Authorization", authHeader);
        }
        connection = [[NSURLConnection alloc] initWithRequest:requestWithInitialCredentials.nsURLRequest() delegate:delegate startImmediately:NO];
    }

    [connection scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:WebCoreSynchronousLoaderRunLoopMode];
    [connection start];
    
    while (![delegate _isDone])
        [[NSRunLoop currentRunLoop] runMode:WebCoreSynchronousLoaderRunLoopMode beforeDate:[NSDate distantFuture]];

    NSData *data = [delegate _data];
    *response = [delegate _response];
    *error = [delegate _error];
    
    [connection cancel];
    
    [connection release];
    [delegate release];

    LOG(Network, "WebCoreSynchronousLoader done");

    return data;
}

@end

#endif
