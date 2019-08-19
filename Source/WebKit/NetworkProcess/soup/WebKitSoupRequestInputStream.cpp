/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebKitSoupRequestInputStream.h"

#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

struct AsyncReadData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    AsyncReadData(GRefPtr<GTask>&& task, void* buffer, gsize count)
        : task(WTFMove(task))
        , buffer(buffer)
        , count(count)
    {
    }

    GRefPtr<GTask> task;
    void* buffer;
    size_t count;
};

struct _WebKitSoupRequestInputStreamPrivate {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    uint64_t contentLength;
    uint64_t bytesReceived;
    uint64_t bytesRead;

    GUniquePtr<GError> error;

    std::unique_ptr<AsyncReadData> pendingAsyncRead;
};

G_DEFINE_TYPE(WebKitSoupRequestInputStream, webkit_soup_request_input_stream, G_TYPE_MEMORY_INPUT_STREAM)

static void webkitSoupRequestInputStreamReadAsyncResultComplete(GTask* task, void* buffer, gsize count)
{
    WebKitSoupRequestInputStream* stream = WEBKIT_SOUP_REQUEST_INPUT_STREAM(g_task_get_source_object(task));
    GError* error = nullptr;
    gssize bytesRead = G_INPUT_STREAM_GET_CLASS(stream)->read_fn(G_INPUT_STREAM(stream), buffer, count, g_task_get_cancellable(task), &error);
    if (!error) {
        stream->priv->bytesRead += bytesRead;
        g_task_return_int(task, bytesRead);
    } else
        g_task_return_error(task, error);
}

static void webkitSoupRequestInputStreamPendingReadAsyncComplete(WebKitSoupRequestInputStream* stream)
{
    if (auto data = WTFMove(stream->priv->pendingAsyncRead))
        webkitSoupRequestInputStreamReadAsyncResultComplete(data->task.get(), data->buffer, data->count);
}

static bool webkitSoupRequestInputStreamHasDataToRead(WebKitSoupRequestInputStream* stream)
{
    return stream->priv->bytesRead < stream->priv->bytesReceived;
}

static bool webkitSoupRequestInputStreamIsWaitingForData(WebKitSoupRequestInputStream* stream)
{
    return !stream->priv->contentLength || stream->priv->bytesReceived < stream->priv->contentLength;
}

static void webkitSoupRequestInputStreamReadAsync(GInputStream* inputStream, void* buffer, gsize count, int /*priority*/, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    ASSERT(RunLoop::isMain());
    WebKitSoupRequestInputStream* stream = WEBKIT_SOUP_REQUEST_INPUT_STREAM(inputStream);
    GRefPtr<GTask> task = adoptGRef(g_task_new(stream, cancellable, callback, userData));

    if (!webkitSoupRequestInputStreamHasDataToRead(stream) && !webkitSoupRequestInputStreamIsWaitingForData(stream)) {
        g_task_return_int(task.get(), 0);
        return;
    }

    if (stream->priv->error.get()) {
        g_task_return_error(task.get(), stream->priv->error.release());
        return;
    }

    if (webkitSoupRequestInputStreamHasDataToRead(stream)) {
        webkitSoupRequestInputStreamReadAsyncResultComplete(task.get(), buffer, count);
        return;
    }

    stream->priv->pendingAsyncRead = makeUnique<AsyncReadData>(WTFMove(task), buffer, count);
}

static gssize webkitSoupRequestInputStreamReadFinish(GInputStream* inputStream, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(g_task_is_valid(result, inputStream), 0);

    return g_task_propagate_int(G_TASK(result), error);
}

static void webkitSoupRequestInputStreamFinalize(GObject* object)
{
    WEBKIT_SOUP_REQUEST_INPUT_STREAM(object)->priv->~WebKitSoupRequestInputStreamPrivate();
    G_OBJECT_CLASS(webkit_soup_request_input_stream_parent_class)->finalize(object);
}

static void webkit_soup_request_input_stream_init(WebKitSoupRequestInputStream* stream)
{
    WebKitSoupRequestInputStreamPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(stream, WEBKIT_TYPE_SOUP_REQUEST_INPUT_STREAM, WebKitSoupRequestInputStreamPrivate);
    stream->priv = priv;
    new (priv) WebKitSoupRequestInputStreamPrivate();
}

static void webkit_soup_request_input_stream_class_init(WebKitSoupRequestInputStreamClass* requestStreamClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(requestStreamClass);
    gObjectClass->finalize = webkitSoupRequestInputStreamFinalize;

    GInputStreamClass* inputStreamClass = G_INPUT_STREAM_CLASS(requestStreamClass);
    inputStreamClass->read_async = webkitSoupRequestInputStreamReadAsync;
    inputStreamClass->read_finish = webkitSoupRequestInputStreamReadFinish;

    g_type_class_add_private(requestStreamClass, sizeof(WebKitSoupRequestInputStreamPrivate));
}

GInputStream* webkitSoupRequestInputStreamNew(uint64_t contentLength)
{
    WebKitSoupRequestInputStream* stream = WEBKIT_SOUP_REQUEST_INPUT_STREAM(g_object_new(WEBKIT_TYPE_SOUP_REQUEST_INPUT_STREAM, NULL));
    stream->priv->contentLength = contentLength;
    return G_INPUT_STREAM(stream);
}

void webkitSoupRequestInputStreamAddData(WebKitSoupRequestInputStream* stream, const void* data, size_t dataLength)
{
    ASSERT(RunLoop::isMain());

    if (webkitSoupRequestInputStreamFinished(stream))
        return;

    if (dataLength) {
        // Truncate the dataLength to the contentLength if it's known.
        if (stream->priv->contentLength && stream->priv->bytesReceived + dataLength > stream->priv->contentLength)
            dataLength = stream->priv->contentLength - stream->priv->bytesReceived;
        stream->priv->bytesReceived += dataLength;
        g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(stream), g_memdup(data, dataLength), dataLength, g_free);
    } else {
        // We have received all the data, set contentLength to bytesReceived to indicate we have finished.
        stream->priv->contentLength = stream->priv->bytesReceived;
        // If there's a pending read to complete, read_fn will return 0 because we haven't added more data to the
        // memory input stream. And if there isn't a pending read, the next call to read_async will return 0 too, because
        // webkitSoupRequestInputStreamFinished() is now TRUE.
    }

    webkitSoupRequestInputStreamPendingReadAsyncComplete(stream);
}

void webkitSoupRequestInputStreamDidFailWithError(WebKitSoupRequestInputStream* stream, const WebCore::ResourceError& resourceError)
{
    GUniquePtr<GError> error(g_error_new(g_quark_from_string(resourceError.domain().utf8().data()), resourceError.errorCode(), "%s", resourceError.localizedDescription().utf8().data()));
    if (auto data = WTFMove(stream->priv->pendingAsyncRead))
        g_task_return_error(data->task.get(), error.release());
    else {
        stream->priv->contentLength = stream->priv->bytesReceived;
        stream->priv->error = WTFMove(error);
    }
}

bool webkitSoupRequestInputStreamFinished(WebKitSoupRequestInputStream* stream)
{
    return !webkitSoupRequestInputStreamIsWaitingForData(stream);
}
