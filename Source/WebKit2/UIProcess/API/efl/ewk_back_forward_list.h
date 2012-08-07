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
 * @file    ewk_back_forward_list.h
 * @brief   Describes the Ewk Back Forward List API.
 */

#ifndef ewk_back_forward_list_h
#define ewk_back_forward_list_h

#include "ewk_back_forward_list_item.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Creates a type name for _Ewk_Back_Forward_List */
typedef struct _Ewk_Back_Forward_List Ewk_Back_Forward_List;

/**
 * Returns the current item in the @a list.
 *
 * @param list the back-forward list instance
 *
 * @return the current item in the @a list or @c NULL in case of error
 */
EAPI Ewk_Back_Forward_List_Item *ewk_back_forward_list_current_item_get(const Ewk_Back_Forward_List *list);

/**
 * Returns the item that precedes the current item in the @a list.
 *
 * @param list the back-forward list instance
 *
 * @return the item that precedes the current item the @a list or @c NULL in case of error
 */
EAPI Ewk_Back_Forward_List_Item *ewk_back_forward_list_previous_item_get(const Ewk_Back_Forward_List *list);

/**
 * Returns the item that follows the current item in the @a list.
 *
 * @param list the back-forward list instance
 *
 * @return the item that follows the current item in the @a list or @c NULL in case of error
 */
EAPI Ewk_Back_Forward_List_Item *ewk_back_forward_list_next_item_get(const Ewk_Back_Forward_List *list);

/**
 * Returns the item at a given @a index relative to the current item.
 *
 * @param list the back-forward list instance
 * @param index the index of the item
 *
 * @return the item at a given @a index relative to the current item or @c NULL in case of error
 */
EAPI Ewk_Back_Forward_List_Item *ewk_back_forward_list_item_at_index_get(const Ewk_Back_Forward_List *list, int index);

/**
 * Returns the length of the back-forward list including current item.
 *
 * @param list the back-forward list instance
 *
 * @return the length of the back-forward list including current item or @c 0 in case of error
 */
EAPI unsigned ewk_back_forward_list_count(Ewk_Back_Forward_List *list);

#ifdef __cplusplus
}
#endif
#endif // ewk_back_forward_list_h
