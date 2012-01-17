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
#include "WebKitBackForwardListItem.h"

#include "WebKitBackForwardListPrivate.h"
#include "WebKitPrivate.h"
#include <wtf/HashMap.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;

struct _WebKitBackForwardListItemPrivate {
    WKRetainPtr<WKBackForwardListItemRef> wkListItem;
    CString uri;
    CString title;
    CString originalURI;
};

G_DEFINE_TYPE(WebKitBackForwardListItem, webkit_back_forward_list_item, G_TYPE_INITIALLY_UNOWNED)

static void webkitBackForwardListItemFinalize(GObject* object)
{
    WEBKIT_BACK_FORWARD_LIST_ITEM(object)->priv->~WebKitBackForwardListItemPrivate();
    G_OBJECT_CLASS(webkit_back_forward_list_item_parent_class)->finalize(object);
}

static void webkit_back_forward_list_item_init(WebKitBackForwardListItem* listItem)
{
    WebKitBackForwardListItemPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(listItem, WEBKIT_TYPE_BACK_FORWARD_LIST_ITEM, WebKitBackForwardListItemPrivate);
    listItem->priv = priv;
    new (priv) WebKitBackForwardListItemPrivate();
}

static void webkit_back_forward_list_item_class_init(WebKitBackForwardListItemClass* listItemClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(listItemClass);

    gObjectClass->finalize = webkitBackForwardListItemFinalize;

    g_type_class_add_private(listItemClass, sizeof(WebKitBackForwardListItemPrivate));
}

typedef HashMap<WKBackForwardListItemRef, WebKitBackForwardListItem*> HistoryItemsMap;

static HistoryItemsMap& historyItemsMap()
{
    DEFINE_STATIC_LOCAL(HistoryItemsMap, itemsMap, ());
    return itemsMap;
}

static void webkitBackForwardListItemFinalized(gpointer wkListItem, GObject* finalizedListItem)
{
    ASSERT(G_OBJECT(historyItemsMap().get(static_cast<WKBackForwardListItemRef>(wkListItem))) == finalizedListItem);
    historyItemsMap().remove(static_cast<WKBackForwardListItemRef>(wkListItem));
}

WebKitBackForwardListItem* webkitBackForwardListItemGetOrCreate(WKBackForwardListItemRef wkListItem)
{
    if (!wkListItem)
        return 0;

    WebKitBackForwardListItem* listItem = historyItemsMap().get(wkListItem);
    if (listItem)
        return listItem;

    listItem = WEBKIT_BACK_FORWARD_LIST_ITEM(g_object_new(WEBKIT_TYPE_BACK_FORWARD_LIST_ITEM, NULL));
    listItem->priv->wkListItem = wkListItem;

    g_object_weak_ref(G_OBJECT(listItem), webkitBackForwardListItemFinalized,
                      const_cast<OpaqueWKBackForwardListItem*>(wkListItem));
    historyItemsMap().set(wkListItem, listItem);

    return listItem;
}

WKBackForwardListItemRef webkitBackForwardListItemGetWKItem(WebKitBackForwardListItem* listItem)
{
    return listItem->priv->wkListItem.get();
}

/**
 * webkit_back_forward_list_item_get_uri:
 * @list_item: a #WebKitBackForwardListItem
 *
 * This URI may differ from the original URI if the page was,
 * for example, redirected to a new location.
 * See also webkit_back_forward_list_item_get_original_uri().
 *
 * Returns: the URI of @list_item or %NULL
 *    when the URI is empty.
 */
const gchar* webkit_back_forward_list_item_get_uri(WebKitBackForwardListItem* listItem)
{
    g_return_val_if_fail(WEBKIT_IS_BACK_FORWARD_LIST_ITEM(listItem), 0);

    WebKitBackForwardListItemPrivate* priv = listItem->priv;
    WKRetainPtr<WKURLRef> wkURI(AdoptWK, WKBackForwardListItemCopyURL(priv->wkListItem.get()));
    if (toImpl(wkURI.get())->string().isEmpty())
        return 0;

    priv->uri = toImpl(wkURI.get())->string().utf8();
    return priv->uri.data();
}

/**
 * webkit_back_forward_list_item_get_title:
 * @list_item: a #WebKitBackForwardListItem
 *
 * Returns: the page title of @list_item or %NULL
 *    when the title is empty.
 */
const gchar* webkit_back_forward_list_item_get_title(WebKitBackForwardListItem* listItem)
{
    g_return_val_if_fail(WEBKIT_IS_BACK_FORWARD_LIST_ITEM(listItem), 0);

    WebKitBackForwardListItemPrivate* priv = listItem->priv;
    WKRetainPtr<WKStringRef> wkTitle(AdoptWK, WKBackForwardListItemCopyTitle(priv->wkListItem.get()));
    if (toImpl(wkTitle.get())->string().isEmpty())
        return 0;

    priv->title = toImpl(wkTitle.get())->string().utf8();
    return priv->title.data();
}

/**
 * webkit_back_forward_list_item_get_original_uri:
 * @list_item: a #WebKitBackForwardListItem
 *
 * See also webkit_back_forward_list_item_get_uri().
 *
 * Returns: the original URI of @list_item or %NULL
 *    when the original URI is empty.
 */
const gchar* webkit_back_forward_list_item_get_original_uri(WebKitBackForwardListItem* listItem)
{
    g_return_val_if_fail(WEBKIT_IS_BACK_FORWARD_LIST_ITEM(listItem), 0);

    WebKitBackForwardListItemPrivate* priv = listItem->priv;
    WKRetainPtr<WKURLRef> wkOriginalURI(AdoptWK, WKBackForwardListItemCopyOriginalURL(priv->wkListItem.get()));
    if (toImpl(wkOriginalURI.get())->string().isEmpty())
        return 0;

    priv->originalURI = toImpl(wkOriginalURI.get())->string().utf8();
    return priv->originalURI.data();
}
