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
#include "WebKitWebResource.h"

#include "APIData.h"
#include "WebFrameProxy.h"
#include "WebKitPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitWebResourcePrivate.h"
#include "WebPageProxy.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * WebKitWebResource:
 *
 * Represents a resource at the end of a URI.
 *
 * A #WebKitWebResource encapsulates content for each resource at the
 * end of a particular URI. For example, one #WebKitWebResource will
 * be created for each separate image and stylesheet when a page is
 * loaded.
 *
 * You can access the response and the URI for a given
 * #WebKitWebResource, using webkit_web_resource_get_uri() and
 * webkit_web_resource_get_response(), as well as the raw data, using
 * webkit_web_resource_get_data().
 */

enum {
    SENT_REQUEST,
    RECEIVED_DATA,
    FINISHED,
    FAILED,
    FAILED_WITH_TLS_ERRORS,

    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_URI,
    PROP_RESPONSE,
    N_PROPERTIES,
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

struct _WebKitWebResourcePrivate {
    RefPtr<WebFrameProxy> frame;
    CString uri;
    GRefPtr<WebKitURIResponse> response;
    bool isMainResource;
};

WEBKIT_DEFINE_TYPE(WebKitWebResource, webkit_web_resource, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0, };

static void webkitWebResourceGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebResource* resource = WEBKIT_WEB_RESOURCE(object);

    switch (propId) {
    case PROP_URI:
        g_value_set_string(value, webkit_web_resource_get_uri(resource));
        break;
    case PROP_RESPONSE:
        g_value_set_object(value, webkit_web_resource_get_response(resource));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_web_resource_class_init(WebKitWebResourceClass* resourceClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(resourceClass);
    objectClass->get_property = webkitWebResourceGetProperty;

    /**
     * WebKitWebResource:uri:
     *
     * The current active URI of the #WebKitWebResource.
     * See webkit_web_resource_get_uri() for more details.
     */
    sObjProperties[PROP_URI] =
        g_param_spec_string(
            "uri",
            _("URI"),
            _("The current active URI of the resource"),
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebResource:response:
     *
     * The #WebKitURIResponse associated with this resource.
     */
    sObjProperties[PROP_RESPONSE] =
        g_param_spec_object(
            "response",
            _("Response"),
            _("The response of the resource"),
            WEBKIT_TYPE_URI_RESPONSE,
            WEBKIT_PARAM_READABLE);

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties);

    /**
     * WebKitWebResource::sent-request:
     * @resource: the #WebKitWebResource
     * @request: a #WebKitURIRequest
     * @redirected_response: a #WebKitURIResponse, or %NULL
     *
     * This signal is emitted when @request has been sent to the
     * server. In case of a server redirection this signal is
     * emitted again with the @request argument containing the new
     * request sent to the server due to the redirection and the
     * @redirected_response parameter containing the response
     * received by the server for the initial request.
     */
    signals[SENT_REQUEST] = g_signal_new(
        "sent-request",
        G_TYPE_FROM_CLASS(objectClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 2,
        WEBKIT_TYPE_URI_REQUEST,
        WEBKIT_TYPE_URI_RESPONSE);

#if !ENABLE(2022_GLIB_API)
    /**
     * WebKitWebResource::received-data:
     * @resource: the #WebKitWebResource
     * @data_length: the length of data received in bytes
     *
     * This signal is emitted after response is received,
     * every time new data has been received. It's
     * useful to know the progress of the resource load operation.
     *
     * This is signal is deprecated since version 2.40 and it's never emitted.
     *
     * Deprecated: 2.40
     */
    signals[RECEIVED_DATA] = g_signal_new(
        "received-data",
        G_TYPE_FROM_CLASS(objectClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        G_TYPE_UINT64);
#endif

    /**
     * WebKitWebResource::finished:
     * @resource: the #WebKitWebResource
     *
     * This signal is emitted when the resource load finishes successfully
     * or due to an error. In case of errors #WebKitWebResource::failed signal
     * is emitted before this one.
     */
    signals[FINISHED] =
        g_signal_new("finished",
                     G_TYPE_FROM_CLASS(objectClass),
                     G_SIGNAL_RUN_LAST,
                     0, 0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * WebKitWebResource::failed:
     * @resource: the #WebKitWebResource
     * @error: the #GError that was triggered
     *
     * This signal is emitted when an error occurs during the resource
     * load operation.
     */
    signals[FAILED] =
        g_signal_new(
            "failed",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__BOXED,
            G_TYPE_NONE, 1,
            G_TYPE_ERROR | G_SIGNAL_TYPE_STATIC_SCOPE);

    /**
     * WebKitWebResource::failed-with-tls-errors:
     * @resource: the #WebKitWebResource
     * @certificate: a #GTlsCertificate
     * @errors: a #GTlsCertificateFlags with the verification status of @certificate
     *
     * This signal is emitted when a TLS error occurs during the resource load operation.
     *
     * Since: 2.8
     */
    signals[FAILED_WITH_TLS_ERRORS] =
        g_signal_new("failed-with-tls-errors",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, nullptr, nullptr,
            g_cclosure_marshal_generic,
            G_TYPE_NONE, 2,
            G_TYPE_TLS_CERTIFICATE,
            G_TYPE_TLS_CERTIFICATE_FLAGS);
}

static void webkitWebResourceUpdateURI(WebKitWebResource* resource, const CString& requestURI)
{
    if (resource->priv->uri == requestURI)
        return;

    resource->priv->uri = requestURI;
    g_object_notify_by_pspec(G_OBJECT(resource), sObjProperties[PROP_URI]);
}

WebKitWebResource* webkitWebResourceCreate(WebFrameProxy& frame, const WebCore::ResourceRequest& request)
{
    WebKitWebResource* resource = WEBKIT_WEB_RESOURCE(g_object_new(WEBKIT_TYPE_WEB_RESOURCE, NULL));
    resource->priv->frame = &frame;
    resource->priv->uri = request.url().string().utf8();
    resource->priv->isMainResource = frame.isMainFrame() && request.requester() == WebCore::ResourceRequestRequester::Main;
    return resource;
}

void webkitWebResourceSentRequest(WebKitWebResource* resource, WebCore::ResourceRequest&& request, WebCore::ResourceResponse&& redirectResponse)
{
    GRefPtr<WebKitURIRequest> uriRequest = adoptGRef(webkitURIRequestCreateForResourceRequest(request));
    webkitWebResourceUpdateURI(resource, webkit_uri_request_get_uri(uriRequest.get()));
    GRefPtr<WebKitURIResponse> uriRedirectResponse = !redirectResponse.isNull() ? adoptGRef(webkitURIResponseCreateForResourceResponse(redirectResponse)) : nullptr;
    g_signal_emit(resource, signals[SENT_REQUEST], 0, uriRequest.get(), uriRedirectResponse.get());
}

void webkitWebResourceSetResponse(WebKitWebResource* resource, WebCore::ResourceResponse&& response)
{
    resource->priv->response = adoptGRef(webkitURIResponseCreateForResourceResponse(response));
    g_object_notify_by_pspec(G_OBJECT(resource), sObjProperties[PROP_RESPONSE]);
}

void webkitWebResourceFinished(WebKitWebResource* resource)
{
    g_signal_emit(resource, signals[FINISHED], 0, nullptr);
}

void webkitWebResourceFailed(WebKitWebResource* resource, WebCore::ResourceError&& resourceError)
{
    if (resourceError.tlsErrors())
        g_signal_emit(resource, signals[FAILED_WITH_TLS_ERRORS], 0, resourceError.certificate(), static_cast<GTlsCertificateFlags>(resourceError.tlsErrors()));
    else {
        GUniquePtr<GError> error(g_error_new_literal(g_quark_from_string(resourceError.domain().utf8().data()),
            toWebKitError(resourceError.errorCode()), resourceError.localizedDescription().utf8().data()));
        g_signal_emit(resource, signals[FAILED], 0, error.get());
    }

    webkitWebResourceFinished(resource);
}

bool webkitWebResourceIsMainResource(WebKitWebResource* resource)
{
    return resource->priv->isMainResource;
}

/**
 * webkit_web_resource_get_uri:
 * @resource: a #WebKitWebResource
 *
 * Returns the current active URI of @resource.
 *
 * The active URI might change during
 * a load operation:
 *
 * <orderedlist>
 * <listitem><para>
 *   When the resource load starts, the active URI is the requested URI
 * </para></listitem>
 * <listitem><para>
 *   When the initial request is sent to the server, #WebKitWebResource::sent-request
 *   signal is emitted without a redirected response, the active URI is the URI of
 *   the request sent to the server.
 * </para></listitem>
 * <listitem><para>
 *   In case of a server redirection, #WebKitWebResource::sent-request signal
 *   is emitted again with a redirected response, the active URI is the URI the request
 *   was redirected to.
 * </para></listitem>
 * <listitem><para>
 *   When the response is received from the server, the active URI is the final
 *   one and it will not change again.
 * </para></listitem>
 * </orderedlist>
 *
 * You can monitor the active URI by connecting to the notify::uri
 * signal of @resource.
 *
 * Returns: the current active URI of @resource
 */
const char* webkit_web_resource_get_uri(WebKitWebResource* resource)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_RESOURCE(resource), 0);

    return resource->priv->uri.data();
}

/**
 * webkit_web_resource_get_response:
 * @resource: a #WebKitWebResource
 *
 * Retrieves the #WebKitURIResponse of the resource load operation.
 *
 * This method returns %NULL if called before the response
 * is received from the server. You can connect to notify::response
 * signal to be notified when the response is received.
 *
 * Returns: (transfer none): the #WebKitURIResponse, or %NULL if
 *     the response hasn't been received yet.
 */
WebKitURIResponse* webkit_web_resource_get_response(WebKitWebResource* resource)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_RESOURCE(resource), 0);

    return resource->priv->response.get();
}

struct ResourceGetDataAsyncData {
    RefPtr<API::Data> webData;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(ResourceGetDataAsyncData)

static void resourceDataCallback(API::Data* wkData, GTask* task)
{
    if (!wkData) {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED, _("Operation was cancelled"));
        return;
    }

    ResourceGetDataAsyncData* data = static_cast<ResourceGetDataAsyncData*>(g_task_get_task_data(task));
    data->webData = wkData;
    if (!wkData->bytes())
        data->webData = API::Data::create(reinterpret_cast<const unsigned char*>(""), 1);
    g_task_return_boolean(task, TRUE);
}

/**
 * webkit_web_resource_get_data:
 * @resource: a #WebKitWebResource
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously get the raw data for @resource.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_web_resource_get_data_finish() to get the result of the operation.
 */
void webkit_web_resource_get_data(WebKitWebResource* resource, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_RESOURCE(resource));

    GRefPtr<GTask> task = adoptGRef(g_task_new(resource, cancellable, callback, userData));
    g_task_set_task_data(task.get(), createResourceGetDataAsyncData(), reinterpret_cast<GDestroyNotify>(destroyResourceGetDataAsyncData));
    if (resource->priv->isMainResource)
        resource->priv->frame->getMainResourceData([task = WTFMove(task)](API::Data* data) {
            resourceDataCallback(data, task.get());
        });
    else {
        String url = String::fromUTF8(resource->priv->uri.data());
        resource->priv->frame->getResourceData(API::URL::create(url).ptr(), [task = WTFMove(task)](API::Data* data) {
            resourceDataCallback(data, task.get());
        });
    }
}

/**
 * webkit_web_resource_get_data_finish:
 * @resource: a #WebKitWebResource
 * @result: a #GAsyncResult
 * @length: (out) (allow-none): return location for the length of the resource data
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_resource_get_data().
 *
 * Returns: (transfer full) (array length=length) (element-type guint8): a
 *    string with the data of @resource, or %NULL in case of error. if @length
 *    is not %NULL, the size of the data will be assigned to it.
 */
guchar* webkit_web_resource_get_data_finish(WebKitWebResource* resource, GAsyncResult* result, gsize* length, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_RESOURCE(resource), nullptr);
    g_return_val_if_fail(g_task_is_valid(result, resource), nullptr);

    GTask* task = G_TASK(result);
    if (!g_task_propagate_boolean(task, error))
        return nullptr;

    ResourceGetDataAsyncData* data = static_cast<ResourceGetDataAsyncData*>(g_task_get_task_data(task));
    if (length)
        *length = data->webData->size();

    auto* bytes = data->webData->bytes();
    if (!bytes || !data->webData->size())
        return nullptr;

    auto* returnValue = g_malloc(data->webData->size());
    memcpy(returnValue, bytes, data->webData->size());
    return static_cast<guchar*>(returnValue);
}
