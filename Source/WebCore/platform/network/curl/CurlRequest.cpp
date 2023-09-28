/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CurlRequest.h"

#if USE(CURL)

#include "CertificateInfo.h"
#include "CurlRequestClient.h"
#include "CurlRequestScheduler.h"
#include "MIMETypeRegistry.h"
#include "NetworkLoadMetrics.h"
#include "ResourceError.h"
#include "SharedBuffer.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/Language.h>
#include <wtf/MainThread.h>

namespace WebCore {

CurlRequest::CurlRequest(const ResourceRequest&request, CurlRequestClient* client, CaptureNetworkLoadMetrics captureExtraMetrics)
    : m_client(client)
    , m_request(request.isolatedCopy())
    , m_formDataStream(m_request.httpBody())
    , m_captureExtraMetrics(captureExtraMetrics == CaptureNetworkLoadMetrics::Extended)
{
    ASSERT(isMainThread());
}

CurlRequest::~CurlRequest() = default;

void CurlRequest::invalidateClient()
{
    ASSERT(isMainThread());

    m_client = nullptr;
}

void CurlRequest::setAuthenticationScheme(ProtectionSpace::AuthenticationScheme scheme)
{
    switch (scheme) {
    case ProtectionSpace::AuthenticationScheme::HTTPBasic:
        m_authType = CURLAUTH_BASIC;
        break;

    case ProtectionSpace::AuthenticationScheme::HTTPDigest:
        m_authType = CURLAUTH_DIGEST;
        break;

    case ProtectionSpace::AuthenticationScheme::NTLM:
        m_authType = CURLAUTH_NTLM;
        break;

    case ProtectionSpace::AuthenticationScheme::Negotiate:
        m_authType = CURLAUTH_NEGOTIATE;
        break;

    default:
        m_authType = CURLAUTH_ANY;
        break;
    }
}

void CurlRequest::setUserPass(const String& user, const String& password)
{
    ASSERT(isMainThread());

    m_user = user.isolatedCopy();
    m_password = password.isolatedCopy();
}

void CurlRequest::resume()
{
    // The pausing of transfer does not work with protocols, like file://.
    // Therefore, PAUSE can not be done in didReceiveData().
    // It means that the same logic as http:// can not be used.
    // In the file scheme, invokeDidReceiveResponse() is done first. 
    // Then StartWithJobManager is called with completeDidReceiveResponse and start transfer with libcurl.

    // http : didReceiveHeader => didReceiveData[PAUSE] => invokeDidReceiveResponse => (MainThread)curlDidReceiveResponse => completeDidReceiveResponse[RESUME] => didReceiveData
    // file : invokeDidReceiveResponseForFile => (MainThread)curlDidReceiveResponse => completeDidReceiveResponse => didReceiveData

    ASSERT(isMainThread());

    if (m_didStartTransfer)
        return;

    m_didStartTransfer = true;

    if (m_request.url().protocolIsFile())
        invokeDidReceiveResponseForFile(m_request.url());
    else
        startWithJobManager();
}

void CurlRequest::startWithJobManager()
{
    ASSERT(isMainThread());

    CurlContext::singleton().scheduler().add(this);
}

void CurlRequest::cancel()
{
    ASSERT(isMainThread());

    {
        Locker locker { m_statusMutex };
        if (m_cancelled)
            return;

        m_cancelled = true;
    }

    auto& scheduler = CurlContext::singleton().scheduler();

    if (needToInvokeDidCancelTransfer()) {
        runOnWorkerThreadIfRequired([this, protectedThis = Ref { *this }]() {
            didCancelTransfer();
        });
    } else if (m_didStartTransfer)
        scheduler.cancel(this);

    invalidateClient();
}

bool CurlRequest::isCancelled()
{
    Locker locker { m_statusMutex };
    return m_cancelled;
}

bool CurlRequest::isCompletedOrCancelled()
{
    Locker locker { m_statusMutex };
    return m_completed || m_cancelled;
}

/* `this` is protected inside this method. */
void CurlRequest::callClient(Function<void(CurlRequest&, CurlRequestClient&)>&& task)
{
    runOnMainThread([this, protectedThis = Ref { *this }, task = WTFMove(task)]() mutable {
        if (m_client)
            task(*this, Ref { *m_client });
    });
}

void CurlRequest::runOnMainThread(Function<void()>&& task)
{
    ensureOnMainThread(WTFMove(task));
}

void CurlRequest::runOnWorkerThreadIfRequired(Function<void()>&& task)
{
    if (isMainThread())
        CurlContext::singleton().scheduler().callOnWorkerThread(WTFMove(task));
    else
        task();
}

CURL* CurlRequest::setupTransfer()
{
    auto httpHeaderFields = m_request.httpHeaderFields();
    appendAcceptLanguageHeader(httpHeaderFields);

    m_curlHandle = makeUnique<CurlHandle>();

    m_curlHandle->setUrl(m_request.url());

    m_curlHandle->appendRequestHeaders(httpHeaderFields);

    const auto& method = m_request.httpMethod();
    if (method == "GET"_s)
        m_curlHandle->enableHttpGetRequest();
    else if (method == "POST"_s)
        setupPOST();
    else if (method == "PUT"_s)
        setupPUT();
    else if (method == "HEAD"_s)
        m_curlHandle->enableHttpHeadRequest();
    else {
        m_curlHandle->setHttpCustomRequest(method);
        setupPUT();
    }

    if (!m_user.isEmpty() || !m_password.isEmpty())
        m_curlHandle->setHttpAuthUserPass(m_user, m_password, m_authType);

    if (m_shouldDisableServerTrustEvaluation)
        m_curlHandle->disableServerTrustEvaluation();

    m_curlHandle->setHeaderCallbackFunction(didReceiveHeaderCallback, this);
    m_curlHandle->setWriteCallbackFunction(didReceiveDataCallback, this);

    if (m_captureExtraMetrics)
        m_curlHandle->setDebugCallbackFunction(didReceiveDebugInfoCallback, this);

    m_curlHandle->setTimeout(timeoutInterval());

    m_performStartTime = MonotonicTime::now();

    return m_curlHandle->handle();
}

Seconds CurlRequest::timeoutInterval() const
{
    // Request specific timeout interval.
    if (m_request.timeoutInterval())
        return Seconds { m_request.timeoutInterval() };

    // Default timeout interval set by application.
    if (m_request.defaultTimeoutInterval())
        return Seconds { m_request.defaultTimeoutInterval() };

    // Use platform default timeout interval.
    return CurlContext::singleton().defaultTimeoutInterval();
}

// This is called to obtain HTTP POST or PUT data.
// Iterate through FormData elements and upload files.
// Carefully respect the given buffer size and fill the rest of the data at the next calls.

size_t CurlRequest::willSendData(char* buffer, size_t blockSize, size_t numberOfBlocks)
{
    if (isCompletedOrCancelled())
        return CURL_READFUNC_ABORT;

    if (!blockSize || !numberOfBlocks)
        return CURL_READFUNC_ABORT;

    // Check for overflow.
    if (blockSize > (std::numeric_limits<size_t>::max() / numberOfBlocks))
        return CURL_READFUNC_ABORT;

    size_t bufferSize = blockSize * numberOfBlocks;
    auto sendBytes = m_formDataStream.read(buffer, bufferSize);
    if (!sendBytes) {
        // Something went wrong so error the job.
        return CURL_READFUNC_ABORT;
    }

    callClient([totalReadSize = m_formDataStream.totalReadSize(), totalSize = m_formDataStream.totalSize()](CurlRequest& request, CurlRequestClient& client) {
        client.curlDidSendData(request, totalReadSize, totalSize);
    });

    return *sendBytes;
}

// This is being called for each HTTP header in the response. This includes '\r\n'
// for the last line of the header.

size_t CurlRequest::didReceiveHeader(String&& header)
{
    static constexpr auto emptyLineCRLF = "\r\n"_s;
    static constexpr auto emptyLineLF = "\n"_s;

    if (isCompletedOrCancelled())
        return CURL_WRITEFUNC_ERROR;

    // libcurl sends all headers that libcurl received to application.
    // So, in digest authentication, a block of response headers are received twice consecutively from libcurl.
    // For example, when authentication succeeds, the first block is "401 Authorization", and the second block is "200 OK".
    // Also, "100 Continue" and "200 Connection Established" do the same behavior.
    // In this process, deletes the first block to send a correct headers to WebCore.
    if (m_didReceiveResponse) {
        m_didReceiveResponse = false;
        m_response = CurlResponse { };
        m_multipartHandle = nullptr;
    }

    auto receiveBytes = static_cast<size_t>(header.length());

    // The HTTP standard requires to use \r\n but for compatibility it recommends to accept also \n.
    if ((header != emptyLineCRLF) && (header != emptyLineLF)) {
        m_response.headers.append(WTFMove(header));
        return receiveBytes;
    }

    long statusCode = 0;
    if (auto code = m_curlHandle->getResponseCode())
        statusCode = *code;

    long httpConnectCode = 0;
    if (auto code = m_curlHandle->getHttpConnectCode())
        httpConnectCode = *code;

    m_didReceiveResponse = true;

    m_response.url = m_request.url();
    m_response.statusCode = statusCode;
    m_response.httpConnectCode = httpConnectCode;

    if (auto length = m_curlHandle->getContentLength())
        m_response.expectedContentLength = *length;

    if (auto proxyURL = m_curlHandle->getProxyUrl())
        m_response.proxyUrl = URL { *proxyURL };

    if (auto auth = m_curlHandle->getHttpAuthAvail())
        m_response.availableHttpAuth = *auth;

    if (auto auth = m_curlHandle->getProxyAuthAvail())
        m_response.availableProxyAuth = *auth;

    if (auto version = m_curlHandle->getHttpVersion())
        m_response.httpVersion = *version;

    if (m_response.availableProxyAuth)
        CurlContext::singleton().setProxyAuthMethod(m_response.availableProxyAuth);

    if (auto info = m_curlHandle->certificateInfo())
        m_response.certificateInfo = WTFMove(*info);

    m_response.networkLoadMetrics = networkLoadMetrics();

    m_multipartHandle = CurlMultipartHandle::createIfNeeded(*this, m_response);

    // Response will send at didReceiveData() or didCompleteTransfer()
    // to receive continueDidRceiveResponse() for asynchronously.

    return receiveBytes;
}

// called with data after all headers have been processed via headerCallback

size_t CurlRequest::didReceiveData(const SharedBuffer& buffer)
{
    if (isCompletedOrCancelled())
        return CURL_WRITEFUNC_ERROR;

    if (needToInvokeDidReceiveResponse()) {
        // Pause until completeDidReceiveResponse() is called.
        invokeDidReceiveResponse(m_response, [this] {
            runOnWorkerThreadIfRequired([this, protectedThis = Ref { *this }]() {
                if (isCompletedOrCancelled() || !m_curlHandle)
                    return;

                // Unpause a connection
                m_curlHandle->pause(CURLPAUSE_CONT);
            });
        });

        return CURL_WRITEFUNC_PAUSE;
    }

    auto receiveBytes = buffer.size();
    m_totalReceivedSize += receiveBytes;

    if (receiveBytes) {
        if (m_multipartHandle) {
            m_multipartHandle->didReceiveMessage(buffer);
            if (m_multipartHandle->hasError())
                return CURL_WRITEFUNC_ERROR;
        } else {
            callClient([buffer = Ref { buffer }](CurlRequest& request, CurlRequestClient& client) {
                client.curlDidReceiveData(request, buffer);
            });
        }
    }

    return receiveBytes;
}

void CurlRequest::didReceiveHeaderFromMultipart(Vector<String>&& headers)
{
    if (isCompletedOrCancelled())
        return;

    CurlResponse response = m_response.isolatedCopy();
    response.expectedContentLength = 0;
    response.headers.clear();

    for (auto& header : headers)
        response.headers.append(WTFMove(header));

    invokeDidReceiveResponse(response, [this] {
        runOnWorkerThreadIfRequired([this, protectedThis = Ref { *this }]() {
            if (isCompletedOrCancelled() || !m_multipartHandle)
                return;

            m_multipartHandle->completeHeaderProcessing();
        });
    });
}

void CurlRequest::didReceiveDataFromMultipart(Ref<SharedBuffer>&& buffer)
{
    if (isCompletedOrCancelled())
        return;

    auto receiveBytes = buffer->size();

    if (receiveBytes) {
        callClient([buffer = WTFMove(buffer)](CurlRequest& request, CurlRequestClient& client) {
            client.curlDidReceiveData(request, buffer.get());
        });
    }
}

void CurlRequest::didCompleteFromMultipart()
{
    ASSERT(m_multipartHandle && m_multipartHandle->completed());

    runOnWorkerThreadIfRequired([this, protectedThis = Ref { *this }]() {
        didCompleteTransfer(CURLE_OK);
    });
}

void CurlRequest::didCompleteTransfer(CURLcode result)
{
    if (isCancelled()) {
        didCancelTransfer();
        return;
    }

    bool isProxyAuthenticationRequired = result == CURLE_RECV_ERROR && m_response.httpConnectCode == 407;
    if (needToInvokeDidReceiveResponse() && (result == CURLE_OK || isProxyAuthenticationRequired)) {
        // Processing of didReceiveResponse() has not been completed. (For example, HEAD Method, Proxy authentication, etc.)
        m_mustInvokeCancelTransfer = true;
        invokeDidReceiveResponse(m_response, [this, result]() mutable {
            m_mustInvokeCancelTransfer = false;
            runOnWorkerThreadIfRequired([this, protectedThis = Ref { *this }, result]() {
                didCompleteTransfer(result);
            });
        });
        return;
    }

    if (result == CURLE_OK) {
        if (m_multipartHandle && !m_multipartHandle->completed()) {
            m_multipartHandle->didCompleteMessage();
            return;
        }

        auto metrics = networkLoadMetrics();

        finalizeTransfer();
        callClient([networkLoadMetrics = WTFMove(metrics)](CurlRequest& request, CurlRequestClient& client) mutable {
            networkLoadMetrics.responseEnd = MonotonicTime::now();
            networkLoadMetrics.markComplete();

            client.curlDidComplete(request, WTFMove(networkLoadMetrics));
        });
    } else {
        auto type = (result == CURLE_OPERATION_TIMEDOUT && timeoutInterval()) ? ResourceError::Type::Timeout : ResourceError::Type::General;
        auto resourceError = ResourceError(result, m_request.url(), type);

        CertificateInfo certificateInfo;
        if (auto info = m_curlHandle->certificateInfo())
            certificateInfo = WTFMove(*info);

        finalizeTransfer();
        callClient([error = WTFMove(resourceError), certificateInfo = WTFMove(certificateInfo)](CurlRequest& request, CurlRequestClient& client) mutable {
            client.curlDidFailWithError(request, WTFMove(error), WTFMove(certificateInfo));
        });
    }

    {
        Locker locker { m_statusMutex };
        m_completed = true;
    }
}

void CurlRequest::didCancelTransfer()
{
    finalizeTransfer();
}

void CurlRequest::finalizeTransfer()
{
    m_formDataStream.clean();
    m_multipartHandle = nullptr;
    m_curlHandle = nullptr;
}

int CurlRequest::didReceiveDebugInfo(curl_infotype type, char* data, size_t size)
{
    if (!data)
        return 0;

    if (type == CURLINFO_HEADER_OUT) {
        String requestHeader(data, size);
        auto headerFields = requestHeader.split("\r\n"_s);
        // Remove the request line
        if (headerFields.size())
            headerFields.remove(0);

        for (auto& header : headerFields) {
            auto pos = header.find(':');
            if (pos != notFound) {
                auto key = header.left(pos).trim(deprecatedIsSpaceOrNewline);
                auto value = header.substring(pos + 1).trim(deprecatedIsSpaceOrNewline);
                m_requestHeaders.add(key, value);
            }
        }
    }

    return 0;
}

void CurlRequest::appendAcceptLanguageHeader(HTTPHeaderMap& header)
{
    for (const auto& language : userPreferredLanguages())
        header.add(HTTPHeaderName::AcceptLanguage, language);
}

void CurlRequest::setupPUT()
{
    curl_off_t totalSize = m_formDataStream.totalSize();

    m_curlHandle->enableHttpPutRequest(totalSize);
    m_curlHandle->setReadCallbackFunction(willSendDataCallback, this);

    // Disable a "Expect: 100-continue" header
    m_curlHandle->removeRequestHeader("Expect"_s);
}

void CurlRequest::setupPOST()
{
    curl_off_t totalSize = m_formDataStream.totalSize();

    m_curlHandle->enableHttpPostRequest(totalSize);
    m_curlHandle->setReadCallbackFunction(willSendDataCallback, this);

    // Override the default POST Content-Type: header
    if (!m_request.hasHTTPHeader(HTTPHeaderName::ContentType) && !totalSize)
        m_curlHandle->removeRequestHeader("Content-Type"_s);

    // Disable a "Expect: 100-continue" header
    m_curlHandle->removeRequestHeader("Expect"_s);
}

void CurlRequest::invokeDidReceiveResponseForFile(const URL& url)
{
    // Since the code in didReceiveHeader() will not have run for local files
    // the code to set the URL and fire didReceiveResponse is never run,
    // which means the ResourceLoader's response does not contain the URL.
    // Run the code here for local files to resolve the issue.

    ASSERT(isMainThread());
    ASSERT(url.protocolIsFile());

    // Determine the MIME type based on the path.
    auto mimeType = MIMETypeRegistry::mimeTypeForPath(url.path().toString());

    // DidReceiveResponse must not be called immediately
    runOnWorkerThreadIfRequired([this, protectedThis = Ref { *this }, url = crossThreadCopy(url), mimeType = crossThreadCopy(WTFMove(mimeType))]() mutable {
        CurlResponse response;
        response.url = WTFMove(url);
        response.statusCode = 200;
        response.headers.append("Content-Type: " + mimeType);

        invokeDidReceiveResponse(response, [this] {
            startWithJobManager();
        });
    });
}

void CurlRequest::invokeDidReceiveResponse(const CurlResponse& response, Function<void()>&& completionHandler)
{
    ASSERT(!m_responseCompletionHandler);
    ASSERT(!m_didNotifyResponse || m_multipartHandle);

    m_didNotifyResponse = true;
    m_responseCompletionHandler = WTFMove(completionHandler);

    // FIXME: Replace this isolatedCopy with WTFMove.
    callClient([response = response.isolatedCopy()](CurlRequest& request, CurlRequestClient& client) mutable {
        client.curlDidReceiveResponse(request, WTFMove(response));
    });
}

void CurlRequest::completeDidReceiveResponse()
{
    ASSERT(isMainThread());
    ASSERT(m_didNotifyResponse);
    ASSERT(!m_didReturnFromNotify || m_multipartHandle);

    if (isCompletedOrCancelled())
        return;

    m_didReturnFromNotify = true;

    if (auto responseCompletionHandler = WTFMove(m_responseCompletionHandler))
        responseCompletionHandler();
}

NetworkLoadMetrics CurlRequest::networkLoadMetrics()
{
    ASSERT(m_curlHandle);

    auto networkLoadMetrics = m_curlHandle->getNetworkLoadMetrics(m_performStartTime);
    if (!networkLoadMetrics)
        return NetworkLoadMetrics();

    networkLoadMetrics->responseBodyDecodedSize = m_totalReceivedSize;

    if (m_captureExtraMetrics) {
        m_curlHandle->addExtraNetworkLoadMetrics(*networkLoadMetrics);
        if (auto* additionalMetrics = networkLoadMetrics->additionalNetworkLoadMetricsForWebInspector.get())
            additionalMetrics->requestHeaders = m_requestHeaders;
    }

    return WTFMove(*networkLoadMetrics);
}

size_t CurlRequest::willSendDataCallback(char* ptr, size_t blockSize, size_t numberOfBlocks, void* userData)
{
    return static_cast<CurlRequest*>(userData)->willSendData(ptr, blockSize, numberOfBlocks);
}

size_t CurlRequest::didReceiveHeaderCallback(char* ptr, size_t blockSize, size_t numberOfBlocks, void* userData)
{
    return static_cast<CurlRequest*>(userData)->didReceiveHeader(String(ptr, blockSize * numberOfBlocks));
}

size_t CurlRequest::didReceiveDataCallback(char* ptr, size_t blockSize, size_t numberOfBlocks, void* userData)
{
    return static_cast<CurlRequest*>(userData)->didReceiveData(SharedBuffer::create(ptr, blockSize * numberOfBlocks));
}

int CurlRequest::didReceiveDebugInfoCallback(CURL*, curl_infotype type, char* data, size_t size, void* userData)
{
    return static_cast<CurlRequest*>(userData)->didReceiveDebugInfo(type, data, size);
}

}

#endif
