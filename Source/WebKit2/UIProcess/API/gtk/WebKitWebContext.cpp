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
#include "WebKitWebContext.h"

#include "WebContext.h"
#include "WebKitDownloadClient.h"
#include "WebKitDownloadPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitWebContextPrivate.h"
#include <wtf/HashMap.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;

enum {
    DOWNLOAD_STARTED,

    LAST_SIGNAL
};

struct _WebKitWebContextPrivate {
    WKRetainPtr<WKContextRef> context;
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(WebKitWebContext, webkit_web_context, G_TYPE_OBJECT)

static void webkitWebContextFinalize(GObject* object)
{
    WEBKIT_WEB_CONTEXT(object)->priv->~WebKitWebContextPrivate();
    G_OBJECT_CLASS(webkit_web_context_parent_class)->finalize(object);
}

static void webkit_web_context_init(WebKitWebContext* webContext)
{
    WebKitWebContextPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(webContext, WEBKIT_TYPE_WEB_CONTEXT, WebKitWebContextPrivate);
    webContext->priv = priv;
    new (priv) WebKitWebContextPrivate();
}

static void webkit_web_context_class_init(WebKitWebContextClass* webContextClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(webContextClass);
    gObjectClass->finalize = webkitWebContextFinalize;

    /**
     * WebKitWebContext::download-started:
     * @context: the #WebKitWebContext
     * @download: the #WebKitDownload associated with this event
     *
     * This signal is emitted when a new download request is made.
     */
    signals[DOWNLOAD_STARTED] =
        g_signal_new("download-started",
                     G_TYPE_FROM_CLASS(gObjectClass),
                     G_SIGNAL_RUN_LAST,
                     0, 0, 0,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE, 1,
                     WEBKIT_TYPE_DOWNLOAD);

    g_type_class_add_private(webContextClass, sizeof(WebKitWebContextPrivate));
}

static gpointer createDefaultWebContext(gpointer)
{
    WebKitWebContext* webContext = WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, NULL));
    webContext->priv->context = WKContextGetSharedProcessContext();
    WKContextSetCacheModel(webContext->priv->context.get(), kWKCacheModelPrimaryWebBrowser);
    attachDownloadClientToContext(webContext);

    return webContext;
}

/**
 * webkit_web_context_get_default:
 *
 * Gets the default web context
 *
 * Returns: (transfer none) a #WebKitWebContext
 */
WebKitWebContext* webkit_web_context_get_default(void)
{
    static GOnce onceInit = G_ONCE_INIT;
    return WEBKIT_WEB_CONTEXT(g_once(&onceInit, createDefaultWebContext, 0));
}

/**
 * webkit_web_context_set_cache_model:
 * @context: the #WebKitWebContext
 * @cache_model: a #WebKitCacheModel
 *
 * Specifies a usage model for WebViews, which WebKit will use to
 * determine its caching behavior. All web views follow the cache
 * model. This cache model determines the RAM and disk space to use
 * for caching previously viewed content .
 *
 * Research indicates that users tend to browse within clusters of
 * documents that hold resources in common, and to revisit previously
 * visited documents. WebKit and the frameworks below it include
 * built-in caches that take advantage of these patterns,
 * substantially improving document load speed in browsing
 * situations. The WebKit cache model controls the behaviors of all of
 * these caches, including various WebCore caches.
 *
 * Browsers can improve document load speed substantially by
 * specifying WEBKIT_CACHE_MODEL_WEB_BROWSER. Applications without a
 * browsing interface can reduce memory usage substantially by
 * specifying WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER. The default value is
 * WEBKIT_CACHE_MODEL_WEB_BROWSER.
 *
 */
void webkit_web_context_set_cache_model(WebKitWebContext* context, WebKitCacheModel model)
{
    WKCacheModel cacheModel;

    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    switch (model) {
    case WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER:
        cacheModel = kWKCacheModelDocumentViewer;
        break;
    case WEBKIT_CACHE_MODEL_WEB_BROWSER:
        cacheModel = kWKCacheModelPrimaryWebBrowser;
        break;
    case WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER:
        cacheModel = kWKCacheModelDocumentBrowser;
        break;
    default:
        g_assert_not_reached();
    }
    WebKitWebContextPrivate* priv = context->priv;
    if (cacheModel != WKContextGetCacheModel(priv->context.get()))
        WKContextSetCacheModel(priv->context.get(), cacheModel);
}

/**
 * webkit_web_context_get_cache_model:
 * @context: the #WebKitWebContext
 *
 * Returns the current cache model. For more information about this
 * value check the documentation of the function
 * webkit_web_context_set_cache_model().
 *
 * Returns: the current #WebKitCacheModel
 */
WebKitCacheModel webkit_web_context_get_cache_model(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), WEBKIT_CACHE_MODEL_WEB_BROWSER);

    WebKitWebContextPrivate* priv = context->priv;
    switch (WKContextGetCacheModel(priv->context.get())) {
    case kWKCacheModelDocumentViewer:
        return WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER;
    case kWKCacheModelPrimaryWebBrowser:
        return WEBKIT_CACHE_MODEL_WEB_BROWSER;
    case kWKCacheModelDocumentBrowser:
        return WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER;
    default:
        g_assert_not_reached();
    }

    return WEBKIT_CACHE_MODEL_WEB_BROWSER;
}

typedef HashMap<WKDownloadRef, GRefPtr<WebKitDownload> > DownloadsMap;

static DownloadsMap& downloadsMap()
{
    DEFINE_STATIC_LOCAL(DownloadsMap, downloads, ());
    return downloads;
}

/**
 * webkit_web_context_download_uri:
 * @context: a #WebKitWebContext
 * @uri: the URI to download
 *
 * Requests downloading of the specified URI string.
 *
 * Returns: (transfer full): a new #WebKitDownload representing the
 *    the download operation.
 */
WebKitDownload* webkit_web_context_download_uri(WebKitWebContext* context, const gchar* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), 0);
    g_return_val_if_fail(uri, 0);

    WebKitWebContextPrivate* priv = context->priv;
    WKRetainPtr<WKURLRef> wkURL(AdoptWK, WKURLCreateWithUTF8CString(uri));
    WKRetainPtr<WKURLRequestRef> wkRequest(AdoptWK, WKURLRequestCreateWithWKURL(wkURL.get()));
    WKRetainPtr<WKDownloadRef> wkDownload = WKContextDownloadURLRequest(priv->context.get(), wkRequest.get());
    WebKitDownload* download = webkitDownloadCreate(wkDownload.get());
    downloadsMap().set(wkDownload.get(), download);
    return download;
}

WebKitDownload* webkitWebContextGetOrCreateDownload(WKDownloadRef wkDownload)
{
    GRefPtr<WebKitDownload> download = downloadsMap().get(wkDownload);
    if (download)
        return download.get();

    download = adoptGRef(webkitDownloadCreate(wkDownload));
    downloadsMap().set(wkDownload, download.get());
    return download.get();
}

void webkitWebContextRemoveDownload(WKDownloadRef wkDownload)
{
    downloadsMap().remove(wkDownload);
}

void webkitWebContextDownloadStarted(WebKitWebContext* context, WebKitDownload* download)
{
    g_signal_emit(context, signals[DOWNLOAD_STARTED], 0, download);
}

WKContextRef webkitWebContextGetWKContext(WebKitWebContext* context)
{
    g_assert(WEBKIT_IS_WEB_CONTEXT(context));

    return context->priv->context.get();
}

