/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "CustomProtocolManagerImpl.h"

#if ENABLE(CUSTOM_PROTOCOLS)

#include "ChildProcess.h"
#include "CustomProtocolManagerProxyMessages.h"
#include "DataReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebKitSoupRequestGeneric.h"
#include "WebKitSoupRequestInputStream.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SoupNetworkSession.h>
#include <WebCore/SoupURIUtils.h>

namespace WebKit {

static uint64_t generateCustomProtocolID()
{
    static uint64_t uniqueCustomProtocolID = 0;
    return ++uniqueCustomProtocolID;
}

struct WebSoupRequestAsyncData {
    WebSoupRequestAsyncData(GTask* task, WebKitSoupRequestGeneric* requestGeneric)
        : task(task)
        , request(requestGeneric)
        , cancellable(g_task_get_cancellable(task))
    {
        // If the struct contains a null request, it is because the request failed.
        g_object_add_weak_pointer(G_OBJECT(request), reinterpret_cast<void**>(&request));
    }

    ~WebSoupRequestAsyncData()
    {
        if (request)
            g_object_remove_weak_pointer(G_OBJECT(request), reinterpret_cast<void**>(&request));
    }

    bool requestFailed()
    {
        return g_cancellable_is_cancelled(cancellable.get()) || !request;
    }

    GRefPtr<GTask> releaseTask()
    {
        GTask* returnValue = task;
        task = nullptr;
        return adoptGRef(returnValue);
    }

    GTask* task;
    WebKitSoupRequestGeneric* request;
    GRefPtr<GCancellable> cancellable;
    GRefPtr<GInputStream> stream;
};

CustomProtocolManagerImpl::CustomProtocolManagerImpl(ChildProcess* childProcess)
    : m_childProcess(childProcess)
    , m_schemes(adoptGRef(g_ptr_array_new_with_free_func(g_free)))
{
}

CustomProtocolManagerImpl::~CustomProtocolManagerImpl()
{
}

void CustomProtocolManagerImpl::registerScheme(const String& scheme)
{
    if (m_schemes->len)
        g_ptr_array_remove_index_fast(m_schemes.get(), m_schemes->len - 1);
    g_ptr_array_add(m_schemes.get(), g_strdup(scheme.utf8().data()));
    g_ptr_array_add(m_schemes.get(), nullptr);

    SoupSession* session = WebCore::SoupNetworkSession::defaultSession().soupSession();
    SoupRequestClass* genericRequestClass = static_cast<SoupRequestClass*>(g_type_class_ref(WEBKIT_TYPE_SOUP_REQUEST_GENERIC));
    genericRequestClass->schemes = const_cast<const char**>(reinterpret_cast<char**>(m_schemes->pdata));
    static_cast<WebKitSoupRequestGenericClass*>(g_type_class_ref(WEBKIT_TYPE_SOUP_REQUEST_GENERIC))->customProtocolManager = this;
    soup_session_add_feature_by_type(session, WEBKIT_TYPE_SOUP_REQUEST_GENERIC);
}

bool CustomProtocolManagerImpl::supportsScheme(const String& scheme)
{
    if (scheme.isNull())
        return false;

    CString cScheme = scheme.utf8();
    for (unsigned i = 0; i < m_schemes->len; ++i) {
        if (cScheme == static_cast<char*>(g_ptr_array_index(m_schemes.get(), i)))
            return true;
    }

    return false;
}

void CustomProtocolManagerImpl::didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError& error)
{
    WebSoupRequestAsyncData* data = m_customProtocolMap.get(customProtocolID);
    ASSERT(data);

    GRefPtr<GTask> task = data->releaseTask();
    ASSERT(task.get());
    g_task_return_new_error(task.get(), g_quark_from_string(error.domain().utf8().data()),
        error.errorCode(), "%s", error.localizedDescription().utf8().data());

    m_customProtocolMap.remove(customProtocolID);
}

void CustomProtocolManagerImpl::didLoadData(uint64_t customProtocolID, const IPC::DataReference& dataReference)
{
    WebSoupRequestAsyncData* data = m_customProtocolMap.get(customProtocolID);
    // The data might have been removed from the request map if a previous chunk failed
    // and a new message was sent by the UI process before being notified about the failure.
    if (!data)
        return;

    if (!data->stream) {
        GRefPtr<GTask> task = data->releaseTask();
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

    if (data->requestFailed()) {
        // ResourceRequest failed or it was cancelled. It doesn't matter here the error or if it was cancelled,
        // because that's already handled by the resource handle client, we just want to notify the UI process
        // to stop reading data from the user input stream. If UI process already sent all the data we simply
        // finish silently.
        if (!webkitSoupRequestInputStreamFinished(WEBKIT_SOUP_REQUEST_INPUT_STREAM(data->stream.get())))
            m_childProcess->send(Messages::CustomProtocolManagerProxy::StopLoading(customProtocolID), 0);

        return;
    }

    webkitSoupRequestInputStreamAddData(WEBKIT_SOUP_REQUEST_INPUT_STREAM(data->stream.get()), dataReference.data(), dataReference.size());
}

void CustomProtocolManagerImpl::didReceiveResponse(uint64_t customProtocolID, const WebCore::ResourceResponse& response)
{
    WebSoupRequestAsyncData* data = m_customProtocolMap.get(customProtocolID);
    ASSERT(data);
    ASSERT(data->task);

    WebKitSoupRequestGeneric* request = WEBKIT_SOUP_REQUEST_GENERIC(g_task_get_source_object(data->task));
    webkitSoupRequestGenericSetContentLength(request, response.expectedContentLength() ? response.expectedContentLength() : -1);
    webkitSoupRequestGenericSetContentType(request, !response.mimeType().isEmpty() ? response.mimeType().utf8().data() : 0);
}

void CustomProtocolManagerImpl::didFinishLoading(uint64_t customProtocolID)
{
    ASSERT(m_customProtocolMap.contains(customProtocolID));
    m_customProtocolMap.remove(customProtocolID);
}

void CustomProtocolManagerImpl::send(GTask* task)
{
    uint64_t customProtocolID = generateCustomProtocolID();
    WebKitSoupRequestGeneric* request = WEBKIT_SOUP_REQUEST_GENERIC(g_task_get_source_object(task));
    m_customProtocolMap.set(customProtocolID, std::make_unique<WebSoupRequestAsyncData>(task, request));

    WebCore::ResourceRequest resourceRequest(WebCore::soupURIToKURL(soup_request_get_uri(SOUP_REQUEST(request))));
    m_childProcess->send(Messages::CustomProtocolManagerProxy::StartLoading(customProtocolID, resourceRequest), 0);
}

GInputStream* CustomProtocolManagerImpl::finish(GTask* task, GError** error)
{
    gpointer inputStream = g_task_propagate_pointer(task, error);
    return inputStream ? G_INPUT_STREAM(inputStream) : 0;
}

} // namespace WebKit

#endif // ENABLE(CUSTOM_PROTOCOLS)
