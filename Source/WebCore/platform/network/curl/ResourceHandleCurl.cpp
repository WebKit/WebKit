/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * All rights reserved.
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

#include "CachedResourceLoader.h"
#include "CredentialStorage.h"
#include "CurlCacheManager.h"
#include "CurlContext.h"
#include "CurlJobManager.h"
#include "FileSystem.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "NetworkingContext.h"
#include "ResourceHandleInternal.h"
#include "SSLHandle.h"
#include "SynchronousLoaderClient.h"
#include <wtf/text/Base64.h>

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
}

bool ResourceHandle::start()
{
    ASSERT(isMainThread());

    // The frame could be null if the ResourceHandle is not associated to any
    // Frame, e.g. if we are downloading a file.
    // If the frame is not null but the page is null this must be an attempted
    // load from an unload handler, so let's just block it.
    // If both the frame and the page are not null the context is valid.
    if (d->m_context && !d->m_context->isValid())
        return false;

    d->initialize();

    d->m_job = CurlJobManager::singleton().add(d->m_curlHandle, [this, protectedThis = makeRef(*this)](CurlJobResult result) {
        ASSERT(isMainThread());

        switch (result) {
        case CurlJobResult::Done:
            d->didFinish();
            break;

        case CurlJobResult::Error:
            d->didFail();
            break;

        case CurlJobResult::Cancelled:
            break;
        }
    });
    ASSERT(d->m_job);

    return true;
}

void ResourceHandle::cancel()
{
    d->m_cancelled = true;
    CurlJobManager::singleton().cancel(d->m_job);
}

void ResourceHandleInternal::initialize()
{
    CurlContext& context = CurlContext::singleton();

    URL url = m_firstRequest.url();

    // Remove any fragment part, otherwise curl will send it as part of the request.
    url.removeFragmentIdentifier();

    String urlString = url.string();

    if (url.isLocalFile()) {
        // Remove any query part sent to a local file.
        if (!url.query().isEmpty()) {
            // By setting the query to a null string it'll be removed.
            url.setQuery(String());
            urlString = url.string();
        }
        // Determine the MIME type based on the path.
        m_response.setMimeType(MIMETypeRegistry::getMIMETypeForPath(url));
    }

    if (m_defersLoading) {
        CURLcode error = m_curlHandle.pause(CURLPAUSE_ALL);
        // If we did not pause the handle, we would ASSERT in the
        // header callback. So just assert here.
        ASSERT_UNUSED(error, error == CURLE_OK);
    }

#ifndef NDEBUG
    m_curlHandle.enableVerboseIfUsed();
    m_curlHandle.enableStdErrIfUsed();
#endif

    m_curlHandle.initialize();
    m_curlHandle.setSslVerifyPeer(CurlHandle::VerifyPeerEnable);
    m_curlHandle.setSslVerifyHost(CurlHandle::VerifyHostStrictNameCheck);
    m_curlHandle.setPrivateData(this);
    m_curlHandle.setWriteCallbackFunction(writeCallback, this);
    m_curlHandle.setHeaderCallbackFunction(headerCallback, this);
    m_curlHandle.enableAutoReferer();
    m_curlHandle.enableFollowLocation();
    m_curlHandle.enableHttpAuthentication(CURLAUTH_ANY);
    m_curlHandle.enableShareHandle();
    m_curlHandle.enableTimeout();
    m_curlHandle.enableAllowedProtocols();
    auto certificate = getSSLClientCertificate(m_firstRequest.url().host());
    if (certificate) {
        m_curlHandle.setSslCert((*certificate).first.utf8().data());
        m_curlHandle.setSslCertType("P12");
        m_curlHandle.setSslKeyPassword((*certificate).second.utf8().data());
    }

    if (CurlContext::singleton().shouldIgnoreSSLErrors())
        m_curlHandle.setSslVerifyPeer(CurlHandle::VerifyPeerDisable);
    else
        setSSLVerifyOptions(m_curlHandle);

    m_curlHandle.enableCAInfoIfExists();

    m_curlHandle.enableAcceptEncoding();
    m_curlHandle.setUrl(urlString);
    m_curlHandle.enableCookieJarIfExists();

    if (m_firstRequest.httpHeaderFields().size()) {
        auto customHeaders = m_firstRequest.httpHeaderFields();
        auto& cache = CurlCacheManager::getInstance();

        bool hasCacheHeaders = customHeaders.contains(HTTPHeaderName::IfModifiedSince) || customHeaders.contains(HTTPHeaderName::IfNoneMatch);
        if (!hasCacheHeaders && cache.isCached(url)) {
            cache.addCacheEntryClient(url, m_handle);

            // append additional cache information
            for (auto entry : cache.requestHeaders(url))
                customHeaders.set(entry.key, entry.value);

            m_addedCacheValidationHeaders = true;
        }

        m_curlHandle.appendRequestHeaders(customHeaders);
    }

    String method = m_firstRequest.httpMethod();
    if ("GET" == method)
        m_curlHandle.enableHttpGetRequest();
    else if ("POST" == method)
        setupPOST();
    else if ("PUT" == method)
        setupPUT();
    else if ("HEAD" == method)
        m_curlHandle.enableHttpHeadRequest();
    else {
        m_curlHandle.setHttpCustomRequest(method);
        setupPUT();
    }

    m_curlHandle.enableRequestHeaders();

    applyAuthentication();

    m_curlHandle.enableProxyIfExists();
}

void ResourceHandleInternal::applyAuthentication()
{
    // m_user/m_pass are credentials given manually, for instance, by the arguments passed to XMLHttpRequest.open().
    String partition = m_firstRequest.cachePartition();

    if (m_handle->shouldUseCredentialStorage()) {
        if (m_user.isEmpty() && m_pass.isEmpty()) {
            // <rdar://problem/7174050> - For URLs that match the paths of those previously challenged for HTTP Basic authentication, 
            // try and reuse the credential preemptively, as allowed by RFC 2617.
            m_initialCredential = CredentialStorage::defaultCredentialStorage().get(partition, m_firstRequest.url());
        } else {
            // If there is already a protection space known for the URL, update stored credentials
            // before sending a request. This makes it possible to implement logout by sending an
            // XMLHttpRequest with known incorrect credentials, and aborting it immediately (so that
            // an authentication dialog doesn't pop up).
            CredentialStorage::defaultCredentialStorage().set(partition, Credential(m_user, m_pass, CredentialPersistenceNone), m_firstRequest.url());
        }
    }

    String user = m_user;
    String password = m_pass;

    if (!m_initialCredential.isEmpty()) {
        user = m_initialCredential.user();
        password = m_initialCredential.password();
        m_curlHandle.enableHttpAuthentication(CURLAUTH_BASIC);
    }

    // It seems we need to set CURLOPT_USERPWD even if username and password is empty.
    // Otherwise cURL will not automatically continue with a new request after a 401 response.

    // curl CURLOPT_USERPWD expects username:password
    m_curlHandle.setHttpAuthUserPass(user, password);
}

static inline size_t getFormElementsCount(ResourceHandle* job)
{
    RefPtr<FormData> formData = job->firstRequest().httpBody();

    if (!formData)
        return 0;

    // Resolve the blob elements so the formData can correctly report it's size.
    formData = formData->resolveBlobReferences();
    size_t size = formData->elements().size();
    job->firstRequest().setHTTPBody(WTFMove(formData));

    return size;
}

void ResourceHandleInternal::setupPUT()
{
    m_curlHandle.enableHttpPutRequest();

    // Disable the Expect: 100 continue header
    m_curlHandle.appendRequestHeader("Expect:");

    size_t numElements = getFormElementsCount(m_handle);
    if (!numElements)
        return;

    setupFormData(false);
}

void ResourceHandleInternal::setupPOST()
{
    m_curlHandle.enableHttpPostRequest();

    size_t numElements = getFormElementsCount(m_handle);
    if (!numElements)
        return;

    // Do not stream for simple POST data
    if (numElements == 1) {
        m_firstRequest.httpBody()->flatten(m_postBytes);
        if (m_postBytes.size())
            m_curlHandle.setPostFields(m_postBytes.data(), m_postBytes.size());
        return;
    }

    setupFormData(true);
}

void ResourceHandleInternal::setupFormData(bool isPostRequest)
{
    Vector<FormDataElement> elements = m_firstRequest.httpBody()->elements();
    size_t numElements = elements.size();

    static const long long maxCurlOffT = m_curlHandle.maxCurlOffT();

    // Obtain the total size of the form data
    curl_off_t size = 0;
    bool chunkedTransfer = false;
    for (size_t i = 0; i < numElements; i++) {
        FormDataElement element = elements[i];
        if (element.m_type == FormDataElement::Type::EncodedFile) {
            long long fileSizeResult;
            if (getFileSize(element.m_filename, fileSizeResult)) {
                if (fileSizeResult > maxCurlOffT) {
                    // File size is too big for specifying it to cURL
                    chunkedTransfer = true;
                    break;
                }
                size += fileSizeResult;
            } else {
                chunkedTransfer = true;
                break;
            }
        } else
            size += elements[i].m_data.size();
    }

    // cURL guesses that we want chunked encoding as long as we specify the header
    if (chunkedTransfer)
        m_curlHandle.appendRequestHeader("Transfer-Encoding: chunked");
    else {
        if (isPostRequest)
            m_curlHandle.setPostFieldLarge(size);
        else
            m_curlHandle.setInFileSizeLarge(size);
    }

    m_curlHandle.setReadCallbackFunction(readCallback, this);
}

#if OS(WINDOWS)

void ResourceHandle::setHostAllowsAnyHTTPSCertificate(const String& host)
{
    ASSERT(isMainThread());

    allowsAnyHTTPSCertificateHosts(host);
}

void ResourceHandle::setClientCertificateInfo(const String& host, const String& certificate, const String& key)
{
    ASSERT(isMainThread());

    if (fileExists(certificate))
        addAllowedClientCertificate(host, certificate, key);
    else
        LOG(Network, "Invalid client certificate file: %s!\n", certificate.latin1().data());
}

#endif

#if OS(WINDOWS) && USE(CF)

void ResourceHandle::setClientCertificate(const String&, CFDataRef)
{
}

#endif

void ResourceHandle::platformSetDefersLoading(bool defers)
{
    ASSERT(isMainThread());

    auto action = [defers, this, protectedThis = makeRef(*this)]() {
        if (defers) {
            CURLcode error = d->m_curlHandle.pause(CURLPAUSE_ALL);
            // If we could not defer the handle, so don't do it.
            if (error != CURLE_OK)
                return;
        } else {
            CURLcode error = d->m_curlHandle.pause(CURLPAUSE_CONT);
            if (error != CURLE_OK) {
                // Restarting the handle has failed so just cancel it.
                cancel();
            }
        }
    };

    if (d->m_job) {
        CurlJobManager::singleton().callOnJobThread(WTFMove(action));
    } else {
        action();
    }
}

void ResourceHandleInternal::didFinish()
{
    calculateWebTimingInformations();

    if (m_cancelled)
        return;

    if (!m_response.responseFired()) {
        handleLocalReceiveResponse();
        if (m_cancelled)
            return;
    }

    if (m_multipartHandle)
        m_multipartHandle->contentEnded();

    if (client()) {
        client()->didFinishLoading(m_handle);
        CurlCacheManager::getInstance().didFinishLoading(*m_handle);
    }
}

void ResourceHandleInternal::didFail()
{
    if (m_cancelled)
        return;
    URL url = m_curlHandle.getEffectiveURL();
    if (client()) {
        client()->didFail(m_handle, ResourceError(CurlContext::errorDomain, m_curlHandle.errorCode(), m_curlHandle.getEffectiveURL(), m_curlHandle.errorDescription(), m_curlHandle.getSslErrors()));
        CurlCacheManager::getInstance().didFail(*m_handle);
    }
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    return (!client() || client()->shouldUseCredentialStorage(this)) && firstRequest().url().protocolIsInHTTPFamily();
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    ASSERT(isMainThread());

    String partition = firstRequest().cachePartition();

    if (!d->m_user.isNull() && !d->m_pass.isNull()) {
        Credential credential(d->m_user, d->m_pass, CredentialPersistenceNone);

        URL urlToStore;
        if (challenge.failureResponse().httpStatusCode() == 401)
            urlToStore = challenge.failureResponse().url();
        CredentialStorage::defaultCredentialStorage().set(partition, credential, challenge.protectionSpace(), urlToStore);
        
        d->m_curlHandle.setHttpAuthUserPass(credential.user(), credential.password());

        d->m_user = String();
        d->m_pass = String();
        // FIXME: Per the specification, the user shouldn't be asked for credentials if there were incorrect ones provided explicitly.
        return;
    }

    if (shouldUseCredentialStorage()) {
        if (!d->m_initialCredential.isEmpty() || challenge.previousFailureCount()) {
            // The stored credential wasn't accepted, stop using it.
            // There is a race condition here, since a different credential might have already been stored by another ResourceHandle,
            // but the observable effect should be very minor, if any.
            CredentialStorage::defaultCredentialStorage().remove(partition, challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            Credential credential = CredentialStorage::defaultCredentialStorage().get(partition, challenge.protectionSpace());
            if (!credential.isEmpty() && credential != d->m_initialCredential) {
                ASSERT(credential.persistence() == CredentialPersistenceNone);
                if (challenge.failureResponse().httpStatusCode() == 401) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    CredentialStorage::defaultCredentialStorage().set(partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }

                d->m_curlHandle.setHttpAuthUserPass(credential.user(), credential.password());
                return;
            }
        }
    }

    d->m_currentWebChallenge = challenge;
    
    if (client())
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    ASSERT(isMainThread());

    if (challenge != d->m_currentWebChallenge)
        return;

    if (credential.isEmpty()) {
        receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    String partition = firstRequest().cachePartition();

    if (shouldUseCredentialStorage()) {
        if (challenge.failureResponse().httpStatusCode() == 401) {
            URL urlToStore = challenge.failureResponse().url();
            CredentialStorage::defaultCredentialStorage().set(partition, credential, challenge.protectionSpace(), urlToStore);
        }
    }

    d->m_curlHandle.setHttpAuthUserPass(credential.user(), credential.password());
    clearAuthentication();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    ASSERT(isMainThread());

    if (challenge != d->m_currentWebChallenge)
        return;

    d->m_curlHandle.setHttpAuthUserPass("", "");
    clearAuthentication();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    ASSERT(isMainThread());

    if (challenge != d->m_currentWebChallenge)
        return;

    if (client())
        client()->receivedCancellation(this, challenge);
}

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandleInternal::calculateWebTimingInformations()
{
    double preTransferTime = 0;
    double dnslookupTime = 0;
    double connectTime = 0;
    double appConnectTime = 0;

    m_curlHandle.getTimes(preTransferTime, dnslookupTime, connectTime, appConnectTime);

    m_response.deprecatedNetworkLoadMetrics().domainLookupStart = Seconds(0);
    m_response.deprecatedNetworkLoadMetrics().domainLookupEnd = Seconds(dnslookupTime);

    m_response.deprecatedNetworkLoadMetrics().connectStart = Seconds(dnslookupTime);
    m_response.deprecatedNetworkLoadMetrics().connectEnd = Seconds(connectTime);

    m_response.deprecatedNetworkLoadMetrics().requestStart = Seconds(connectTime);
    m_response.deprecatedNetworkLoadMetrics().responseStart = Seconds(preTransferTime);

    if (appConnectTime)
        m_response.deprecatedNetworkLoadMetrics().secureConnectionStart = Seconds(connectTime);
}

void ResourceHandleInternal::handleLocalReceiveResponse()
{
    ASSERT(isMainThread());

    // since the code in headerCallback will not have run for local files
    // the code to set the URL and fire didReceiveResponse is never run,
    // which means the ResourceLoader's response does not contain the URL.
    // Run the code here for local files to resolve the issue.
    // TODO: See if there is a better approach for handling this.
    URL url = m_curlHandle.getEffectiveURL();
    ASSERT(url.isValid());
    m_response.setURL(url);
    if (client())
        client()->didReceiveResponse(m_handle, ResourceResponse(m_response));
    m_response.setResponseFired(true);
}

inline static bool isHttpInfo(int statusCode)
{
    return 100 <= statusCode && statusCode < 200;
}

inline static bool isHttpRedirect(int statusCode)
{
    return 300 <= statusCode && statusCode < 400 && statusCode != 304;
}

inline static bool isHttpAuthentication(int statusCode)
{
    return statusCode == 401;
}

inline static bool isHttpNotModified(int statusCode)
{
    return statusCode == 304;
}

static bool isAppendableHeader(const String &key)
{
    static const char* appendableHeaders[] = {
        "access-control-allow-headers",
        "access-control-allow-methods",
        "access-control-allow-origin",
        "access-control-expose-headers",
        "allow",
        "cache-control",
        "connection",
        "content-encoding",
        "content-language",
        "if-match",
        "if-none-match",
        "keep-alive",
        "pragma",
        "proxy-authenticate",
        "public",
        "server",
        "set-cookie",
        "te",
        "trailer",
        "transfer-encoding",
        "upgrade",
        "user-agent",
        "vary",
        "via",
        "warning",
        "www-authenticate"
    };

    // Custom headers start with 'X-', and need no further checking.
    if (key.startsWith("x-", /* caseSensitive */ false))
        return true;

    for (auto& header : appendableHeaders) {
        if (equalIgnoringASCIICase(key, header))
            return true;
    }

    return false;
}

static void removeLeadingAndTrailingQuotes(String& value)
{
    unsigned length = value.length();
    if (value.startsWith('"') && value.endsWith('"') && length > 1)
        value = value.substring(1, length - 2);
}

static bool getProtectionSpace(ResourceHandle* job, const ResourceResponse& response, ProtectionSpace& protectionSpace)
{
    ResourceHandleInternal* d = job->getInternal();

    CURLcode err;

    long port = 0;
    err = d->m_curlHandle.getPrimaryPort(port);
    if (err != CURLE_OK)
        return false;

    long availableAuth = CURLAUTH_NONE;
    err = d->m_curlHandle.getHttpAuthAvail(availableAuth);
    if (err != CURLE_OK)
        return false;

    URL url = d->m_curlHandle.getEffectiveURL();
    if (!url.isValid())
        return false;

    String host = url.host();
    StringView protocol = url.protocol();

    String realm;

    const String authHeader = response.httpHeaderField(HTTPHeaderName::Authorization);
    const String realmString = "realm=";
    int realmPos = authHeader.find(realmString);
    if (realmPos > 0) {
        realm = authHeader.substring(realmPos + realmString.length());
        realm = realm.left(realm.find(','));
        removeLeadingAndTrailingQuotes(realm);
    }

    ProtectionSpaceServerType serverType = ProtectionSpaceServerHTTP;
    if (protocol == "https")
        serverType = ProtectionSpaceServerHTTPS;

    ProtectionSpaceAuthenticationScheme authScheme = ProtectionSpaceAuthenticationSchemeUnknown;

    if (availableAuth & CURLAUTH_BASIC)
        authScheme = ProtectionSpaceAuthenticationSchemeHTTPBasic;
    if (availableAuth & CURLAUTH_DIGEST)
        authScheme = ProtectionSpaceAuthenticationSchemeHTTPDigest;
    if (availableAuth & CURLAUTH_GSSNEGOTIATE)
        authScheme = ProtectionSpaceAuthenticationSchemeNegotiate;
    if (availableAuth & CURLAUTH_NTLM)
        authScheme = ProtectionSpaceAuthenticationSchemeNTLM;

    protectionSpace = ProtectionSpace(host, port, serverType, realm, authScheme);

    return true;
}

size_t ResourceHandleInternal::willPrepareSendData(char* ptr, size_t blockSize, size_t numberOfBlocks)
{
    if (!m_formDataStream.hasMoreElements())
        return 0;

    size_t size = m_formDataStream.read(ptr, blockSize, numberOfBlocks);

    // Something went wrong so cancel the job.
    if (!size) {
        m_handle->cancel();
        return 0;
    }

    return size;

}

void ResourceHandleInternal::didReceiveHeaderLine(const String& header)
{
    ASSERT(isMainThread());

    auto splitPosition = header.find(":");
    if (splitPosition != notFound) {
        String key = header.left(splitPosition).stripWhiteSpace();
        String value = header.substring(splitPosition + 1).stripWhiteSpace();

        if (isAppendableHeader(key))
            m_response.addHTTPHeaderField(key, value);
        else
            m_response.setHTTPHeaderField(key, value);
    } else if (header.startsWith("HTTP", false)) {
        // This is the first line of the response.
        // Extract the http status text from this.
        //
        // If the FOLLOWLOCATION option is enabled for the curl handle then
        // curl will follow the redirections internally. Thus this header callback
        // will be called more than one time with the line starting "HTTP" for one job.
        long httpCode = 0;
        m_curlHandle.getResponseCode(httpCode);

        String httpCodeString = String::number(httpCode);
        int statusCodePos = header.find(httpCodeString);

        if (statusCodePos != notFound) {
            // The status text is after the status code.
            String status = header.substring(statusCodePos + httpCodeString.length());
            m_response.setHTTPStatusText(status.stripWhiteSpace());
        }
    }
}

void ResourceHandleInternal::didReceiveAllHeaders(long httpCode, long long contentLength)
{
    ASSERT(isMainThread());

    m_response.setExpectedContentLength(contentLength);

    m_response.setURL(m_curlHandle.getEffectiveURL());

    m_response.setHTTPStatusCode(httpCode);
    m_response.setMimeType(extractMIMETypeFromMediaType(m_response.httpHeaderField(HTTPHeaderName::ContentType)).convertToASCIILowercase());
    m_response.setTextEncodingName(extractCharsetFromMediaType(m_response.httpHeaderField(HTTPHeaderName::ContentType)));

    if (m_response.isMultipart()) {
        String boundary;
        bool parsed = MultipartHandle::extractBoundary(m_response.httpHeaderField(HTTPHeaderName::ContentType), boundary);
        if (parsed)
            m_multipartHandle = std::make_unique<MultipartHandle>(m_handle, boundary);
    }

    // HTTP redirection
    if (isHttpRedirect(httpCode)) {
        String location = m_response.httpHeaderField(HTTPHeaderName::Location);
        if (!location.isEmpty()) {
            URL newURL = URL(m_firstRequest.url(), location);

            ResourceRequest redirectedRequest = m_firstRequest;
            redirectedRequest.setURL(newURL);
            ResourceResponse response = m_response;
            if (client())
                client()->willSendRequest(m_handle, WTFMove(redirectedRequest), WTFMove(response));

            m_firstRequest.setURL(newURL);

            return;
        }
    } else if (isHttpAuthentication(httpCode)) {
        ProtectionSpace protectionSpace;
        if (getProtectionSpace(m_handle, m_response, protectionSpace)) {
            Credential credential;
            AuthenticationChallenge challenge(protectionSpace, credential, m_authFailureCount, m_response, ResourceError());
            challenge.setAuthenticationClient(m_handle);
            m_handle->didReceiveAuthenticationChallenge(challenge);
            m_authFailureCount++;
            return;
        }
    }

    if (client()) {
        if (isHttpNotModified(httpCode)) {
            const String& url = m_firstRequest.url().string();
            if (CurlCacheManager::getInstance().getCachedResponse(url, m_response)) {
                if (m_addedCacheValidationHeaders) {
                    m_response.setHTTPStatusCode(200);
                    m_response.setHTTPStatusText("OK");
                }
            }
        }
        client()->didReceiveResponse(m_handle, ResourceResponse(m_response));
        CurlCacheManager::getInstance().didReceiveResponse(*m_handle, m_response);
    }

    m_response.setResponseFired(true);
}

void ResourceHandleInternal::didReceiveContentData()
{
    ASSERT(isMainThread());

    if (!m_response.responseFired())
        handleLocalReceiveResponse();

    Vector<char> buffer;
    {
        LockHolder locker { m_receivedBufferMutex };
        buffer = WTFMove(m_receivedBuffer);
    }

    char* ptr = buffer.begin();
    size_t size = buffer.size();

    if (m_multipartHandle)
        m_multipartHandle->contentReceived(static_cast<const char*>(ptr), size);
    else if (client()) {
        client()->didReceiveData(m_handle, ptr, size, 0);
        CurlCacheManager::getInstance().didReceiveData(*m_handle, ptr, size);
    }
}

/* This is called to obtain HTTP POST or PUT data.
Iterate through FormData elements and upload files.
Carefully respect the given buffer size and fill the rest of the data at the next calls.
*/
size_t ResourceHandleInternal::readCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    ASSERT(!isMainThread());

    ResourceHandleInternal* d = static_cast<ResourceHandleInternal*>(data);

    if (d->m_cancelled)
        return 0;

    // We should never be called when deferred loading is activated.
    ASSERT(!d->m_defersLoading);

    if (!size || !nmemb)
        return 0;

    return d->willPrepareSendData(ptr, size, nmemb);
}

/*
* This is being called for each HTTP header in the response. This includes '\r\n'
* for the last line of the header.
*
* We will add each HTTP Header to the ResourceResponse and on the termination
* of the header (\r\n) we will parse Content-Type and Content-Disposition and
* update the ResourceResponse and then send it away.
*
*/
size_t ResourceHandleInternal::headerCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    ASSERT(!isMainThread());

    ResourceHandleInternal* d = static_cast<ResourceHandleInternal*>(data);
    ResourceHandle* job = d->m_handle;

    if (d->m_cancelled)
        return 0;

    // We should never be called when deferred loading is activated.
    ASSERT(!d->m_defersLoading);

    size_t totalSize = size * nmemb;

    String header(static_cast<const char*>(ptr), totalSize);

    /*
    * a) We can finish and send the ResourceResponse
    * b) We will add the current header to the HTTPHeaderMap of the ResourceResponse
    *
    * The HTTP standard requires to use \r\n but for compatibility it recommends to
    * accept also \n.
    */
    if (header == AtomicString("\r\n") || header == AtomicString("\n")) {
        long httpCode = 0;
        d->m_curlHandle.getResponseCode(httpCode);

        if (!httpCode) {
            // Comes here when receiving 200 Connection Established. Just return.
            return totalSize;
        }
        if (isHttpInfo(httpCode)) {
            // Just return when receiving http info, e.g. HTTP/1.1 100 Continue.
            // If not, the request might be cancelled, because the MIME type will be empty for this response.
            return totalSize;
        }

        long long contentLength = 0;
        d->m_curlHandle.getContentLenghtDownload(contentLength);

        callOnMainThread([job = RefPtr<ResourceHandle>(job), d, httpCode, contentLength] {
            if (!d->m_cancelled)
                d->didReceiveAllHeaders(httpCode, contentLength);
        });
    } else {
        callOnMainThread([job = RefPtr<ResourceHandle>(job), d, header] {
            if (!d->m_cancelled)
                d->didReceiveHeaderLine(header);
        });
    }

    return totalSize;
}

// called with data after all headers have been processed via headerCallback
size_t ResourceHandleInternal::writeCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    ASSERT(!isMainThread());

    ResourceHandleInternal* d = static_cast<ResourceHandleInternal*>(data);
    ResourceHandle* job = d->m_handle;

    if (d->m_cancelled)
        return 0;

    // We should never be called when deferred loading is activated.
    ASSERT(!d->m_defersLoading);

    size_t totalSize = size * nmemb;

    // this shouldn't be necessary but apparently is. CURL writes the data
    // of html page even if it is a redirect that was handled internally
    // can be observed e.g. on gmail.com
    long httpCode = 0;
    CURLcode errCd = d->m_curlHandle.getResponseCode(httpCode);
    if (CURLE_OK == errCd && httpCode >= 300 && httpCode < 400)
        return totalSize;

    bool shouldCall { false };
    {
        LockHolder locker(d->m_receivedBufferMutex);
        
        if (d->m_receivedBuffer.isEmpty())
            shouldCall = true;
        
        d->m_receivedBuffer.append(ptr, totalSize);
    }

    if (shouldCall) {
        callOnMainThread([job = RefPtr<ResourceHandle>(job), d] {
            if (!d->m_cancelled)
                d->didReceiveContentData();
        });
    }

    return totalSize;
}

// sync loader

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    ASSERT(isMainThread());

    SynchronousLoaderClient client;
    RefPtr<ResourceHandle> handle = adoptRef(new ResourceHandle(context, request, &client, false, false));

    handle->d->dispatchSynchronousJob();

    error = client.error();
    data.swap(client.mutableData());
    response = client.response();
}

void ResourceHandleInternal::dispatchSynchronousJob()
{
    URL kurl = m_firstRequest.url();

    if (kurl.protocolIsData()) {
        handleDataURL();
        return;
    }

    // If defersLoading is true and we call curl_easy_perform
    // on a paused handle, libcURL would do the transfert anyway
    // and we would assert so force defersLoading to be false.
    m_defersLoading = false;

    initialize();

    // curl_easy_perform blocks until the transfert is finished.
    CURLcode ret = m_curlHandle.perform();

    calculateWebTimingInformations();

    if (client()) {
        if (ret != CURLE_OK)
            client()->didFail(m_handle, ResourceError(CurlContext::errorDomain, m_curlHandle.errorCode(), m_curlHandle.getEffectiveURL(), m_curlHandle.errorDescription(), m_curlHandle.getSslErrors()));
        else
            client()->didReceiveResponse(m_handle, ResourceResponse(m_response));
    }
}

void ResourceHandleInternal::handleDataURL()
{
    ASSERT(m_firstRequest.url().protocolIsData());
    String url = m_firstRequest.url().string();

    ASSERT(client());

    int index = url.find(',');
    if (index == -1) {
        client()->cannotShowURL(m_handle);
        return;
    }

    String mediaType = url.substring(5, index - 5);
    String data = url.substring(index + 1);

    bool base64 = mediaType.endsWith(";base64", false);
    if (base64)
        mediaType = mediaType.left(mediaType.length() - 7);

    if (mediaType.isEmpty())
        mediaType = "text/plain";

    String mimeType = extractMIMETypeFromMediaType(mediaType);
    String charset = extractCharsetFromMediaType(mediaType);

    if (charset.isEmpty())
        charset = "US-ASCII";

    ResourceResponse response;
    response.setMimeType(mimeType);
    response.setTextEncodingName(charset);
    response.setURL(m_firstRequest.url());

    if (base64) {
        data = decodeURLEscapeSequences(data);
        client()->didReceiveResponse(m_handle, WTFMove(response));

        // didReceiveResponse might cause the client to be deleted.
        if (client()) {
            Vector<char> out;
            if (base64Decode(data, out, Base64IgnoreSpacesAndNewLines) && out.size() > 0)
                client()->didReceiveData(m_handle, out.data(), out.size(), 0);
        }
    } else {
        TextEncoding encoding(charset);
        data = decodeURLEscapeSequences(data, encoding);
        client()->didReceiveResponse(m_handle, WTFMove(response));

        // didReceiveResponse might cause the client to be deleted.
        if (client()) {
            CString encodedData = encoding.encode(data, URLEncodedEntitiesForUnencodables);
            if (encodedData.length())
                client()->didReceiveData(m_handle, encodedData.data(), encodedData.length(), 0);
        }
    }

    if (client())
        client()->didFinishLoading(m_handle);
}

} // namespace WebCore

#endif
