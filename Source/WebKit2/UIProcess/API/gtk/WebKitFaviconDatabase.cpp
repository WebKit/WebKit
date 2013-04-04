/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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
#include "WebKitFaviconDatabase.h"

#include "WebKitFaviconDatabasePrivate.h"
#include "WebKitMarshal.h"
#include "WebKitPrivate.h"
#include <WebCore/FileSystem.h>
#include <WebCore/Image.h>
#include <WebCore/IntSize.h>
#include <WebCore/RefPtrCairo.h>
#include <glib/gi18n-lib.h>
#include <wtf/MainThread.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * SECTION: WebKitFaviconDatabase
 * @Short_description: A WebKit favicon database
 * @Title: WebKitFaviconDatabase
 *
 * #WebKitFaviconDatabase provides access to the icons associated with
 * web sites.
 *
 * WebKit will automatically look for available icons in &lt;link&gt;
 * elements on opened pages as well as an existing favicon.ico and
 * load the images found into a memory cache if possible. That cache
 * is frozen to an on-disk database for persistence.
 *
 * If #WebKitSettings:enable-private-browsing is %TRUE, new icons
 * won't be added to the on-disk database and no existing icons will
 * be deleted from it. Nevertheless, WebKit will still store them in
 * the in-memory cache during the current execution.
 *
 */

enum {
    FAVICON_CHANGED,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

typedef Vector<GRefPtr<GSimpleAsyncResult> > PendingIconRequestVector;
typedef HashMap<String, PendingIconRequestVector*> PendingIconRequestMap;

struct _WebKitFaviconDatabasePrivate {
    RefPtr<WebIconDatabase> iconDatabase;
    PendingIconRequestMap pendingIconRequests;
    HashMap<String, String> pageURLToIconURLMap;
};

WEBKIT_DEFINE_TYPE(WebKitFaviconDatabase, webkit_favicon_database, G_TYPE_OBJECT)

static void webkitFaviconDatabaseDispose(GObject* object)
{
    WebKitFaviconDatabase* database = WEBKIT_FAVICON_DATABASE(object);

    WebKitFaviconDatabasePrivate* priv = database->priv;
    if (priv->iconDatabase->isOpen())
        priv->iconDatabase->close();

    G_OBJECT_CLASS(webkit_favicon_database_parent_class)->dispose(object);
}

static void webkit_favicon_database_class_init(WebKitFaviconDatabaseClass* faviconDatabaseClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(faviconDatabaseClass);
    gObjectClass->dispose = webkitFaviconDatabaseDispose;

    /**
     * WebKitFaviconDatabase::favicon-changed:
     * @database: the object on which the signal is emitted
     * @page_uri: the URI of the Web page containing the icon
     * @favicon_uri: the URI of the favicon
     *
     * This signal is emitted when the favicon URI of @page_uri has
     * been changed to @favicon_uri in the database. You can connect
     * to this signal and call webkit_favicon_database_get_favicon()
     * to get the favicon. If you are interested in the favicon of a
     * #WebKitWebView it's easier to use the #WebKitWebView:favicon
     * property. See webkit_web_view_get_favicon() for more details.
     */
    signals[FAVICON_CHANGED] =
        g_signal_new(
            "favicon-changed",
            G_TYPE_FROM_CLASS(faviconDatabaseClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            webkit_marshal_VOID__STRING_STRING,
            G_TYPE_NONE, 2,
            G_TYPE_STRING,
            G_TYPE_STRING);
}

struct GetFaviconSurfaceAsyncData {
    ~GetFaviconSurfaceAsyncData()
    {
        if (shouldReleaseIconForPageURL)
            faviconDatabase->priv->iconDatabase->releaseIconForPageURL(pageURL);
    }

    GRefPtr<WebKitFaviconDatabase> faviconDatabase;
    String pageURL;
    RefPtr<cairo_surface_t> icon;
    GRefPtr<GCancellable> cancellable;
    bool shouldReleaseIconForPageURL;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(GetFaviconSurfaceAsyncData)

static PassRefPtr<cairo_surface_t> getIconSurfaceSynchronously(WebKitFaviconDatabase* database, const String& pageURL, GError** error)
{
    ASSERT(isMainThread());

    // The exact size we pass is irrelevant to the iconDatabase code.
    // We must pass something greater than 0x0 to get an icon.
    WebCore::Image* iconImage = database->priv->iconDatabase->imageForPageURL(pageURL, WebCore::IntSize(1, 1));
    if (!iconImage) {
        g_set_error(error, WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN, _("Unknown favicon for page %s"), pageURL.utf8().data());
        return 0;
    }

    RefPtr<cairo_surface_t> surface = iconImage->nativeImageForCurrentFrame();
    if (!surface) {
        g_set_error(error, WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_NOT_FOUND, _("Page %s does not have a favicon"), pageURL.utf8().data());
        return 0;
    }

    return surface.release();
}

static void deletePendingIconRequests(WebKitFaviconDatabase* database, PendingIconRequestVector* requests, const String& pageURL)
{
    database->priv->pendingIconRequests.remove(pageURL);
    delete requests;
}

static void processPendingIconsForPageURL(WebKitFaviconDatabase* database, const String& pageURL)
{
    PendingIconRequestVector* pendingIconRequests = database->priv->pendingIconRequests.get(pageURL);
    if (!pendingIconRequests)
        return;

    GOwnPtr<GError> error;
    RefPtr<cairo_surface_t> icon = getIconSurfaceSynchronously(database, pageURL, &error.outPtr());

    for (size_t i = 0; i < pendingIconRequests->size(); ++i) {
        GSimpleAsyncResult* result = pendingIconRequests->at(i).get();
        GetFaviconSurfaceAsyncData* data = static_cast<GetFaviconSurfaceAsyncData*>(g_simple_async_result_get_op_res_gpointer(result));
        if (!g_cancellable_is_cancelled(data->cancellable.get())) {
            if (error)
                g_simple_async_result_take_error(result, error.release());
            else {
                data->icon = icon;
                data->shouldReleaseIconForPageURL = false;
            }
        }

        g_simple_async_result_complete(result);
    }
    deletePendingIconRequests(database, pendingIconRequests, pageURL);
}

static void didChangeIconForPageURLCallback(WKIconDatabaseRef wkIconDatabase, WKURLRef wkPageURL, const void* clientInfo)
{
    WebKitFaviconDatabase* database = WEBKIT_FAVICON_DATABASE(clientInfo);
    if (!database->priv->iconDatabase->isUrlImportCompleted())
        return;

    // Wait until there's an icon record in the database for this page URL.
    String pageURL = toImpl(wkPageURL)->string();
    WebCore::Image* iconImage = database->priv->iconDatabase->imageForPageURL(pageURL, WebCore::IntSize(1, 1));
    if (!iconImage || iconImage->isNull())
        return;

    String currentIconURL;
    database->priv->iconDatabase->synchronousIconURLForPageURL(pageURL, currentIconURL);
    const String& iconURL = database->priv->pageURLToIconURLMap.get(pageURL);
    if (iconURL == currentIconURL)
        return;

    database->priv->pageURLToIconURLMap.set(pageURL, currentIconURL);
    g_signal_emit(database, signals[FAVICON_CHANGED], 0, pageURL.utf8().data(), currentIconURL.utf8().data());
}

static void iconDataReadyForPageURLCallback(WKIconDatabaseRef wkIconDatabase, WKURLRef wkPageURL, const void* clientInfo)
{
    ASSERT(isMainThread());
    processPendingIconsForPageURL(WEBKIT_FAVICON_DATABASE(clientInfo), toImpl(wkPageURL)->string());
}

WebKitFaviconDatabase* webkitFaviconDatabaseCreate(WebIconDatabase* iconDatabase)
{
    WebKitFaviconDatabase* faviconDatabase = WEBKIT_FAVICON_DATABASE(g_object_new(WEBKIT_TYPE_FAVICON_DATABASE, NULL));
    faviconDatabase->priv->iconDatabase = iconDatabase;

    WKIconDatabaseClient wkIconDatabaseClient = {
        kWKIconDatabaseClientCurrentVersion,
        faviconDatabase, // clientInfo
        didChangeIconForPageURLCallback,
        0, // didRemoveAllIconsCallback
        iconDataReadyForPageURLCallback,
    };
    WKIconDatabaseSetIconDatabaseClient(toAPI(iconDatabase), &wkIconDatabaseClient);
    return faviconDatabase;
}

static PendingIconRequestVector* getOrCreatePendingIconRequests(WebKitFaviconDatabase* database, const String& pageURL)
{
    PendingIconRequestVector* icons = database->priv->pendingIconRequests.get(pageURL);
    if (!icons) {
        icons = new PendingIconRequestVector;
        database->priv->pendingIconRequests.set(pageURL, icons);
    }

    return icons;
}

static void setErrorForAsyncResult(GSimpleAsyncResult* result, WebKitFaviconDatabaseError error, const String& pageURL = String())
{
    ASSERT(result);

    switch (error) {
    case WEBKIT_FAVICON_DATABASE_ERROR_NOT_INITIALIZED:
        g_simple_async_result_set_error(result, WEBKIT_FAVICON_DATABASE_ERROR, error, _("Favicons database not initialized yet"));
        break;

    case WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_NOT_FOUND:
        g_simple_async_result_set_error(result, WEBKIT_FAVICON_DATABASE_ERROR, error, _("Page %s does not have a favicon"), pageURL.utf8().data());
        break;

    case WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN:
        g_simple_async_result_set_error(result, WEBKIT_FAVICON_DATABASE_ERROR, error, _("Unknown favicon for page %s"), pageURL.utf8().data());
        break;

    default:
        ASSERT_NOT_REACHED();
    }
}

GQuark webkit_favicon_database_error_quark(void)
{
    return g_quark_from_static_string("WebKitFaviconDatabaseError");
}

/**
 * webkit_favicon_database_get_favicon:
 * @database: a #WebKitFaviconDatabase
 * @page_uri: URI of the page for which we want to retrieve the favicon
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: (scope async): A #GAsyncReadyCallback to call when the request is
 *            satisfied or %NULL if you don't care about the result.
 * @user_data: (closure): The data to pass to @callback.
 *
 * Asynchronously obtains a #cairo_surface_t of the favicon for the
 * given page URI. It returns the cached icon if it's in the database
 * asynchronously waiting for the icon to be read from the database.
 *
 * This is an asynchronous method. When the operation is finished, callback will
 * be invoked. You can then call webkit_favicon_database_get_favicon_finish()
 * to get the result of the operation.
 */
void webkit_favicon_database_get_favicon(WebKitFaviconDatabase* database, const gchar* pageURI, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_FAVICON_DATABASE(database));
    g_return_if_fail(pageURI);

    GRefPtr<GSimpleAsyncResult> result = adoptGRef(g_simple_async_result_new(G_OBJECT(database), callback, userData, reinterpret_cast<gpointer>(webkit_favicon_database_get_favicon)));
    g_simple_async_result_set_check_cancellable(result.get(), cancellable);

    GetFaviconSurfaceAsyncData* data = createGetFaviconSurfaceAsyncData();
    g_simple_async_result_set_op_res_gpointer(result.get(), data, reinterpret_cast<GDestroyNotify>(destroyGetFaviconSurfaceAsyncData));
    data->faviconDatabase = database;
    data->pageURL = String::fromUTF8(pageURI);
    data->cancellable = cancellable;

    WebKitFaviconDatabasePrivate* priv = database->priv;
    WebIconDatabase* iconDatabaseImpl = priv->iconDatabase.get();
    if (!iconDatabaseImpl->isOpen()) {
        setErrorForAsyncResult(result.get(), WEBKIT_FAVICON_DATABASE_ERROR_NOT_INITIALIZED);
        g_simple_async_result_complete_in_idle(result.get());
        return;
    }

    if (data->pageURL.isEmpty() || data->pageURL.startsWith("about:")) {
        setErrorForAsyncResult(result.get(), WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_NOT_FOUND, data->pageURL);
        g_simple_async_result_complete_in_idle(result.get());
        return;
    }

    priv->iconDatabase->retainIconForPageURL(data->pageURL);

    // We ask for the icon directly. If we don't get the icon data now,
    // we'll be notified later (even if the database is still importing icons).
    GOwnPtr<GError> error;
    data->icon = getIconSurfaceSynchronously(database, data->pageURL, &error.outPtr());
    if (data->icon) {
        g_simple_async_result_complete_in_idle(result.get());
        return;
    }

    // At this point we still don't know whether we will get a valid icon for pageURL.
    data->shouldReleaseIconForPageURL = true;

    if (g_error_matches(error.get(), WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_NOT_FOUND)) {
        g_simple_async_result_take_error(result.get(), error.release());
        g_simple_async_result_complete_in_idle(result.get());
        return;
    }

    // If there's not a valid icon, but there's an iconURL registered,
    // or it's still not registered but the import process hasn't
    // finished yet, we need to wait for iconDataReadyForPage to be
    // called before making and informed decision.
    String iconURLForPageURL;
    iconDatabaseImpl->synchronousIconURLForPageURL(data->pageURL, iconURLForPageURL);
    if (!iconURLForPageURL.isEmpty() || !iconDatabaseImpl->isUrlImportCompleted()) {
        PendingIconRequestVector* icons = getOrCreatePendingIconRequests(database, data->pageURL);
        ASSERT(icons);
        icons->append(result);
        return;
    }

    setErrorForAsyncResult(result.get(), WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN, data->pageURL);
    g_simple_async_result_complete_in_idle(result.get());
}

/**
 * webkit_favicon_database_get_favicon_finish:
 * @database: a #WebKitFaviconDatabase
 * @result: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to webkit_favicon_database_get_favicon()
 * @error: (allow-none): Return location for error or %NULL.
 *
 * Finishes an operation started with webkit_favicon_database_get_favicon().
 *
 * Returns: (transfer full): a new reference to a #cairo_surface_t, or
 * %NULL in case of error.
 */
cairo_surface_t* webkit_favicon_database_get_favicon_finish(WebKitFaviconDatabase* database, GAsyncResult* result, GError** error)
{
    GSimpleAsyncResult* simpleResult = G_SIMPLE_ASYNC_RESULT(result);
    g_warn_if_fail(g_simple_async_result_get_source_tag(simpleResult) == webkit_favicon_database_get_favicon);

    if (g_simple_async_result_propagate_error(simpleResult, error))
        return 0;

    GetFaviconSurfaceAsyncData* data = static_cast<GetFaviconSurfaceAsyncData*>(g_simple_async_result_get_op_res_gpointer(simpleResult));
    ASSERT(data);
    return cairo_surface_reference(data->icon.get());
}

/**
 * webkit_favicon_database_get_favicon_uri:
 * @database: a #WebKitFaviconDatabase
 * @page_uri: URI of the page containing the icon
 *
 * Obtains the URI of the favicon for the given @page_uri.
 *
 * Returns: a newly allocated URI for the favicon, or %NULL if the
 * database doesn't have a favicon for @page_uri.
 */
gchar* webkit_favicon_database_get_favicon_uri(WebKitFaviconDatabase* database, const gchar* pageURL)
{
    g_return_val_if_fail(WEBKIT_IS_FAVICON_DATABASE(database), 0);
    g_return_val_if_fail(pageURL, 0);
    ASSERT(isMainThread());

    String iconURLForPageURL;
    database->priv->iconDatabase->synchronousIconURLForPageURL(String::fromUTF8(pageURL), iconURLForPageURL);
    if (iconURLForPageURL.isEmpty())
        return 0;

    return g_strdup(iconURLForPageURL.utf8().data());
}

/**
 * webkit_favicon_database_clear:
 * @database: a #WebKitFaviconDatabase
 *
 * Clears all icons from the database.
 */
void webkit_favicon_database_clear(WebKitFaviconDatabase* database)
{
    g_return_if_fail(WEBKIT_IS_FAVICON_DATABASE(database));

    database->priv->iconDatabase->removeAllIcons();
}
