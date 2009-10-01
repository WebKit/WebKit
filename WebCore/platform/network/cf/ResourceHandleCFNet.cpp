/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#include "config.h"

#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"

#include "AuthenticationCF.h"
#include "AuthenticationChallenge.h"
#include "Base64.h"
#include "CString.h"
#include "CookieStorageWin.h"
#include "CredentialStorage.h"
#include "DocLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "ResourceError.h"
#include "ResourceResponse.h"

#include <wtf/HashMap.h>
#include <wtf/Threading.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <process.h> // for _beginthread()

#include <CFNetwork/CFNetwork.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>

namespace WebCore {

static CFStringRef WebCoreSynchronousLoaderRunLoopMode = CFSTR("WebCoreSynchronousLoaderRunLoopMode");

class WebCoreSynchronousLoader {
public:
    static RetainPtr<CFDataRef> load(const ResourceRequest&, StoredCredentials, ResourceResponse&, ResourceError&);

private:
    WebCoreSynchronousLoader(ResourceResponse& response, ResourceError& error)
        : m_isDone(false)
        , m_response(response)
        , m_error(error)
    {
    }

    static CFURLRequestRef willSendRequest(CFURLConnectionRef, CFURLRequestRef, CFURLResponseRef, const void* clientInfo);
    static void didReceiveResponse(CFURLConnectionRef, CFURLResponseRef, const void* clientInfo);
    static void didReceiveData(CFURLConnectionRef, CFDataRef, CFIndex, const void* clientInfo);
    static void didFinishLoading(CFURLConnectionRef, const void* clientInfo);
    static void didFail(CFURLConnectionRef, CFErrorRef, const void* clientInfo);
    static void didReceiveChallenge(CFURLConnectionRef, CFURLAuthChallengeRef, const void* clientInfo);
    static Boolean shouldUseCredentialStorage(CFURLConnectionRef, const void* clientInfo);

    bool m_isDone;
    RetainPtr<CFURLRef> m_url;
    RetainPtr<CFStringRef> m_user;
    RetainPtr<CFStringRef> m_pass;
    // Store the preemptively used initial credential so that if we get an authentication challenge, we won't use the same one again.
    Credential m_initialCredential;
    bool m_allowStoredCredentials;
    ResourceResponse& m_response;
    RetainPtr<CFMutableDataRef> m_data;
    ResourceError& m_error;
};

static HashSet<String>& allowsAnyHTTPSCertificateHosts()
{
    static HashSet<String> hosts;

    return hosts;
}

static HashMap<String, RetainPtr<CFDataRef> >& clientCerts()
{
    static HashMap<String, RetainPtr<CFDataRef> > certs;
    return certs;
}

static void setDefaultMIMEType(CFURLResponseRef response)
{
    static CFStringRef defaultMIMETypeString = defaultMIMEType().createCFString();
    
    CFURLResponseSetMIMEType(response, defaultMIMETypeString);
}

static String encodeBasicAuthorization(const String& user, const String& password)
{
    CString unencodedString = (user + ":" + password).utf8();
    Vector<char> unencoded(unencodedString.length());
    std::copy(unencodedString.data(), unencodedString.data() + unencodedString.length(), unencoded.begin());
    Vector<char> encoded;
    base64Encode(unencoded, encoded);
    return String(encoded.data(), encoded.size());
}

CFURLRequestRef willSendRequest(CFURLConnectionRef conn, CFURLRequestRef cfRequest, CFURLResponseRef cfRedirectResponse, const void* clientInfo)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(const_cast<void*>(clientInfo));

    if (!cfRedirectResponse) {
        CFRetain(cfRequest);
        return cfRequest;
    }

    LOG(Network, "CFNet - willSendRequest(conn=%p, handle=%p) (%s)", conn, handle, handle->request().url().string().utf8().data());

    ResourceRequest request;
    if (cfRedirectResponse) {
        CFHTTPMessageRef httpMessage = CFURLResponseGetHTTPResponse(cfRedirectResponse);
        if (httpMessage && CFHTTPMessageGetResponseStatusCode(httpMessage) == 307) {
            RetainPtr<CFStringRef> originalMethod(AdoptCF, handle->request().httpMethod().createCFString());
            RetainPtr<CFStringRef> newMethod(AdoptCF, CFURLRequestCopyHTTPRequestMethod(cfRequest));
            if (CFStringCompareWithOptions(originalMethod.get(), newMethod.get(), CFRangeMake(0, CFStringGetLength(originalMethod.get())), kCFCompareCaseInsensitive)) {
                RetainPtr<CFMutableURLRequestRef> mutableRequest(AdoptCF, CFURLRequestCreateMutableCopy(0, cfRequest));
                CFURLRequestSetHTTPRequestMethod(mutableRequest.get(), originalMethod.get());

                FormData* body = handle->request().httpBody();
                if (!equalIgnoringCase(handle->request().httpMethod(), "GET") && body && !body->isEmpty())
                    WebCore::setHTTPBody(mutableRequest, body);

                String originalContentType = m_handle->request.httpContentType();
                if (!originalContentType->isEmpty())
                    CFURLRequestSetHTTPHeaderFieldValue(mutableRequest.get(), CFSTR("Content-Type"), originalContentType);

                request = mutableRequest.get();
            }
        }
    }
    if (request.isNull())
        request = cfRequest;
    
    handle->willSendRequest(request, cfRedirectResponse);

    if (request.isNull())
        return 0;

    cfRequest = request.cfURLRequest();

    CFRetain(cfRequest);
    return cfRequest;
}

void didReceiveResponse(CFURLConnectionRef conn, CFURLResponseRef cfResponse, const void* clientInfo) 
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(const_cast<void*>(clientInfo));

    LOG(Network, "CFNet - didReceiveResponse(conn=%p, handle=%p) (%s)", conn, handle, handle->request().url().string().utf8().data());

    if (!handle->client())
        return;

    if (!CFURLResponseGetMIMEType(cfResponse)) {
        // We should never be applying the default MIMEType if we told the networking layer to do content sniffing for handle.
        ASSERT(!handle->shouldContentSniff());
        setDefaultMIMEType(cfResponse);
    }
    
    handle->client()->didReceiveResponse(handle, cfResponse);
}

void didReceiveData(CFURLConnectionRef conn, CFDataRef data, CFIndex originalLength, const void* clientInfo) 
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(const_cast<void*>(clientInfo));
    const UInt8* bytes = CFDataGetBytePtr(data);
    CFIndex length = CFDataGetLength(data);

    LOG(Network, "CFNet - didReceiveData(conn=%p, handle=%p, bytes=%d) (%s)", conn, handle, length, handle->request().url().string().utf8().data());

    if (handle->client())
        handle->client()->didReceiveData(handle, (const char*)bytes, length, originalLength);
}

static void didSendBodyData(CFURLConnectionRef conn, CFIndex bytesWritten, CFIndex totalBytesWritten, CFIndex totalBytesExpectedToWrite, const void *clientInfo)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(const_cast<void*>(clientInfo));
    if (!handle || !handle->client())
        return;
    handle->client()->didSendData(handle, totalBytesWritten, totalBytesExpectedToWrite);
}

static Boolean shouldUseCredentialStorageCallback(CFURLConnectionRef conn, const void* clientInfo)
{
    ResourceHandle* handle = const_cast<ResourceHandle*>(static_cast<const ResourceHandle*>(clientInfo));

    LOG(Network, "CFNet - shouldUseCredentialStorage(conn=%p, handle=%p) (%s)", conn, handle, handle->request().url().string().utf8().data());

    if (!handle)
        return false;

    return handle->shouldUseCredentialStorage();
}

void didFinishLoading(CFURLConnectionRef conn, const void* clientInfo) 
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(const_cast<void*>(clientInfo));

    LOG(Network, "CFNet - didFinishLoading(conn=%p, handle=%p) (%s)", conn, handle, handle->request().url().string().utf8().data());

    if (handle->client())
        handle->client()->didFinishLoading(handle);
}

void didFail(CFURLConnectionRef conn, CFErrorRef error, const void* clientInfo) 
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(const_cast<void*>(clientInfo));

    LOG(Network, "CFNet - didFail(conn=%p, handle=%p, error = %p) (%s)", conn, handle, error, handle->request().url().string().utf8().data());

    if (handle->client())
        handle->client()->didFail(handle, ResourceError(error));
}

CFCachedURLResponseRef willCacheResponse(CFURLConnectionRef conn, CFCachedURLResponseRef cachedResponse, const void* clientInfo) 
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(const_cast<void*>(clientInfo));

    if (handle->client() && !handle->client()->shouldCacheResponse(handle, cachedResponse))
        return 0;

    CacheStoragePolicy policy = static_cast<CacheStoragePolicy>(CFCachedURLResponseGetStoragePolicy(cachedResponse));

    if (handle->client())
        handle->client()->willCacheResponse(handle, policy);

    if (static_cast<CFURLCacheStoragePolicy>(policy) != CFCachedURLResponseGetStoragePolicy(cachedResponse))
        cachedResponse = CFCachedURLResponseCreateWithUserInfo(kCFAllocatorDefault, 
                                                               CFCachedURLResponseGetWrappedResponse(cachedResponse),
                                                               CFCachedURLResponseGetReceiverData(cachedResponse),
                                                               CFCachedURLResponseGetUserInfo(cachedResponse), 
                                                               static_cast<CFURLCacheStoragePolicy>(policy));
    CFRetain(cachedResponse);

    return cachedResponse;
}

void didReceiveChallenge(CFURLConnectionRef conn, CFURLAuthChallengeRef challenge, const void* clientInfo)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(const_cast<void*>(clientInfo));
    ASSERT(handle);
    LOG(Network, "CFNet - didReceiveChallenge(conn=%p, handle=%p (%s)", conn, handle, handle->request().url().string().utf8().data());

    handle->didReceiveAuthenticationChallenge(AuthenticationChallenge(challenge, handle));
}

void addHeadersFromHashMap(CFMutableURLRequestRef request, const HTTPHeaderMap& requestHeaders) 
{
    if (!requestHeaders.size())
        return;

    HTTPHeaderMap::const_iterator end = requestHeaders.end();
    for (HTTPHeaderMap::const_iterator it = requestHeaders.begin(); it != end; ++it) {
        CFStringRef key = it->first.createCFString();
        CFStringRef value = it->second.createCFString();
        CFURLRequestSetHTTPHeaderFieldValue(request, key, value);
        CFRelease(key);
        CFRelease(value);
    }
}

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_connection) {
        LOG(Network, "CFNet - Cancelling connection %p (%s)", m_connection, m_request.url().string().utf8().data());
        CFURLConnectionCancel(m_connection.get());
    }
}

ResourceHandle::~ResourceHandle()
{
    LOG(Network, "CFNet - Destroying job %p (%s)", this, d->m_request.url().string().utf8().data());
}

CFArrayRef arrayFromFormData(const FormData& d)
{
    size_t size = d.elements().size();
    CFMutableArrayRef a = CFArrayCreateMutable(0, d.elements().size(), &kCFTypeArrayCallBacks);
    for (size_t i = 0; i < size; ++i) {
        const FormDataElement& e = d.elements()[i];
        if (e.m_type == FormDataElement::data) {
            CFDataRef data = CFDataCreate(0, (const UInt8*)e.m_data.data(), e.m_data.size());
            CFArrayAppendValue(a, data);
            CFRelease(data);
        } else {
            ASSERT(e.m_type == FormDataElement::encodedFile);
            CFStringRef filename = e.m_filename.createCFString();
            CFArrayAppendValue(a, filename);
            CFRelease(filename);
        }
    }
    return a;
}

void emptyPerform(void* unused) 
{
}

static CFRunLoopRef loaderRL = 0;
void* runLoaderThread(void *unused)
{
    loaderRL = CFRunLoopGetCurrent();

    // Must add a source to the run loop to prevent CFRunLoopRun() from exiting
    CFRunLoopSourceContext ctxt = {0, (void *)1 /*must be non-NULL*/, 0, 0, 0, 0, 0, 0, 0, emptyPerform};
    CFRunLoopSourceRef bogusSource = CFRunLoopSourceCreate(0, 0, &ctxt);
    CFRunLoopAddSource(loaderRL, bogusSource,kCFRunLoopDefaultMode);

    CFRunLoopRun();

    return 0;
}

CFRunLoopRef ResourceHandle::loaderRunLoop()
{
    if (!loaderRL) {
        createThread(runLoaderThread, 0, "WebCore: CFNetwork Loader");
        while (loaderRL == 0) {
            // FIXME: sleep 10? that can't be right...
            Sleep(10);
        }
    }
    return loaderRL;
}

static CFURLRequestRef makeFinalRequest(const ResourceRequest& request, bool shouldContentSniff)
{
    CFMutableURLRequestRef newRequest = CFURLRequestCreateMutableCopy(kCFAllocatorDefault, request.cfURLRequest());
    
    if (!shouldContentSniff)
        wkSetCFURLRequestShouldContentSniff(newRequest, false);

    RetainPtr<CFMutableDictionaryRef> sslProps;

    if (allowsAnyHTTPSCertificateHosts().contains(request.url().host().lower())) {
        sslProps.adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionaryAddValue(sslProps.get(), kCFStreamSSLAllowsAnyRoot, kCFBooleanTrue);
        CFDictionaryAddValue(sslProps.get(), kCFStreamSSLAllowsExpiredRoots, kCFBooleanTrue);
        CFDictionaryAddValue(sslProps.get(), kCFStreamSSLAllowsExpiredCertificates, kCFBooleanTrue);
        CFDictionaryAddValue(sslProps.get(), kCFStreamSSLValidatesCertificateChain, kCFBooleanFalse);
    }

    HashMap<String, RetainPtr<CFDataRef> >::iterator clientCert = clientCerts().find(request.url().host().lower());
    if (clientCert != clientCerts().end()) {
        if (!sslProps)
            sslProps.adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        wkSetClientCertificateInSSLProperties(sslProps.get(), (clientCert->second).get());
    }

    if (sslProps)
        CFURLRequestSetSSLProperties(newRequest, sslProps.get());

    if (CFHTTPCookieStorageRef cookieStorage = currentCookieStorage()) {
        CFURLRequestSetHTTPCookieStorage(newRequest, cookieStorage);
        CFURLRequestSetHTTPCookieStorageAcceptPolicy(newRequest, CFHTTPCookieStorageGetCookieAcceptPolicy(cookieStorage));
    }

    return newRequest;
}

bool ResourceHandle::start(Frame* frame)
{
    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if (!frame->page())
        return false;

    if ((!d->m_user.isEmpty() || !d->m_pass.isEmpty()) && !d->m_request.url().protocolInHTTPFamily()) {
        // Credentials for ftp can only be passed in URL, the didReceiveAuthenticationChallenge delegate call won't be made.
        KURL urlWithCredentials(d->m_request.url());
        urlWithCredentials.setUser(d->m_user);
        urlWithCredentials.setPass(d->m_pass);
        d->m_request.setURL(urlWithCredentials);
    }

    // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication, 
    // try and reuse the credential preemptively, as allowed by RFC 2617.
    if (!client() || client()->shouldUseCredentialStorage(this) && d->m_request.url().protocolInHTTPFamily())
        d->m_initialCredential = CredentialStorage::getDefaultAuthenticationCredential(d->m_request.url());
        
    if (!d->m_initialCredential.isEmpty()) {
        String authHeader = "Basic " + encodeBasicAuthorization(d->m_initialCredential.user(), d->m_initialCredential.password());
        d->m_request.addHTTPHeaderField("Authorization", authHeader);
    }

    RetainPtr<CFURLRequestRef> request(AdoptCF, makeFinalRequest(d->m_request, d->m_shouldContentSniff));

    CFURLConnectionClient_V3 client = { 3, this, 0, 0, 0, WebCore::willSendRequest, didReceiveResponse, didReceiveData, NULL, didFinishLoading, didFail, willCacheResponse, didReceiveChallenge, didSendBodyData, shouldUseCredentialStorageCallback, 0};

    d->m_connection.adoptCF(CFURLConnectionCreate(0, request.get(), reinterpret_cast<CFURLConnectionClient*>(&client)));

    CFURLConnectionScheduleWithCurrentMessageQueue(d->m_connection.get());
    CFURLConnectionScheduleDownloadWithRunLoop(d->m_connection.get(), loaderRunLoop(), kCFRunLoopDefaultMode);
    CFURLConnectionStart(d->m_connection.get());

    LOG(Network, "CFNet - Starting URL %s (handle=%p, conn=%p)", d->m_request.url().string().utf8().data(), this, d->m_connection);

    return true;
}

void ResourceHandle::cancel()
{
    if (d->m_connection) {
        CFURLConnectionCancel(d->m_connection.get());
        d->m_connection = 0;
    }
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool ResourceHandle::supportsBufferedData()
{
    return false;
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
    LOG(Network, "CFNet - shouldUseCredentialStorage()");
    if (client())
        return client()->shouldUseCredentialStorage(this);

    return false;
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    LOG(Network, "CFNet - didReceiveAuthenticationChallenge()");
    ASSERT(!d->m_currentCFChallenge);
    ASSERT(d->m_currentWebChallenge.isNull());
    // Since CFURLConnection networking relies on keeping a reference to the original CFURLAuthChallengeRef,
    // we make sure that is actually present
    ASSERT(challenge.cfURLAuthChallengeRef());

    if (!d->m_user.isNull() && !d->m_pass.isNull()) {
        RetainPtr<CFStringRef> user(AdoptCF, d->m_user.createCFString());
        RetainPtr<CFStringRef> pass(AdoptCF, d->m_pass.createCFString());
        RetainPtr<CFURLCredentialRef> credential(AdoptCF,
            CFURLCredentialCreate(kCFAllocatorDefault, user.get(), pass.get(), 0, kCFURLCredentialPersistenceNone));
        
        KURL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = d->m_request.url();
        CredentialStorage::set(core(credential.get()), challenge.protectionSpace(), urlToStore);
        
        CFURLConnectionUseCredential(d->m_connection.get(), credential.get(), challenge.cfURLAuthChallengeRef());
        d->m_user = String();
        d->m_pass = String();
        // FIXME: Per the specification, the user shouldn't be asked for credentials if there were incorrect ones provided explicitly.
        return;
    }

    if (!challenge.previousFailureCount() && (!client() || client()->shouldUseCredentialStorage(this))) {
        Credential credential = CredentialStorage::get(challenge.protectionSpace());
        if (!credential.isEmpty() && credential != d->m_initialCredential) {
            ASSERT(credential.persistence() == CredentialPersistenceNone);
            RetainPtr<CFURLCredentialRef> cfCredential(AdoptCF, createCF(credential));
            CFURLConnectionUseCredential(d->m_connection.get(), cfCredential.get(), challenge.cfURLAuthChallengeRef());
            return;
        }
    }

    d->m_currentCFChallenge = challenge.cfURLAuthChallengeRef();
    d->m_currentWebChallenge = AuthenticationChallenge(d->m_currentCFChallenge, this);
    
    if (client())
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    LOG(Network, "CFNet - receivedCredential()");
    ASSERT(!challenge.isNull());
    ASSERT(challenge.cfURLAuthChallengeRef());
    if (challenge != d->m_currentWebChallenge)
        return;

    if (credential.persistence() == CredentialPersistenceForSession) {
        // Manage per-session credentials internally, because once NSURLCredentialPersistencePerSession is used, there is no way
        // to ignore it for a particular request (short of removing it altogether).
        Credential webCredential(credential.user(), credential.password(), CredentialPersistenceNone);
        RetainPtr<CFURLCredentialRef> cfCredential(AdoptCF, createCF(webCredential));
        
        KURL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = d->m_request.url();      
        CredentialStorage::set(webCredential, challenge.protectionSpace(), urlToStore);

        CFURLConnectionUseCredential(d->m_connection.get(), cfCredential.get(), challenge.cfURLAuthChallengeRef());
    } else {
        RetainPtr<CFURLCredentialRef> cfCredential(AdoptCF, createCF(credential));
        CFURLConnectionUseCredential(d->m_connection.get(), cfCredential.get(), challenge.cfURLAuthChallengeRef());
    }

    clearAuthentication();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    LOG(Network, "CFNet - receivedRequestToContinueWithoutCredential()");
    ASSERT(!challenge.isNull());
    ASSERT(challenge.cfURLAuthChallengeRef());
    if (challenge != d->m_currentWebChallenge)
        return;

    CFURLConnectionUseCredential(d->m_connection.get(), 0, challenge.cfURLAuthChallengeRef());

    clearAuthentication();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    LOG(Network, "CFNet - receivedCancellation()");
    if (challenge != d->m_currentWebChallenge)
        return;

    if (client())
        client()->receivedCancellation(this, challenge);
}

CFURLConnectionRef ResourceHandle::connection() const
{
    return d->m_connection.get();
}

CFURLConnectionRef ResourceHandle::releaseConnectionForDownload()
{
    LOG(Network, "CFNet - Job %p releasing connection %p for download", this, d->m_connection.get());
    return d->m_connection.releaseRef();
}

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& vector, Frame*)
{
    ASSERT(!request.isEmpty());

    RetainPtr<CFDataRef> data = WebCoreSynchronousLoader::load(request, storedCredentials, response, error);

    if (!error.isNull()) {
        response = ResourceResponse(request.url(), String(), 0, String(), String());

        CFErrorRef cfError = error;
        CFStringRef domain = CFErrorGetDomain(cfError);
        // FIXME: Return the actual response for failed authentication.
        if (domain == kCFErrorDomainCFNetwork)
            response.setHTTPStatusCode(CFErrorGetCode(cfError));
        else
            response.setHTTPStatusCode(404);
    }

    if (data) {
        ASSERT(vector.isEmpty());
        vector.append(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()));
    }
}

void ResourceHandle::setHostAllowsAnyHTTPSCertificate(const String& host)
{
    allowsAnyHTTPSCertificateHosts().add(host.lower());
}

void ResourceHandle::setClientCertificate(const String& host, CFDataRef cert)
{
    clientCerts().set(host.lower(), cert);
}

void ResourceHandle::setDefersLoading(bool defers)
{
    if (!d->m_connection)
        return;

    if (defers)
        CFURLConnectionHalt(d->m_connection.get());
    else
        CFURLConnectionResume(d->m_connection.get());
}

bool ResourceHandle::loadsBlocked()
{
    return false;
}

bool ResourceHandle::willLoadFromCache(ResourceRequest& request, Frame* frame)
{
    request.setCachePolicy(ReturnCacheDataDontLoad);
    
    CFURLResponseRef cfResponse = 0;
    CFErrorRef cfError = 0;
    RetainPtr<CFURLRequestRef> cfRequest(AdoptCF, makeFinalRequest(request, true));
    RetainPtr<CFDataRef> data(AdoptCF, CFURLConnectionSendSynchronousRequest(cfRequest.get(), &cfResponse, &cfError, request.timeoutInterval()));
    bool cached = cfResponse && !cfError;

    if (cfError)
        CFRelease(cfError);
    if (cfResponse)
        CFRelease(cfResponse);

    return cached;
}

CFURLRequestRef WebCoreSynchronousLoader::willSendRequest(CFURLConnectionRef, CFURLRequestRef cfRequest, CFURLResponseRef cfRedirectResponse, const void* clientInfo)
{
    WebCoreSynchronousLoader* loader = static_cast<WebCoreSynchronousLoader*>(const_cast<void*>(clientInfo));

    // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
    if (loader->m_url && !protocolHostAndPortAreEqual(loader->m_url.get(), CFURLRequestGetURL(cfRequest))) {
        RetainPtr<CFErrorRef> cfError(AdoptCF, CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainCFNetwork, kCFURLErrorBadServerResponse, 0));
        loader->m_error = cfError.get();
        loader->m_isDone = true;
        return 0;
    }

    loader->m_url = CFURLRequestGetURL(cfRequest);

    if (cfRedirectResponse) {
        // Take user/pass out of the URL.
        loader->m_user.adoptCF(CFURLCopyUserName(loader->m_url.get()));
        loader->m_pass.adoptCF(CFURLCopyPassword(loader->m_url.get()));
        if (loader->m_user || loader->m_pass) {
            ResourceRequest requestWithoutCredentials = cfRequest;
            requestWithoutCredentials.removeCredentials();
            cfRequest = requestWithoutCredentials.cfURLRequest();
        }
    }

    CFRetain(cfRequest);
    return cfRequest;
}

void WebCoreSynchronousLoader::didReceiveResponse(CFURLConnectionRef, CFURLResponseRef cfResponse, const void* clientInfo) 
{
    WebCoreSynchronousLoader* loader = static_cast<WebCoreSynchronousLoader*>(const_cast<void*>(clientInfo));

    loader->m_response = cfResponse;
}

void WebCoreSynchronousLoader::didReceiveData(CFURLConnectionRef, CFDataRef data, CFIndex originalLength, const void* clientInfo)
{
    WebCoreSynchronousLoader* loader = static_cast<WebCoreSynchronousLoader*>(const_cast<void*>(clientInfo));

    if (!loader->m_data)
        loader->m_data.adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));

    const UInt8* bytes = CFDataGetBytePtr(data);
    CFIndex length = CFDataGetLength(data);

    CFDataAppendBytes(loader->m_data.get(), bytes, length);
}

void WebCoreSynchronousLoader::didFinishLoading(CFURLConnectionRef, const void* clientInfo)
{
    WebCoreSynchronousLoader* loader = static_cast<WebCoreSynchronousLoader*>(const_cast<void*>(clientInfo));

    loader->m_isDone = true;
}

void WebCoreSynchronousLoader::didFail(CFURLConnectionRef, CFErrorRef error, const void* clientInfo)
{
    WebCoreSynchronousLoader* loader = static_cast<WebCoreSynchronousLoader*>(const_cast<void*>(clientInfo));

    loader->m_error = error;
    loader->m_isDone = true;
}

void WebCoreSynchronousLoader::didReceiveChallenge(CFURLConnectionRef conn, CFURLAuthChallengeRef challenge, const void* clientInfo)
{
    WebCoreSynchronousLoader* loader = static_cast<WebCoreSynchronousLoader*>(const_cast<void*>(clientInfo));

    if (loader->m_user && loader->m_pass) {
        Credential credential(loader->m_user.get(), loader->m_pass.get(), CredentialPersistenceNone);
        RetainPtr<CFURLCredentialRef> cfCredential(AdoptCF, createCF(credential));
        
        CFURLResponseRef urlResponse = (CFURLResponseRef)CFURLAuthChallengeGetFailureResponse(challenge);
        CFHTTPMessageRef httpResponse = urlResponse ? CFURLResponseGetHTTPResponse(urlResponse) : 0;
        KURL urlToStore;
        if (httpResponse && CFHTTPMessageGetResponseStatusCode(httpResponse) == 401)
            urlToStore = loader->m_url.get();
            
        CredentialStorage::set(credential, core(CFURLAuthChallengeGetProtectionSpace(challenge)), urlToStore);
        
        CFURLConnectionUseCredential(conn, cfCredential.get(), challenge);
        loader->m_user = 0;
        loader->m_pass = 0;
        return;
    }
    if (!CFURLAuthChallengeGetPreviousFailureCount(challenge) && loader->m_allowStoredCredentials) {
        Credential credential = CredentialStorage::get(core(CFURLAuthChallengeGetProtectionSpace(challenge)));
        if (!credential.isEmpty() && credential != loader->m_initialCredential) {
            ASSERT(credential.persistence() == CredentialPersistenceNone);
            RetainPtr<CFURLCredentialRef> cfCredential(AdoptCF, createCF(credential));
            CFURLConnectionUseCredential(conn, cfCredential.get(), challenge);
            return;
        }
    }
    // FIXME: The user should be asked for credentials, as in async case.
    CFURLConnectionUseCredential(conn, 0, challenge);
}

Boolean WebCoreSynchronousLoader::shouldUseCredentialStorage(CFURLConnectionRef, const void* clientInfo)
{
    WebCoreSynchronousLoader* loader = static_cast<WebCoreSynchronousLoader*>(const_cast<void*>(clientInfo));

    // FIXME: We should ask FrameLoaderClient whether using credential storage is globally forbidden.
    return loader->m_allowStoredCredentials;
}

RetainPtr<CFDataRef> WebCoreSynchronousLoader::load(const ResourceRequest& request, StoredCredentials storedCredentials, ResourceResponse& response, ResourceError& error)
{
    ASSERT(response.isNull());
    ASSERT(error.isNull());

    WebCoreSynchronousLoader loader(response, error);

    KURL url = request.url();

    if (url.user().length())
        loader.m_user.adoptCF(url.user().createCFString());
    if (url.pass().length())
        loader.m_pass.adoptCF(url.pass().createCFString());
    loader.m_allowStoredCredentials = (storedCredentials == AllowStoredCredentials);

    // Take user/pass out of the URL.
    // Credentials for ftp can only be passed in URL, the didReceiveAuthenticationChallenge delegate call won't be made.
    RetainPtr<CFURLRequestRef> cfRequest;
    if ((loader.m_user || loader.m_pass) && url.protocolInHTTPFamily()) {
        ResourceRequest requestWithoutCredentials(request);
        requestWithoutCredentials.removeCredentials();
        cfRequest.adoptCF(makeFinalRequest(requestWithoutCredentials, ResourceHandle::shouldContentSniffURL(requestWithoutCredentials.url())));
    } else {
        // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication, 
        // try and reuse the credential preemptively, as allowed by RFC 2617.
        ResourceRequest requestWithInitialCredential(request);
        if (loader.m_allowStoredCredentials && url.protocolInHTTPFamily())
            loader.m_initialCredential = CredentialStorage::getDefaultAuthenticationCredential(url);

        if (!loader.m_initialCredential.isEmpty()) {
            String authHeader = "Basic " + encodeBasicAuthorization(loader.m_initialCredential.user(), loader.m_initialCredential.password());
            requestWithInitialCredential.addHTTPHeaderField("Authorization", authHeader);
        }

        cfRequest.adoptCF(makeFinalRequest(requestWithInitialCredential, ResourceHandle::shouldContentSniffURL(requestWithInitialCredential.url())));
    }

    CFURLConnectionClient_V3 client = { 3, &loader, 0, 0, 0, willSendRequest, didReceiveResponse, didReceiveData, 0, didFinishLoading, didFail, 0, didReceiveChallenge, 0, shouldUseCredentialStorage, 0 };
    RetainPtr<CFURLConnectionRef> connection(AdoptCF, CFURLConnectionCreate(kCFAllocatorDefault, cfRequest.get(), reinterpret_cast<CFURLConnectionClient*>(&client)));

    CFURLConnectionScheduleWithRunLoop(connection.get(), CFRunLoopGetCurrent(), WebCoreSynchronousLoaderRunLoopMode);
    CFURLConnectionScheduleDownloadWithRunLoop(connection.get(), CFRunLoopGetCurrent(), WebCoreSynchronousLoaderRunLoopMode);
    CFURLConnectionStart(connection.get());

    while (!loader.m_isDone)
        CFRunLoopRunInMode(WebCoreSynchronousLoaderRunLoopMode, UINT_MAX, true);

    CFURLConnectionCancel(connection.get());
    
    if (error.isNull() && loader.m_response.mimeType().isNull())
        setDefaultMIMEType(loader.m_response.cfURLResponse());

    return loader.m_data;
}

} // namespace WebCore
