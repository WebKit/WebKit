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

#include "webkitwebbackforwardlist.h"
#include "webkitprivate.h"
#include "webkitwebhistoryitem.h"
#include "webkitwebview.h"

#include <glib.h>

#include "BackForwardList.h"
#include "HistoryItem.h"

using namespace WebKit;

extern "C" {

struct _WebKitWebBackForwardListPrivate {
    WebCore::BackForwardList* backForwardList;
};

#define WEBKIT_WEB_BACK_FORWARD_LIST_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_BACK_FORWARD_LIST, WebKitWebBackForwardListPrivate))

G_DEFINE_TYPE(WebKitWebBackForwardList, webkit_web_back_forward_list, G_TYPE_OBJECT);

static void webkit_web_back_forward_list_class_init(WebKitWebBackForwardListClass* klass)
{
    g_type_class_add_private(klass, sizeof(WebKitWebBackForwardListPrivate));
}

static void webkit_web_back_forward_list_init(WebKitWebBackForwardList* webBackForwardList)
{
    webBackForwardList->priv = WEBKIT_WEB_BACK_FORWARD_LIST_GET_PRIVATE(webBackForwardList);
}

/**
 * webkit_web_back_forward_list_new_with_web_view:
 * @web_view: the back forward list's #WebKitWebView
 *
 * Creates an instance of the back forward list with a controlling #WebKitWebView
 *
 * Return value: a #WebKitWebBackForwardList
 */
WebKitWebBackForwardList* webkit_web_back_forward_list_new_with_web_view(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebBackForwardList* webBackForwardList;

    webBackForwardList = WEBKIT_WEB_BACK_FORWARD_LIST(g_object_new(WEBKIT_TYPE_WEB_BACK_FORWARD_LIST, NULL));
    WebKitWebBackForwardListPrivate* priv = webBackForwardList->priv;

    priv->backForwardList = core(webView)->backForwardList();
    priv->backForwardList->setEnabled(TRUE);

    return webBackForwardList;
}

/**
 * webkit_web_back_forward_list_go_forward:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 *
 * Steps forward in the back forward list
 */
void webkit_web_back_forward_list_go_forward(WebKitWebBackForwardList* webBackForwardList)
{
    g_return_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList));

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (backForwardList->enabled())
        backForwardList->goForward();
}

/**
 * webkit_web_back_forward_list_go_back:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 *
 * Steps backward in the back forward list
 */
void webkit_web_back_forward_list_go_back(WebKitWebBackForwardList* webBackForwardList)
{
    g_return_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList));

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (backForwardList->enabled())
        backForwardList->goBack();
}

/**
 * webkit_web_back_forward_list_contains_item:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 * @history_item: the #WebKitWebHistoryItem to check
 *
 * Checks if @web_history_item is in the back forward list
 *
 * Return: %TRUE if @web_history_item is in the back forward list, %FALSE if it doesn't
 */
gboolean webkit_web_back_forward_list_contains_item(WebKitWebBackForwardList* webBackForwardList, WebKitWebHistoryItem* webHistoryItem)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), FALSE);
    g_return_val_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(webHistoryItem), FALSE);

    WebCore::HistoryItem* historyItem = core(webHistoryItem);

    g_return_val_if_fail(historyItem != NULL, FALSE);

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);

    return (backForwardList->enabled() ? backForwardList->containsItem(historyItem) : FALSE);
}

/**
 * webkit_web_back_forward_list_go_to_item:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 * @history_item: the #WebKitWebHistoryItem to go to
 *
 * Go to the specified @web_history_item in the back forward list
 */
void webkit_web_back_forward_list_go_to_item(WebKitWebBackForwardList* webBackForwardList, WebKitWebHistoryItem* webHistoryItem)
{
    g_return_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList));
    g_return_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(webHistoryItem));

    WebCore::HistoryItem* historyItem = core(webHistoryItem);
    WebCore::BackForwardList* backForwardList = core(webBackForwardList);

    if (backForwardList->enabled() && historyItem)
        backForwardList->goToItem(historyItem);
}

/**
 * webkit_web_back_forward_list_get_forward_list_with_limit:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 * @limit: the number of items to retrieve
 *
 * Returns a list of items that succeed the current item, limited by @limit
 *
 * Return value: a #GList of items succeeding the current item, limited by @limit
 */
GList* webkit_web_back_forward_list_get_forward_list_with_limit(WebKitWebBackForwardList* webBackForwardList, gint limit)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), NULL);

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (!backForwardList || !backForwardList->enabled())
        return NULL;

    WebCore::HistoryItemVector items;
    GList* forwardItems = { 0 };

    backForwardList->forwardListWithLimit(limit, items);

    for (unsigned i = 0; i < items.size(); i++) {
        WebKitWebHistoryItem* webHistoryItem = webkit_web_history_item_new_with_core_item(items[i].get());
        forwardItems = g_list_prepend(forwardItems, g_object_ref(webHistoryItem));
    }

    return g_list_reverse(forwardItems);
}

/**
 * webkit_web_back_forward_list_get_back_list_with_limit:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 * @limit: the number of items to retrieve
 *
 * Returns a list of items that precede the current item, limited by @limit
 *
 * Return value: a #GList of items preceding the current item, limited by @limit
 */
GList* webkit_web_back_forward_list_get_back_list_with_limit(WebKitWebBackForwardList* webBackForwardList, gint limit)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), NULL);

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (!backForwardList || !backForwardList->enabled())
        return NULL;

    WebCore::HistoryItemVector items;
    GList* backItems = { 0 };

    backForwardList->backListWithLimit(limit, items);

    for (unsigned i = 0; i < items.size(); i++) {
        WebKitWebHistoryItem* webHistoryItem = webkit_web_history_item_new_with_core_item(items[i].get());
        backItems = g_list_prepend(backItems, g_object_ref(webHistoryItem));
    }

    return g_list_reverse(backItems);
}
/**
 * webkit_web_back_forward_list_get_back_item:
 * @web_back_forward_list: a #WebBackForwardList
 *
 * Returns the item that precedes the current item
 *
 * Return value: the #WebKitWebHistoryItem preceding the current item
 */
WebKitWebHistoryItem* webkit_web_back_forward_list_get_back_item(WebKitWebBackForwardList* webBackForwardList)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), NULL);

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (!backForwardList || !backForwardList->enabled())
        return NULL;

    WebCore::HistoryItem* historyItem = backForwardList->backItem();

    return (historyItem ? kit(historyItem) : NULL);
}

/**
 * webkit_web_back_forward_list_get_current_item:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 *
 * Returns the current item.
 *
 * Returns a NULL value if the back forward list is empty
 *
 * Return value: a #WebKitWebHistoryItem
 */
WebKitWebHistoryItem* webkit_web_back_forward_list_get_current_item(WebKitWebBackForwardList* webBackForwardList)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), NULL);

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (!backForwardList || !backForwardList->enabled())
        return NULL;

    WebCore::HistoryItem* historyItem = backForwardList->currentItem();

    return (historyItem ? kit(historyItem) : NULL);
}

/**
 * webkit_web_back_forward_list_get_forward_item:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 *
 * Returns the item that succeeds the current item.
 *
 * Returns a NULL value if there nothing that succeeds the current item
 *
 * Return value: a #WebKitWebHistoryItem
 */
WebKitWebHistoryItem* webkit_web_back_forward_list_get_forward_item(WebKitWebBackForwardList* webBackForwardList)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), NULL);

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (!backForwardList || !backForwardList->enabled())
        return NULL;

    WebCore::HistoryItem* historyItem = backForwardList->forwardItem();

    return (historyItem ? kit(historyItem) : NULL);
}

/**
 * webkit_web_back_forward_list_get_nth_item:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 * @index: the index of the item
 *
 * Returns the item at a given index relative to the current item.
 *
 * Return value: the #WebKitWebHistoryItem located at the specified index relative to the current item
 */
WebKitWebHistoryItem* webkit_web_back_forward_list_get_nth_item(WebKitWebBackForwardList* webBackForwardList, gint index)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), NULL);

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (!backForwardList)
        return NULL;

    WebCore::HistoryItem* historyItem = backForwardList->itemAtIndex(index);

    return (historyItem ? kit(historyItem) : NULL);
}

/**
 * webkit_web_back_forward_list_get_back_length:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 *
 * Returns the number of items that preced the current item.
 *
 * Return value: a #gint corresponding to the number of items preceding the current item
 */
gint webkit_web_back_forward_list_get_back_length(WebKitWebBackForwardList* webBackForwardList)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), 0);

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (!backForwardList || !backForwardList->enabled())
        return 0;

    return backForwardList->backListCount();
}

/**
 * webkit_web_back_forward_list_get_forward_length:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 *
 * Returns the number of items that succeed the current item.
 *
 * Return value: a #gint corresponding to the nuber of items succeeding the current item
 */
gint webkit_web_back_forward_list_get_forward_length(WebKitWebBackForwardList* webBackForwardList)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), 0);

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (!backForwardList || !backForwardList->enabled())
        return 0;

    return backForwardList->forwardListCount();
}

/**
 * webkit_web_back_forward_list_get_limit:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 *
 * Returns the maximum limit of the back forward list.
 *
 * Return value: a #gint indicating the number of #WebHistoryItem the back forward list can hold
 */
gint webkit_web_back_forward_list_get_limit(WebKitWebBackForwardList* webBackForwardList)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), 0);

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (!backForwardList || !backForwardList->enabled())
        return 0;

    return backForwardList->capacity();
}

/**
 * webkit_web_back_forward_list_set_limit:
 * @web_back_forward_list: a #WebKitWebBackForwardList
 * @limit: the limit to set the back forward list to
 *
 * Sets the maximum limit of the back forward list. If the back forward list
 * exceeds its capacity, items will be removed everytime a new item has been
 * added.
 */
void webkit_web_back_forward_list_set_limit(WebKitWebBackForwardList* webBackForwardList, gint limit)
{
    g_return_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList));

    WebCore::BackForwardList* backForwardList = core(webBackForwardList);
    if (backForwardList)
        backForwardList->setCapacity(limit);
}

} /* end extern "C" */

WebCore::BackForwardList* WebKit::core(WebKitWebBackForwardList* webBackForwardList)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_BACK_FORWARD_LIST(webBackForwardList), NULL);

    return webBackForwardList->priv ? webBackForwardList->priv->backForwardList : 0;
}
