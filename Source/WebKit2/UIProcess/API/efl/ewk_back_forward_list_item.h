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

/**
 * @file    ewk_back_forward_list_item.h
 * @brief   Describes the Ewk Back Forward List Item API.
 */

#ifndef ewk_back_forward_list_item_h
#define ewk_back_forward_list_item_h

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Creates a type name for Ewk_Back_Forward_List_Item */
typedef struct Ewk_Back_Forward_List_Item Ewk_Back_Forward_List_Item;

/**
 * Increases the reference count of the given object.
 *
 * @param item the back-forward list item instance to increase the reference count
 *
 * @return a pointer to the object on success, @c NULL otherwise.
 */
EAPI Ewk_Back_Forward_List_Item *ewk_back_forward_list_item_ref(Ewk_Back_Forward_List_Item *item);

/**
 * Decreases the reference count of the given object, possibly freeing it.
 *
 * When the reference count reaches 0, the item is freed.
 *
 * @param item the back-forward list item instance to decrease the reference count
 */
EAPI void ewk_back_forward_list_item_unref(Ewk_Back_Forward_List_Item *item);

/**
 * Returns URL of the item.
 *
 * The returned URL may differ from the original URL (For example if the page was redirected).
 *
 * @see ewk_back_forward_list_item_original_url_get()
 *
 * @param item the back-forward list item instance
 *
 * @return the URL of the @a item or @c NULL in case of error. This pointer is
 *         guaranteed to be eina_stringshare, so whenever possible
 *         save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup()
 */
EAPI const char *ewk_back_forward_list_item_url_get(const Ewk_Back_Forward_List_Item *item);

/**
 * Returns title of the item.
 *
 * @param item the back-forward list item instance
 *
 * @return the title of the @a item or @c NULL in case of error. This pointer is
 *         guaranteed to be eina_stringshare, so whenever possible
 *         save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup()
 */
EAPI const char *ewk_back_forward_list_item_title_get(const Ewk_Back_Forward_List_Item *item);

/**
 * Returns original URL of the item.
 *
 * @see ewk_back_forward_list_item_url_get()
 *
 * @param item the back-forward list item instance
 *
 * @return the original URL of the @a item or @c NULL in case of error. This pointer is
 *         guaranteed to be eina_stringshare, so whenever possible
 *         save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup()
 */
EAPI const char *ewk_back_forward_list_item_original_url_get(const Ewk_Back_Forward_List_Item *item);

#ifdef __cplusplus
}
#endif
#endif // ewk_back_forward_list_item_h
