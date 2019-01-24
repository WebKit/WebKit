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
#include "WebPreferences.h"
#include <WebCore/Image.h>
#include <WebCore/IntSize.h>
#include <WebCore/RefPtrCairo.h>
#include <WebCore/SharedBuffer.h>
#include <glib/gi18n-lib.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/SetForScope.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

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

typedef Vector<GRefPtr<GTask> > PendingIconRequestVector;
typedef HashMap<String, PendingIconRequestVector*> PendingIconRequestMap;

struct _WebKitFaviconDatabasePrivate {
    std::unique_ptr<IconDatabase> iconDatabase;
    Vector<std::pair<String, Function<void(bool)>>> pendingLoadDecisions;
    PendingIconRequestMap pendingIconRequests;
    HashMap<String, String> pageURLToIconURLMap;
    bool isURLImportCompleted;
    bool isSettingIcon;
};

WEBKIT_DEFINE_TYPE(WebKitFaviconDatabase, webkit_favicon_database, G_TYPE_OBJECT)

static void webkitFaviconDatabaseDispose(GObject* object)
{
    WebKitFaviconDatabase* database = WEBKIT_FAVICON_DATABASE(object);

    if (webkitFaviconDatabaseIsOpen(database))
        database->priv->iconDatabase->close();

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

#if PLATFORM(GTK)
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

static RefPtr<cairo_surface_t> getIconSurfaceSynchronously(WebKitFaviconDatabase* database, const String& pageURL, GError** error)
{
    ASSERT(RunLoop::isMain());

    // The exact size we pass is irrelevant to the iconDatabase code.
    // We must pass something greater than 0x0 to get an icon.
    auto iconData = database->priv->iconDatabase->synchronousIconForPageURL(pageURL, WebCore::IntSize(1, 1));
    if (iconData.second == IconDatabase::IsKnownIcon::No) {
        g_set_error(error, WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN, _("Unknown favicon for page %s"), pageURL.utf8().data());
        return nullptr;
    }

    if (!iconData.first) {
        g_set_error(error, WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_NOT_FOUND, _("Page %s does not have a favicon"), pageURL.utf8().data());
        return nullptr;
    }

    return iconData.first;
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

    GUniqueOutPtr<GError> error;
    RefPtr<cairo_surface_t> icon = getIconSurfaceSynchronously(database, pageURL, &error.outPtr());

    for (size_t i = 0; i < pendingIconRequests->size(); ++i) {
        GTask* task = pendingIconRequests->at(i).get();
        if (error)
            g_task_return_error(task, error.release().release());
        else {
            GetFaviconSurfaceAsyncData* data = static_cast<GetFaviconSurfaceAsyncData*>(g_task_get_task_data(task));
            data->icon = icon;
            data->shouldReleaseIconForPageURL = false;
            g_task_return_boolean(task, TRUE);
        }
    }
    deletePendingIconRequests(database, pendingIconRequests, pageURL);
}
#endif

static void webkitFaviconDatabaseSetIconURLForPageURL(WebKitFaviconDatabase* database, const String& iconURL, const String& pageURL)
{
    WebKitFaviconDatabasePrivate* priv = database->priv;
    if (!priv->isURLImportCompleted)
        return;

    if (pageURL.isEmpty())
        return;

    const String& currentIconURL = priv->pageURLToIconURLMap.get(pageURL);
    if (iconURL == currentIconURL)
        return;

    priv->pageURLToIconURLMap.set(pageURL, iconURL);
    if (priv->isSettingIcon)
        return;

    g_signal_emit(database, signals[FAVICON_CHANGED], 0, pageURL.utf8().data(), iconURL.utf8().data());
}

class WebKitIconDatabaseClient final : public IconDatabaseClient {
public:
    explicit WebKitIconDatabaseClient(WebKitFaviconDatabase* database)
        : m_database(database)
    {
    }

private:
    void didImportIconURLForPageURL(const String& pageURL) override
    {
        String iconURL = m_database->priv->iconDatabase->synchronousIconURLForPageURL(pageURL);
        webkitFaviconDatabaseSetIconURLForPageURL(m_database, iconURL, pageURL);
    }

    void didChangeIconForPageURL(const String& pageURL) override
    {
        if (m_database->priv->isSettingIcon)
            return;
        String iconURL = m_database->priv->iconDatabase->synchronousIconURLForPageURL(pageURL);
        webkitFaviconDatabaseSetIconURLForPageURL(m_database, iconURL, pageURL);
    }

    void didImportIconDataForPageURL(const String& pageURL) override
    {
#if PLATFORM(GTK)
        processPendingIconsForPageURL(m_database, pageURL);
#endif
        String iconURL = m_database->priv->iconDatabase->synchronousIconURLForPageURL(pageURL);
        webkitFaviconDatabaseSetIconURLForPageURL(m_database, iconURL, pageURL);
    }

    void didFinishURLImport() override
    {
        WebKitFaviconDatabasePrivate* priv = m_database->priv;

        while (!priv->pendingLoadDecisions.isEmpty()) {
            auto iconURLAndCallback = priv->pendingLoadDecisions.takeLast();
            auto decision = priv->iconDatabase->synchronousLoadDecisionForIconURL(iconURLAndCallback.first);
            // Decisions should never be unknown after the inital import is complete.
            ASSERT(decision != IconDatabase::IconLoadDecision::Unknown);
            iconURLAndCallback.second(decision == IconDatabase::IconLoadDecision::Yes);
        }

        priv->isURLImportCompleted = true;
    }

    WebKitFaviconDatabase* m_database;
};

WebKitFaviconDatabase* webkitFaviconDatabaseCreate()
{
    return WEBKIT_FAVICON_DATABASE(g_object_new(WEBKIT_TYPE_FAVICON_DATABASE, nullptr));
}

void webkitFaviconDatabaseOpen(WebKitFaviconDatabase* database, const String& path)
{
    if (webkitFaviconDatabaseIsOpen(database))
        return;

    WebKitFaviconDatabasePrivate* priv = database->priv;
    priv->iconDatabase = std::make_unique<IconDatabase>();
    priv->iconDatabase->setClient(std::make_unique<WebKitIconDatabaseClient>(database));
    IconDatabase::delayDatabaseCleanup();
    priv->iconDatabase->setEnabled(true);
    priv->iconDatabase->setPrivateBrowsingEnabled(WebPreferences::anyPagesAreUsingPrivateBrowsing());

    if (!priv->iconDatabase->open(FileSystem::directoryName(path), FileSystem::pathGetFileName(path))) {
        priv->iconDatabase = nullptr;
        IconDatabase::allowDatabaseCleanup();
    }
}

bool webkitFaviconDatabaseIsOpen(WebKitFaviconDatabase* database)
{
    return database->priv->iconDatabase && database->priv->iconDatabase->isOpen();
}

void webkitFaviconDatabaseSetPrivateBrowsingEnabled(WebKitFaviconDatabase* database, bool enabled)
{
    if (database->priv->iconDatabase)
        database->priv->iconDatabase->setPrivateBrowsingEnabled(enabled);
}

#if PLATFORM(GTK)
void webkitFaviconDatabaseGetLoadDecisionForIcon(WebKitFaviconDatabase* database, const LinkIcon& icon, const String& pageURL, Function<void(bool)>&& completionHandler)
{
    if (!webkitFaviconDatabaseIsOpen(database)) {
        completionHandler(false);
        return;
    }

    WebKitFaviconDatabasePrivate* priv = database->priv;
    auto decision = priv->iconDatabase->synchronousLoadDecisionForIconURL(icon.url.string());
    switch (decision) {
    case IconDatabase::IconLoadDecision::Unknown:
        priv->pendingLoadDecisions.append(std::make_pair(icon.url.string(), WTFMove(completionHandler)));
        priv->iconDatabase->setIconURLForPageURL(icon.url.string(), pageURL);
        break;
    case IconDatabase::IconLoadDecision::No:
        priv->iconDatabase->setIconURLForPageURL(icon.url.string(), pageURL);
        completionHandler(false);
        break;
    case IconDatabase::IconLoadDecision::Yes:
        completionHandler(true);
        break;
    }
}

void webkitFaviconDatabaseSetIconForPageURL(WebKitFaviconDatabase* database, const LinkIcon& icon, API::Data& iconData, const String& pageURL)
{
    if (!webkitFaviconDatabaseIsOpen(database))
        return;

    if (pageURL.isEmpty())
        return;

    WebKitFaviconDatabasePrivate* priv = database->priv;
    SetForScope<bool> change(priv->isSettingIcon, true);
    priv->iconDatabase->setIconURLForPageURL(icon.url.string(), pageURL);
    priv->iconDatabase->setIconDataForIconURL(SharedBuffer::create(iconData.bytes(), iconData.size()), icon.url.string());
    webkitFaviconDatabaseSetIconURLForPageURL(database, icon.url.string(), pageURL);
    g_signal_emit(database, signals[FAVICON_CHANGED], 0, pageURL.utf8().data(), icon.url.string().utf8().data());
    processPendingIconsForPageURL(database, pageURL);
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
#endif

GQuark webkit_favicon_database_error_quark(void)
{
    return g_quark_from_static_string("WebKitFaviconDatabaseError");
}

#if PLATFORM(GTK)
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

    GetFaviconSurfaceAsyncData* data = createGetFaviconSurfaceAsyncData();
    data->faviconDatabase = database;
    data->pageURL = String::fromUTF8(pageURI);
    g_task_set_task_data(task.get(), data, reinterpret_cast<GDestroyNotify>(destroyGetFaviconSurfaceAsyncData));

    WebKitFaviconDatabasePrivate* priv = database->priv;
    priv->iconDatabase->retainIconForPageURL(data->pageURL);

    // We ask for the icon directly. If we don't get the icon data now,
    // we'll be notified later (even if the database is still importing icons).
    GUniqueOutPtr<GError> error;
    data->icon = getIconSurfaceSynchronously(database, data->pageURL, &error.outPtr());
    if (data->icon) {
        g_task_return_boolean(task.get(), TRUE);
        return;
    }

    // At this point we still don't know whether we will get a valid icon for pageURL.
    data->shouldReleaseIconForPageURL = true;

    if (g_error_matches(error.get(), WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_NOT_FOUND)) {
        g_task_return_error(task.get(), error.release().release());
        return;
    }

    // If there's not a valid icon, but there's an iconURL registered,
    // or it's still not registered but the import process hasn't
    // finished yet, we need to wait for iconDataReadyForPage to be
    // called before making and informed decision.
    String iconURLForPageURL = priv->iconDatabase->synchronousIconURLForPageURL(data->pageURL);
    if (!iconURLForPageURL.isEmpty() || !priv->isURLImportCompleted) {
        PendingIconRequestVector* iconRequests = getOrCreatePendingIconRequests(database, data->pageURL);
        ASSERT(iconRequests);
        iconRequests->append(task);
        return;
    }

    g_task_return_new_error(task.get(), WEBKIT_FAVICON_DATABASE_ERROR, WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN,
        _("Unknown favicon for page %s"), pageURI);
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
    if (!g_task_propagate_boolean(task, error))
        return 0;

    GetFaviconSurfaceAsyncData* data = static_cast<GetFaviconSurfaceAsyncData*>(g_task_get_task_data(task));
    return cairo_surface_reference(data->icon.get());
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

    String iconURLForPageURL = database->priv->iconDatabase->synchronousIconURLForPageURL(String::fromUTF8(pageURL));
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

    database->priv->iconDatabase->removeAllIcons();
}
