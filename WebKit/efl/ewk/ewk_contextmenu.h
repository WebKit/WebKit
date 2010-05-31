/*
    Copyright (C) 2010 ProFUSION embedded systems
    Copyright (C) 2010 Samsung Electronics

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

#ifndef ewk_contextmenu_h
#define ewk_contextmenu_h

#include "ewk_eapi.h"

#include <Eina.h>
#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

// keep this in sync with ContextMenuItem.h
enum _Ewk_Context_Menu_Action {
    EWK_CONTEXT_MENU_ITEM_TAG_NO_ACTION = 0, // this item is not actually in web_uidelegate.h
    EWK_CONTEXT_MENU_ITEM_TAG_OPEN_LINK_IN_NEW_WINDOW = 1,
    EWK_CONTEXT_MENU_ITEM_TAG_DOWNLOAD_LINK_TO_DISK,
    EWK_CONTEXT_MENU_ITEM_TAG_COPY_LINK_TO_CLIPBOARD,
    EWK_CONTEXT_MENU_ITEM_TAG_OPEN_IMAGE_IN_NEW_WINDOW,
    EWK_CONTEXT_MENU_ITEM_TAG_DOWNLOAD_IMAGE_TO_DISK,
    EWK_CONTEXT_MENU_ITEM_TAG_COPY_IMAGE_TO_CLIPBOARD,
    EWK_CONTEXT_MENU_ITEM_TAG_OPEN_FRAME_IN_NEW_WINDOW,
    EWK_CONTEXT_MENU_ITEM_TAG_COPY,
    EWK_CONTEXT_MENU_ITEM_TAG_GO_BACK,
    EWK_CONTEXT_MENU_ITEM_TAG_GO_FORWARD,
    EWK_CONTEXT_MENU_ITEM_TAG_STOP,
    EWK_CONTEXT_MENU_ITEM_TAG_RELOAD,
    EWK_CONTEXT_MENU_ITEM_TAG_CUT,
    EWK_CONTEXT_MENU_ITEM_TAG_PASTE,
    EWK_CONTEXT_MENU_ITEM_TAG_SPELLING_GUESS,
    EWK_CONTEXT_MENU_ITEM_TAG_NO_GUESSES_FOUND,
    EWK_CONTEXT_MENU_ITEM_TAG_IGNORE_SPELLING,
    EWK_CONTEXT_MENU_ITEM_TAG_LEARN_SPELLING,
    EWK_CONTEXT_MENU_ITEM_TAG_OTHER,
    EWK_CONTEXT_MENU_ITEM_TAG_SEARCH_IN_SPOTLIGHT,
    EWK_CONTEXT_MENU_ITEM_TAG_SEARCH_WEB,
    EWK_CONTEXT_MENU_ITEM_TAG_LOOK_UP_IN_DICTIONARY,
    EWK_CONTEXT_MENU_ITEM_TAG_OPEN_WITH_DEFAULT_APPLICATION,
    EWK_CONTEXT_MENU_ITEM_PDFACTUAL_SIZE,
    EWK_CONTEXT_MENU_ITEM_PDFZOOM_IN,
    EWK_CONTEXT_MENU_ITEM_PDFZOOM_OUT,
    EWK_CONTEXT_MENU_ITEM_PDFAUTO_SIZE,
    EWK_CONTEXT_MENU_ITEM_PDFSINGLE_PAGE,
    EWK_CONTEXT_MENU_ITEM_PDFFACING_PAGES,
    EWK_CONTEXT_MENU_ITEM_PDFCONTINUOUS,
    EWK_CONTEXT_MENU_ITEM_PDFNEXT_PAGE,
    EWK_CONTEXT_MENU_ITEM_PDFPREVIOUS_PAGE,
    EWK_CONTEXT_MENU_ITEM_TAG_OPEN_LINK = 2000,
    EWK_CONTEXT_MENU_ITEM_TAG_IGNORE_GRAMMAR,
    EWK_CONTEXT_MENU_ITEM_TAG_SPELLING_MENU, // spelling or spelling/grammar sub-menu
    EWK_CONTEXT_MENU_ITEM_TAG_SHOW_SPELLING_PANEL,
    EWK_CONTEXT_MENU_ITEM_TAG_CHECK_SPELLING,
    EWK_CONTEXT_MENU_ITEM_TAG_CHECK_SPELLING_WHILE_TYPING,
    EWK_CONTEXT_MENU_ITEM_TAG_CHECK_GRAMMAR_WITH_SPELLING,
    EWK_CONTEXT_MENU_ITEM_TAG_FONT_MENU, // font sub-menu
    EWK_CONTEXT_MENU_ITEM_TAG_SHOW_FONTS,
    EWK_CONTEXT_MENU_ITEM_TAG_BOLD,
    EWK_CONTEXT_MENU_ITEM_TAG_ITALIC,
    EWK_CONTEXT_MENU_ITEM_TAG_UNDERLINE,
    EWK_CONTEXT_MENU_ITEM_TAG_OUTLINE,
    EWK_CONTEXT_MENU_ITEM_TAG_STYLES,
    EWK_CONTEXT_MENU_ITEM_TAG_SHOW_COLORS,
    EWK_CONTEXT_MENU_ITEM_TAG_SPEECH_MENU, // speech sub-menu
    EWK_CONTEXT_MENU_ITEM_TAG_START_SPEAKING,
    EWK_CONTEXT_MENU_ITEM_TAG_STOP_SPEAKING,
    EWK_CONTEXT_MENU_ITEM_TAG_WRITING_DIRECTION_MENU, // writing direction sub-menu
    EWK_CONTEXT_MENU_ITEM_TAG_DEFAULT_DIRECTION,
    EWK_CONTEXT_MENU_ITEM_TAG_LEFT_TO_RIGHT,
    EWK_CONTEXT_MENU_ITEM_TAG_RIGHT_TO_LEFT,
    EWK_CONTEXT_MENU_ITEM_TAG_PDFSINGLE_PAGE_SCROLLING,
    EWK_CONTEXT_MENU_ITEM_TAG_PDFFACING_PAGES_SCROLLING,
    EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_MENU, // text direction sub-menu
    EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_DEFAULT,
    EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_LEFT_TO_RIGHT,
    EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_RIGHT_TO_LEFT,
    EWK_CONTEXT_MENU_ITEM_BASE_CUSTOM_TAG = 5000,
    EWK_CONTEXT_MENU_ITEM_BASE_APPLICATION_TAG = 10000
};
typedef enum _Ewk_Context_Menu_Action Ewk_Context_Menu_Action;

// keep this in sync with ContextMenuItem.h
enum _Ewk_Context_Menu_Item_Type {
    EWK_ACTION_TYPE,
    EWK_CHECKABLE_ACTION_TYPE,
    EWK_SEPARATOR_TYPE,
    EWK_SUBMENU_TYPE
};
typedef enum _Ewk_Context_Menu_Item_Type    Ewk_Context_Menu_Item_Type;

/**
 * The structure to contain Context Menu data
 */
typedef struct _Ewk_Context_Menu            Ewk_Context_Menu;

/**
 * Represents one item from Ewk_Context_Menu
 */
typedef struct _Ewk_Context_Menu_Item       Ewk_Context_Menu_Item;



/************************** Exported functions ***********************/

EAPI void                        ewk_context_menu_ref(Ewk_Context_Menu* menu);
EAPI void                        ewk_context_menu_unref(Ewk_Context_Menu* menu);
EAPI Eina_Bool                   ewk_context_menu_destroy(Ewk_Context_Menu* menu);

EAPI const Eina_List*            ewk_context_menu_item_list_get(Ewk_Context_Menu* o);

EAPI Ewk_Context_Menu_Item*      ewk_context_menu_item_new(Ewk_Context_Menu_Item_Type type, Ewk_Context_Menu_Action action, Ewk_Context_Menu* submenu, const char* title, Eina_Bool checked, Eina_Bool enabled);
EAPI void                        ewk_context_menu_item_free(Ewk_Context_Menu_Item* item);
EAPI Eina_Bool                   ewk_context_menu_item_select(Ewk_Context_Menu* menu, Ewk_Context_Menu_Item* item);
EAPI Ewk_Context_Menu_Item_Type  ewk_context_menu_item_type_get(Ewk_Context_Menu_Item* o);
EAPI Eina_Bool                   ewk_context_menu_item_type_set(Ewk_Context_Menu_Item* o, Ewk_Context_Menu_Item_Type type);
EAPI Ewk_Context_Menu_Action     ewk_context_menu_item_action_get(Ewk_Context_Menu_Item* o);
EAPI Eina_Bool                   ewk_context_menu_item_action_set(Ewk_Context_Menu_Item* o, Ewk_Context_Menu_Action action);
EAPI const char*                 ewk_context_menu_item_title_get(Ewk_Context_Menu_Item* o);
EAPI const char*                 ewk_context_menu_item_title_set(Ewk_Context_Menu_Item* o, const char* title);
EAPI Eina_Bool                   ewk_context_menu_item_checked_get(Ewk_Context_Menu_Item* o);
EAPI Eina_Bool                   ewk_context_menu_item_checked_set(Ewk_Context_Menu_Item* o, Eina_Bool checked);
EAPI Eina_Bool                   ewk_context_menu_item_enabled_get(Ewk_Context_Menu_Item* o);
EAPI Eina_Bool                   ewk_context_menu_item_enabled_set(Ewk_Context_Menu_Item* o, Eina_Bool enabled);

#ifdef __cplusplus
}
#endif
#endif // ewk_contextmenu_h
