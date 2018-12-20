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
#include "NetworkProcessMessages.h"
#include "SharedBufferDataReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SWContextManager.h>
#include <wtf/RunLoop.h>

namespace WebKit {
using namespace WebCore;

WebServiceWorkerFetchTaskClient::~WebServiceWorkerFetchTaskClient()
{
    if (m_connection)
        RunLoop::main().dispatch([connection = WTFMove(m_connection)] { });
}

WebServiceWorkerFetchTaskClient::WebServiceWorkerFetchTaskClient(Ref<IPC::Connection>&& connection, WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, WebCore::SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier)
    : m_connection(WTFMove(connection))
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
    , m_serviceWorkerIdentifier(serviceWorkerIdentifier)
    , m_fetchIdentifier(fetchIdentifier)
{
}

void WebServiceWorkerFetchTaskClient::didReceiveResponse(const ResourceResponse& response)
{
    if (!m_connection)
        return;
    m_connection->send(Messages::NetworkProcess::DidReceiveFetchResponse { m_serverConnectionIdentifier, m_fetchIdentifier, response }, 0);
}

void WebServiceWorkerFetchTaskClient::didReceiveData(Ref<SharedBuffer>&& buffer)
{
    if (!m_connection)
        return;
    m_connection->send(Messages::NetworkProcess::DidReceiveFetchData { m_serverConnectionIdentifier, m_fetchIdentifier, { buffer }, static_cast<int64_t>(buffer->size()) }, 0);
}

void WebServiceWorkerFetchTaskClient::didReceiveFormDataAndFinish(Ref<FormData>&& formData)
{
    if (!m_connection)
        return;

    // FIXME: We should send this form data to the other process and consume it there.
    // For now and for the case of blobs, we read it there and send the data through IPC.
    URL blobURL = formData->asBlobURL();
    if (blobURL.isNull()) {
        m_connection->send(Messages::NetworkProcess::DidReceiveFetchFormData { m_serverConnectionIdentifier, m_fetchIdentifier, IPC::FormDataReference { WTFMove(formData) } }, 0);
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

    m_connection->send(Messages::NetworkProcess::DidReceiveFetchData { m_serverConnectionIdentifier, m_fetchIdentifier, { reinterpret_cast<const uint8_t*>(data), size }, static_cast<int64_t>(size) }, 0);
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

    m_connection->send(Messages::NetworkProcess::DidFailFetch { m_serverConnectionIdentifier, m_fetchIdentifier, error }, 0);

    cleanup();
}

void WebServiceWorkerFetchTaskClient::didFinish()
{
    if (!m_connection)
        return;

    m_connection->send(Messages::NetworkProcess::DidFinishFetch { m_serverConnectionIdentifier, m_fetchIdentifier }, 0);

    cleanup();
}

void WebServiceWorkerFetchTaskClient::didNotHandle()
{
    if (!m_connection)
        return;

    m_connection->send(Messages::NetworkProcess::DidNotHandleFetch { m_serverConnectionIdentifier, m_fetchIdentifier }, 0);

    cleanup();
}

void WebServiceWorkerFetchTaskClient::cancel()
{
    m_connection = nullptr;
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
