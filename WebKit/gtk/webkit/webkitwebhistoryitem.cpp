/*
 * Copyright (C) 2008 Jan Michael C. Alonzo
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

#include "webkitwebhistoryitem.h"
#include "webkitprivate.h"

#include <glib.h>

#include "CString.h"
#include "HistoryItem.h"
#include "PlatformString.h"

using namespace WebKit;

extern "C" {

struct _WebKitWebHistoryItemPrivate {
    WebCore::HistoryItem* historyItem;

    gchar* title;
    gchar* alternateTitle;
    gchar* uri;
    gchar* originalUri;
};

#define WEBKIT_WEB_HISTORY_ITEM_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_HISTORY_ITEM, WebKitWebHistoryItemPrivate))

G_DEFINE_TYPE(WebKitWebHistoryItem, webkit_web_history_item, G_TYPE_OBJECT);


static GHashTable* webkit_history_items()
{
    static GHashTable* historyItems = g_hash_table_new(g_direct_hash, g_direct_equal);
    return historyItems;
}

static void webkit_history_item_add(WebKitWebHistoryItem* webHistoryItem, WebCore::HistoryItem* historyItem)
{
    g_return_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(webHistoryItem));

    GHashTable* table = webkit_history_items();

    historyItem->ref();
    g_hash_table_insert(table, historyItem, g_object_ref(webHistoryItem));
}

static void webkit_history_item_remove(WebCore::HistoryItem* historyItem)
{
    GHashTable* table = webkit_history_items();
    WebKitWebHistoryItem* webHistoryItem = (WebKitWebHistoryItem*) g_hash_table_lookup(table, historyItem);

    g_return_if_fail(webHistoryItem != NULL);

    g_hash_table_remove(table, historyItem);
    historyItem->deref();
    g_object_unref(webHistoryItem);
}

static void webkit_web_history_item_dispose(GObject* object)
{
    WebKitWebHistoryItem* webHistoryItem = WEBKIT_WEB_HISTORY_ITEM(object);
    WebKitWebHistoryItemPrivate* priv = webHistoryItem->priv;

    webkit_history_item_remove(priv->historyItem);
    delete priv->historyItem;

    /* destroy table if empty */
    GHashTable* table = webkit_history_items();
    if (!g_hash_table_size(table))
        g_hash_table_destroy(table);

    G_OBJECT_CLASS(webkit_web_history_item_parent_class)->dispose(object);
}

static void webkit_web_history_item_finalize(GObject* object)
{
    WebKitWebHistoryItem* webHistoryItem = WEBKIT_WEB_HISTORY_ITEM(object);
    WebKitWebHistoryItemPrivate* priv = webHistoryItem->priv;

    g_free(priv->title);
    g_free(priv->alternateTitle);
    g_free(priv->uri);
    g_free(priv->originalUri);

    G_OBJECT_CLASS(webkit_web_history_item_parent_class)->finalize(object);
}

static void webkit_web_history_item_class_init(WebKitWebHistoryItemClass* klass)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = webkit_web_history_item_dispose;
    gobject_class->finalize = webkit_web_history_item_finalize;

    g_type_class_add_private(gobject_class, sizeof(WebKitWebHistoryItemPrivate));
}

static void webkit_web_history_item_init(WebKitWebHistoryItem* webHistoryItem)
{
    webHistoryItem->priv = WEBKIT_WEB_HISTORY_ITEM_GET_PRIVATE(webHistoryItem);
}

/* Helper function to create a new WebHistoryItem instance when needed */
WebKitWebHistoryItem* webkit_web_history_item_new_with_core_item(WebCore::HistoryItem* item)
{
    WebKitWebHistoryItem* webHistoryItem = kit(item);

    if (!webHistoryItem) {
        webHistoryItem = WEBKIT_WEB_HISTORY_ITEM(g_object_new(WEBKIT_TYPE_WEB_HISTORY_ITEM, NULL));
        WebKitWebHistoryItemPrivate* priv = webHistoryItem->priv;
        priv->historyItem = item;
        webkit_history_item_add(webHistoryItem, priv->historyItem);
    }

    return webHistoryItem;
}


/**
 * webkit_web_history_item_new:
 *
 * Creates a new #WebKitWebHistoryItem instance
 *
 * Return value: the new #WebKitWebHistoryItem
 */
WebKitWebHistoryItem* webkit_web_history_item_new(void)
{
    WebKitWebHistoryItem* webHistoryItem = WEBKIT_WEB_HISTORY_ITEM(g_object_new(WEBKIT_TYPE_WEB_HISTORY_ITEM, NULL));
    WebKitWebHistoryItemPrivate* priv = webHistoryItem->priv;

    priv->historyItem = new WebCore::HistoryItem();
    webkit_history_item_add(webHistoryItem, priv->historyItem);

    return webHistoryItem;
}

/**
 * webkit_web_history_item_new_with_data:
 * @uri: the uri of the page
 * @title: the title of the page
 *
 * Creates a new #WebKitWebHistoryItem with the given URI and title
 *
 * Return value: the new #WebKitWebHistoryItem
 */
WebKitWebHistoryItem* webkit_web_history_item_new_with_data(const gchar* uri, const gchar* title)
{
    WebCore::KURL historyUri(uri);
    WebCore::String historyTitle(title);

    WebKitWebHistoryItem* webHistoryItem = webkit_web_history_item_new();
    WebKitWebHistoryItemPrivate* priv = webHistoryItem->priv;

    priv->historyItem = new WebCore::HistoryItem(historyUri, historyTitle);
    webkit_history_item_add(webHistoryItem, priv->historyItem);

    return webHistoryItem;
}

/**
 * webkit_web_history_item_get_title:
 * @webHistoryItem: a #WebKitWebHistoryItem
 *
 * Returns the page title of @webHistoryItem
 */
const gchar* webkit_web_history_item_get_title(WebKitWebHistoryItem* webHistoryItem)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(webHistoryItem), NULL);

    WebCore::HistoryItem* item = core(webHistoryItem);

    g_return_val_if_fail(item != NULL, NULL);

    WebKitWebHistoryItemPrivate* priv = webHistoryItem->priv;
    WebCore::String title = item->title();
    g_free(priv->title);
    priv->title = g_strdup(title.utf8().data());

    return priv->title;
}

/**
 * webkit_web_history_item_get_alternate_title:
 * @webHistoryItem: a #WebKitWebHistoryItem
 *
 * Returns the alternate title of @webHistoryItem
 *
 * Return value: the alternate title of @webHistoryItem
 */
const gchar* webkit_web_history_item_get_alternate_title(WebKitWebHistoryItem* webHistoryItem)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(webHistoryItem), NULL);

    WebCore::HistoryItem* item = core(webHistoryItem);

    g_return_val_if_fail(item != NULL, NULL);

    WebKitWebHistoryItemPrivate* priv = webHistoryItem->priv;
    WebCore::String alternateTitle = item->alternateTitle();
    g_free(priv->alternateTitle);
    priv->alternateTitle = g_strdup(alternateTitle.utf8().data());

    return priv->alternateTitle;
}

/**
 * webkit_web_history_item_set_alternate_title:
 * @webHistoryItem: a #WebKitWebHistoryItem
 * @title: the alternate title for @this history item
 *
 * Sets an alternate title for @webHistoryItem
 */
void webkit_web_history_item_set_alternate_title(WebKitWebHistoryItem* webHistoryItem, const gchar* title)
{
    g_return_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(webHistoryItem));

    WebCore::HistoryItem* item = core(webHistoryItem);

    item->setAlternateTitle(WebCore::String::fromUTF8(title));
}

/**
 * webkit_web_history_item_get_uri:
 * @webHistoryItem: a #WebKitWebHistoryItem
 *
 * Returns the URI of @this
 *
 * Return value: the URI of @webHistoryItem
 */
const gchar* webkit_web_history_item_get_uri(WebKitWebHistoryItem* webHistoryItem)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(webHistoryItem), NULL);

    WebCore::HistoryItem* item = core(WEBKIT_WEB_HISTORY_ITEM(webHistoryItem));

    g_return_val_if_fail(item != NULL, NULL);

    WebCore::String uri = item->urlString();
    WebKitWebHistoryItemPrivate* priv = webHistoryItem->priv;
    g_free(priv->uri);
    priv->uri = g_strdup(uri.utf8().data());

    return priv->uri;
}

/**
 * webkit_web_history_item_get_original_uri:
 * @webHistoryItem: a #WebKitWebHistoryItem
 *
 * Returns the original URI of @webHistoryItem.
 *
 * Return value: the original URI of @webHistoryITem
 */
const gchar* webkit_web_history_item_get_original_uri(WebKitWebHistoryItem* webHistoryItem)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(webHistoryItem), NULL);

    WebCore::HistoryItem* item = core(WEBKIT_WEB_HISTORY_ITEM(webHistoryItem));

    g_return_val_if_fail(item != NULL, NULL);

    WebCore::String originalUri = item->originalURLString();
    WebKitWebHistoryItemPrivate* priv = webHistoryItem->priv;
    g_free(priv->originalUri);
    priv->originalUri = g_strdup(originalUri.utf8().data());

    return webHistoryItem->priv->originalUri;
}

/**
 * webkit_web_history_item_get_last_visisted_time :
 * @webHistoryItem: a #WebKitWebHistoryItem
 *
 * Returns the last time @webHistoryItem was visited
 *
 * Return value: the time in seconds this @webHistoryItem was last visited
 */
gdouble webkit_web_history_item_get_last_visited_time(WebKitWebHistoryItem* webHistoryItem)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(webHistoryItem), 0);

    WebCore::HistoryItem* item = core(WEBKIT_WEB_HISTORY_ITEM(webHistoryItem));

    g_return_val_if_fail(item != NULL, 0);

    return item->lastVisitedTime();
}

} /* end extern "C" */

WebCore::HistoryItem* WebKit::core(WebKitWebHistoryItem* webHistoryItem)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(webHistoryItem), NULL);

    WebKitWebHistoryItemPrivate* priv = webHistoryItem->priv;
    WebCore::HistoryItem* historyItem = priv->historyItem;

    return historyItem ? historyItem : 0;
}

WebKitWebHistoryItem* WebKit::kit(WebCore::HistoryItem* historyItem)
{
    g_return_val_if_fail(historyItem != NULL, NULL);

    WebKitWebHistoryItem* webHistoryItem;
    GHashTable* table = webkit_history_items();

    webHistoryItem = (WebKitWebHistoryItem*) g_hash_table_lookup(table, historyItem);
    return webHistoryItem;
}

