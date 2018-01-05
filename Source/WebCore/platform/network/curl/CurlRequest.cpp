/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "CurlRequestClient.h"
#include "CurlRequestScheduler.h"
#include "MIMETypeRegistry.h"
#include "ResourceError.h"
#include "SharedBuffer.h"
#include <wtf/MainThread.h>

namespace WebCore {

CurlRequest::CurlRequest(const ResourceRequest&request, CurlRequestClient* client, bool shouldSuspend)
    : m_request(request.isolatedCopy())
    , m_shouldSuspend(shouldSuspend)
{
    ASSERT(isMainThread());

    setClient(client);
    resolveBlobReferences(m_request);
}

void CurlRequest::setUserPass(const String& user, const String& password)
{
    ASSERT(isMainThread());

    m_user = user.isolatedCopy();
    m_password = password.isolatedCopy();
}

void CurlRequest::start(bool isSyncRequest)
{
    // The pausing of transfer does not work with protocols, like file://.
    // Therefore, PAUSE can not be done in didReceiveData().
    // It means that the same logic as http:// can not be used.
    // In the file scheme, invokeDidReceiveResponse() is done first. 
    // Then StartWithJobManager is called with completeDidReceiveResponse and start transfer with libcurl.

    // http : didReceiveHeader => didReceiveData[PAUSE] => invokeDidReceiveResponse => (MainThread)curlDidReceiveResponse => completeDidReceiveResponse[RESUME] => didReceiveData
    // file : invokeDidReceiveResponseForFile => (MainThread)curlDidReceiveResponse => completeDidReceiveResponse => didReceiveData

    ASSERT(isMainThread());

    m_isSyncRequest = isSyncRequest;

    auto url = m_request.url().isolatedCopy();

    if (!m_isSyncRequest) {
        // For asynchronous, use CurlRequestScheduler. Curl processes runs on sub thread.
        if (url.isLocalFile())
            invokeDidReceiveResponseForFile(url);
        else
            startWithJobManager();
    } else {
        // For synchronous, does not use CurlRequestScheduler. Curl processes runs on main thread.
        // curl_easy_perform blocks until the transfer is finished.
        retain();
        if (url.isLocalFile())
            invokeDidReceiveResponseForFile(url);

        setupTransfer();
        CURLcode resultCode = m_curlHandle->perform();
        didCompleteTransfer(resultCode);
        release();
    }
}

void CurlRequest::startWithJobManager()
{
    ASSERT(isMainThread());

    CurlRequestScheduler::singleton().add(this);
}

void CurlRequest::cancel()
{
    ASSERT(isMainThread());

    if (isCompletedOrCancelled())
        return;

    m_cancelled = true;

    if (!m_isSyncRequest) {
        auto& scheduler = CurlRequestScheduler::singleton();

        if (needToInvokeDidCancelTransfer()) {
            scheduler.callOnWorkerThread([protectedThis = makeRef(*this)]() {
                protectedThis->didCancelTransfer();
            });
        } else
            scheduler.cancel(this);
    } else {
        if (needToInvokeDidCancelTransfer())
            didCancelTransfer();
    }

    setRequestPaused(false);
    setCallbackPaused(false);
}

void CurlRequest::suspend()
{
    ASSERT(isMainThread());

    setRequestPaused(true);
}

void CurlRequest::resume()
{
    ASSERT(isMainThread());

    setRequestPaused(false);
}

/* `this` is protected inside this method. */
void CurlRequest::callClient(WTF::Function<void(CurlRequestClient*)> task)
{
    if (isMainThread()) {
        if (CurlRequestClient* client = m_client)
            task(client);
    } else {
        callOnMainThread([protectedThis = makeRef(*this), task = WTFMove(task)]() mutable {
            if (CurlRequestClient* client = protectedThis->m_client)
                task(client);
        });
    }
}

CURL* CurlRequest::setupTransfer()
{
    auto& sslHandle = CurlContext::singleton().sslHandle();

    m_curlHandle = std::make_unique<CurlHandle>();

    m_curlHandle->initialize();
    m_curlHandle->setUrl(m_request.url());
    m_curlHandle->appendRequestHeaders(m_request.httpHeaderFields());

    const auto& method = m_request.httpMethod();
    if (method == "GET")
        m_curlHandle->enableHttpGetRequest();
    else if (method == "POST")
        setupPOST(m_request);
    else if (method == "PUT")
        setupPUT(m_request);
    else if (method == "HEAD")
        m_curlHandle->enableHttpHeadRequest();
    else {
        m_curlHandle->setHttpCustomRequest(method);
        setupPUT(m_request);
    }

    if (!m_user.isEmpty() || !m_password.isEmpty()) {
        m_curlHandle->enableHttpAuthentication(CURLAUTH_ANY);
        m_curlHandle->setHttpAuthUserPass(m_user, m_password);
    }

    m_curlHandle->setHeaderCallbackFunction(didReceiveHeaderCallback, this);
    m_curlHandle->setWriteCallbackFunction(didReceiveDataCallback, this);

    m_curlHandle->enableShareHandle();
    m_curlHandle->enableAllowedProtocols();
    m_curlHandle->enableAcceptEncoding();
    m_curlHandle->enableTimeout();

    m_curlHandle->enableProxyIfExists();
    m_curlHandle->enableCookieJarIfExists();

    m_curlHandle->setSslVerifyPeer(CurlHandle::VerifyPeer::Enable);
    m_curlHandle->setSslVerifyHost(CurlHandle::VerifyHost::StrictNameCheck);

    auto sslClientCertificate = sslHandle.getSSLClientCertificate(m_request.url().host());
    if (sslClientCertificate) {
        m_curlHandle->setSslCert(sslClientCertificate->first.utf8().data());
        m_curlHandle->setSslCertType("P12");
        m_curlHandle->setSslKeyPassword(sslClientCertificate->second.utf8().data());
    }

    if (sslHandle.shouldIgnoreSSLErrors())
        m_curlHandle->setSslVerifyPeer(CurlHandle::VerifyPeer::Disable);
    else
        m_curlHandle->setSslCtxCallbackFunction(willSetupSslCtxCallback, this);

    m_curlHandle->setCACertPath(sslHandle.getCACertPath());

    if (m_shouldSuspend)
        suspend();

#ifndef NDEBUG
    m_curlHandle->enableVerboseIfUsed();
    m_curlHandle->enableStdErrIfUsed();
#endif

    return m_curlHandle->handle();
}

CURLcode CurlRequest::willSetupSslCtx(void* sslCtx)
{
    m_sslVerifier.setCurlHandle(m_curlHandle.get());
    m_sslVerifier.setHostName(m_request.url().host());
    m_sslVerifier.setSslCtx(sslCtx);

    return CURLE_OK;
}

// This is called to obtain HTTP POST or PUT data.
// Iterate through FormData elements and upload files.
// Carefully respect the given buffer size and fill the rest of the data at the next calls.

size_t CurlRequest::willSendData(char* ptr, size_t blockSize, size_t numberOfBlocks)
{
    if (isCompletedOrCancelled())
        return CURL_READFUNC_ABORT;

    if (!blockSize || !numberOfBlocks)
        return 0;

    if (!m_formDataStream || !m_formDataStream->hasMoreElements())
        return 0;

    auto sendBytes = m_formDataStream->read(ptr, blockSize, numberOfBlocks);
    if (!sendBytes) {
        // Something went wrong so error the job.
        return CURL_READFUNC_ABORT;
    }

    return sendBytes;
}

// This is being called for each HTTP header in the response. This includes '\r\n'
// for the last line of the header.

size_t CurlRequest::didReceiveHeader(String&& header)
{
    static const auto emptyLineCRLF = "\r\n";
    static const auto emptyLineLF = "\n";

    if (isCompletedOrCancelled())
        return 0;

    // libcurl sends all headers that libcurl received to application.
    // So, in digest authentication, a block of response headers are received twice consecutively from libcurl.
    // For example, when authentication succeeds, the first block is "401 Authorization", and the second block is "200 OK".
    // Also, "100 Continue" and "200 Connection Established" do the same behavior.
    // In this process, deletes the first block to send a correct headers to WebCore.
    if (m_didReceiveResponse) {
        m_didReceiveResponse = false;
        m_response = CurlResponse { };
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

    if (auto length = m_curlHandle->getContentLength())
        m_response.expectedContentLength = *length;

    if (auto port = m_curlHandle->getPrimaryPort())
        m_response.connectPort = *port;

    if (auto auth = m_curlHandle->getHttpAuthAvail())
        m_response.availableHttpAuth = *auth;

    if (auto metrics = m_curlHandle->getNetworkLoadMetrics())
        m_networkLoadMetrics = *metrics;

    // Response will send at didReceiveData() or didCompleteTransfer()
    // to receive continueDidRceiveResponse() for asynchronously.

    return receiveBytes;
}

// called with data after all headers have been processed via headerCallback

size_t CurlRequest::didReceiveData(Ref<SharedBuffer>&& buffer)
{
    if (isCompletedOrCancelled())
        return 0;

    if (needToInvokeDidReceiveResponse()) {
        if (!m_isSyncRequest) {
            // For asynchronous, pause until completeDidReceiveResponse() is called.
            setCallbackPaused(true);
            invokeDidReceiveResponse(Action::ReceiveData);
            return CURL_WRITEFUNC_PAUSE;
        }

        // For synchronous, completeDidReceiveResponse() is called in invokeDidReceiveResponse().
        // In this case, pause is unnecessary.
        invokeDidReceiveResponse(Action::None);
    }

    auto receiveBytes = buffer->size();

    writeDataToDownloadFileIfEnabled(buffer);

    if (receiveBytes) {
        callClient([this, buffer = WTFMove(buffer)](CurlRequestClient* client) mutable {
            if (client)
                client->curlDidReceiveBuffer(WTFMove(buffer));
        });
    }

    return receiveBytes;
}

void CurlRequest::didCompleteTransfer(CURLcode result)
{
    if (m_cancelled) {
        m_curlHandle = nullptr;
        return;
    }

    if (result == CURLE_OK) {
        if (needToInvokeDidReceiveResponse()) {
            // Processing of didReceiveResponse() has not been completed. (For example, HEAD method)
            // When completeDidReceiveResponse() is called, didCompleteTransfer() will be called again.

            m_finishedResultCode = result;
            invokeDidReceiveResponse(Action::FinishTransfer);
        } else {
            if (auto metrics = m_curlHandle->getNetworkLoadMetrics())
                m_networkLoadMetrics = *metrics;

            finalizeTransfer();
            callClient([this](CurlRequestClient* client) {
                if (client)
                    client->curlDidComplete();
            });
        }
    } else {
        auto resourceError = ResourceError::httpError(result, m_request.url());
        if (m_sslVerifier.sslErrors())
            resourceError.setSslErrors(m_sslVerifier.sslErrors());

        finalizeTransfer();
        callClient([this, error = resourceError.isolatedCopy()](CurlRequestClient* client) {
            if (client)
                client->curlDidFailWithError(error);
        });
    }
}

void CurlRequest::didCancelTransfer()
{
    finalizeTransfer();
    cleanupDownloadFile();
}

void CurlRequest::finalizeTransfer()
{
    m_formDataStream = nullptr;
    closeDownloadFile();
    m_curlHandle = nullptr;
}

void CurlRequest::resolveBlobReferences(ResourceRequest& request)
{
    ASSERT(isMainThread());

    auto body = request.httpBody();
    if (!body || body->isEmpty())
        return;

    // Resolve the blob elements so the formData can correctly report it's size.
    RefPtr<FormData> formData = body->resolveBlobReferences();
    request.setHTTPBody(WTFMove(formData));
}

void CurlRequest::setupPUT(ResourceRequest& request)
{
    m_curlHandle->enableHttpPutRequest();

    // Disable the Expect: 100 continue header
    m_curlHandle->removeRequestHeader("Expect");

    auto body = request.httpBody();
    if (!body || body->isEmpty())
        return;

    setupFormData(request, false);
}

void CurlRequest::setupPOST(ResourceRequest& request)
{
    m_curlHandle->enableHttpPostRequest();

    auto body = request.httpBody();
    if (!body || body->isEmpty())
        return;

    auto numElements = body->elements().size();
    if (!numElements)
        return;

    // Do not stream for simple POST data
    if (numElements == 1) {
        m_postBuffer = body->flatten();
        if (m_postBuffer.size())
            m_curlHandle->setPostFields(m_postBuffer.data(), m_postBuffer.size());
    } else
        setupFormData(request, true);
}

void CurlRequest::setupFormData(ResourceRequest& request, bool isPostRequest)
{
    static auto maxCurlOffT = CurlHandle::maxCurlOffT();

    // Obtain the total size of the form data
    curl_off_t size = 0;
    bool chunkedTransfer = false;
    auto elements = request.httpBody()->elements();

    for (auto element : elements) {
        if (element.m_type == FormDataElement::Type::EncodedFile) {
            long long fileSizeResult;
            if (FileSystem::getFileSize(element.m_filename, fileSizeResult)) {
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
            size += element.m_data.size();
    }

    // cURL guesses that we want chunked encoding as long as we specify the header
    if (chunkedTransfer)
        m_curlHandle->appendRequestHeader("Transfer-Encoding: chunked");
    else {
        if (isPostRequest)
            m_curlHandle->setPostFieldLarge(size);
        else
            m_curlHandle->setInFileSizeLarge(size);
    }

    m_formDataStream = std::make_unique<FormDataStream>();
    m_formDataStream->setHTTPBody(request.httpBody());

    m_curlHandle->setReadCallbackFunction(willSendDataCallback, this);
}

void CurlRequest::invokeDidReceiveResponseForFile(URL& url)
{
    // Since the code in didReceiveHeader() will not have run for local files
    // the code to set the URL and fire didReceiveResponse is never run,
    // which means the ResourceLoader's response does not contain the URL.
    // Run the code here for local files to resolve the issue.

    ASSERT(isMainThread());
    ASSERT(url.isLocalFile());

    m_response.url = url;
    m_response.statusCode = 200;

    // Determine the MIME type based on the path.
    m_response.headers.append(String("Content-Type: " + MIMETypeRegistry::getMIMETypeForPath(m_response.url.path())));

    if (!m_isSyncRequest) {
        // DidReceiveResponse must not be called immediately
        CurlRequestScheduler::singleton().callOnWorkerThread([protectedThis = makeRef(*this)]() {
            protectedThis->invokeDidReceiveResponse(Action::StartTransfer);
        });
    } else {
        // For synchronous, completeDidReceiveResponse() is called in platformContinueSynchronousDidReceiveResponse().
        invokeDidReceiveResponse(Action::None);
    }
}

void CurlRequest::invokeDidReceiveResponse(Action behaviorAfterInvoke)
{
    ASSERT(!m_didNotifyResponse);

    m_didNotifyResponse = true;
    m_actionAfterInvoke = behaviorAfterInvoke;

    callClient([this, response = m_response.isolatedCopy()](CurlRequestClient* client) {
        if (client)
            client->curlDidReceiveResponse(response);
    });
}

void CurlRequest::completeDidReceiveResponse()
{
    ASSERT(isMainThread());
    ASSERT(m_didNotifyResponse);
    ASSERT(!m_didReturnFromNotify);

    if (isCancelled())
        return;

    if (m_actionAfterInvoke != Action::StartTransfer && isCompleted())
        return;

    m_didReturnFromNotify = true;

    if (m_actionAfterInvoke == Action::ReceiveData) {
        // Resume transfer
        setCallbackPaused(false);
    } else if (m_actionAfterInvoke == Action::StartTransfer) {
        // Start transfer for file scheme
        startWithJobManager();
    } else if (m_actionAfterInvoke == Action::FinishTransfer) {
        if (!m_isSyncRequest) {
            CurlRequestScheduler::singleton().callOnWorkerThread([protectedThis = makeRef(*this), finishedResultCode = m_finishedResultCode]() {
                protectedThis->didCompleteTransfer(finishedResultCode);
            });
        } else
            didCompleteTransfer(m_finishedResultCode);
    }
}

void CurlRequest::setRequestPaused(bool paused)
{
    auto wasPaused = isPaused();

    m_isPausedOfRequest = paused;

    if (isPaused() == wasPaused)
        return;

    pausedStatusChanged();
}

void CurlRequest::setCallbackPaused(bool paused)
{
    auto wasPaused = isPaused();

    m_isPausedOfCallback = paused;

    if (isPaused() == wasPaused)
        return;

    // In this case, PAUSE will be executed within didReceiveData(). Change pause state and return.
    if (paused)
        return;

    pausedStatusChanged();
}

void CurlRequest::pausedStatusChanged()
{
    if (isCompletedOrCancelled())
        return;

    if (!m_isSyncRequest && isMainThread()) {
        CurlRequestScheduler::singleton().callOnWorkerThread([protectedThis = makeRef(*this), paused = isPaused()]() {
            if (protectedThis->isCompletedOrCancelled())
                return;

            auto error = protectedThis->m_curlHandle->pause(paused ? CURLPAUSE_ALL : CURLPAUSE_CONT);
            if ((error != CURLE_OK) && !paused) {
                // Restarting the handle has failed so just cancel it.
                callOnMainThread([protectedThis = makeRef(protectedThis.get())]() {
                    protectedThis->cancel();
                });
            }
        });
    } else {
        auto error = m_curlHandle->pause(isPaused() ? CURLPAUSE_ALL : CURLPAUSE_CONT);
        if ((error != CURLE_OK) && !isPaused())
            cancel();
    }
}

void CurlRequest::enableDownloadToFile()
{
    LockHolder locker(m_downloadMutex);
    m_isEnabledDownloadToFile = true;
}

const String& CurlRequest::getDownloadedFilePath()
{
    LockHolder locker(m_downloadMutex);
    return m_downloadFilePath;
}

void CurlRequest::writeDataToDownloadFileIfEnabled(const SharedBuffer& buffer)
{
    {
        LockHolder locker(m_downloadMutex);

        if (!m_isEnabledDownloadToFile)
            return;

        if (m_downloadFilePath.isEmpty())
            m_downloadFilePath = FileSystem::openTemporaryFile("download", m_downloadFileHandle);
    }

    if (m_downloadFileHandle != FileSystem::invalidPlatformFileHandle)
        FileSystem::writeToFile(m_downloadFileHandle, buffer.data(), buffer.size());
}

void CurlRequest::closeDownloadFile()
{
    LockHolder locker(m_downloadMutex);

    if (m_downloadFileHandle == FileSystem::invalidPlatformFileHandle)
        return;

    FileSystem::closeFile(m_downloadFileHandle);
    m_downloadFileHandle = FileSystem::invalidPlatformFileHandle;
}

void CurlRequest::cleanupDownloadFile()
{
    LockHolder locker(m_downloadMutex);

    if (!m_downloadFilePath.isEmpty()) {
        FileSystem::deleteFile(m_downloadFilePath);
        m_downloadFilePath = String();
    }
}

CURLcode CurlRequest::willSetupSslCtxCallback(CURL*, void* sslCtx, void* userData)
{
    return static_cast<CurlRequest*>(userData)->willSetupSslCtx(sslCtx);
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

}

#endif
