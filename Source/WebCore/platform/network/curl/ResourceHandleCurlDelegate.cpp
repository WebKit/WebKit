/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * All rights reserved.
 * Copyright (C) 2017 NAVER Corp. All rights reserved.
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
#include "ResourceHandleCurlDelegate.h"

#if USE(CURL)

#include "CredentialStorage.h"
#include "CurlCacheManager.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "MultipartHandle.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "URL.h"
#include <wtf/MainThread.h>
#include <wtf/text/Base64.h>

namespace WebCore {

ResourceHandleCurlDelegate::ResourceHandleCurlDelegate(ResourceHandle* handle)
    : m_handle(handle)
    , m_firstRequest(handle->firstRequest().isolatedCopy())
    , m_customHTTPHeaderFields(m_firstRequest.httpHeaderFields().isolatedCopy())
    , m_shouldUseCredentialStorage(handle->shouldUseCredentialStorage())
    , m_user(handle->getInternal()->m_user.isolatedCopy())
    , m_pass(handle->getInternal()->m_pass.isolatedCopy())
    , m_initialCredential(handle->getInternal()->m_initialCredential)
    , m_defersLoading(handle->getInternal()->m_defersLoading)
{
    const URL& url = m_firstRequest.url();

    if (url.isLocalFile()) {
        // Determine the MIME type based on the path.
        response().setMimeType(MIMETypeRegistry::getMIMETypeForPath(url));
    }

    if (m_customHTTPHeaderFields.size()) {
        auto& cache = CurlCacheManager::getInstance();
        bool hasCacheHeaders = m_customHTTPHeaderFields.contains(HTTPHeaderName::IfModifiedSince) || m_customHTTPHeaderFields.contains(HTTPHeaderName::IfNoneMatch);
        if (!hasCacheHeaders && cache.isCached(url)) {
            cache.addCacheEntryClient(url, m_handle);
            // append additional cache information
            for (auto entry : cache.requestHeaders(url))
                m_customHTTPHeaderFields.set(entry.key, entry.value);
            m_addedCacheValidationHeaders = true;
        }
    }

    setupAuthentication();
}

ResourceHandleCurlDelegate::~ResourceHandleCurlDelegate()
{
}

bool ResourceHandleCurlDelegate::hasHandle() const
{
    return !!m_handle;
}

void ResourceHandleCurlDelegate::releaseHandle()
{
    m_handle = nullptr;
}

void ResourceHandleCurlDelegate::start(bool isSyncRequest)
{
    m_isSyncRequest = isSyncRequest;

    if (!m_isSyncRequest) {
        // For asynchronous, use CurlJobManager. Curl processes runs on sub thread.
        CurlJobManager::singleton().add(this);
    } else {
        // For synchronous, does not use CurlJobManager. Curl processes runs on main thread.
        retain();
        setupTransfer();

        // curl_easy_perform blocks until the transfer is finished.
        CURLcode resultCode = m_curlHandle.perform();
        didCompleteTransfer(resultCode);
        release();
    }
}

void ResourceHandleCurlDelegate::cancel()
{
    releaseHandle();

    if (!m_isSyncRequest)
        CurlJobManager::singleton().cancel(this);
}

void ResourceHandleCurlDelegate::setDefersLoading(bool defers)
{
    if (defers == m_defersLoading)
        return;

    m_defersLoading = defers;

    auto action = [protectedThis = makeRef(*this)]() {
        if (protectedThis->m_defersLoading) {
            CURLcode error = protectedThis->m_curlHandle.pause(CURLPAUSE_ALL);
            // If we could not defer the handle, so don't do it.
            if (error != CURLE_OK)
                return;
        } else {
            CURLcode error = protectedThis->m_curlHandle.pause(CURLPAUSE_CONT);
            if (error != CURLE_OK) {
                // Restarting the handle has failed so just cancel it.
                protectedThis->m_handle->cancel();
            }
        }
    };

    CurlJobManager::singleton().callOnJobThread(WTFMove(action));
}

void ResourceHandleCurlDelegate::setAuthentication(const String& user, const String& pass)
{
    auto action = [protectedThis = makeRef(*this), user = user.isolatedCopy(), pass = pass.isolatedCopy()]() {
        protectedThis->m_user = user;
        protectedThis->m_pass = pass;
        protectedThis->m_curlHandle.setHttpAuthUserPass(user, pass);
    };

    CurlJobManager::singleton().callOnJobThread(WTFMove(action));
}

void ResourceHandleCurlDelegate::dispatchSynchronousJob()
{
    if (m_firstRequest.url().protocolIsData()) {
        handleDataURL();
        return;
    }

    // If defersLoading is true and we call curl_easy_perform
    // on a paused handle, libcURL would do the transfert anyway
    // and we would assert so force defersLoading to be false.
    m_defersLoading = false;

    start(true);
}

void ResourceHandleCurlDelegate::retain()
{
    ref();
}

void ResourceHandleCurlDelegate::release()
{
    deref();
}

CURL* ResourceHandleCurlDelegate::setupTransfer()
{
    CurlContext& context = CurlContext::singleton();

    m_curlHandle.initialize();

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

    auto& sslHandle = CurlContext::singleton().sslHandle();

    m_curlHandle.setSslVerifyPeer(CurlHandle::VerifyPeer::Enable);
    m_curlHandle.setSslVerifyHost(CurlHandle::VerifyHost::StrictNameCheck);
    m_curlHandle.setWriteCallbackFunction(didReceiveDataCallback, this);
    m_curlHandle.setHeaderCallbackFunction(didReceiveHeaderCallback, this);
    m_curlHandle.enableAutoReferer();
    m_curlHandle.enableFollowLocation();
    m_curlHandle.enableHttpAuthentication(CURLAUTH_ANY);
    m_curlHandle.enableShareHandle();
    m_curlHandle.enableTimeout();
    m_curlHandle.enableAllowedProtocols();

    auto sslClientCertificate = sslHandle.getSSLClientCertificate(m_firstRequest.url().host());
    if (sslClientCertificate) {
        m_curlHandle.setSslCert(sslClientCertificate->first.utf8().data());
        m_curlHandle.setSslCertType("P12");
        m_curlHandle.setSslKeyPassword(sslClientCertificate->second.utf8().data());
    }

    if (sslHandle.shouldIgnoreSSLErrors())
        m_curlHandle.setSslVerifyPeer(CurlHandle::VerifyPeer::Disable);
    else
        m_curlHandle.setSslCtxCallbackFunction(willSetupSslCtxCallback, this);

    m_curlHandle.setCACertPath(sslHandle.getCACertPath());

    m_curlHandle.enableAcceptEncoding();
    m_curlHandle.setUrl(m_firstRequest.url());
    m_curlHandle.enableCookieJarIfExists();

    if (m_customHTTPHeaderFields.size())
        m_curlHandle.appendRequestHeaders(m_customHTTPHeaderFields);

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

    applyAuthentication();

    m_curlHandle.enableProxyIfExists();

    return m_curlHandle.handle();
}

void ResourceHandleCurlDelegate::didCompleteTransfer(CURLcode result)
{
    if (result == CURLE_OK) {
        NetworkLoadMetrics networkLoadMetrics = getNetworkLoadMetrics();

        if (isMainThread())
            didFinish(networkLoadMetrics);
        else {
            callOnMainThread([protectedThis = makeRef(*this), metrics = networkLoadMetrics.isolatedCopy()] {
                if (!protectedThis->m_handle)
                    return;
                protectedThis->didFinish(metrics);
            });
        }
    } else {
        ResourceError resourceError = ResourceError::httpError(result, m_firstRequest.url());
        if (m_sslVerifier.sslErrors())
            resourceError.setSslErrors(m_sslVerifier.sslErrors());

        if (isMainThread())
            didFail(resourceError);
        else {
            callOnMainThread([protectedThis = makeRef(*this), error = resourceError.isolatedCopy()] {
                if (!protectedThis->m_handle)
                    return;
                protectedThis->didFail(error);
            });
        }
    }

    m_formDataStream = nullptr;
}

void ResourceHandleCurlDelegate::didCancelTransfer()
{
    m_formDataStream = nullptr;
}

ResourceResponse& ResourceHandleCurlDelegate::response()
{
    return m_handle->getInternal()->m_response;
}

void ResourceHandleCurlDelegate::setupAuthentication()
{
    // m_user/m_pass are credentials given manually, for instance, by the arguments passed to XMLHttpRequest.open().
    String partition = m_firstRequest.cachePartition();

    if (m_shouldUseCredentialStorage) {
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
}

inline static bool isHttpInfo(int statusCode)
{
    return 100 <= statusCode && statusCode < 200;
}

void ResourceHandleCurlDelegate::didReceiveAllHeaders(long httpCode, long long contentLength, uint16_t connectPort, long availableHttpAuth)
{
    ASSERT(isMainThread());

    response().setURL(m_curlHandle.getEffectiveURL());

    response().setExpectedContentLength(contentLength);
    response().setHTTPStatusCode(httpCode);
    response().setMimeType(extractMIMETypeFromMediaType(response().httpHeaderField(HTTPHeaderName::ContentType)).convertToASCIILowercase());
    response().setTextEncodingName(extractCharsetFromMediaType(response().httpHeaderField(HTTPHeaderName::ContentType)));

    if (response().isMultipart()) {
        String boundary;
        bool parsed = MultipartHandle::extractBoundary(response().httpHeaderField(HTTPHeaderName::ContentType), boundary);
        if (parsed)
            m_multipartHandle = std::make_unique<MultipartHandle>(m_handle, boundary);
    }

    // HTTP redirection
    if (response().isRedirection()) {
        String location = response().httpHeaderField(HTTPHeaderName::Location);
        if (!location.isEmpty()) {
            URL newURL = URL(m_firstRequest.url(), location);

            ResourceRequest redirectedRequest = m_firstRequest;
            redirectedRequest.setURL(newURL);
            ResourceResponse localResponse = response();
            if (m_handle->client())
                m_handle->client()->willSendRequest(m_handle, WTFMove(redirectedRequest), WTFMove(localResponse));

            m_firstRequest.setURL(newURL);

            return;
        }
    } else if (response().isUnauthorized()) {
        AuthenticationChallenge challenge(connectPort, availableHttpAuth, m_authFailureCount, response(), m_handle);
        m_handle->didReceiveAuthenticationChallenge(challenge);
        m_authFailureCount++;
        return;
    }

    response().setResponseFired(true);

    if (m_handle->client()) {
        if (response().isNotModified()) {
            const String& url = m_firstRequest.url().string();
            if (CurlCacheManager::getInstance().getCachedResponse(url, response())) {
                if (m_addedCacheValidationHeaders) {
                    response().setHTTPStatusCode(200);
                    response().setHTTPStatusText("OK");
                }
            }
        }
        CurlCacheManager::getInstance().didReceiveResponse(*m_handle, response());
        m_handle->client()->didReceiveResponse(m_handle, ResourceResponse(response()));
    }
}

void ResourceHandleCurlDelegate::didReceiveContentData(Ref<SharedBuffer>&& buffer)
{
    ASSERT(isMainThread());

    if (!response().responseFired())
        handleLocalReceiveResponse();

    if (m_multipartHandle)
        m_multipartHandle->contentReceived(buffer->data(), buffer->size());
    else if (m_handle->client()) {
        CurlCacheManager::getInstance().didReceiveData(*m_handle, buffer->data(), buffer->size());
        m_handle->client()->didReceiveBuffer(m_handle, WTFMove(buffer), buffer->size());
    }
}

void ResourceHandleCurlDelegate::handleLocalReceiveResponse()
{
    ASSERT(isMainThread());

    // since the code in headerCallback will not have run for local files
    // the code to set the URL and fire didReceiveResponse is never run,
    // which means the ResourceLoader's response does not contain the URL.
    // Run the code here for local files to resolve the issue.
    // TODO: See if there is a better approach for handling this.
    response().setURL(m_curlHandle.getEffectiveURL());
    response().setResponseFired(true);
    if (m_handle->client())
        m_handle->client()->didReceiveResponse(m_handle, ResourceResponse(response()));
}

void ResourceHandleCurlDelegate::prepareSendData(char* buffer, size_t blockSize, size_t numberOfBlocks)
{
    ASSERT(isMainThread());
    ASSERT(!m_sendBytes);

    std::unique_lock<Lock> lock(m_workerThreadMutex);

    if (!m_formDataStream || !m_formDataStream->hasMoreElements()) {
        m_workerThreadConditionVariable.notifyOne();
        return;
    }

    size_t size = m_formDataStream->read(buffer, blockSize, numberOfBlocks);
    if (!size) {
        // Something went wrong so cancel the job.
        m_handle->cancel();
        m_workerThreadConditionVariable.notifyOne();
        return;
    }

    m_sendBytes = size;
    m_workerThreadConditionVariable.notifyOne();
}

void ResourceHandleCurlDelegate::didFinish(NetworkLoadMetrics networkLoadMetrics)
{
    response().setDeprecatedNetworkLoadMetrics(networkLoadMetrics);

    if (!m_handle)
        return;

    if (!response().responseFired()) {
        handleLocalReceiveResponse();
        if (!m_handle)
            return;
    }

    if (m_multipartHandle)
        m_multipartHandle->contentEnded();

    if (m_handle->client()) {
        CurlCacheManager::getInstance().didFinishLoading(*m_handle);
        m_handle->client()->didFinishLoading(m_handle);
    }
}

void ResourceHandleCurlDelegate::didFail(const ResourceError& resourceError)
{
    if (!m_handle)
        return;

    if (m_handle->client()) {
        CurlCacheManager::getInstance().didFail(*m_handle);
        m_handle->client()->didFail(m_handle, resourceError);
    }
}

void ResourceHandleCurlDelegate::handleDataURL()
{
    ASSERT(m_firstRequest.url().protocolIsData());
    String url = m_firstRequest.url().string();

    ASSERT(m_handle->client());

    int index = url.find(',');
    if (index == -1) {
        m_handle->client()->cannotShowURL(m_handle);
        return;
    }

    String mediaType = url.substring(5, index - 5);
    String data = url.substring(index + 1);
    auto originalSize = data.length();

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
        m_handle->client()->didReceiveResponse(m_handle, WTFMove(response));

        // didReceiveResponse might cause the client to be deleted.
        if (m_handle->client()) {
            Vector<char> out;
            if (base64Decode(data, out, Base64IgnoreSpacesAndNewLines) && out.size() > 0)
                m_handle->client()->didReceiveBuffer(m_handle, SharedBuffer::create(out.data(), out.size()), originalSize);
        }
    } else {
        TextEncoding encoding(charset);
        data = decodeURLEscapeSequences(data, encoding);
        m_handle->client()->didReceiveResponse(m_handle, WTFMove(response));

        // didReceiveResponse might cause the client to be deleted.
        if (m_handle->client()) {
            CString encodedData = encoding.encode(data, URLEncodedEntitiesForUnencodables);
            if (encodedData.length())
                m_handle->client()->didReceiveBuffer(m_handle, SharedBuffer::create(encodedData.data(), encodedData.length()), originalSize);
        }
    }

    if (m_handle->client())
        m_handle->client()->didFinishLoading(m_handle);
}

void ResourceHandleCurlDelegate::setupPOST()
{
    m_curlHandle.enableHttpPostRequest();

    size_t numElements = getFormElementsCount();
    if (!numElements)
        return;

    // Do not stream for simple POST data
    if (numElements == 1) {
        m_postBytes = m_firstRequest.httpBody()->flatten();
        if (m_postBytes.size())
            m_curlHandle.setPostFields(m_postBytes.data(), m_postBytes.size());
        return;
    }

    setupFormData(true);
}

void ResourceHandleCurlDelegate::setupPUT()
{
    m_curlHandle.enableHttpPutRequest();

    // Disable the Expect: 100 continue header
    m_curlHandle.removeRequestHeader("Expect");

    size_t numElements = getFormElementsCount();
    if (!numElements)
        return;

    setupFormData(false);
}

size_t ResourceHandleCurlDelegate::getFormElementsCount()
{
    RefPtr<FormData> formData = m_firstRequest.httpBody();
    if (!formData)
        return 0;

    // Resolve the blob elements so the formData can correctly report it's size.
    formData = formData->resolveBlobReferences();
    size_t size = formData->elements().size();
    m_firstRequest.setHTTPBody(WTFMove(formData));
    return size;
}

void ResourceHandleCurlDelegate::setupFormData(bool isPostRequest)
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

    m_formDataStream = std::make_unique<FormDataStream>();
    m_formDataStream->setHTTPBody(m_firstRequest.httpBody());

    m_curlHandle.setReadCallbackFunction(willSendDataCallback, this);
}

void ResourceHandleCurlDelegate::applyAuthentication()
{
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

NetworkLoadMetrics ResourceHandleCurlDelegate::getNetworkLoadMetrics()
{
    NetworkLoadMetrics networkLoadMetrics;
    if (auto metrics = m_curlHandle.getNetworkLoadMetrics())
        networkLoadMetrics = *metrics;

    return networkLoadMetrics;
}

CURLcode ResourceHandleCurlDelegate::willSetupSslCtx(void* sslCtx)
{
    m_sslVerifier.setCurlHandle(&m_curlHandle);
    m_sslVerifier.setHostName(m_firstRequest.url().host());
    m_sslVerifier.setSslCtx(sslCtx);

    return CURLE_OK;
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
size_t ResourceHandleCurlDelegate::didReceiveHeader(String&& header)
{
    if (!m_handle)
        return 0;

    if (m_defersLoading)
        return 0;

    /*
    * a) We can finish and send the ResourceResponse
    * b) We will add the current header to the HTTPHeaderMap of the ResourceResponse
    *
    * The HTTP standard requires to use \r\n but for compatibility it recommends to
    * accept also \n.
    */
    if (header == AtomicString("\r\n") || header == AtomicString("\n")) {
        long httpCode = 0;
        if (auto code = m_curlHandle.getResponseCode())
            httpCode = *code;

        if (!httpCode) {
            // Comes here when receiving 200 Connection Established. Just return.
            return header.length();
        }

        if (isHttpInfo(httpCode)) {
            // Just return when receiving http info, e.g. HTTP/1.1 100 Continue.
            // If not, the request might be cancelled, because the MIME type will be empty for this response.
            return header.length();
        }

        long long contentLength = 0;
        if (auto length = m_curlHandle.getContentLength())
            contentLength = *length;

        uint16_t connectPort = 0;
        if (auto port = m_curlHandle.getPrimaryPort())
            connectPort = *port;

        long availableAuth = CURLAUTH_NONE;
        if (auto auth = m_curlHandle.getHttpAuthAvail())
            availableAuth = *auth;

        if (isMainThread())
            didReceiveAllHeaders(httpCode, contentLength, connectPort, availableAuth);
        else {
            callOnMainThread([protectedThis = makeRef(*this), httpCode, contentLength, connectPort, availableAuth] {
                if (!protectedThis->m_handle)
                    return;
                protectedThis->didReceiveAllHeaders(httpCode, contentLength, connectPort, availableAuth);
            });
        }
    } else {
        // If the FOLLOWLOCATION option is enabled for the curl handle then
        // curl will follow the redirections internally. Thus this header callback
        // will be called more than one time with the line starting "HTTP" for one job.
        if (isMainThread())
            response().appendHTTPHeaderField(header);
        else {
            callOnMainThread([protectedThis = makeRef(*this), copyHeader = header.isolatedCopy() ] {
                if (!protectedThis->m_handle)
                    return;

                protectedThis->response().appendHTTPHeaderField(copyHeader);
            });
        }
    }

    return header.length();
}

// called with data after all headers have been processed via headerCallback
size_t ResourceHandleCurlDelegate::didReceiveData(Ref<SharedBuffer>&& buffer)
{
    if (!m_handle)
        return 0;

    if (m_defersLoading)
        return 0;

    size_t receiveBytes = buffer->size();

    // this shouldn't be necessary but apparently is. CURL writes the data
    // of html page even if it is a redirect that was handled internally
    // can be observed e.g. on gmail.com
    if (auto httpCode = m_curlHandle.getResponseCode()) {
        if (*httpCode >= 300 && *httpCode < 400)
            return receiveBytes;
    }

    if (receiveBytes) {
        if (isMainThread())
            didReceiveContentData(WTFMove(buffer));
        else {
            callOnMainThread([protectedThis = makeRef(*this), buf = WTFMove(buffer)]() mutable {
                if (!protectedThis->m_handle)
                    return;
                protectedThis->didReceiveContentData(WTFMove(buf));
            });
        }
    }

    return receiveBytes;
}

/* This is called to obtain HTTP POST or PUT data.
Iterate through FormData elements and upload files.
Carefully respect the given buffer blockSize and fill the rest of the data at the next calls.
*/
size_t ResourceHandleCurlDelegate::willSendData(char* buffer, size_t blockSize, size_t numberOfBlocks)
{
    ASSERT(!isMainThread());

    if (!m_handle)
        return 0;

    if (m_defersLoading)
        return 0;

    if (!blockSize || !numberOfBlocks)
        return 0;

    {
        std::unique_lock<Lock> lock(m_workerThreadMutex);

        m_sendBytes = 0;

        if (isMainThread())
            prepareSendData(buffer, blockSize, numberOfBlocks);
        else {
            callOnMainThread([protectedThis = makeRef(*this), buffer, blockSize, numberOfBlocks] {
                if (!protectedThis->m_handle)
                    return;
                protectedThis->prepareSendData(buffer, blockSize, numberOfBlocks);
            });

            m_workerThreadConditionVariable.wait(lock, [this] {
                return m_sendBytes;
            });
        }
    }

    return m_sendBytes;
}

CURLcode ResourceHandleCurlDelegate::willSetupSslCtxCallback(CURL*, void* sslCtx, void* userData)
{
    return static_cast<ResourceHandleCurlDelegate*>(userData)->willSetupSslCtx(sslCtx);
}

size_t ResourceHandleCurlDelegate::didReceiveHeaderCallback(char* ptr, size_t blockSize, size_t numberOfBlocks, void* data)
{
    return static_cast<ResourceHandleCurlDelegate*>(const_cast<void*>(data))->didReceiveHeader(String(static_cast<const char*>(ptr), blockSize * numberOfBlocks));
}

size_t ResourceHandleCurlDelegate::didReceiveDataCallback(char* ptr, size_t blockSize, size_t numberOfBlocks, void* data)
{
    return static_cast<ResourceHandleCurlDelegate*>(const_cast<void*>(data))->didReceiveData(SharedBuffer::create(ptr, blockSize * numberOfBlocks));
}

size_t ResourceHandleCurlDelegate::willSendDataCallback(char* ptr, size_t blockSize, size_t numberOfBlocks, void* data)
{
    return static_cast<ResourceHandleCurlDelegate*>(const_cast<void*>(data))->willSendData(ptr, blockSize, numberOfBlocks);
}

} // namespace WebCore

#endif
