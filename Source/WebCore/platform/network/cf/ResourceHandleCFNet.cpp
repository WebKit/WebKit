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

#include "config.h"

#include "ResourceHandleInternal.h"

#include "AuthenticationCF.h"
#include "AuthenticationChallenge.h"
#include "CFNetworkSPI.h"
#include "CredentialStorage.h"
#include "CachedResourceLoader.h"
#include "FormDataStreamCFNet.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTTPHeaderNames.h"
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
    DEPRECATED_DEFINE_STATIC_LOCAL(HashSet<String>, hosts, ());
    return hosts;
}

static HashMap<String, RetainPtr<CFDataRef>>& clientCerts()
{
    typedef HashMap<String, RetainPtr<CFDataRef>> CertsMap;
    DEPRECATED_DEFINE_STATIC_LOCAL(CertsMap, certs, ());
    return certs;
}

static void applyBasicAuthorizationHeader(ResourceRequest& request, const Credential& credential)
{
    String authenticationHeader = "Basic " + base64Encode(String(credential.user() + ":" + credential.password()).utf8());

    request.setHTTPHeaderField(HTTPHeaderName::Authorization, authenticationHeader);
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
            d->m_initialCredential = d->m_context->storageSession().credentialStorage().get(firstRequest().url());
        } else {
            // If there is already a protection space known for the URL, update stored credentials before sending a request.
            // This makes it possible to implement logout by sending an XMLHttpRequest with known incorrect credentials, and aborting it immediately
            // (so that an authentication dialog doesn't pop up).
            d->m_context->storageSession().credentialStorage().set(Credential(d->m_user, d->m_pass, CredentialPersistenceNone), firstRequest().url());
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

#if PLATFORM(COCOA)
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
#if HAVE(TIMINGDATAOPTIONS)
    int64_t value = static_cast<int64_t>(_TimingDataOptionsEnableW3CNavigationTiming);
    auto enableW3CNavigationTiming = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value));
    auto timingDataOptionsDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionaryAddValue(timingDataOptionsDictionary.get(), CFSTR("_kCFURLConnectionPropertyTimingDataOptions"), enableW3CNavigationTiming.get());
    CFDictionaryAddValue(propertiesDictionary.get(), CFSTR("kCFURLConnectionURLConnectionProperties"), timingDataOptionsDictionary.get());
#endif

    // FIXME: This code is different from iOS code in ResourceHandleMac.mm in that here we ignore stream properties that were present in client properties.
    CFDictionaryAddValue(propertiesDictionary.get(), kCFURLConnectionSocketStreamProperties, streamProperties);
    CFRelease(streamProperties);

#if PLATFORM(COCOA)
    if (d->m_usesAsyncCallbacks)
        d->m_connectionDelegate = adoptRef(new ResourceHandleCFURLConnectionDelegateWithOperationQueue(this));
    else
        d->m_connectionDelegate = adoptRef(new SynchronousResourceHandleCFURLConnectionDelegate(this));
#else
    d->m_connectionDelegate = adoptRef(new SynchronousResourceHandleCFURLConnectionDelegate(this));
#endif
    d->m_connectionDelegate->setupRequest(request.get());

    CFURLConnectionClient_V6 client = d->m_connectionDelegate->makeConnectionClient();
    if (shouldUseCredentialStorage)
        client.shouldUseCredentialStorage = 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    d->m_connection = adoptCF(CFURLConnectionCreateWithProperties(0, request.get(), reinterpret_cast<CFURLConnectionClient*>(&client), propertiesDictionary.get()));
#pragma clang diagnostic pop
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

#if ENABLE(WEB_TIMING) && PLATFORM(COCOA) && !HAVE(TIMINGDATAOPTIONS)
    setCollectsTimingData();
#endif

    SchedulingBehavior schedulingBehavior = client()->loadingSynchronousXHR() ? SchedulingBehavior::Synchronous : SchedulingBehavior::Asynchronous;

    createCFURLConnection(shouldUseCredentialStorage, d->m_shouldContentSniff, schedulingBehavior, client()->connectionProperties(this).get());

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
        // The network layer might carry over some headers from the original request that
        // we want to strip here because the redirect is cross-origin.
        request.clearHTTPAuthorization();
        request.clearHTTPOrigin();
    } else {
        // Only consider applying authentication credentials if this is actually a redirect and the redirect
        // URL didn't include credentials of its own.
        if (d->m_user.isEmpty() && d->m_pass.isEmpty() && !redirectResponse.isNull()) {
            Credential credential = d->m_context->storageSession().credentialStorage().get(request.url());
            if (!credential.isEmpty()) {
                d->m_initialCredential = credential;
                
                // FIXME: Support Digest authentication, and Proxy-Authorization.
                applyBasicAuthorizationHeader(request, d->m_initialCredential);
            }
        }
    }

    Ref<ResourceHandle> protect(*this);
    if (d->m_usesAsyncCallbacks)
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
        ASSERT(!d->m_usesAsyncCallbacks);
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

    if (tryHandlePasswordBasedAuthentication(challenge))
        return;

    d->m_currentWebChallenge = challenge;
    
    if (client())
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
    else {
        clearAuthentication();
        CFURLConnectionPerformDefaultHandlingForChallenge(d->m_connection.get(), challenge.cfURLAuthChallengeRef());
    }
}

bool ResourceHandle::tryHandlePasswordBasedAuthentication(const AuthenticationChallenge& challenge)
{
    if (!challenge.protectionSpace().isPasswordBased())
        return false;

    if (!d->m_user.isNull() && !d->m_pass.isNull()) {
        RetainPtr<CFURLCredentialRef> cfCredential = adoptCF(CFURLCredentialCreate(kCFAllocatorDefault, d->m_user.createCFString().get(), d->m_pass.createCFString().get(), 0, kCFURLCredentialPersistenceNone));
#if PLATFORM(COCOA)
        Credential credential = Credential(cfCredential.get());
#else
        Credential credential = core(cfCredential.get());
#endif
        
        URL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = challenge.failureResponse().url();
        d->m_context->storageSession().credentialStorage().set(credential, challenge.protectionSpace(), urlToStore);
        
        CFURLConnectionUseCredential(d->m_connection.get(), cfCredential.get(), challenge.cfURLAuthChallengeRef());
        d->m_user = String();
        d->m_pass = String();
        // FIXME: Per the specification, the user shouldn't be asked for credentials if there were incorrect ones provided explicitly.
        return true;
    }

    if (!client() || client()->shouldUseCredentialStorage(this)) {
        if (!d->m_initialCredential.isEmpty() || challenge.previousFailureCount()) {
            // The stored credential wasn't accepted, stop using it.
            // There is a race condition here, since a different credential might have already been stored by another ResourceHandle,
            // but the observable effect should be very minor, if any.
            d->m_context->storageSession().credentialStorage().remove(challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            Credential credential = d->m_context->storageSession().credentialStorage().get(challenge.protectionSpace());
            if (!credential.isEmpty() && credential != d->m_initialCredential) {
                ASSERT(credential.persistence() == CredentialPersistenceNone);
                if (challenge.failureResponse().httpStatusCode() == 401) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    d->m_context->storageSession().credentialStorage().set(credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }
#if PLATFORM(COCOA)
                CFURLConnectionUseCredential(d->m_connection.get(), credential.cfCredential(), challenge.cfURLAuthChallengeRef());
#else
                RetainPtr<CFURLCredentialRef> cfCredential = adoptCF(createCF(credential));
                CFURLConnectionUseCredential(d->m_connection.get(), cfCredential.get(), challenge.cfURLAuthChallengeRef());
#endif
                return true;
            }
        }
    }

    return false;
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool ResourceHandle::canAuthenticateAgainstProtectionSpace(const ProtectionSpace& protectionSpace)
{
    ResourceHandleClient* client = this->client();
    if (d->m_usesAsyncCallbacks) {
        if (client)
            client->canAuthenticateAgainstProtectionSpaceAsync(this, protectionSpace);
        else
            continueCanAuthenticateAgainstProtectionSpace(false);
        return false; // Ignored by caller.
    }

    return client && client->canAuthenticateAgainstProtectionSpace(this, protectionSpace);
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

    if (credential.persistence() == CredentialPersistenceForSession && challenge.protectionSpace().authenticationScheme() != ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested) {
        // Manage per-session credentials internally, because once NSURLCredentialPersistencePerSession is used, there is no way
        // to ignore it for a particular request (short of removing it altogether).
        Credential webCredential(credential, CredentialPersistenceNone);

        URL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = challenge.failureResponse().url();      
        d->m_context->storageSession().credentialStorage().set(webCredential, challenge.protectionSpace(), urlToStore);

        if (d->m_connection) {
#if PLATFORM(COCOA)
            CFURLConnectionUseCredential(d->m_connection.get(), webCredential.cfCredential(), challenge.cfURLAuthChallengeRef());
#else
            RetainPtr<CFURLCredentialRef> cfCredential = adoptCF(createCF(webCredential));
            CFURLConnectionUseCredential(d->m_connection.get(), cfCredential.get(), challenge.cfURLAuthChallengeRef());
#endif
        }
    } else if (d->m_connection) {
#if PLATFORM(COCOA)
        CFURLConnectionUseCredential(d->m_connection.get(), credential.cfCredential(), challenge.cfURLAuthChallengeRef());
#else
        RetainPtr<CFURLCredentialRef> cfCredential = adoptCF(createCF(credential));
        CFURLConnectionUseCredential(d->m_connection.get(), cfCredential.get(), challenge.cfURLAuthChallengeRef());
#endif
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

    if (d->m_connection)
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

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge& challenge)
{
    LOG(Network, "CFNet - receivedRequestToPerformDefaultHandling()");
    ASSERT(!challenge.isNull());
    ASSERT(challenge.cfURLAuthChallengeRef());
    if (challenge != d->m_currentWebChallenge)
        return;

    if (d->m_connection)
        CFURLConnectionPerformDefaultHandlingForChallenge(d->m_connection.get(), challenge.cfURLAuthChallengeRef());

    clearAuthentication();
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge& challenge)
{
    LOG(Network, "CFNet - receivedChallengeRejection()");
    ASSERT(!challenge.isNull());
    ASSERT(challenge.cfURLAuthChallengeRef());
    if (challenge != d->m_currentWebChallenge)
        return;

    if (d->m_connection)
        CFURLConnectionRejectChallenge(d->m_connection.get(), challenge.cfURLAuthChallengeRef());

    clearAuthentication();
}

CFURLStorageSessionRef ResourceHandle::storageSession() const
{
    return d->m_storageSession.get();
}

CFURLConnectionRef ResourceHandle::connection() const
{
    return d->m_connection.get();
}

RetainPtr<CFURLConnectionRef> ResourceHandle::releaseConnectionForDownload()
{
    LOG(Network, "CFNet - Job %p releasing connection %p for download", this, d->m_connection.get());
    return WTF::move(d->m_connection);
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

    SynchronousLoaderClient client;
    client.setAllowStoredCredentials(storedCredentials == AllowStoredCredentials);

    RefPtr<ResourceHandle> handle = adoptRef(new ResourceHandle(context, request, &client, false /*defersLoading*/, true /*shouldContentSniff*/));

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

    while (!client.isDone())
        CFRunLoopRunInMode(synchronousLoadRunLoopMode(), UINT_MAX, true);

    error = client.error();

    CFURLConnectionCancel(handle->connection());

    if (error.isNull())
        response = client.response();

    data.swap(client.mutableData());
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

#if PLATFORM(COCOA)
void ResourceHandle::schedule(SchedulePair& pair)
{
    CFRunLoopRef runLoop = pair.runLoop();
    if (!runLoop)
        return;

    CFURLConnectionScheduleWithRunLoop(d->m_connection.get(), runLoop, pair.mode());
    if (d->m_startWhenScheduled) {
        CFURLConnectionStart(d->m_connection.get());
        d->m_startWhenScheduled = false;
    }
}

void ResourceHandle::unschedule(SchedulePair& pair)
{
    CFRunLoopRef runLoop = pair.runLoop();
    if (!runLoop)
        return;

    CFURLConnectionUnscheduleFromRunLoop(d->m_connection.get(), runLoop, pair.mode());
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

} // namespace WebCore
