/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "WebKitWebView.h"

#include "WebKitWebContextPrivate.h"
#include "WebKitWebLoaderClient.h"
#include "WebKitWebLoaderClientPrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitPrivate.h"
#include "WebPageProxy.h"
#include <WebCore/DragIcon.h>
#include <WebKit2/WKBase.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKURL.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;
using namespace WebCore;

enum {
    PROP_0,

    PROP_WEB_CONTEXT,
    PROP_ESTIMATED_LOAD_PROGRESS
};

struct _WebKitWebViewPrivate {
    WebKitWebContext* context;
    CString customTextEncoding;
    double estimatedLoadProgress;

    GRefPtr<WebKitWebLoaderClient> loaderClient;
};

G_DEFINE_TYPE(WebKitWebView, webkit_web_view, WEBKIT_TYPE_WEB_VIEW_BASE)

static void webkitWebViewConstructed(GObject* object)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);
    WebKitWebViewPrivate* priv = webView->priv;

    webkitWebViewBaseCreateWebPage(WEBKIT_WEB_VIEW_BASE(webView), webkitWebContextGetWKContext(priv->context), 0);

    static GRefPtr<WebKitWebLoaderClient> defaultLoaderClient = adoptGRef(WEBKIT_WEB_LOADER_CLIENT(g_object_new(WEBKIT_TYPE_WEB_LOADER_CLIENT, NULL)));
    webkit_web_view_set_loader_client(webView, defaultLoaderClient.get());
}

static void webkitWebViewSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

    switch (propId) {
    case PROP_WEB_CONTEXT:
        webView->priv->context = WEBKIT_WEB_CONTEXT(g_value_get_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebViewGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

    switch (propId) {
    case PROP_WEB_CONTEXT:
        g_value_take_object(value, webView->priv->context);
        break;
    case PROP_ESTIMATED_LOAD_PROGRESS:
        g_value_set_double(value, webkit_web_view_get_estimated_load_progress(webView));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebViewFinalize(GObject* object)
{
    WEBKIT_WEB_VIEW(object)->priv->~WebKitWebViewPrivate();
    G_OBJECT_CLASS(webkit_web_view_parent_class)->finalize(object);
}

static void webkit_web_view_init(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(webView, WEBKIT_TYPE_WEB_VIEW, WebKitWebViewPrivate);
    webView->priv = priv;
    new (priv) WebKitWebViewPrivate();
}

static void webkit_web_view_class_init(WebKitWebViewClass* webViewClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(webViewClass);

    gObjectClass->constructed = webkitWebViewConstructed;
    gObjectClass->set_property = webkitWebViewSetProperty;
    gObjectClass->get_property = webkitWebViewGetProperty;
    gObjectClass->finalize = webkitWebViewFinalize;

    g_type_class_add_private(webViewClass, sizeof(WebKitWebViewPrivate));

    /**
     * WebKitWebView:web-context:
     *
     * The #WebKitWebContext of the view.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_WEB_CONTEXT,
                                    g_param_spec_object("web-context",
                                                        "Web Context",
                                                        "The web context for the view",
                                                        WEBKIT_TYPE_WEB_CONTEXT,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    /**
     * WebKitWebView:estimated-load-progress:
     *
     * An estimate of the percent completion for the current loading operation.
     * This value will range from 0.0 to 1.0 and, once a load completes,
     * will remain at 1.0 until a new load starts, at which point it
     * will be reset to 0.0.
     * The value is an estimate based on the total number of bytes expected
     * to be received for a document, including all its possible subresources
     * and child documents.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ESTIMATED_LOAD_PROGRESS,
                                    g_param_spec_double("estimated-load-progress",
                                                        "Estimated Load Progress",
                                                        "An estimate of the percent completion for a document load",
                                                        0.0, 1.0, 0.0,
                                                        WEBKIT_PARAM_READABLE));
}

void webkitWebViewSetEstimatedLoadProgress(WebKitWebView* webView, double estimatedLoadProgress)
{
    if (webView->priv->estimatedLoadProgress == estimatedLoadProgress)
        return;
    webView->priv->estimatedLoadProgress = estimatedLoadProgress;
    g_object_notify(G_OBJECT(webView), "estimated-load-progress");
}

/**
 * webkit_web_view_new:
 *
 * Creates a new #WebKitWebView with the default #WebKitWebContext.
 * See also webkit_web_view_new_with_context().
 *
 * Returns: The newly created #WebKitWebView widget
 */
GtkWidget* webkit_web_view_new()
{
    return webkit_web_view_new_with_context(webkit_web_context_get_default());
}

/**
 * webkit_web_view_new_with_context:
 * @context: the #WebKitWebContext to be used by the #WebKitWebView
 *
 * Creates a new #WebKitWebView with the given #WebKitWebContext.
 *
 * Returns: The newly created #WebKitWebView widget
 */
GtkWidget* webkit_web_view_new_with_context(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), 0);

    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", context, NULL));
}

/**
 * webkit_web_view_get_context:
 * @web_view: a #WebKitWebView
 *
 * Gets the web context of @web_view.
 *
 * Returns: (transfer none): the #WebKitWebContext of the view
 */
WebKitWebContext* webkit_web_view_get_context(WebKitWebView *webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->context;
}

/**
 * webkit_web_view_get_loader_client:
 * @web_view: a #WebKitWebView
 *
 * Returns the #WebKitWebLoaderClient of @web_view. You can use it
 * to monitor the status of load operations happening on @web_view.
 *
 * Returns: (transfer none): the #WebKitWebLoaderClient of @web_view.
 */
WebKitWebLoaderClient* webkit_web_view_get_loader_client(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->loaderClient.get();
}

/**
 * webkit_web_view_set_loader_client:
 * @web_view: a #WebKitWebView
 * @loader_client: a #WebKitWebLoaderClient
 *
 * Sets the #WebKitWebLoaderClient that the view will use during
 * load operations.
 */
void webkit_web_view_set_loader_client(WebKitWebView* webView, WebKitWebLoaderClient* loaderClient)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_WEB_LOADER_CLIENT(loaderClient));

    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->loaderClient.get() == loaderClient)
        return;

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    webkitWebLoaderClientAttachLoaderClientToPage(loaderClient, toAPI(page));
    priv->loaderClient = loaderClient;
}

/**
 * webkit_web_view_load_uri:
 * @web_view: a #WebKitWebView
 * @uri: an URI string
 *
 * Requests loading of the specified URI string.
 * You can monitor the status of the load operation using the
 * #WebKitWebLoaderClient of @web_view. See webkit_web_view_get_loader_client().
 */
void webkit_web_view_load_uri(WebKitWebView* webView, const gchar* uri)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(uri);

    WKRetainPtr<WKURLRef> url(AdoptWK, WKURLCreateWithUTF8CString(uri));
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKPageLoadURL(toAPI(page), url.get());
}

/**
 * webkit_web_view_load_alternate_html:
 * @web_view: a #WebKitWebView
 * @content: the alternate content to display as the main page of the @web_view
 * @base_uri: the base URI for relative locations
 * @unreachable_uri: the URI for the alternate page content
 *
 * Request loading of an alternate content for a URI that is unreachable. This allows clients
 * to display page-loading errors in the #WebKitWebView itself. This is typically called from
 * #WebKitWebLoaderClient::provisional-load-failed or #WebKitWebLoaderClient::load-failed
 * signals.
 * When called from those signals this method will preserve the back-forward list. The URI passed in
 * @base_uri has to be an absolute URI.
 * You can monitor the status of the load operation using the
 * #WebKitWebLoaderClient of @web_view. See webkit_web_view_get_loader_client().
 */
void webkit_web_view_load_alternate_html(WebKitWebView* webView, const gchar* content, const gchar* baseURI, const gchar* unreachableURI)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(content);

    WKRetainPtr<WKStringRef> htmlString(AdoptWK, WKStringCreateWithUTF8CString(content));
    WKRetainPtr<WKURLRef> baseURL = baseURI ? adoptWK(WKURLCreateWithUTF8CString(baseURI)) : 0;
    WKRetainPtr<WKURLRef> unreachableURL = unreachableURI ? adoptWK(WKURLCreateWithUTF8CString(unreachableURI)) : 0;
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKPageLoadAlternateHTMLString(toAPI(page), htmlString.get(), baseURL.get(), unreachableURL.get());
}

/**
 * webkit_web_view_reload:
 * @web_view: a #WebKitWebView
 *
 * Reloads the current contents of @web_view.
 * See also webkit_web_view_reload_bypass_cache().
 */
void webkit_web_view_reload(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WKPageReload(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
}

/**
 * webkit_web_view_reload_bypass_cache:
 * @web_view: a #WebKitWebView
 *
 * Reloads the current contents of @web_view without
 * using any cached data.
 */
void webkit_web_view_reload_bypass_cache(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WKPageReloadFromOrigin(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
}

/**
 * webkit_web_view_stop_loading:
 * @web_view: a #WebKitWebView
 *
 * Stops any ongoing loading operation in @web_view.
 * This method does nothing if no content is being loaded.
 * If there is a loading operation in progress, it will be cancelled and
 * #WebKitWebLoaderClient::provisional-load-failed or
 * #WebKitWebLoaderClient::load-failed will be emitted on the current
 * #WebKitWebLoaderClient with %WEBKIT_NETWORK_ERROR_CANCELLED error.
 * See also webkit_web_view_get_loader_client().
 */
void webkit_web_view_stop_loading(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WKPageStopLoading(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))));
}

/**
 * webkit_web_view_go_back:
 * @web_view: a #WebKitWebView
 *
 * Loads the previous history item.
 * You can monitor the status of the load operation using the
 * #WebKitWebLoaderClient of @web_view. See webkit_web_view_get_loader_client().
 */
void webkit_web_view_go_back(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKPageGoBack(toAPI(page));
}

/**
 * webkit_web_view_go_forward:
 * @web_view: a #WebKitWebView
 *
 * Loads the next history item.
 * You can monitor the status of the load operation using the
 * #WebKitWebLoaderClient of @web_view. See webkit_web_view_get_loader_client().
 */
void webkit_web_view_go_forward(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKPageGoForward(toAPI(page));
}

/**
 * webkit_web_view_get_custom_charset:
 * @web_view: a #WebKitWebView
 *
 * Returns the current custom character encoding name of @web_view.
 *
 * Returns: the current custom character encoding name or %NULL if no
 *    custom character encoding has been set.
 */
const gchar* webkit_web_view_get_custom_charset(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKRetainPtr<WKStringRef> wkCustomEncoding(AdoptWK, WKPageCopyCustomTextEncodingName(toAPI(page)));
    if (WKStringIsEmpty(wkCustomEncoding.get()))
        return 0;

    webView->priv->customTextEncoding = toImpl(wkCustomEncoding.get())->string().utf8();
    return webView->priv->customTextEncoding.data();
}

/**
 * webkit_web_view_set_custom_charset:
 * @web_view: a #WebKitWebView
 * @charset: (allow-none): a character encoding name or %NULL
 *
 * Sets the current custom character encoding override of @web_view. The custom
 * character encoding will override any text encoding detected via HTTP headers or
 * META tags. Calling this method will stop any current load operation and reload the
 * current page. Setting the custom character encoding to %NULL removes the character
 * encoding override.
 */
void webkit_web_view_set_custom_charset(WebKitWebView* webView, const gchar* charset)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    WKRetainPtr<WKStringRef> wkEncodingName = charset ? adoptWK(WKStringCreateWithUTF8CString(charset)) : 0;
    WKPageSetCustomTextEncodingName(toAPI(page), wkEncodingName.get());
}

/*
 * webkit_web_view_get_estimated_load_progress:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of #WebKitWebView:estimated-load-progress.
 * You can monitor the estimated progress of a load operation by
 * connecting to the ::notify signal of @web_view.
 *
 * Returns: an estimate of the of the percent complete for a document
 *     load as a range from 0.0 to 1.0.
 */
gdouble webkit_web_view_get_estimated_load_progress(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    return webView->priv->estimatedLoadProgress;
}
