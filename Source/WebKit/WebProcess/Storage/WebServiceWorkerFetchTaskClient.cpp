/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebServiceWorkerFetchTaskClient.h"

#if ENABLE(SERVICE_WORKER)

#include "FormDataReference.h"
#include "Logging.h"
#include "ServiceWorkerDownloadTaskMessages.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "SharedBufferReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include <WebCore/FetchEvent.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SWContextManager.h>
#include <wtf/RunLoop.h>

namespace WebKit {
using namespace WebCore;

WebServiceWorkerFetchTaskClient::WebServiceWorkerFetchTaskClient(Ref<IPC::Connection>&& connection, WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, WebCore::SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier, bool needsContinueDidReceiveResponseMessage)
    : m_connection(WTFMove(connection))
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
    , m_serviceWorkerIdentifier(serviceWorkerIdentifier)
    , m_fetchIdentifier(fetchIdentifier)
    , m_needsContinueDidReceiveResponseMessage(needsContinueDidReceiveResponseMessage)
{
}

void WebServiceWorkerFetchTaskClient::didReceiveRedirection(const WebCore::ResourceResponse& response)
{
    if (!m_connection)
        return;
    m_connection->send(Messages::ServiceWorkerFetchTask::DidReceiveRedirectResponse { response }, m_fetchIdentifier);

    cleanup();
}

void WebServiceWorkerFetchTaskClient::didReceiveResponse(const ResourceResponse& response)
{
    if (!m_connection)
        return;

    if (m_needsContinueDidReceiveResponseMessage)
        m_waitingForContinueDidReceiveResponseMessage = true;

    m_connection->send(Messages::ServiceWorkerFetchTask::DidReceiveResponse { response, m_needsContinueDidReceiveResponseMessage }, m_fetchIdentifier);
}

void WebServiceWorkerFetchTaskClient::didReceiveData(const SharedBuffer& buffer)
{
    if (!m_connection)
        return;

    if (m_waitingForContinueDidReceiveResponseMessage) {
        if (!std::holds_alternative<SharedBufferBuilder>(m_responseData))
            m_responseData = SharedBufferBuilder();
        std::get<SharedBufferBuilder>(m_responseData).append(buffer);
        return;
    }

    if (m_isDownload)
        m_connection->send(Messages::ServiceWorkerDownloadTask::DidReceiveData { IPC::SharedBufferReference(buffer), buffer.size() }, m_fetchIdentifier);
    else
        m_connection->send(Messages::ServiceWorkerFetchTask::DidReceiveData { IPC::SharedBufferReference(buffer), buffer.size() }, m_fetchIdentifier);
}

void WebServiceWorkerFetchTaskClient::didReceiveFormDataAndFinish(Ref<FormData>&& formData)
{
    if (auto sharedBuffer = formData->asSharedBuffer()) {
        didReceiveData(sharedBuffer.releaseNonNull());
        didFinish({ });
        return;
    }

    if (!m_connection)
        return;

    if (m_waitingForContinueDidReceiveResponseMessage) {
        m_responseData = formData->isolatedCopy();
        return;
    }

    // FIXME: We should send this form data to the other process and consume it there.
    // For now and for the case of blobs, we read it there and send the data through IPC.
    URL blobURL = formData->asBlobURL();
    if (blobURL.isNull()) {
        if (m_isDownload)
            m_connection->send(Messages::ServiceWorkerDownloadTask::DidReceiveFormData { IPC::FormDataReference { WTFMove(formData) } }, m_fetchIdentifier);
        else
            m_connection->send(Messages::ServiceWorkerFetchTask::DidReceiveFormData { IPC::FormDataReference { WTFMove(formData) } }, m_fetchIdentifier);
        return;
    }

    callOnMainRunLoop([this, protectedThis = Ref { *this }, blobURL = WTFMove(blobURL).isolatedCopy()] () {
        auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(m_serviceWorkerIdentifier);
        if (!serviceWorkerThreadProxy) {
            didFail(internalError(blobURL));
            return;
        }

        m_blobLoader.emplace(*this);
        auto loader = serviceWorkerThreadProxy->createBlobLoader(*m_blobLoader, blobURL);
        if (!loader) {
            m_blobLoader = std::nullopt;
            didFail(internalError(blobURL));
            return;
        }

        m_blobLoader->loader = WTFMove(loader);
    });
}

void WebServiceWorkerFetchTaskClient::didReceiveBlobChunk(const SharedBuffer& buffer)
{
    if (!m_connection)
        return;

    if (m_isDownload)
        m_connection->send(Messages::ServiceWorkerDownloadTask::DidReceiveData { IPC::SharedBufferReference(buffer), buffer.size() }, m_fetchIdentifier);
    else
        m_connection->send(Messages::ServiceWorkerFetchTask::DidReceiveData { IPC::SharedBufferReference(buffer), buffer.size() }, m_fetchIdentifier);
}

void WebServiceWorkerFetchTaskClient::didFinishBlobLoading()
{
    didFinish({ });

    std::exchange(m_blobLoader, std::nullopt);
}

void WebServiceWorkerFetchTaskClient::didFail(const ResourceError& error)
{
    if (!m_connection)
        return;

    if (m_waitingForContinueDidReceiveResponseMessage) {
        RELEASE_LOG(ServiceWorker, "ServiceWorkerFrameLoaderClient::didFail while waiting, fetch identifier %llu", m_fetchIdentifier.toUInt64());

        m_responseData = makeUniqueRef<ResourceError>(error.isolatedCopy());
        return;
    }

    if (m_isDownload)
        m_connection->send(Messages::ServiceWorkerDownloadTask::DidFail { error }, m_fetchIdentifier);
    else
        m_connection->send(Messages::ServiceWorkerFetchTask::DidFail { error }, m_fetchIdentifier);

    cleanup();
}

void WebServiceWorkerFetchTaskClient::didFinish(const NetworkLoadMetrics& metrics)
{
    if (!m_connection)
        return;

    if (m_waitingForContinueDidReceiveResponseMessage) {
        RELEASE_LOG(ServiceWorker, "ServiceWorkerFrameLoaderClient::didFinish while waiting, fetch identifier %llu", m_fetchIdentifier.toUInt64());

        m_didFinish = true;
        m_networkLoadMetrics = metrics.isolatedCopy();
        return;
    }

    if (m_isDownload)
        m_connection->send(Messages::ServiceWorkerDownloadTask::DidFinish { }, m_fetchIdentifier);
    else
        m_connection->send(Messages::ServiceWorkerFetchTask::DidFinish { metrics }, m_fetchIdentifier);

    cleanup();
}

void WebServiceWorkerFetchTaskClient::didNotHandle()
{
    if (!m_connection)
        return;

    m_connection->send(Messages::ServiceWorkerFetchTask::DidNotHandle { }, m_fetchIdentifier);

    cleanup();
}

void WebServiceWorkerFetchTaskClient::cancel()
{
    ASSERT(!isMainRunLoop());
    m_connection = nullptr;
    if (m_cancelledCallback)
        m_cancelledCallback();
}

void WebServiceWorkerFetchTaskClient::convertFetchToDownload()
{
    m_isDownload = true;
    continueDidReceiveResponse();
}

void WebServiceWorkerFetchTaskClient::setCancelledCallback(Function<void()>&& callback)
{
    ASSERT(!m_cancelledCallback);
    m_cancelledCallback = WTFMove(callback);
}

void WebServiceWorkerFetchTaskClient::setFetchEvent(Ref<WebCore::FetchEvent>&& event)
{
    m_event = WTFMove(event);

    if (!m_preloadResponse.isNull()) {
        m_event->navigationPreloadIsReady(WTFMove(m_preloadResponse));
        m_event = nullptr;
        return;
    }

    if (!m_preloadError.isNull()) {
        m_event->navigationPreloadFailed(WTFMove(m_preloadError));
        m_event = nullptr;
    }
}

void WebServiceWorkerFetchTaskClient::navigationPreloadIsReady(ResourceResponse&& response)
{
    if (!m_event) {
        m_preloadResponse = WTFMove(response);
        return;
    }

    m_event->navigationPreloadIsReady(WTFMove(response));
    m_event = nullptr;
}

void WebServiceWorkerFetchTaskClient::navigationPreloadFailed(ResourceError&& error)
{
    if (!m_event) {
        m_preloadError = WTFMove(error);
        return;
    }
    m_event->navigationPreloadFailed(WTFMove(error));
    m_event = nullptr;
}
      
void WebServiceWorkerFetchTaskClient::usePreload()
{
    if (!m_connection)
        return;

    m_connection->send(Messages::ServiceWorkerFetchTask::UsePreload { }, m_fetchIdentifier);

    cleanup();
}

void WebServiceWorkerFetchTaskClient::continueDidReceiveResponse()
{
    RELEASE_LOG(ServiceWorker, "ServiceWorkerFrameLoaderClient::continueDidReceiveResponse, has connection %d, didFinish %d, response type %ld", !!m_connection, m_didFinish, static_cast<long>(m_responseData.index()));

    if (!m_connection)
        return;

    m_waitingForContinueDidReceiveResponseMessage = false;

    switchOn(m_responseData, [this](std::nullptr_t&) {
        if (m_didFinish)
            didFinish(m_networkLoadMetrics);
    }, [this](const SharedBufferBuilder& buffer) {
        didReceiveData(buffer.copy()->makeContiguous());
        if (m_didFinish)
            didFinish(m_networkLoadMetrics);
    }, [this](Ref<FormData>& formData) {
        didReceiveFormDataAndFinish(WTFMove(formData));
    }, [this](UniqueRef<ResourceError>& error) {
        didFail(error.get());
    });
    m_responseData = nullptr;
}

void WebServiceWorkerFetchTaskClient::cleanup()
{
    m_connection = nullptr;
    m_event = nullptr;
    ensureOnMainRunLoop([serviceWorkerIdentifier = m_serviceWorkerIdentifier, serverConnectionIdentifier = m_serverConnectionIdentifier, fetchIdentifier = m_fetchIdentifier] {
        if (auto* proxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier))
            proxy->removeFetch(serverConnectionIdentifier, fetchIdentifier);
    });
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
