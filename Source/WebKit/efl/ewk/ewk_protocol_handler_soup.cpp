/*
    Copyright (C) 2011 ProFUSION embedded systems

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ewk_protocol_handler_soup.h"

#include "FrameLoaderClientEfl.h"
#include "FrameNetworkingContextEfl.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ewk_private.h"
#include <glib-object.h>
#include <glib.h>
#include <libsoup/soup-requester.h>
#include <libsoup/soup.h>

struct _EwkProtocolHandlerPrivate {
    char* mime;
    size_t bytesRead;
};

G_DEFINE_TYPE(EwkCustomProtocolHandler, ewk_custom_protocol_handler, SOUP_TYPE_REQUEST)

static char** schemes = 0;
static unsigned customProtocolAddedCount = 0;

static void ewk_custom_protocol_handler_init(EwkCustomProtocolHandler* customProtocolHandler)
{
    customProtocolHandler->priv = G_TYPE_INSTANCE_GET_PRIVATE(customProtocolHandler, EWK_TYPE_CUSTOM_PROTOCOL_HANDLER,
                                                              EwkProtocolHandlerPrivate);
    EINA_SAFETY_ON_NULL_RETURN(customProtocolHandler->priv);

    customProtocolHandler->priv->mime = 0;
    customProtocolHandler->priv->bytesRead = 0;
}

static void ewk_custom_protocol_handler_finalize(GObject* obj)
{
    EwkProtocolHandlerPrivate* priv  = G_TYPE_INSTANCE_GET_PRIVATE(obj, EWK_TYPE_CUSTOM_PROTOCOL_HANDLER,
                                                                   EwkProtocolHandlerPrivate);
    if (priv)
        free(priv->mime);

    G_OBJECT_CLASS(ewk_custom_protocol_handler_parent_class)->finalize(obj);
}

static gboolean ewk_custom_protocol_handler_check_uri(SoupRequest* request, SoupURI* uri, GError** error)
{
    return TRUE;
}

static GInputStream* ewk_custom_protocol_handler_send(SoupRequest* request, GCancellable* cancellable, GError** error)
{
    void* buffer;
    char* mime = 0;
    size_t bytesRead = 0;

    WebCore::ResourceHandle* resource = static_cast<
        WebCore::ResourceHandle*>(g_object_get_data(G_OBJECT(request), "webkit-resource"));
    EINA_SAFETY_ON_NULL_RETURN_VAL(resource, 0);

    const WebCore::FrameNetworkingContextEfl* frameContext = static_cast<
        WebCore::FrameNetworkingContextEfl*>(resource->getInternal()->m_context.get());
    EINA_SAFETY_ON_NULL_RETURN_VAL(frameContext, 0);

    const WebCore::FrameLoaderClientEfl* frameLoaderClient = static_cast<
        WebCore::FrameLoaderClientEfl*>(frameContext->coreFrame()->loader()->client());
    EINA_SAFETY_ON_NULL_RETURN_VAL(frameLoaderClient, 0);

    SoupURI* uri = soup_request_get_uri(request);
    EINA_SAFETY_ON_NULL_RETURN_VAL(uri, 0);

    EwkProtocolHandlerPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(
        reinterpret_cast<EwkCustomProtocolHandler*>(request),
        EWK_TYPE_CUSTOM_PROTOCOL_HANDLER,
        EwkProtocolHandlerPrivate);

    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, 0);


    if (uri->path[0] == '/') {
        buffer = ewk_view_protocol_handler_resource_get(frameLoaderClient->webView(),
                                                        &bytesRead, &mime, uri->path + 1); // The path is always initialized with /.
    } else
        buffer = ewk_view_protocol_handler_resource_get(frameLoaderClient->webView(), &bytesRead, &mime, uri->host);

    priv->bytesRead = bytesRead;
    if (mime)
        priv->mime = strdup(mime);

    return g_memory_input_stream_new_from_data(buffer, bytesRead, free);
}

static goffset ewk_custom_protocol_handler_get_content_length(SoupRequest* request)
{
    EwkProtocolHandlerPrivate* priv  = G_TYPE_INSTANCE_GET_PRIVATE(reinterpret_cast<EwkCustomProtocolHandler*>(request),
                                                                   EWK_TYPE_CUSTOM_PROTOCOL_HANDLER,
                                                                   EwkProtocolHandlerPrivate);
    return (priv) ? priv->bytesRead : 0;
}

static const char* ewk_custom_protocol_handler_get_content_type(SoupRequest* request)
{
    EwkProtocolHandlerPrivate* priv  = G_TYPE_INSTANCE_GET_PRIVATE(reinterpret_cast<EwkCustomProtocolHandler*>(request),
                                                                   EWK_TYPE_CUSTOM_PROTOCOL_HANDLER,
                                                                   EwkProtocolHandlerPrivate);
    return (priv && priv->mime) ? priv->mime : "text/html";
}

static void ewk_custom_protocol_handler_class_init(EwkCustomProtocolHandlerClass* customProtocolHandlerClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(customProtocolHandlerClass);
    SoupRequestClass* requestClass = SOUP_REQUEST_CLASS(customProtocolHandlerClass);

    gobjectClass->finalize = ewk_custom_protocol_handler_finalize;
    requestClass->schemes = const_cast<const char**>(schemes);
    requestClass->check_uri = ewk_custom_protocol_handler_check_uri;
    requestClass->send = ewk_custom_protocol_handler_send;
    requestClass->get_content_length = ewk_custom_protocol_handler_get_content_length;
    requestClass->get_content_type = ewk_custom_protocol_handler_get_content_type;

    g_type_class_add_private(customProtocolHandlerClass, sizeof(EwkProtocolHandlerPrivate));
}

Eina_Bool ewk_custom_protocol_handler_soup_set(const char** protocols)
{
    guint protocolsSize;
    SoupSession* session = WebCore::ResourceHandle::defaultSession();
    SoupSessionFeature* requester;

    protocolsSize = g_strv_length(const_cast<gchar**>(protocols));

    // This array must be null terminated.
    EINA_SAFETY_ON_TRUE_RETURN_VAL(!protocolsSize || protocols[protocolsSize], EINA_FALSE);

    requester = soup_session_get_feature(session, SOUP_TYPE_REQUESTER);
    if (!requester) {
        requester = SOUP_SESSION_FEATURE(soup_requester_new());
        soup_session_add_feature(session, requester);
        g_object_unref(requester);
    }

    if (soup_session_feature_has_feature(requester, EWK_TYPE_CUSTOM_PROTOCOL_HANDLER)) {
        customProtocolAddedCount++;
        return EINA_TRUE;
    }

    schemes = g_strdupv(const_cast<gchar**>(protocols));
    if (!(schemes && soup_session_feature_add_feature(requester, EWK_TYPE_CUSTOM_PROTOCOL_HANDLER)))
        return EINA_FALSE;

    customProtocolAddedCount++;
    return EINA_TRUE;
}

Eina_Bool ewk_custom_protocol_handler_soup_all_unset()
{
    SoupSession* session = WebCore::ResourceHandle::defaultSession();
    SoupSessionFeature* requester;

    if (!customProtocolAddedCount)
        return EINA_FALSE;

    requester = soup_session_get_feature(session, SOUP_TYPE_REQUESTER);
    if (!requester)
        return EINA_FALSE;

    if (!soup_session_feature_has_feature(requester, EWK_TYPE_CUSTOM_PROTOCOL_HANDLER))
        return EINA_FALSE;

    if (customProtocolAddedCount == 1) {
        if (soup_session_feature_remove_feature(requester, EWK_TYPE_CUSTOM_PROTOCOL_HANDLER))
            g_strfreev(schemes);
        else
            return EINA_FALSE;
    }

    customProtocolAddedCount--;
    return EINA_TRUE;
}
