/*
 * Copyright (C) 2012 Samsung Electronics
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file    ewk_popup_menu_item.h
 * @brief   Describes the Ewk Popup Menu Item API.
 */

#ifndef ewk_popup_menu_item_h
#define ewk_popup_menu_item_h

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Enum values containing type of popup menu item. */
typedef enum {
    EWK_POPUP_MENU_SEPARATOR,
    EWK_POPUP_MENU_ITEM,
    EWK_POPUP_MENU_UNKNOWN = -1
} Ewk_Popup_Menu_Item_Type;

/** Creates a type name for _Ewk_Popup_Menu_Item */
typedef struct _Ewk_Popup_Menu_Item Ewk_Popup_Menu_Item;

/**
 * Returns type of the popup menu item.
 *
 * @param item the popup menu item instance
 *
 * @return the type of the @a item or @c EWK_POPUP_MENU_UNKNOWN in case of error.
 */
EAPI Ewk_Popup_Menu_Item_Type ewk_popup_menu_item_type_get(const Ewk_Popup_Menu_Item *item);

/**
 * Returns text of the popup menu item.
 *
 * @param item the popup menu item instance
 *
 * @return the text of the @a item or @c NULL in case of error. This pointer is
 *         guaranteed to be eina_stringshare, so whenever possible
 *         save yourself some cpu cycles and use
 *         eina_stringshare_ref() instead of eina_stringshare_add() or
 *         strdup()
 */
EAPI const char *ewk_popup_menu_item_text_get(const Ewk_Popup_Menu_Item *item);
    
#ifdef __cplusplus
}
#endif
#endif // ewk_popup_menu_item_h
