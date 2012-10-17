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

enum {
    ICON_READY,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

typedef Vector<GRefPtr<GSimpleAsyncResult> > PendingIconRequestVector;
typedef HashMap<String, PendingIconRequestVector*> PendingIconRequestMap;

struct _WebKitFaviconDatabasePrivate {
    RefPtr<WebIconDatabase> iconDatabase;
    PendingIconRequestMap pendingIconRequests;
};

G_DEFINE_TYPE(WebKitFaviconDatabase, webkit_favicon_database, G_TYPE_OBJECT)

static void webkit_favicon_database_init(WebKitFaviconDatabase* manager)
{
    WebKitFaviconDatabasePrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(manager, WEBKIT_TYPE_FAVICON_DATABASE, WebKitFaviconDatabasePrivate);
    manager->priv = priv;
    new (priv) WebKitFaviconDatabasePrivate();
}

static void webkitFaviconDatabaseDispose(GObject* object)
{
    WebKitFaviconDatabase* database = WEBKIT_FAVICON_DATABASE(object);

    WebKitFaviconDatabasePrivate* priv = database->priv;
    if (priv->iconDatabase->isOpen())
        priv->iconDatabase->close();

    G_OBJECT_CLASS(webkit_favicon_database_parent_class)->dispose(object);
}

static void webkitFaviconDatabaseFinalize(GObject* object)
{
    WebKitFaviconDatabase* database = WEBKIT_FAVICON_DATABASE(object);
    database->priv->~WebKitFaviconDatabasePrivate();
    G_OBJECT_CLASS(webkit_favicon_database_parent_class)->finalize(object);
}

static void webkit_favicon_database_class_init(WebKitFaviconDatabaseClass* faviconDatabaseClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(faviconDatabaseClass);
    gObjectClass->dispose = webkitFaviconDatabaseDispose;
    gObjectClass->finalize = webkitFaviconDatabaseFinalize;

    /**
     * WebKitFaviconDatabase::favicon-ready:
     * @database: the object on which the signal is emitted
     * @page_uri: the URI of the Web page containing the icon.
     *
     * This signal gets emitted when the favicon of @page_uri is
     * ready. This means that the favicon's data is ready to be used
     * by the application, either because it has been loaded from the
     * network, if it's the first time it gets retrieved, or because
     * it has been already imported from the icon database.
     */
    signals[ICON_READY] =
        g_signal_new("favicon-ready",
                     G_TYPE_FROM_CLASS(faviconDatabaseClass),
                     G_SIGNAL_RUN_LAST,
                     0, 0, 0,
                     g_cclosure_marshal_VOID__STRING,
                     G_TYPE_NONE, 1,
                     G_TYPE_STRING);

    g_type_class_add_private(faviconDatabaseClass, sizeof(WebKitFaviconDatabasePrivate));
}

struct GetFaviconSurfaceAsyncData {
    GRefPtr<WebKitFaviconDatabase> faviconDatabase;
    String pageURL;
    RefPtr<cairo_surface_t> icon;
    GRefPtr<GCancellable> cancellable;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(GetFaviconSurfaceAsyncData)

static cairo_surface_t* getIconSurfaceSynchronously(WebKitFaviconDatabase* database, const String& pageURL, GError** error)
{
    ASSERT(isMainThread());

    database->priv->iconDatabase->retainIconForPageURL(pageURL);

    // The exact size we pass is irrelevant to the iconDatabase code.
    // We must pass something greater than 0x0 to get an icon.
    WebCore::Image* iconImage = database->priv->iconDatabase->imageForPageURL(pageURL, WebCore::IntSize(1, 1));
    if (!iconImage) {
        g_set_error(error, WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN, _("Unknown favicon for page %s"), pageURL.utf8().data());
        database->priv->iconDatabase->releaseIconForPageURL(pageURL);
        return 0;
    }

    WebCore::NativeImagePtr icon = iconImage->nativeImageForCurrentFrame();
    if (!icon) {
        g_set_error(error, WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_NOT_FOUND, _("Page %s does not have a favicon"), pageURL.utf8().data());
        database->priv->iconDatabase->releaseIconForPageURL(pageURL);
        return 0;
    }

    return icon->surface();
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
            else
                data->icon = icon;
        }

        g_simple_async_result_complete(result);
    }
    deletePendingIconRequests(database, pendingIconRequests, pageURL);
}

static void iconDataReadyForPageURLCallback(WKIconDatabaseRef wkIconDatabase, WKURLRef wkPageURL, const void* clientInfo)
{
    ASSERT(isMainThread());

    WebKitFaviconDatabase* database = WEBKIT_FAVICON_DATABASE(clientInfo);
    String pageURL = toImpl(wkPageURL)->string();

    processPendingIconsForPageURL(database, pageURL);
    g_signal_emit(database, signals[ICON_READY], 0, pageURL.utf8().data());
}

WebKitFaviconDatabase* webkitFaviconDatabaseCreate(WebIconDatabase* iconDatabase)
{
    WebKitFaviconDatabase* faviconDatabase = WEBKIT_FAVICON_DATABASE(g_object_new(WEBKIT_TYPE_FAVICON_DATABASE, NULL));
    faviconDatabase->priv->iconDatabase = iconDatabase;

    WKIconDatabaseClient wkIconDatabaseClient = {
        kWKIconDatabaseClientCurrentVersion,
        faviconDatabase, // clientInfo
        0, // didChangeIconForPageURLCallback
        0, // didRemoveAllIconsCallback
        iconDataReadyForPageURLCallback,
    };
    WKIconDatabaseSetIconDatabaseClient(toAPI(iconDatabase), &wkIconDatabaseClient);
    return faviconDatabase;
}

cairo_surface_t* webkitFaviconDatabaseGetFavicon(WebKitFaviconDatabase* database, const CString& pageURL)
{
    ASSERT(WEBKIT_IS_FAVICON_DATABASE(database));
    ASSERT(!pageURL.isNull());

    cairo_surface_t* iconSurface = getIconSurfaceSynchronously(database, String::fromUTF8(pageURL.data()), 0);
    if (!iconSurface)
        return 0;

    return cairo_surface_reference(iconSurface);
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

    // We ask for the icon directly. If we don't get the icon data now,
    // we'll be notified later (even if the database is still importing icons).
    GOwnPtr<GError> error;
    data->icon = getIconSurfaceSynchronously(database, data->pageURL, &error.outPtr());
    if (data->icon) {
        g_simple_async_result_complete_in_idle(result.get());
        return;
    }

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
