/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "LegacyCustomProtocolManager.h"

#include "DataReference.h"
#include "LegacyCustomProtocolManagerMessages.h"
#include "NetworkProcess.h"
#include "WebKitSoupRequestInputStream.h"
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SoupNetworkSession.h>
#include <WebCore/WebKitSoupRequestGeneric.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

RefPtr<NetworkProcess>& lastCreatedNetworkProcess()
{
    static NeverDestroyed<RefPtr<NetworkProcess>> networkProcess;
    return networkProcess.get();
}

void LegacyCustomProtocolManager::networkProcessCreated(NetworkProcess& networkProcess)
{
    lastCreatedNetworkProcess() = &networkProcess;
}

LegacyCustomProtocolManager::WebSoupRequestAsyncData::WebSoupRequestAsyncData(GRefPtr<GTask>&& requestTask, WebKitSoupRequestGeneric* requestGeneric)
    : task(WTFMove(requestTask))
    , request(requestGeneric)
    , cancellable(g_task_get_cancellable(task.get()))
{
    // If the struct contains a null request, it is because the request failed.
    g_object_add_weak_pointer(G_OBJECT(request), reinterpret_cast<void**>(&request));
}

LegacyCustomProtocolManager::WebSoupRequestAsyncData::~WebSoupRequestAsyncData()
{
    if (request)
        g_object_remove_weak_pointer(G_OBJECT(request), reinterpret_cast<void**>(&request));
}

class CustomProtocolRequestClient final : public WebKitSoupRequestGenericClient {
public:
    static CustomProtocolRequestClient& singleton()
    {
        static NeverDestroyed<CustomProtocolRequestClient> client;
        return client;
    }

private:
    void startRequest(GRefPtr<GTask>&& task) override
    {
        WebKitSoupRequestGeneric* request = WEBKIT_SOUP_REQUEST_GENERIC(g_task_get_source_object(task.get()));
        auto* customProtocolManager = NetworkProcess::singleton().supplement<LegacyCustomProtocolManager>();
        if (!customProtocolManager)
            return;

        auto customProtocolID = customProtocolManager->addCustomProtocol(std::make_unique<LegacyCustomProtocolManager::WebSoupRequestAsyncData>(WTFMove(task), request));
        customProtocolManager->startLoading(customProtocolID, webkitSoupRequestGenericGetRequest(request));
    }
};

void LegacyCustomProtocolManager::registerProtocolClass()
{
    static_cast<WebKitSoupRequestGenericClass*>(g_type_class_ref(WEBKIT_TYPE_SOUP_REQUEST_GENERIC))->client = &CustomProtocolRequestClient::singleton();
    SoupNetworkSession::setCustomProtocolRequestType(WEBKIT_TYPE_SOUP_REQUEST_GENERIC);
}

void LegacyCustomProtocolManager::registerScheme(const String& scheme)
{
    if (!m_registeredSchemes)
        m_registeredSchemes = adoptGRef(g_ptr_array_new_with_free_func(g_free));

    if (m_registeredSchemes->len)
        g_ptr_array_remove_index_fast(m_registeredSchemes.get(), m_registeredSchemes->len - 1);
    g_ptr_array_add(m_registeredSchemes.get(), g_strdup(scheme.utf8().data()));
    g_ptr_array_add(m_registeredSchemes.get(), nullptr);

    auto* genericRequestClass = static_cast<SoupRequestClass*>(g_type_class_peek(WEBKIT_TYPE_SOUP_REQUEST_GENERIC));
    ASSERT(genericRequestClass);
    genericRequestClass->schemes = const_cast<const char**>(reinterpret_cast<char**>(m_registeredSchemes->pdata));
    lastCreatedNetworkProcess()->forEachNetworkStorageSession([](const auto& session) {
        session.soupNetworkSession().setupCustomProtocols();
    });
}

void LegacyCustomProtocolManager::unregisterScheme(const String&)
{
    notImplemented();
}

bool LegacyCustomProtocolManager::supportsScheme(const String& scheme)
{
    if (scheme.isNull())
        return false;

    CString cScheme = scheme.utf8();
    for (unsigned i = 0; i < m_registeredSchemes->len; ++i) {
        if (cScheme == static_cast<char*>(g_ptr_array_index(m_registeredSchemes.get(), i)))
            return true;
    }

    return false;
}

void LegacyCustomProtocolManager::didFailWithError(uint64_t customProtocolID, const ResourceError& error)
{
    auto* data = m_customProtocolMap.get(customProtocolID);
    ASSERT(data);

    // Either we haven't started reading the stream yet, in which case we need to complete the
    // task first, or we failed reading it and the task was already completed by didLoadData().
    ASSERT(!data->stream || !data->task);

    if (!data->stream) {
        GRefPtr<GTask> task = std::exchange(data->task, nullptr);
        ASSERT(task.get());
        g_task_return_new_error(task.get(), g_quark_from_string(error.domain().utf8().data()),
            error.errorCode(), "%s", error.localizedDescription().utf8().data());
    } else
        webkitSoupRequestInputStreamDidFailWithError(WEBKIT_SOUP_REQUEST_INPUT_STREAM(data->stream.get()), error);

    removeCustomProtocol(customProtocolID);
}

void LegacyCustomProtocolManager::didLoadData(uint64_t customProtocolID, const IPC::DataReference& dataReference)
{
    auto* data = m_customProtocolMap.get(customProtocolID);
    // The data might have been removed from the request map if a previous chunk failed
    // and a new message was sent by the UI process before being notified about the failure.
    if (!data)
        return;

    if (!data->stream) {
        GRefPtr<GTask> task = std::exchange(data->task, nullptr);
        ASSERT(task.get());

        goffset soupContentLength = soup_request_get_content_length(SOUP_REQUEST(g_task_get_source_object(task.get())));
        uint64_t contentLength = soupContentLength == -1 ? 0 : static_cast<uint64_t>(soupContentLength);
        if (!dataReference.size()) {
            // Empty reply, just create and empty GMemoryInputStream.
            data->stream = g_memory_input_stream_new();
        } else if (dataReference.size() == contentLength) {
            // We don't expect more data, so we can just create a GMemoryInputStream with all the data.
            data->stream = g_memory_input_stream_new_from_data(g_memdup(dataReference.data(), dataReference.size()), contentLength, g_free);
        } else {
            // We expect more data chunks from the UI process.
            data->stream = webkitSoupRequestInputStreamNew(contentLength);
            webkitSoupRequestInputStreamAddData(WEBKIT_SOUP_REQUEST_INPUT_STREAM(data->stream.get()), dataReference.data(), dataReference.size());
        }
        g_task_return_pointer(task.get(), data->stream.get(), g_object_unref);
        return;
    }

    if (g_cancellable_is_cancelled(data->cancellable.get()) || !data->request) {
        // ResourceRequest failed or it was cancelled. It doesn't matter here the error or if it was cancelled,
        // because that's already handled by the resource handle client, we just want to notify the UI process
        // to stop reading data from the user input stream. If UI process already sent all the data we simply
        // finish silently.
        if (!webkitSoupRequestInputStreamFinished(WEBKIT_SOUP_REQUEST_INPUT_STREAM(data->stream.get())))
            stopLoading(customProtocolID);

        return;
    }

    webkitSoupRequestInputStreamAddData(WEBKIT_SOUP_REQUEST_INPUT_STREAM(data->stream.get()), dataReference.data(), dataReference.size());
}

void LegacyCustomProtocolManager::didReceiveResponse(uint64_t customProtocolID, const ResourceResponse& response, uint32_t)
{
    auto* data = m_customProtocolMap.get(customProtocolID);
    // The data might have been removed from the request map if an error happened even before this point.
    if (!data)
        return;

    ASSERT(data->task);

    WebKitSoupRequestGeneric* request = WEBKIT_SOUP_REQUEST_GENERIC(g_task_get_source_object(data->task.get()));
    webkitSoupRequestGenericSetContentLength(request, response.expectedContentLength() ? response.expectedContentLength() : -1);
    webkitSoupRequestGenericSetContentType(request, !response.mimeType().isEmpty() ? response.mimeType().utf8().data() : 0);
}

void LegacyCustomProtocolManager::didFinishLoading(uint64_t customProtocolID)
{
    ASSERT(m_customProtocolMap.contains(customProtocolID));
    removeCustomProtocol(customProtocolID);
}

void LegacyCustomProtocolManager::wasRedirectedToRequest(uint64_t, const ResourceRequest&, const ResourceResponse&)
{
    notImplemented();
}

} // namespace WebKit
