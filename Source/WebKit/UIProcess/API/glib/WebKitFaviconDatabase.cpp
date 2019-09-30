/*
 * Copyright (C) 2012, 2017 Igalia S.L.
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

#include "IconDatabase.h"
#include "WebKitFaviconDatabasePrivate.h"
#include <WebCore/Image.h>
#include <WebCore/IntSize.h>
#include <WebCore/RefPtrCairo.h>
#include <WebCore/SharedBuffer.h>
#include <glib/gi18n-lib.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebKit;
using namespace WebCore;

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

struct _WebKitFaviconDatabasePrivate {
    RefPtr<IconDatabase> iconDatabase;
};

WEBKIT_DEFINE_TYPE(WebKitFaviconDatabase, webkit_favicon_database, G_TYPE_OBJECT)

static void webkit_favicon_database_class_init(WebKitFaviconDatabaseClass* faviconDatabaseClass)
{
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
    signals[FAVICON_CHANGED] = g_signal_new(
        "favicon-changed",
        G_TYPE_FROM_CLASS(faviconDatabaseClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 2,
        G_TYPE_STRING,
        G_TYPE_STRING);
}

WebKitFaviconDatabase* webkitFaviconDatabaseCreate()
{
    return WEBKIT_FAVICON_DATABASE(g_object_new(WEBKIT_TYPE_FAVICON_DATABASE, nullptr));
}

static bool webkitFaviconDatabaseIsOpen(WebKitFaviconDatabase* database)
{
    return !!database->priv->iconDatabase;
}

void webkitFaviconDatabaseOpen(WebKitFaviconDatabase* database, const String& path, bool isEphemeral)
{
    if (webkitFaviconDatabaseIsOpen(database))
        return;

    database->priv->iconDatabase = IconDatabase::create(path, isEphemeral ? IconDatabase::AllowDatabaseWrite::No : IconDatabase::AllowDatabaseWrite::Yes);
}

void webkitFaviconDatabaseClose(WebKitFaviconDatabase* database)
{
    database->priv->iconDatabase = nullptr;
}

#if PLATFORM(GTK)
void webkitFaviconDatabaseGetLoadDecisionForIcon(WebKitFaviconDatabase* database, const LinkIcon& icon, const String& pageURL, bool isEphemeral, Function<void(bool)>&& completionHandler)
{
    if (!webkitFaviconDatabaseIsOpen(database)) {
        completionHandler(false);
        return;
    }

    database->priv->iconDatabase->checkIconURLAndSetPageURLIfNeeded(icon.url.string(), pageURL,
        isEphemeral ? IconDatabase::AllowDatabaseWrite::No : IconDatabase::AllowDatabaseWrite::Yes,
            [database = GRefPtr<WebKitFaviconDatabase>(database), url = icon.url.string().isolatedCopy(), pageURL = pageURL.isolatedCopy(), completionHandler = WTFMove(completionHandler)](bool found, bool changed) {
            if (!webkitFaviconDatabaseIsOpen(database.get()))
                return;

            if (found && changed)
                g_signal_emit(database.get(), signals[FAVICON_CHANGED], 0, pageURL.utf8().data(), url.utf8().data());
            completionHandler(!found);
        });
}

void webkitFaviconDatabaseSetIconForPageURL(WebKitFaviconDatabase* database, const LinkIcon& icon, API::Data& iconData, const String& pageURL, bool isEphemeral)
{
    if (!webkitFaviconDatabaseIsOpen(database))
        return;

    database->priv->iconDatabase->setIconForPageURL(icon.url.string(), iconData.bytes(), iconData.size(), pageURL,
        isEphemeral ? IconDatabase::AllowDatabaseWrite::No : IconDatabase::AllowDatabaseWrite::Yes,
        [database = GRefPtr<WebKitFaviconDatabase>(database), url = icon.url.string().isolatedCopy(), pageURL = pageURL.isolatedCopy()](bool success) {
            if (!webkitFaviconDatabaseIsOpen(database.get()) || !success)
                return;

            g_signal_emit(database.get(), signals[FAVICON_CHANGED], 0, pageURL.utf8().data(), url.utf8().data());
        });
}
#endif

GQuark webkit_favicon_database_error_quark(void)
{
    return g_quark_from_static_string("WebKitFaviconDatabaseError");
}

#if PLATFORM(GTK)
void webkitFaviconDatabaseGetFaviconInternal(WebKitFaviconDatabase* database, const gchar* pageURI, bool isEphemeral, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    if (!webkitFaviconDatabaseIsOpen(database)) {
        g_task_report_new_error(database, callback, userData, 0,
            WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_NOT_INITIALIZED, _("Favicons database not initialized yet"));
        return;
    }

    if (g_str_has_prefix(pageURI, "about:")) {
        g_task_report_new_error(database, callback, userData, 0,
            WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_NOT_FOUND, _("Page %s does not have a favicon"), pageURI);
        return;
    }

    GRefPtr<GTask> task = adoptGRef(g_task_new(database, cancellable, callback, userData));
    WebKitFaviconDatabasePrivate* priv = database->priv;
    priv->iconDatabase->loadIconForPageURL(String::fromUTF8(pageURI), isEphemeral ? IconDatabase::AllowDatabaseWrite::No : IconDatabase::AllowDatabaseWrite::Yes,
        [task = WTFMove(task), pageURI = CString(pageURI)](RefPtr<cairo_surface_t>&& icon) {
            if (!icon) {
                g_task_return_new_error(task.get(), WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN,
                    _("Unknown favicon for page %s"), pageURI.data());
                return;
            }
            g_task_return_pointer(task.get(), icon.leakRef(), reinterpret_cast<GDestroyNotify>(cairo_surface_destroy));
        });
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
 *
 * You must call webkit_web_context_set_favicon_database_directory() for
 * the #WebKitWebContext associated with this #WebKitFaviconDatabase
 * before attempting to use this function; otherwise,
 * webkit_favicon_database_get_favicon_finish() will return
 * %WEBKIT_FAVICON_DATABASE_ERROR_NOT_INITIALIZED.
 */
void webkit_favicon_database_get_favicon(WebKitFaviconDatabase* database, const gchar* pageURI, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_FAVICON_DATABASE(database));
    g_return_if_fail(pageURI);

    webkitFaviconDatabaseGetFaviconInternal(database, pageURI, false, cancellable, callback, userData);
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
    g_return_val_if_fail(WEBKIT_IS_FAVICON_DATABASE(database), 0);
    g_return_val_if_fail(g_task_is_valid(result, database), 0);

    GTask* task = G_TASK(result);
    if (auto* icon = g_task_propagate_pointer(task, error))
        return static_cast<cairo_surface_t*>(icon);

    return nullptr;
}
#endif

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
    g_return_val_if_fail(WEBKIT_IS_FAVICON_DATABASE(database), nullptr);
    g_return_val_if_fail(pageURL, nullptr);
    ASSERT(RunLoop::isMain());

    if (!webkitFaviconDatabaseIsOpen(database))
        return nullptr;

    String iconURLForPageURL = database->priv->iconDatabase->iconURLForPageURL(String::fromUTF8(pageURL));
    if (iconURLForPageURL.isEmpty())
        return nullptr;

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

    if (!webkitFaviconDatabaseIsOpen(database))
        return;

    database->priv->iconDatabase->clear([] { });
}
