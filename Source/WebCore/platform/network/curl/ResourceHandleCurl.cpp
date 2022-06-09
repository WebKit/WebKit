/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * All rights reserved.
 * Copyright (C) 2017 NAVER Corp.
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
#include "ResourceHandle.h"

#if USE(CURL)

#include "CookieJar.h"
#include "CredentialStorage.h"
#include "CurlCacheManager.h"
#include "CurlContext.h"
#include "CurlRequest.h"
#include "HTTPParsers.h"
#include "Logging.h"
#include "NetworkLoadMetrics.h"
#include "NetworkStorageSession.h"
#include "ResourceHandleInternal.h"
#include "SameSiteInfo.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include "SynchronousLoaderClient.h"
#include <pal/text/TextEncoding.h>
#include <wtf/CompletionHandler.h>
#include <wtf/FileSystem.h>
#include <wtf/text/Base64.h>

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_curlRequest)
        m_curlRequest->invalidateClient();
}

ResourceHandle::~ResourceHandle() = default;

bool ResourceHandle::start()
{
    ASSERT(isMainThread());

    CurlContext::singleton();

    // The frame could be null if the ResourceHandle is not associated to any
    // Frame, e.g. if we are downloading a file.
    // If the frame is not null but the page is null this must be an attempted
    // load from an unload handler, so let's just block it.
    // If both the frame and the page are not null the context is valid.
    if (d->m_context && !d->m_context->isValid())
        return false;

    // Only allow the POST and GET methods for non-HTTP requests.
    auto request = firstRequest();
    if (!request.url().protocolIsInHTTPFamily() && request.httpMethod() != "GET" && request.httpMethod() != "POST") {
        scheduleFailure(InvalidURLFailure); // Error must not be reported immediately
        return true;
    }

    d->m_curlRequest = createCurlRequest(WTFMove(request));

    if (auto credential = getCredential(d->m_firstRequest, false)) {
        d->m_curlRequest->setUserPass(credential->user(), credential->password());
        d->m_curlRequest->setAuthenticationScheme(ProtectionSpace::AuthenticationScheme::HTTPBasic);
    }

    d->m_curlRequest->start();

    return true;
}

void ResourceHandle::cancel()
{
    ASSERT(isMainThread());

    d->m_cancelled = true;

    if (d->m_curlRequest)
        d->m_curlRequest->cancel();
}

bool ResourceHandle::cancelledOrClientless()
{
    if (d->m_cancelled)
        return true;

    return !client();
}

void ResourceHandle::addCacheValidationHeaders(ResourceRequest& request)
{
    ASSERT(isMainThread());

    d->m_addedCacheValidationHeaders = false;

    auto hasCacheHeaders = request.httpHeaderFields().contains(HTTPHeaderName::IfModifiedSince) || request.httpHeaderFields().contains(HTTPHeaderName::IfNoneMatch);
    if (hasCacheHeaders)
        return;

    auto& cache = CurlCacheManager::singleton();
    URL cacheUrl = request.url();
    cacheUrl.removeFragmentIdentifier();

    if (cache.isCached(cacheUrl.string())) {
        cache.addCacheEntryClient(cacheUrl.string(), this);

        for (const auto& entry : cache.requestHeaders(cacheUrl.string()))
            request.addHTTPHeaderField(entry.key, entry.value);

        d->m_addedCacheValidationHeaders = true;
    }
}

Ref<CurlRequest> ResourceHandle::createCurlRequest(ResourceRequest&& request, RequestStatus status)
{
    ASSERT(isMainThread());

    if (status == RequestStatus::NewRequest) {
        addCacheValidationHeaders(request);

        auto includeSecureCookies = request.url().protocolIs("https") ? IncludeSecureCookies::Yes : IncludeSecureCookies::No;
        String cookieHeaderField = d->m_context->storageSession()->cookieRequestHeaderFieldValue(request.firstPartyForCookies(), SameSiteInfo::create(request), request.url(), std::nullopt, std::nullopt, includeSecureCookies, ShouldAskITP::Yes, ShouldRelaxThirdPartyCookieBlocking::No).first;
        if (!cookieHeaderField.isEmpty())
            request.addHTTPHeaderField(HTTPHeaderName::Cookie, cookieHeaderField);
    }

    CurlRequest::ShouldSuspend shouldSuspend = d->m_defersLoading ? CurlRequest::ShouldSuspend::Yes : CurlRequest::ShouldSuspend::No;
    auto curlRequest = CurlRequest::create(request, *delegate(), shouldSuspend, CurlRequest::EnableMultipart::Yes, CurlRequest::CaptureNetworkLoadMetrics::Basic, RefPtr<SynchronousLoaderMessageQueue>(d->m_messageQueue));

    return curlRequest;
}

CurlResourceHandleDelegate* ResourceHandle::delegate()
{
    if (!d->m_delegate)
        d->m_delegate = makeUnique<CurlResourceHandleDelegate>(*this);

    return d->m_delegate.get();
}

#if OS(WINDOWS)

void ResourceHandle::setHostAllowsAnyHTTPSCertificate(const String& host)
{
    ASSERT(isMainThread());

    CurlContext::singleton().sslHandle().allowAnyHTTPSCertificatesForHost(host);
}

void ResourceHandle::setClientCertificateInfo(const String& host, const String& certificate, const String& key)
{
    ASSERT(isMainThread());

    if (FileSystem::fileExists(certificate))
        CurlContext::singleton().sslHandle().setClientCertificateInfo(host, certificate, key);
    else
        LOG(Network, "Invalid client certificate file: %s!\n", certificate.latin1().data());
}

#endif

void ResourceHandle::platformSetDefersLoading(bool defers)
{
    ASSERT(isMainThread());

    if (defers == d->m_defersLoading)
        return;

    d->m_defersLoading = defers;

    if (!d->m_curlRequest)
        return;

    if (d->m_defersLoading)
        d->m_curlRequest->suspend();
    else
        d->m_curlRequest->resume();
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    return (!client() || client()->shouldUseCredentialStorage(this)) && firstRequest().url().protocolIsInHTTPFamily();
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    ASSERT(isMainThread());

    String partition = firstRequest().cachePartition();

    if (!d->m_user.isNull() && !d->m_password.isNull()) {
        Credential credential(d->m_user, d->m_password, CredentialPersistenceNone);

        URL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = challenge.failureResponse().url();
        d->m_context->storageSession()->credentialStorage().set(partition, credential, challenge.protectionSpace(), urlToStore);

        restartRequestWithCredential(challenge.protectionSpace(), credential);

        d->m_user = String();
        d->m_password = String();
        // FIXME: Per the specification, the user shouldn't be asked for credentials if there were incorrect ones provided explicitly.
        return;
    }

    if (shouldUseCredentialStorage()) {
        if (!d->m_initialCredential.isEmpty() || challenge.previousFailureCount()) {
            // The stored credential wasn't accepted, stop using it.
            // There is a race condition here, since a different credential might have already been stored by another ResourceHandle,
            // but the observable effect should be very minor, if any.
            d->m_context->storageSession()->credentialStorage().remove(partition, challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            Credential credential = d->m_context->storageSession()->credentialStorage().get(partition, challenge.protectionSpace());
            if (!credential.isEmpty() && credential != d->m_initialCredential) {
                ASSERT(credential.persistence() == CredentialPersistenceNone);
                if (challenge.failureResponse().httpStatusCode() == 401) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    d->m_context->storageSession()->credentialStorage().set(partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }

                restartRequestWithCredential(challenge.protectionSpace(), credential);
                return;
            }
        }
    }

    d->m_currentWebChallenge = challenge;

    if (client()) {
        Ref protectedThis { *this };
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
    }
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    ASSERT(isMainThread());

    if (!AuthenticationChallengeBase::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge))
        return;

    if (credential.isEmpty()) {
        receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    String partition = firstRequest().cachePartition();

    if (shouldUseCredentialStorage()) {
        if (challenge.failureResponse().httpStatusCode() == 401) {
            URL urlToStore = challenge.failureResponse().url();
            d->m_context->storageSession()->credentialStorage().set(partition, credential, challenge.protectionSpace(), urlToStore);
        }
    }

    restartRequestWithCredential(challenge.protectionSpace(), credential);

    clearAuthentication();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    ASSERT(isMainThread());

    if (!AuthenticationChallengeBase::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge))
        return;

    clearAuthentication();

    didReceiveResponse(ResourceResponse(delegate()->response()), [this, protectedThis = Ref { *this }] {
        continueAfterDidReceiveResponse();
    });
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    ASSERT(isMainThread());

    if (!AuthenticationChallengeBase::equalForWebKitLegacyChallengeComparison(challenge, d->m_currentWebChallenge))
        return;

    if (client()) {
        Ref protectedThis { *this };
        client()->receivedCancellation(this, challenge);
    }
}

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

std::optional<Credential> ResourceHandle::getCredential(const ResourceRequest& request, bool redirect)
{
    // m_user/m_pass are credentials given manually, for instance, by the arguments passed to XMLHttpRequest.open().
    Credential credential { d->m_user, d->m_password, CredentialPersistenceNone };

    if (shouldUseCredentialStorage()) {
        String partition = request.cachePartition();

        if (credential.isEmpty()) {
            // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication, 
            // try and reuse the credential preemptively, as allowed by RFC 2617.
            d->m_initialCredential = d->m_context->storageSession()->credentialStorage().get(partition, request.url());
        } else if (!redirect) {
            // If there is already a protection space known for the URL, update stored credentials
            // before sending a request. This makes it possible to implement logout by sending an
            // XMLHttpRequest with known incorrect credentials, and aborting it immediately (so that
            // an authentication dialog doesn't pop up).
            d->m_context->storageSession()->credentialStorage().set(partition, credential, request.url());
        }
    }

    if (!d->m_initialCredential.isEmpty())
        return d->m_initialCredential;

    return std::nullopt;
}

void ResourceHandle::restartRequestWithCredential(const ProtectionSpace& protectionSpace, const Credential& credential)
{
    ASSERT(isMainThread());

    if (!d->m_curlRequest)
        return;
    
    auto previousRequest = d->m_curlRequest->resourceRequest();
    d->m_curlRequest->cancel();

    d->m_curlRequest = createCurlRequest(WTFMove(previousRequest), RequestStatus::ReusedRequest);
    d->m_curlRequest->setAuthenticationScheme(protectionSpace.authenticationScheme());
    d->m_curlRequest->setUserPass(credential.user(), credential.password());
    d->m_curlRequest->start();
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentialsPolicy storedCredentialsPolicy, SecurityOrigin*, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    ASSERT(isMainThread());
    ASSERT(!request.isEmpty());

    SynchronousLoaderClient client;
    client.setAllowStoredCredentials(storedCredentialsPolicy == StoredCredentialsPolicy::Use);

    bool defersLoading = false;
    bool shouldContentSniff = true;
    bool shouldContentEncodingSniff = true;
    RefPtr<ResourceHandle> handle = adoptRef(new ResourceHandle(context, request, &client, defersLoading, shouldContentSniff, shouldContentEncodingSniff, nullptr, false));
    handle->d->m_messageQueue = &client.messageQueue();

    if (request.url().protocolIsData()) {
        handle->handleDataURL();
        return;
    }

    auto requestCopy = handle->firstRequest();
    handle->d->m_curlRequest = handle->createCurlRequest(WTFMove(requestCopy));

    if (auto credential = handle->getCredential(handle->d->m_firstRequest, false)) {
        handle->d->m_curlRequest->setUserPass(credential->user(), credential->password());
        handle->d->m_curlRequest->setAuthenticationScheme(ProtectionSpace::AuthenticationScheme::HTTPBasic);
    }

    handle->d->m_curlRequest->start();

    do {
        if (auto task = client.messageQueue().waitForMessage())
            (*task)();
    } while (!client.messageQueue().killed() && !handle->cancelledOrClientless());

    error = client.error();
    data.swap(client.mutableData());
    response = client.response();
}

void ResourceHandle::platformContinueSynchronousDidReceiveResponse()
{
    ASSERT(isMainThread());

    continueAfterDidReceiveResponse();
}

void ResourceHandle::continueAfterDidReceiveResponse()
{
    ASSERT(isMainThread());

    // continueDidReceiveResponse might cancel the load.
    if (cancelledOrClientless() || !d->m_curlRequest)
        return;

    d->m_curlRequest->completeDidReceiveResponse();
}

bool ResourceHandle::shouldRedirectAsGET(const ResourceRequest& request, bool crossOrigin)
{
    if (request.httpMethod() == "GET" || request.httpMethod() == "HEAD")
        return false;

    if (!request.url().protocolIsInHTTPFamily())
        return true;

    if (delegate()->response().isSeeOther())
        return true;

    if ((delegate()->response().isMovedPermanently() || delegate()->response().isFound()) && (request.httpMethod() == "POST"))
        return true;

    if (crossOrigin && (request.httpMethod() == "DELETE"))
        return true;

    return false;
}

void ResourceHandle::willSendRequest()
{
    ASSERT(isMainThread());

    static const int maxRedirects = 20;

    if (d->m_redirectCount++ > maxRedirects) {
        client()->didFail(this, ResourceError::httpError(CURLE_TOO_MANY_REDIRECTS, delegate()->response().url()));
        return;
    }

    String location = delegate()->response().httpHeaderField(HTTPHeaderName::Location);
    URL newURL = URL(delegate()->response().url(), location);
    bool crossOrigin = !protocolHostAndPortAreEqual(d->m_firstRequest.url(), newURL);

    ResourceRequest newRequest = d->m_firstRequest;
    newRequest.setURL(newURL);

    if (shouldRedirectAsGET(newRequest, crossOrigin)) {
        newRequest.setHTTPMethod("GET");
        newRequest.setHTTPBody(nullptr);
        newRequest.clearHTTPContentType();
    }

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (!newURL.protocolIs("https") && protocolIs(newRequest.httpReferrer(), "https") && context()->shouldClearReferrerOnHTTPSToHTTPRedirect())
        newRequest.clearHTTPReferrer();

    d->m_user = newURL.user();
    d->m_password = newURL.password();
    newRequest.removeCredentials();

    if (crossOrigin) {
        // If the network layer carries over authentication headers from the original request
        // in a cross-origin redirect, we want to clear those headers here. 
        newRequest.clearHTTPAuthorization();
        newRequest.clearHTTPOrigin();
    }

    ResourceResponse responseCopy = delegate()->response();
    client()->willSendRequestAsync(this, WTFMove(newRequest), WTFMove(responseCopy), [this, protectedThis = Ref { *this }] (ResourceRequest&& request) {
        continueAfterWillSendRequest(WTFMove(request));
    });
}

void ResourceHandle::continueAfterWillSendRequest(ResourceRequest&& request)
{
    ASSERT(isMainThread());

    // willSendRequest might cancel the load.
    if (cancelledOrClientless() || !d->m_curlRequest)
        return;

    if (request.isNull()) {
        cancel();
        return;
    }

    auto shouldForwardCredential = protocolHostAndPortAreEqual(request.url(), delegate()->response().url());
    auto credential = getCredential(request, true);

    d->m_curlRequest->cancel();
    d->m_curlRequest = createCurlRequest(WTFMove(request));

    if (shouldForwardCredential && credential)
        d->m_curlRequest->setUserPass(credential->user(), credential->password());

    d->m_curlRequest->start();
}

void ResourceHandle::handleDataURL()
{
    ASSERT(d->m_firstRequest.url().protocolIsData());
    String url = d->m_firstRequest.url().string();

    ASSERT(client());

    auto index = url.find(',');
    if (index == notFound) {
        client()->cannotShowURL(this);
        return;
    }

    String mediaType = url.substring(5, index - 5);
    String data = url.substring(index + 1);
    auto originalSize = data.length();

    bool base64 = mediaType.endsWithIgnoringASCIICase(";base64");
    if (base64)
        mediaType = mediaType.left(mediaType.length() - 7);

    if (mediaType.isEmpty())
        mediaType = "text/plain"_s;

    String mimeType = extractMIMETypeFromMediaType(mediaType);
    String charset = extractCharsetFromMediaType(mediaType);

    if (charset.isEmpty())
        charset = "US-ASCII"_s;

    ResourceResponse response;
    response.setMimeType(mimeType);
    response.setTextEncodingName(charset);
    response.setURL(d->m_firstRequest.url());

    if (base64) {
        data = PAL::decodeURLEscapeSequences(data);
        didReceiveResponse(WTFMove(response), [this, protectedThis = Ref { *this }] {
            continueAfterDidReceiveResponse();
        });

        // didReceiveResponse might cause the client to be deleted.
        if (client()) {
            auto decodedData = base64Decode(data, Base64DecodeOptions::IgnoreSpacesAndNewLines);
            if (decodedData && decodedData->size() > 0)
                client()->didReceiveBuffer(this, SharedBuffer::create(decodedData->data(), decodedData->size()), originalSize);
        }
    } else {
        PAL::TextEncoding encoding(charset);
        data = PAL::decodeURLEscapeSequences(data, encoding);
        didReceiveResponse(WTFMove(response), [this, protectedThis = Ref { *this }] {
            continueAfterDidReceiveResponse();
        });

        // didReceiveResponse might cause the client to be deleted.
        if (client()) {
            auto encodedData = encoding.encode(data, PAL::UnencodableHandling::URLEncodedEntities);
            if (encodedData.size())
                client()->didReceiveBuffer(this, SharedBuffer::create(WTFMove(encodedData)), originalSize);
        }
    }

    if (client())
        client()->didFinishLoading(this, { });
}

} // namespace WebCore

#endif
