/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this item of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this item of conditions and the following disclaimer in the
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
#include "ewk_back_forward_list_item.h"

#include "WKAPICast.h"
#include "WKBackForwardListItem.h"
#include "WKEinaSharedString.h"

using namespace WebKit;

/**
 * \struct  _Ewk_Back_Forward_List
 * @brief   Contains the Back Forward List data.
 */
struct _Ewk_Back_Forward_List_Item {
    unsigned int __ref; /**< the reference count of the object */
    WKRetainPtr<WKBackForwardListItemRef> wkItem;
    mutable WKEinaSharedString uri;
    mutable WKEinaSharedString title;
    mutable WKEinaSharedString originalUri;

    _Ewk_Back_Forward_List_Item(WKBackForwardListItemRef itemRef)
        : __ref(1)
        , wkItem(itemRef)
    { }

    ~_Ewk_Back_Forward_List_Item()
    {
        ASSERT(!__ref);
    }
};

#define EWK_BACK_FORWARD_LIST_ITEM_WK_GET_OR_RETURN(item, wkItem_, ...)    \
    if (!(item)) {                                                         \
        EINA_LOG_CRIT("item is NULL.");                                    \
        return __VA_ARGS__;                                                \
    }                                                                      \
    if (!(item)->wkItem) {                                                 \
        EINA_LOG_CRIT("item->wkItem is NULL.");                            \
        return __VA_ARGS__;                                                \
    }                                                                      \
    WKBackForwardListItemRef wkItem_ = (item)->wkItem.get()

void ewk_back_forward_list_item_ref(Ewk_Back_Forward_List_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN(item);
    ++item->__ref;
}

void ewk_back_forward_list_item_unref(Ewk_Back_Forward_List_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN(item);

    if (--item->__ref)
        return;

    delete item;
}

const char* ewk_back_forward_list_item_uri_get(const Ewk_Back_Forward_List_Item* item)
{
    EWK_BACK_FORWARD_LIST_ITEM_WK_GET_OR_RETURN(item, wkItem, 0);

    item->uri = WKEinaSharedString(AdoptWK, WKBackForwardListItemCopyURL(wkItem));

    return item->uri;
}

const char* ewk_back_forward_list_item_title_get(const Ewk_Back_Forward_List_Item* item)
{
    EWK_BACK_FORWARD_LIST_ITEM_WK_GET_OR_RETURN(item, wkItem, 0);

    item->title = WKEinaSharedString(AdoptWK, WKBackForwardListItemCopyTitle(wkItem));

    return item->title;
}

const char* ewk_back_forward_list_item_original_uri_get(const Ewk_Back_Forward_List_Item* item)
{
    EWK_BACK_FORWARD_LIST_ITEM_WK_GET_OR_RETURN(item, wkItem, 0);

    item->originalUri = WKEinaSharedString(AdoptWK, WKBackForwardListItemCopyOriginalURL(wkItem));

    return item->originalUri;
}

Ewk_Back_Forward_List_Item* ewk_back_forward_list_item_new(WKBackForwardListItemRef backForwardListItemData)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(backForwardListItemData, 0);

    return new Ewk_Back_Forward_List_Item(backForwardListItemData);
}
