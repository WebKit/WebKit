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
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WKURL.h"
#include <wtf/text/CString.h>

using namespace WebKit;

class WKEinaSharedString {
public:
    template <typename WKRefType>
    WKEinaSharedString(WKAdoptTag, WKRefType strRef)
        : m_string(0)
    {
        if (strRef) {
            m_string = eina_stringshare_add(toImpl(strRef)->string().utf8().data());
            ASSERT(m_string);
            WKRelease(strRef); // Have stored a copy into eina_stringshare, do not need adopted strRef.
        }
    }

    template <typename WKRefType>
    WKEinaSharedString(WKRefType strRef)
        : m_string(0)
    {
        if (strRef) {
            m_string = eina_stringshare_add(toImpl(strRef)->string().utf8().data());
            ASSERT(m_string);
        }
    }

    ~WKEinaSharedString() { eina_stringshare_del(m_string); }
    inline operator const char* () const { return m_string; }

private:
    const char* m_string;
};

/**
 * \struct  _Ewk_Back_Forward_List
 * @brief   Contains the Back Forward List data.
 */
struct _Ewk_Back_Forward_List_Item {
    unsigned int __ref; /**< the reference count of the object */
    WKRetainPtr<WKBackForwardListItemRef> wkItem;
    WKEinaSharedString uri;
    WKEinaSharedString title;
    WKEinaSharedString originalUri;

    _Ewk_Back_Forward_List_Item(WKBackForwardListItemRef itemRef)
        : __ref(1)
        , wkItem(itemRef)
        , uri(AdoptWK, WKBackForwardListItemCopyURL(itemRef))
        , title(AdoptWK, WKBackForwardListItemCopyTitle(itemRef))
        , originalUri(AdoptWK, WKBackForwardListItemCopyOriginalURL(itemRef))
    { }

    ~_Ewk_Back_Forward_List_Item()
    {
        ASSERT(!__ref);
    }
};

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
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, 0);

    return item->uri;
}

const char* ewk_back_forward_list_item_title_get(const Ewk_Back_Forward_List_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, 0);

    return item->title;
}

const char* ewk_back_forward_list_item_original_uri_get(const Ewk_Back_Forward_List_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, 0);

    return item->originalUri;
}

Ewk_Back_Forward_List_Item* ewk_back_forward_list_item_new(WKBackForwardListItemRef backForwardListItemData)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(backForwardListItemData, 0);

    return new Ewk_Back_Forward_List_Item(backForwardListItemData);
}
