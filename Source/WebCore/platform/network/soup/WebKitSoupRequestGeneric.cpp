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
#include "WebKitSoupRequestGeneric.h"

#include "ResourceRequest.h"
#include <wtf/text/CString.h>

using namespace WebCore;

G_DEFINE_TYPE(WebKitSoupRequestGeneric, webkit_soup_request_generic, SOUP_TYPE_REQUEST)

struct _WebKitSoupRequestGenericPrivate {
    CString mimeType;
    goffset contentLength;
    ResourceRequest resourceRequest;
};

static void webkitSoupRequestGenericFinalize(GObject* object)
{
    WEBKIT_SOUP_REQUEST_GENERIC(object)->priv->~WebKitSoupRequestGenericPrivate();
    G_OBJECT_CLASS(webkit_soup_request_generic_parent_class)->finalize(object);
}

static void webkit_soup_request_generic_init(WebKitSoupRequestGeneric* request)
{
    WebKitSoupRequestGenericPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(request, WEBKIT_TYPE_SOUP_REQUEST_GENERIC, WebKitSoupRequestGenericPrivate);
    request->priv = priv;
    new (priv) WebKitSoupRequestGenericPrivate();
}

static void webkitSoupRequestGenericSendAsync(SoupRequest* request, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    WebKitSoupRequestGenericClient* client = WEBKIT_SOUP_REQUEST_GENERIC_GET_CLASS(request)->client;
    ASSERT(client);
    client->startRequest(adoptGRef(g_task_new(request, cancellable, callback, userData)));
}

static GInputStream* webkitSoupRequestGenericSendFinish(SoupRequest* request, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(g_task_is_valid(result, request), nullptr);
    auto* inputStream = g_task_propagate_pointer(G_TASK(result), error);
    return inputStream ? G_INPUT_STREAM(inputStream) : nullptr;
}

static goffset webkitSoupRequestGenericGetContentLength(SoupRequest* request)
{
    return WEBKIT_SOUP_REQUEST_GENERIC(request)->priv->contentLength;
}

static const char* webkitSoupRequestGenericGetContentType(SoupRequest* request)
{
    return WEBKIT_SOUP_REQUEST_GENERIC(request)->priv->mimeType.data();
}

static void webkit_soup_request_generic_class_init(WebKitSoupRequestGenericClass* requestGenericClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(requestGenericClass);
    gObjectClass->finalize = webkitSoupRequestGenericFinalize;

    SoupRequestClass* requestClass = SOUP_REQUEST_CLASS(requestGenericClass);
    requestClass->send_async = webkitSoupRequestGenericSendAsync;
    requestClass->send_finish = webkitSoupRequestGenericSendFinish;
    requestClass->get_content_length = webkitSoupRequestGenericGetContentLength;
    requestClass->get_content_type = webkitSoupRequestGenericGetContentType;

    g_type_class_add_private(requestGenericClass, sizeof(WebKitSoupRequestGenericPrivate));
}

void webkitSoupRequestGenericSetContentLength(WebKitSoupRequestGeneric* request, goffset contentLength)
{
    request->priv->contentLength = contentLength;
}

void webkitSoupRequestGenericSetContentType(WebKitSoupRequestGeneric* request, const char* mimeType)
{
    request->priv->mimeType = mimeType;
}

void webkitSoupRequestGenericSetRequest(WebKitSoupRequestGeneric* request, const ResourceRequest& resourceRequest)
{
    request->priv->resourceRequest = resourceRequest;
}

const ResourceRequest& webkitSoupRequestGenericGetRequest(WebKitSoupRequestGeneric* request)
{
    return request->priv->resourceRequest;
}
