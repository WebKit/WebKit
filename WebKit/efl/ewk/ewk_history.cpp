/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2010 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ewk_history.h"

#include "BackForwardList.h"
#include "EWebKit.h"
#include "HistoryItem.h"
#include "Image.h"
#include "ewk_private.h"
#include <wtf/text/CString.h>

#include <Eina.h>
#include <eina_safety_checks.h>

struct _Ewk_History {
    WebCore::BackForwardList *core;
};

#define EWK_HISTORY_CORE_GET_OR_RETURN(history, core_, ...)      \
    if (!(history)) {                                            \
        CRITICAL("history is NULL.");                            \
        return __VA_ARGS__;                                      \
    }                                                            \
    if (!(history)->core) {                                      \
        CRITICAL("history->core is NULL.");                      \
        return __VA_ARGS__;                                      \
    }                                                            \
    if (!(history)->core->enabled()) {                           \
        ERR("history->core is disabled!.");                      \
        return __VA_ARGS__;                                      \
    }                                                            \
    WebCore::BackForwardList *core_ = (history)->core


struct _Ewk_History_Item {
    WebCore::HistoryItem *core;

    const char *title;
    const char *alternate_title;
    const char *uri;
    const char *original_uri;
};

#define EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core_, ...) \
    if (!(item)) {                                            \
        CRITICAL("item is NULL.");                            \
        return __VA_ARGS__;                                   \
    }                                                         \
    if (!(item)->core) {                                      \
        CRITICAL("item->core is NULL.");                      \
        return __VA_ARGS__;                                   \
    }                                                         \
    WebCore::HistoryItem *core_ = (item)->core


static inline Ewk_History_Item *_ewk_history_item_new(WebCore::HistoryItem *core)
{
    Ewk_History_Item* item;

    if (!core) {
        ERR("WebCore::HistoryItem is NULL.");
        return 0;
    }

    item = (Ewk_History_Item *)calloc(1, sizeof(Ewk_History_Item));
    if (!item) {
        CRITICAL("Could not allocate item memory.");
        return 0;
    }

    core->ref();
    item->core = core;

    return item;
}

static inline Eina_List *_ewk_history_item_list_get(const WebCore::HistoryItemVector &core_items)
{
    Eina_List* ret = 0;
    unsigned int i, size;

    size = core_items.size();
    for (i = 0; i < size; i++) {
        Ewk_History_Item* item = _ewk_history_item_new(core_items[i].get());
        if (item)
            ret = eina_list_append(ret, item);
    }

    return ret;
}

/**
 * Go forward in history one item, if possible.
 *
 * @param history which history instance to modify.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE on failure.
 */
Eina_Bool ewk_history_forward(Ewk_History* history)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, EINA_FALSE);
    if (core->forwardListCount() < 1)
        return EINA_FALSE;
    core->goForward();
    return EINA_TRUE;
}

/**
 * Go back in history one item, if possible.
 *
 * @param history which history instance to modify.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE on failure.
 */
Eina_Bool ewk_history_back(Ewk_History* history)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, EINA_FALSE);
    if (core->backListCount() < 1)
        return EINA_FALSE;
    core->goBack();
    return EINA_TRUE;
}

/**
 * Adds the given item to history.
 *
 * Memory handling: This will not modify or even take references to
 * given item (Ewk_History_Item), so you should still handle it with
 * ewk_history_item_free().
 *
 * @param history which history instance to modify.
 * @param item reference to add to history. Unmodified. Must @b not be @c NULL.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE on failure.
 */
Eina_Bool ewk_history_history_item_add(Ewk_History* history, const Ewk_History_Item* item)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, history_core, EINA_FALSE);
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, item_core, EINA_FALSE);
    history_core->addItem(item_core);
    return EINA_TRUE;
}

/**
 * Sets the given item as current in history (go to item).
 *
 * Memory handling: This will not modify or even take references to
 * given item (Ewk_History_Item), so you should still handle it with
 * ewk_history_item_free().
 *
 * @param history which history instance to modify.
 * @param item reference to go to history. Unmodified. Must @b not be @c NULL.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE on failure.
 */
Eina_Bool ewk_history_history_item_set(Ewk_History* history, const Ewk_History_Item* item)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, history_core, EINA_FALSE);
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, item_core, EINA_FALSE);
    history_core->goToItem(item_core);
    return EINA_TRUE;
}

/**
 * Get the first item from back list, if any.
 *
 * @param history which history instance to query.
 *
 * @return the @b newly allocated item instance. This memory must be
 *         released with ewk_history_item_free() after use.
 */
Ewk_History_Item* ewk_history_history_item_back_get(const Ewk_History* history)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    return _ewk_history_item_new(core->backItem());
}

/**
 * Get the current item in history, if any.
 *
 * @param history which history instance to query.
 *
 * @return the @b newly allocated item instance. This memory must be
 *         released with ewk_history_item_free() after use.
 */
Ewk_History_Item* ewk_history_history_item_current_get(const Ewk_History* history)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    return _ewk_history_item_new(core->currentItem());
}

/**
 * Get the first item from forward list, if any.
 *
 * @param history which history instance to query.
 *
 * @return the @b newly allocated item instance. This memory must be
 *         released with ewk_history_item_free() after use.
 */
Ewk_History_Item* ewk_history_history_item_forward_get(const Ewk_History* history)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    return _ewk_history_item_new(core->forwardItem());
}

/**
 * Get item at given position, if any at that index.
 *
 * @param history which history instance to query.
 * @param index position of item to get.
 *
 * @return the @b newly allocated item instance. This memory must be
 *         released with ewk_history_item_free() after use.
 */
Ewk_History_Item* ewk_history_history_item_nth_get(const Ewk_History* history, int index)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    return _ewk_history_item_new(core->itemAtIndex(index));
}

/**
 * Queries if given item is in history.
 *
 * Memory handling: This will not modify or even take references to
 * given item (Ewk_History_Item), so you should still handle it with
 * ewk_history_item_free().
 *
 * @param history which history instance to modify.
 * @param item reference to check in history. Must @b not be @c NULL.
 *
 * @return @c EINA_TRUE if in history, @c EINA_FALSE if not or failure.
 */
Eina_Bool ewk_history_history_item_contains(const Ewk_History* history, const Ewk_History_Item* item)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, history_core, EINA_FALSE);
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, item_core, EINA_FALSE);
    return history_core->containsItem(item_core);
}

/**
 * Get the whole forward list.
 *
 * @param history which history instance to query.
 *
 * @return a newly allocated list of @b newly allocated item
 *         instance. This memory of each item must be released with
 *         ewk_history_item_free() after use. use
 *         ewk_history_item_list_free() for convenience.
 *
 * @see ewk_history_item_list_free()
 * @see ewk_history_forward_list_get_with_limit()
 */
Eina_List* ewk_history_forward_list_get(const Ewk_History* history)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    WebCore::HistoryItemVector items;
    int limit = core->forwardListCount();
    core->forwardListWithLimit(limit, items);
    return _ewk_history_item_list_get(items);
}

/**
 * Get the forward list within the given limit.
 *
 * @param history which history instance to query.
 * @param limit the maximum number of items to return.
 *
 * @return a newly allocated list of @b newly allocated item
 *         instance. This memory of each item must be released with
 *         ewk_history_item_free() after use. use
 *         ewk_history_item_list_free() for convenience.
 *
 * @see ewk_history_item_list_free()
 * @see ewk_history_forward_list_length()
 * @see ewk_history_forward_list_get()
 */
Eina_List* ewk_history_forward_list_get_with_limit(const Ewk_History* history, int limit)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    WebCore::HistoryItemVector items;
    core->forwardListWithLimit(limit, items);
    return _ewk_history_item_list_get(items);
}

/**
 * Get the whole size of forward list.
 *
 * @param history which history instance to query.
 *
 * @return number of elements in whole list.
 *
 * @see ewk_history_forward_list_get_with_limit()
 */
int ewk_history_forward_list_length(const Ewk_History* history)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    return core->forwardListCount();
}

/**
 * Get the whole back list.
 *
 * @param history which history instance to query.
 *
 * @return a newly allocated list of @b newly allocated item
 *         instance. This memory of each item must be released with
 *         ewk_history_item_free() after use. use
 *         ewk_history_item_list_free() for convenience.
 *
 * @see ewk_history_item_list_free()
 * @see ewk_history_back_list_get_with_limit()
 */
Eina_List* ewk_history_back_list_get(const Ewk_History* history)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    WebCore::HistoryItemVector items;
    int limit = core->backListCount();
    core->backListWithLimit(limit, items);
    return _ewk_history_item_list_get(items);
}

/**
 * Get the back list within the given limit.
 *
 * @param history which history instance to query.
 * @param limit the maximum number of items to return.
 *
 * @return a newly allocated list of @b newly allocated item
 *         instance. This memory of each item must be released with
 *         ewk_history_item_free() after use. use
 *         ewk_history_item_list_free() for convenience.
 *
 * @see ewk_history_item_list_free()
 * @see ewk_history_back_list_length()
 * @see ewk_history_back_list_get()
 */
Eina_List* ewk_history_back_list_get_with_limit(const Ewk_History* history, int limit)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    WebCore::HistoryItemVector items;
    core->backListWithLimit(limit, items);
    return _ewk_history_item_list_get(items);
}

/**
 * Get the whole size of back list.
 *
 * @param history which history instance to query.
 *
 * @return number of elements in whole list.
 *
 * @see ewk_history_back_list_get_with_limit()
 */
int ewk_history_back_list_length(const Ewk_History* history)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    return core->backListCount();
}

/**
 * Get maximum capacity of given history.
 *
 * @param history which history instance to query.
 *
 * @return maximum number of entries this history will hold.
 */
int ewk_history_limit_get(Ewk_History* history)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, 0);
    return core->capacity();
}

/**
 * Set maximum capacity of given history.
 *
 * @param history which history instance to modify.
 * @param limit maximum size to allow.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_history_limit_set(const Ewk_History* history, int limit)
{
    EWK_HISTORY_CORE_GET_OR_RETURN(history, core, EINA_FALSE);
    core->setCapacity(limit);
    return EINA_TRUE;
}

/**
 * Create a new history item with given URI and title.
 *
 * @param uri where this resource is located.
 * @param title resource title.
 *
 * @return newly allocated history item or @c NULL on errors. You must
 *         free this item with ewk_history_item_free().
 */
Ewk_History_Item* ewk_history_item_new(const char* uri, const char* title)
{
    WebCore::String u = WebCore::String::fromUTF8(uri);
    WebCore::String t = WebCore::String::fromUTF8(title);
    WTF::RefPtr<WebCore::HistoryItem> core = WebCore::HistoryItem::create(u, t, 0);
    Ewk_History_Item* item = _ewk_history_item_new(core.release().releaseRef());
    return item;
}

static inline void _ewk_history_item_free(Ewk_History_Item* item, WebCore::HistoryItem* core)
{
    core->deref();
    free(item);
}

/**
 * Free given history item instance.
 *
 * @param item what to free.
 */
void ewk_history_item_free(Ewk_History_Item* item)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core);
    _ewk_history_item_free(item, core);
}

/**
 * Free given list and associated history items instances.
 *
 * @param history_items list of items to free (both list nodes and
 *        item instances).
 */
void ewk_history_item_list_free(Eina_List* history_items)
{
    void* d;
    EINA_LIST_FREE(history_items, d) {
        Ewk_History_Item* item = (Ewk_History_Item*)d;
        _ewk_history_item_free(item, item->core);
    }
}

/**
 * Query title for given history item.
 *
 * @param item history item to query.
 *
 * @return the title pointer, that may be @c NULL. This pointer is
 *         guaranteed to be eina_stringshare, so whenever possible
 *         save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup().
 */
const char* ewk_history_item_title_get(const Ewk_History_Item* item)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core, 0);
    // hide the following optimzation from outside
    Ewk_History_Item* i = (Ewk_History_Item*)item;
    eina_stringshare_replace(&i->title, core->title().utf8().data());
    return i->title;
}

/**
 * Query alternate title for given history item.
 *
 * @param item history item to query.
 *
 * @return the alternate title pointer, that may be @c NULL. This
 *         pointer is guaranteed to be eina_stringshare, so whenever
 *         possible save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup().
 */
const char* ewk_history_item_title_alternate_get(const Ewk_History_Item* item)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core, 0);
    // hide the following optimzation from outside
    Ewk_History_Item* i = (Ewk_History_Item*)item;
    eina_stringshare_replace(&i->alternate_title,
                             core->alternateTitle().utf8().data());
    return i->alternate_title;
}

/**
 * Set alternate title for given history item.
 *
 * @param item history item to query.
 * @param title new alternate title to use for given item. No
 *        references are kept after this function returns.
 */
void ewk_history_item_title_alternate_set(Ewk_History_Item* item, const char* title)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core);
    if (!eina_stringshare_replace(&item->alternate_title, title))
        return;
    core->setAlternateTitle(WebCore::String::fromUTF8(title));
}

/**
 * Query URI for given history item.
 *
 * @param item history item to query.
 *
 * @return the URI pointer, that may be @c NULL. This pointer is
 *         guaranteed to be eina_stringshare, so whenever possible
 *         save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup().
 */
const char* ewk_history_item_uri_get(const Ewk_History_Item* item)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core, 0);
    // hide the following optimzation from outside
    Ewk_History_Item* i = (Ewk_History_Item*)item;
    eina_stringshare_replace(&i->uri, core->urlString().utf8().data());
    return i->uri;
}

/**
 * Query original URI for given history item.
 *
 * @param item history item to query.
 *
 * @return the original URI pointer, that may be @c NULL. This pointer
 *         is guaranteed to be eina_stringshare, so whenever possible
 *         save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup().
 */
const char* ewk_history_item_uri_original_get(const Ewk_History_Item* item)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core, 0);
    // hide the following optimzation from outside
    Ewk_History_Item* i = (Ewk_History_Item*)item;
    eina_stringshare_replace(&i->original_uri,
                             core->originalURLString().utf8().data());
    return i->original_uri;
}

/**
 * Query last visited time for given history item.
 *
 * @param item history item to query.
 *
 * @return the time in seconds this item was visited.
 */
double ewk_history_item_time_last_visited_get(const Ewk_History_Item* item)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core, 0.0);
    return core->lastVisitedTime();
}

/**
 * Get the icon (aka favicon) associated with this history item.
 *
 * @note in order to have this working, one must open icon database
 *       with ewk_settings_icon_database_path_set().
 *
 * @param item history item to query.
 *
 * @return the surface reference or @c NULL on errors. Note that the
 *         reference may be to a standard fallback icon.
 */
cairo_surface_t* ewk_history_item_icon_surface_get(const Ewk_History_Item* item)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core, 0);
    WebCore::Image* icon = core->icon();
    if (!icon) {
        ERR("icon is NULL.");
        return 0;
    }
    return icon->nativeImageForCurrentFrame();
}

/**
 * Add an Evas_Object of type 'image' to given canvas with history item icon.
 *
 * This is an utility function that creates an Evas_Object of type
 * image set to have fill always match object size
 * (evas_object_image_filled_add()), saving some code to use it from Evas.
 *
 * @note in order to have this working, one must open icon database
 *       with ewk_settings_icon_database_path_set().
 *
 * @param item history item to query.
 * @param canvas evas instance where to add resulting object.
 *
 * @return newly allocated Evas_Object instance or @c NULL on
 *         errors. Delete the object with evas_object_del().
 */
Evas_Object* ewk_history_item_icon_object_add(const Ewk_History_Item* item, Evas* canvas)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(canvas, 0);
    WebCore::Image* icon = core->icon();
    cairo_surface_t* surface;

    if (!icon) {
        ERR("icon is NULL.");
        return 0;
    }

    surface = icon->nativeImageForCurrentFrame();
    return ewk_util_image_from_cairo_surface_add(canvas, surface);
}

/**
 * Query if given item is still in page cache.
 *
 * @param item history item to query.
 *
 * @return @c EINA_TRUE if in cache, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_history_item_page_cache_exists(const Ewk_History_Item* item)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core, EINA_FALSE);
    return core->isInPageCache();
}

/**
 * Query number of times item was visited.
 *
 * @param item history item to query.
 *
 * @return number of visits.
 */
int ewk_history_item_visit_count(const Ewk_History_Item* item)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core, 0);
    return core->visitCount();
}

/**
 * Query if last visit to item was failure or not.
 *
 * @param item history item to query.
 *
 * @return @c EINA_TRUE if last visit was failure, @c EINA_FALSE if it
 *         was fine.
 */
Eina_Bool ewk_history_item_visit_last_failed(const Ewk_History_Item* item)
{
    EWK_HISTORY_ITEM_CORE_GET_OR_RETURN(item, core, EINA_TRUE);
    return core->lastVisitWasFailure();
}


/* internal methods ****************************************************/
/**
 * @internal
 *
 * Creates history for given view. Called internally by ewk_view and
 * should never be called from outside.
 *
 * @param core WebCore::BackForwardList instance to use internally.
 *
 * @return newly allocated history instance or @c NULL on errors.
 */
Ewk_History* ewk_history_new(WebCore::BackForwardList* core)
{
    Ewk_History* history;
    EINA_SAFETY_ON_NULL_RETURN_VAL(core, 0);
    DBG("core=%p", core);

    history = (Ewk_History*)malloc(sizeof(Ewk_History));
    if (!history) {
        CRITICAL("Could not allocate history memory.");
        return 0;
    }

    core->ref();
    history->core = core;

    return history;
}

/**
 * @internal
 *
 * Destroys previously allocated history instance. This is called
 * automatically by ewk_view and should never be called from outside.
 *
 * @param history instance to free
 */
void ewk_history_free(Ewk_History* history)
{
    DBG("history=%p", history);
    history->core->deref();
    free(history);
}
