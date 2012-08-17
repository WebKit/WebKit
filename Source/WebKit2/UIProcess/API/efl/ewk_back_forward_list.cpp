/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ewk_back_forward_list.h"

#include "WKAPICast.h"
#include "WKArray.h"
#include "WKBackForwardList.h"
#include "WKRetainPtr.h"
#include "ewk_back_forward_list_item_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

typedef HashMap<WKBackForwardListItemRef, Ewk_Back_Forward_List_Item*> ItemsMap;

/**
 * \struct  _Ewk_Back_Forward_List
 * @brief   Contains the Back Forward List data.
 */
struct _Ewk_Back_Forward_List {
    WKRetainPtr<WKBackForwardListRef> wkList;
    mutable ItemsMap wrapperCache;

    _Ewk_Back_Forward_List(WKBackForwardListRef listRef)
        : wkList(listRef)
    { }

    ~_Ewk_Back_Forward_List()
    {
        ItemsMap::iterator it = wrapperCache.begin();
        ItemsMap::iterator end = wrapperCache.end();
        for (; it != end; ++it)
            ewk_back_forward_list_item_unref(it->second);
    }
};

#define EWK_BACK_FORWARD_LIST_WK_GET_OR_RETURN(list, wkList_, ...)  \
    if (!(list)) {                                             \
        EINA_LOG_CRIT("list is NULL.");                        \
        return __VA_ARGS__;                                    \
    }                                                          \
    if (!(list)->wkList) {                                     \
        EINA_LOG_CRIT("list->wkList is NULL.");                \
        return __VA_ARGS__;                                    \
    }                                                          \
    WKBackForwardListRef wkList_ = (list)->wkList.get()


static inline Ewk_Back_Forward_List_Item* addItemToWrapperCache(const Ewk_Back_Forward_List* list, WKBackForwardListItemRef wkItem)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(list, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(wkItem, 0);

    Ewk_Back_Forward_List_Item* item = list->wrapperCache.get(wkItem);
    if (!item) {
        item = ewk_back_forward_list_item_new(wkItem);
        list->wrapperCache.set(wkItem, item);
    }

    return item;
}

Ewk_Back_Forward_List_Item* ewk_back_forward_list_current_item_get(const Ewk_Back_Forward_List* list)
{
    EWK_BACK_FORWARD_LIST_WK_GET_OR_RETURN(list, wkList, 0);

    return addItemToWrapperCache(list, WKBackForwardListGetCurrentItem(wkList));
}

Ewk_Back_Forward_List_Item* ewk_back_forward_list_previous_item_get(const Ewk_Back_Forward_List* list)
{
    EWK_BACK_FORWARD_LIST_WK_GET_OR_RETURN(list, wkList, 0);

    return addItemToWrapperCache(list, WKBackForwardListGetBackItem(wkList));
}

Ewk_Back_Forward_List_Item* ewk_back_forward_list_next_item_get(const Ewk_Back_Forward_List* list)
{
    EWK_BACK_FORWARD_LIST_WK_GET_OR_RETURN(list, wkList, 0);

    return addItemToWrapperCache(list, WKBackForwardListGetForwardItem(wkList));
}

Ewk_Back_Forward_List_Item* ewk_back_forward_list_item_at_index_get(const Ewk_Back_Forward_List* list, int index)
{
    EWK_BACK_FORWARD_LIST_WK_GET_OR_RETURN(list, wkList, 0);

    return addItemToWrapperCache(list, WKBackForwardListGetItemAtIndex(wkList, index));
}

unsigned ewk_back_forward_list_count(Ewk_Back_Forward_List* list)
{
    EWK_BACK_FORWARD_LIST_WK_GET_OR_RETURN(list, wkList, 0);

    const unsigned currentItem = ewk_back_forward_list_current_item_get(list) ? 1 : 0;

    return WKBackForwardListGetBackListCount(wkList) + WKBackForwardListGetForwardListCount(wkList) + currentItem;
}


/**
 * @internal
 * Updates items cache.
 */
void ewk_back_forward_list_changed(Ewk_Back_Forward_List* list, WKBackForwardListItemRef wkAddedItem, WKArrayRef wkRemovedItems)
{
    if (wkAddedItem) // Checking also here to avoid EINA_SAFETY_ON_NULL_RETURN_VAL warnings.
        addItemToWrapperCache(list, wkAddedItem); // Puts new item to the cache.

    const size_t removedItemsSize = wkRemovedItems ? WKArrayGetSize(wkRemovedItems) : 0;
    for (size_t i = 0; i < removedItemsSize; ++i) {
        WKBackForwardListItemRef wkItem = static_cast<WKBackForwardListItemRef>(WKArrayGetItemAtIndex(wkRemovedItems, i));
        if (Ewk_Back_Forward_List_Item* item = list->wrapperCache.take(wkItem))
            ewk_back_forward_list_item_unref(item);
    }
}

/**
 * @internal
 * Constructs a Ewk_Back_Forward_List from a WKBackForwardListRef.
 */
Ewk_Back_Forward_List* ewk_back_forward_list_new(WKBackForwardListRef wkBackForwardListRef)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(wkBackForwardListRef, 0);

    return new Ewk_Back_Forward_List(wkBackForwardListRef);
}

/**
 * @internal
 * Frees a Ewk_Back_Forward_List object.
 */
void ewk_back_forward_list_free(Ewk_Back_Forward_List* list)
{
    EINA_SAFETY_ON_NULL_RETURN(list);

    delete list;
}
