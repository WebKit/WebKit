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

#include "DataReference.h"
#include "FormDataReference.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "SharedBufferDataReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
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

void WebServiceWorkerFetchTaskClient::didReceiveData(Ref<SharedBuffer>&& buffer)
{
    if (!m_connection)
        return;

    if (m_waitingForContinueDidReceiveResponseMessage) {
        if (!WTF::holds_alternative<Ref<SharedBuffer>>(m_responseData))
            m_responseData = buffer->copy();
        else
            WTF::get<Ref<SharedBuffer>>(m_responseData)->append(WTFMove(buffer));
        return;
    }

    m_connection->send(Messages::ServiceWorkerFetchTask::DidReceiveData { { buffer }, static_cast<int64_t>(buffer->size()) }, m_fetchIdentifier);
}

void WebServiceWorkerFetchTaskClient::didReceiveFormDataAndFinish(Ref<FormData>&& formData)
{
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
        m_connection->send(Messages::ServiceWorkerFetchTask::DidReceiveFormData { IPC::FormDataReference { WTFMove(formData) } }, m_fetchIdentifier);
        return;
    }

    callOnMainThread([this, protectedThis = makeRef(*this), blobURL = blobURL.isolatedCopy()] () {
        auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(m_serviceWorkerIdentifier);
        if (!serviceWorkerThreadProxy) {
            didFail(internalError(blobURL));
            return;
        }

        m_blobLoader.emplace(*this);
        auto loader = serviceWorkerThreadProxy->createBlobLoader(*m_blobLoader, blobURL);
        if (!loader) {
            m_blobLoader = WTF::nullopt;
            didFail(internalError(blobURL));
            return;
        }

        m_blobLoader->loader = WTFMove(loader);
    });
}

void WebServiceWorkerFetchTaskClient::didReceiveBlobChunk(const char* data, size_t size)
{
    if (!m_connection)
        return;

    m_connection->send(Messages::ServiceWorkerFetchTask::DidReceiveData { { reinterpret_cast<const uint8_t*>(data), size }, static_cast<int64_t>(size) }, m_fetchIdentifier);
}

void WebServiceWorkerFetchTaskClient::didFinishBlobLoading()
{
    didFinish();

    std::exchange(m_blobLoader, WTF::nullopt);
}

void WebServiceWorkerFetchTaskClient::didFail(const ResourceError& error)
{
    if (!m_connection)
        return;

    if (m_waitingForContinueDidReceiveResponseMessage) {
        m_responseData = makeUniqueRef<ResourceError>(error.isolatedCopy());
        return;
    }

    m_connection->send(Messages::ServiceWorkerFetchTask::DidFail { error }, m_fetchIdentifier);

    cleanup();
}

void WebServiceWorkerFetchTaskClient::didFinish()
{
    if (!m_connection)
        return;

    if (m_waitingForContinueDidReceiveResponseMessage) {
        m_didFinish = true;
        return;
    }

    m_connection->send(Messages::ServiceWorkerFetchTask::DidFinish { }, m_fetchIdentifier);

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
    m_connection = nullptr;
}

void WebServiceWorkerFetchTaskClient::continueDidReceiveResponse()
{
    if (!m_connection)
        return;

    m_waitingForContinueDidReceiveResponseMessage = false;

    switchOn(m_responseData, [this](std::nullptr_t&) {
        if (m_didFinish)
            didFinish();
    }, [this](Ref<SharedBuffer>& buffer) {
        didReceiveData(WTFMove(buffer));
        if (m_didFinish)
            didFinish();
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

    if (!isMainThread()) {
        callOnMainThread([protectedThis = makeRef(*this)] () {
            protectedThis->cleanup();
        });
        return;
    }
    if (auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(m_serviceWorkerIdentifier))
        serviceWorkerThreadProxy->removeFetch(m_serverConnectionIdentifier, m_fetchIdentifier);
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
