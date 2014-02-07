/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011, 2013 Apple Inc. All rights reserved.
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

#include "ResourceHandleInternal.h"

#include "AuthenticationCF.h"
#include "AuthenticationChallenge.h"
#include "CredentialStorage.h"
#include "CachedResourceLoader.h"
#include "FormDataStreamCFNet.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Logging.h"
#include "NetworkingContext.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "SynchronousLoaderClient.h"
#include "SynchronousResourceHandleCFURLConnectionDelegate.h"
#include <CFNetwork/CFNetwork.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/Threading.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>

#if PLATFORM(COCOA)
#include "ResourceHandleCFURLConnectionDelegateWithOperationQueue.h"
#include "WebCoreSystemInterface.h"
#if USE(CFNETWORK)
#include "WebCoreURLResponse.h"
#include <CFNetwork/CFURLConnectionPriv.h>
#include <CFNetwork/CFURLRequestPriv.h>
#endif
#endif

#if PLATFORM(WIN)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include <process.h>

// FIXME: Remove this declaration once it's in WebKitSupportLibrary.
extern "C" {
__declspec(dllimport) CFURLConnectionRef CFURLConnectionCreateWithProperties(
  CFAllocatorRef           alloc,
  CFURLRequestRef          request,
  CFURLConnectionClient *  client,
  CFDictionaryRef properties);
}
#endif

namespace WebCore {

#if USE(CFNETWORK)

static HashSet<String>& allowsAnyHTTPSCertificateHosts()
{
    DEFINE_STATIC_LOCAL(HashSet<String>, hosts, ());
    return hosts;
}

static HashMap<String, RetainPtr<CFDataRef>>& clientCerts()
{
    typedef HashMap<String, RetainPtr<CFDataRef>> CertsMap;
    DEFINE_STATIC_LOCAL(CertsMap, certs, ());
    return certs;
}

static void applyBasicAuthorizationHeader(ResourceRequest& request, const Credential& credential)
{
    String authenticationHeader = "Basic " + base64Encode(String(credential.user() + ":" + credential.password()).utf8());
    request.clearHTTPAuthorization(); // FIXME: Should addHTTPHeaderField be smart enough to not build comma-separated lists in headers like Authorization?
    request.addHTTPHeaderField("Authorization", authenticationHeader);
}

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_connectionDelegate)
        m_connectionDelegate->releaseHandle();

    if (m_connection) {
        LOG(Network, "CFNet - Cancelling connection %p (%s)", m_connection.get(), m_firstRequest.url().string().utf8().data());
        CFURLConnectionCancel(m_connection.get());
    }
}

ResourceHandle::~ResourceHandle()
{
    LOG(Network, "CFNet - Destroying job %p (%s)", this, d->m_firstRequest.url().string().utf8().data());
}

void ResourceHandle::createCFURLConnection(bool shouldUseCredentialStorage, bool shouldContentSniff, SchedulingBehavior schedulingBehavior, CFDictionaryRef clientProperties)
{
    if ((!d->m_user.isEmpty() || !d->m_pass.isEmpty()) && !firstRequest().url().protocolIsInHTTPFamily()) {
        // Credentials for ftp can only be passed in URL, the didReceiveAuthenticationChallenge delegate call won't be made.
        URL urlWithCredentials(firstRequest().url());
        urlWithCredentials.setUser(d->m_user);
        urlWithCredentials.setPass(d->m_pass);
        firstRequest().setURL(urlWithCredentials);
    }

    // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication,
    // try and reuse the credential preemptively, as allowed by RFC 2617.
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

    RetainPtr<CFMutableURLRequestRef> request = adoptCF(CFURLRequestCreateMutableCopy(kCFAllocatorDefault, firstRequest().cfURLRequest(UpdateHTTPBody)));
    wkSetRequestStorageSession(d->m_storageSession.get(), request.get());
    
    if (!shouldContentSniff)
        wkSetCFURLRequestShouldContentSniff(request.get(), false);

    RetainPtr<CFMutableDictionaryRef> sslProps;

#if PLATFORM(IOS)
    sslProps = adoptCF(ResourceHandle::createSSLPropertiesFromNSURLRequest(firstRequest()));
#else
    if (allowsAnyHTTPSCertificateHosts().contains(firstRequest().url().host().lower())) {
        sslProps = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionaryAddValue(sslProps.get(), kCFStreamSSLAllowsAnyRoot, kCFBooleanTrue);
        CFDictionaryAddValue(sslProps.get(), kCFStreamSSLAllowsExpiredRoots, kCFBooleanTrue);
        CFDictionaryAddValue(sslProps.get(), kCFStreamSSLAllowsExpiredCertificates, kCFBooleanTrue);
        CFDictionaryAddValue(sslProps.get(), kCFStreamSSLValidatesCertificateChain, kCFBooleanFalse);
    }

    HashMap<String, RetainPtr<CFDataRef>>::iterator clientCert = clientCerts().find(firstRequest().url().host().lower());
    if (clientCert != clientCerts().end()) {
        if (!sslProps)
            sslProps = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
#if PLATFORM(WIN)
        wkSetClientCertificateInSSLProperties(sslProps.get(), (clientCert->value).get());
#endif
    }
#endif // PLATFORM(IOS)

    if (sslProps)
        CFURLRequestSetSSLProperties(request.get(), sslProps.get());

#if PLATFORM(WIN)
    if (CFHTTPCookieStorageRef cookieStorage = overridenCookieStorage()) {
        // Overridden cookie storage doesn't come from a session, so the request does not have it yet.
        CFURLRequestSetHTTPCookieStorage(request.get(), cookieStorage);
    }
#endif

    CFMutableDictionaryRef streamProperties  = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (!shouldUseCredentialStorage) {
        // Avoid using existing connections, because they may be already authenticated.
        CFDictionarySetValue(streamProperties, CFSTR("_kCFURLConnectionSessionID"), CFSTR("WebKitPrivateSession"));
    }

    if (schedulingBehavior == SchedulingBehavior::Synchronous) {
        // Synchronous requests should not be subject to regular connection count limit to avoid deadlocks.
        // If we are using all available connections for async requests, and make a sync request, then prior
        // requests may get stuck waiting for delegate calls while we are in nested run loop, and the sync
        // request won't start because there are no available connections.
        // Connections are grouped by their socket stream properties, with each group having a separate count.
        CFDictionarySetValue(streamProperties, CFSTR("_WebKitSynchronousRequest"), kCFBooleanTrue);
    }

#if PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090)
    RetainPtr<CFDataRef> sourceApplicationAuditData = d->m_context->sourceApplicationAuditData();
    if (sourceApplicationAuditData)
        CFDictionarySetValue(streamProperties, CFSTR("kCFStreamPropertySourceApplication"), sourceApplicationAuditData.get());
#endif

    static const CFStringRef kCFURLConnectionSocketStreamProperties = CFSTR("kCFURLConnectionSocketStreamProperties");
    RetainPtr<CFMutableDictionaryRef> propertiesDictionary;
    if (clientProperties)
        propertiesDictionary = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, clientProperties));
    else
        propertiesDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // FIXME: This code is different from iOS code in ResourceHandleMac.mm in that here we ignore stream properties that were present in client properties.
    CFDictionaryAddValue(propertiesDictionary.get(), kCFURLConnectionSocketStreamProperties, streamProperties);
    CFRelease(streamProperties);

#if PLATFORM(COCOA)
    if (client() && client()->usesAsyncCallbacks())
        d->m_connectionDelegate = adoptRef(new ResourceHandleCFURLConnectionDelegateWithOperationQueue(this));
    else
        d->m_connectionDelegate = adoptRef(new SynchronousResourceHandleCFURLConnectionDelegate(this));
#else
    d->m_connectionDelegate = adoptRef(new SynchronousResourceHandleCFURLConnectionDelegate(this));
#endif
    d->m_connectionDelegate->setupRequest(request.get());

    CFURLConnectionClient_V6 client = d->m_connectionDelegate->makeConnectionClient();

    d->m_connection = adoptCF(CFURLConnectionCreateWithProperties(0, request.get(), reinterpret_cast<CFURLConnectionClient*>(&client), propertiesDictionary.get()));
}

bool ResourceHandle::start()
{
    if (!d->m_context)
        return false;

    // If NetworkingContext is invalid then we are no longer attached to a Page,
    // this must be an attempted load from an unload handler, so let's just block it.
    if (!d->m_context->isValid())
        return false;

    d->m_storageSession = d->m_context->storageSession().platformSession();

    bool shouldUseCredentialStorage = !client() || client()->shouldUseCredentialStorage(this);

    createCFURLConnection(shouldUseCredentialStorage, d->m_shouldContentSniff, SchedulingBehavior::Asynchronous, client()->connectionProperties(this).get());

    d->m_connectionDelegate->setupConnectionScheduling(d->m_connection.get());
    CFURLConnectionStart(d->m_connection.get());

    LOG(Network, "CFNet - Starting URL %s (handle=%p, conn=%p)", firstRequest().url().string().utf8().data(), this, d->m_connection.get());

    return true;
}

void ResourceHandle::cancel()
{
    if (d->m_connection) {
        CFURLConnectionCancel(d->m_connection.get());
        d->m_connection = 0;
    }
}

void ResourceHandle::willSendRequest(ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    const URL& url = request.url();
    d->m_user = url.user();
    d->m_pass = url.pass();
    d->m_lastHTTPMethod = request.httpMethod();
    request.removeCredentials();

    if (!protocolHostAndPortAreEqual(request.url(), redirectResponse.url())) {
        // If the network layer carries over authentication headers from the original request
        // in a cross-origin redirect, we want to clear those headers here.
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

    Ref<ResourceHandle> protect(*this);
    if (client()->usesAsyncCallbacks())
        client()->willSendRequestAsync(this, request, redirectResponse);
    else {
        client()->willSendRequest(this, request, redirectResponse);

        // Client call may not preserve the session, especially if the request is sent over IPC.
        if (!request.isNull()) {
            request.setStorageSession(d->m_storageSession.get());

            d->m_currentRequest = request;
        }
    }
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    LOG(Network, "CFNet - shouldUseCredentialStorage()");
    if (ResourceHandleClient* client = this->client()) {
        if (client->usesAsyncCallbacks())
            client->shouldUseCredentialStorageAsync(this);
        else
            return client->shouldUseCredentialStorage(this);
    }
    return false;
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    LOG(Network, "CFNet - didReceiveAuthenticationChallenge()");
    ASSERT(d->m_currentWebChallenge.isNull());
    // Since CFURLConnection networking relies on keeping a reference to the original CFURLAuthChallengeRef,
    // we make sure that is actually present
    ASSERT(challenge.cfURLAuthChallengeRef());
    ASSERT(challenge.authenticationClient() == this); // Should be already set.

#if !PLATFORM(WIN)
    // Proxy authentication is handled by CFNetwork internally. We can get here if the user cancels
    // CFNetwork authentication dialog, and we shouldn't ask the client to display another one in that case.
    if (challenge.protectionSpace().isProxy()) {
        // Cannot use receivedRequestToContinueWithoutCredential(), because current challenge is not yet set.
        CFURLConnectionUseCredential(d->m_connection.get(), 0, challenge.cfURLAuthChallengeRef());
        return;
    }
#endif

    if (!d->m_user.isNull() && !d->m_pass.isNull()) {
        RetainPtr<CFURLCredentialRef> credential = adoptCF(CFURLCredentialCreate(kCFAllocatorDefault, d->m_user.createCFString().get(), d->m_pass.createCFString().get(), 0, kCFURLCredentialPersistenceNone));
        
        URL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = challenge.failureResponse().url();
        CredentialStorage::set(core(credential.get()), challenge.protectionSpace(), urlToStore);
        
        CFURLConnectionUseCredential(d->m_connection.get(), credential.get(), challenge.cfURLAuthChallengeRef());
        d->m_user = String();
        d->m_pass = String();
        // FIXME: Per the specification, the user shouldn't be asked for credentials if there were incorrect ones provided explicitly.
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
                RetainPtr<CFURLCredentialRef> cfCredential = adoptCF(createCF(credential));
                CFURLConnectionUseCredential(d->m_connection.get(), cfCredential.get(), challenge.cfURLAuthChallengeRef());
                return;
            }
        }
    }

    d->m_currentWebChallenge = challenge;
    
    if (client())
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool ResourceHandle::canAuthenticateAgainstProtectionSpace(const ProtectionSpace& protectionSpace)
{
    if (ResourceHandleClient* client = this->client()) {
        if (client->usesAsyncCallbacks())
            client->canAuthenticateAgainstProtectionSpaceAsync(this, protectionSpace);
        else
            return client->canAuthenticateAgainstProtectionSpace(this, protectionSpace);
    }
    return false;
}
#endif

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    LOG(Network, "CFNet - receivedCredential()");
    ASSERT(!challenge.isNull());
    ASSERT(challenge.cfURLAuthChallengeRef());
    if (challenge != d->m_currentWebChallenge)
        return;

    // FIXME: Support empty credentials. Currently, an empty credential cannot be stored in WebCore credential storage, as that's empty value for its map.
    if (credential.isEmpty()) {
        receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    if (credential.persistence() == CredentialPersistenceForSession) {
        // Manage per-session credentials internally, because once NSURLCredentialPersistencePerSession is used, there is no way
        // to ignore it for a particular request (short of removing it altogether).
        Credential webCredential(credential.user(), credential.password(), CredentialPersistenceNone);
        RetainPtr<CFURLCredentialRef> cfCredential = adoptCF(createCF(webCredential));
        
        URL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = challenge.failureResponse().url();      
        CredentialStorage::set(webCredential, challenge.protectionSpace(), urlToStore);

        CFURLConnectionUseCredential(d->m_connection.get(), cfCredential.get(), challenge.cfURLAuthChallengeRef());
    } else {
        RetainPtr<CFURLCredentialRef> cfCredential = adoptCF(createCF(credential));
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

CFURLStorageSessionRef ResourceHandle::storageSession() const
{
    return d->m_storageSession.get();
}

CFURLConnectionRef ResourceHandle::connection() const
{
    return d->m_connection.get();
}

CFURLConnectionRef ResourceHandle::releaseConnectionForDownload()
{
    LOG(Network, "CFNet - Job %p releasing connection %p for download", this, d->m_connection.get());
    return d->m_connection.leakRef();
}

CFStringRef ResourceHandle::synchronousLoadRunLoopMode()
{
    return CFSTR("WebCoreSynchronousLoaderRunLoopMode");
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    LOG(Network, "ResourceHandle::platformLoadResourceSynchronously:%s allowStoredCredentials:%u", request.url().string().utf8().data(), storedCredentials);

    ASSERT(!request.isEmpty());

    ASSERT(response.isNull());
    ASSERT(error.isNull());

    OwnPtr<SynchronousLoaderClient> client = SynchronousLoaderClient::create();
    client->setAllowStoredCredentials(storedCredentials == AllowStoredCredentials);

    RefPtr<ResourceHandle> handle = adoptRef(new ResourceHandle(context, request, client.get(), false /*defersLoading*/, true /*shouldContentSniff*/));

    handle->d->m_storageSession = context->storageSession().platformSession();

    if (handle->d->m_scheduledFailureType != NoFailure) {
        error = context->blockedError(request);
        return;
    }

    handle->createCFURLConnection(storedCredentials == AllowStoredCredentials, ResourceHandle::shouldContentSniffURL(request.url()),
        SchedulingBehavior::Synchronous, handle->client()->connectionProperties(handle.get()).get());

    CFURLConnectionScheduleWithRunLoop(handle->connection(), CFRunLoopGetCurrent(), synchronousLoadRunLoopMode());
    CFURLConnectionScheduleDownloadWithRunLoop(handle->connection(), CFRunLoopGetCurrent(), synchronousLoadRunLoopMode());
    CFURLConnectionStart(handle->connection());

    while (!client->isDone())
        CFRunLoopRunInMode(synchronousLoadRunLoopMode(), UINT_MAX, true);

    error = client->error();

    CFURLConnectionCancel(handle->connection());

    if (error.isNull())
        response = client->response();
    else {
        response = ResourceResponse(request.url(), String(), 0, String(), String());
        // FIXME: ResourceHandleMac also handles authentication errors by setting code to 401. CFNet version should probably do the same.
        if (error.domain() == String(kCFErrorDomainCFNetwork))
            response.setHTTPStatusCode(error.errorCode());
        else
            response.setHTTPStatusCode(404);
    }

    data.swap(client->mutableData());
}

void ResourceHandle::setHostAllowsAnyHTTPSCertificate(const String& host)
{
    allowsAnyHTTPSCertificateHosts().add(host.lower());
}

void ResourceHandle::setClientCertificate(const String& host, CFDataRef cert)
{
    clientCerts().set(host.lower(), cert);
}

void ResourceHandle::platformSetDefersLoading(bool defers)
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

#if PLATFORM(COCOA)
void ResourceHandle::schedule(SchedulePair* pair)
{
    CFRunLoopRef runLoop = pair->runLoop();
    if (!runLoop)
        return;

    CFURLConnectionScheduleWithRunLoop(d->m_connection.get(), runLoop, pair->mode());
    if (d->m_startWhenScheduled) {
        CFURLConnectionStart(d->m_connection.get());
        d->m_startWhenScheduled = false;
    }
}

void ResourceHandle::unschedule(SchedulePair* pair)
{
    CFRunLoopRef runLoop = pair->runLoop();
    if (!runLoop)
        return;

    CFURLConnectionUnscheduleFromRunLoop(d->m_connection.get(), runLoop, pair->mode());
}
#endif

const ResourceRequest& ResourceHandle::currentRequest() const
{
    return d->m_currentRequest;
}

void ResourceHandle::continueWillSendRequest(const ResourceRequest& request)
{
    ResourceRequest requestResult = request;
    if (!requestResult.isNull())
        requestResult.setStorageSession(d->m_storageSession.get());
    d->m_connectionDelegate->continueWillSendRequest(requestResult.cfURLRequest(UpdateHTTPBody));
}

void ResourceHandle::continueDidReceiveResponse()
{
    d->m_connectionDelegate->continueDidReceiveResponse();
}

void ResourceHandle::continueShouldUseCredentialStorage(bool useCredentialStorage)
{
    d->m_connectionDelegate->continueShouldUseCredentialStorage(useCredentialStorage);
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void ResourceHandle::continueCanAuthenticateAgainstProtectionSpace(bool canAuthenticate)
{
    d->m_connectionDelegate->continueCanAuthenticateAgainstProtectionSpace(canAuthenticate);
}
#endif

void ResourceHandle::continueWillCacheResponse(CFCachedURLResponseRef response)
{
    d->m_connectionDelegate->continueWillCacheResponse(response);
}
#endif // USE(CFNETWORK)

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
void ResourceHandle::handleDataArray(CFArrayRef dataArray)
{
    ASSERT(client());
    if (client()->supportsDataArray()) {
        client()->didReceiveDataArray(this, dataArray);
        return;
    }

    CFIndex count = CFArrayGetCount(dataArray);
    ASSERT(count);
    if (count == 1) {
        CFDataRef data = static_cast<CFDataRef>(CFArrayGetValueAtIndex(dataArray, 0));
        client()->didReceiveBuffer(this, SharedBuffer::wrapCFData(data), -1);
        return;
    }

    CFIndex totalSize = 0;
    CFIndex index;
    for (index = 0; index < count; index++)
        totalSize += CFDataGetLength(static_cast<CFDataRef>(CFArrayGetValueAtIndex(dataArray, index)));

    RetainPtr<CFMutableDataRef> mergedData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, totalSize));
    for (index = 0; index < count; index++) {
        CFDataRef data = static_cast<CFDataRef>(CFArrayGetValueAtIndex(dataArray, index));
        CFDataAppendBytes(mergedData.get(), CFDataGetBytePtr(data), CFDataGetLength(data));
    }

    client()->didReceiveBuffer(this, SharedBuffer::wrapCFData(mergedData.get()), -1);
}
#endif

} // namespace WebCore
