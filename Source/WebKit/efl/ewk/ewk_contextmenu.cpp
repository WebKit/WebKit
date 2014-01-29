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

#include "config.h"
#include "ewk_contextmenu.h"

#include "ContextMenu.h"
#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "ewk_contextmenu_private.h"
#include <Eina.h>
#include <eina_safety_checks.h>
#include <wtf/text/CString.h>

/**
 * \struct  _Ewk_Context_Menu
 * @brief   Contains the context menu data.
 */
struct _Ewk_Context_Menu {
    unsigned int __ref; /**< the reference count of the object */
#if ENABLE(CONTEXT_MENUS)
    WebCore::ContextMenuController* controller; /**< the WebCore's object which is responsible for the context menu */
#endif
    Evas_Object* view; /**< the view object */

    Eina_List* items; /**< the list of items */
};

/**
 * \struct  _Ewk_Context_Menu_Item
 * @brief   Represents one item of the context menu object.
 */
struct _Ewk_Context_Menu_Item {
    Ewk_Context_Menu_Item_Type type; /**< contains the type of the item */
    Ewk_Context_Menu_Action action; /**< contains the action of the item */

    const char* title; /**< contains the title of the item */
    Ewk_Context_Menu* submenu; /**< contains the pointer to the submenu of the item */
    Ewk_Context_Menu* parentMenu; /**< contains the pointer to parent menu of the item */

    bool checked : 1;
    bool enabled : 1;
};

#if ENABLE(CONTEXT_MENUS)
inline static Ewk_Context_Menu_Action toEwkContextMenuAction(WebCore::ContextMenuAction action)
{
    switch (action) {
    case WebCore::ContextMenuItemTagNoAction:
        return EWK_CONTEXT_MENU_ITEM_TAG_NO_ACTION;
    case WebCore::ContextMenuItemTagOpenLinkInNewWindow:
        return EWK_CONTEXT_MENU_ITEM_TAG_OPEN_LINK_IN_NEW_WINDOW;
    case WebCore::ContextMenuItemTagDownloadLinkToDisk:
        return EWK_CONTEXT_MENU_ITEM_TAG_DOWNLOAD_LINK_TO_DISK;
    case WebCore::ContextMenuItemTagCopyLinkToClipboard:
        return EWK_CONTEXT_MENU_ITEM_TAG_COPY_LINK_TO_CLIPBOARD;
    case WebCore::ContextMenuItemTagOpenImageInNewWindow:
        return EWK_CONTEXT_MENU_ITEM_TAG_OPEN_IMAGE_IN_NEW_WINDOW;
    case WebCore::ContextMenuItemTagDownloadImageToDisk:
        return EWK_CONTEXT_MENU_ITEM_TAG_DOWNLOAD_IMAGE_TO_DISK;
    case WebCore::ContextMenuItemTagCopyImageToClipboard:
        return EWK_CONTEXT_MENU_ITEM_TAG_COPY_IMAGE_TO_CLIPBOARD;
    case WebCore::ContextMenuItemTagCopyImageUrlToClipboard:
        return EWK_CONTEXT_MENU_ITEM_TAG_COPY_IMAGE_URL_TO_CLIPBOARD;
    case WebCore::ContextMenuItemTagOpenFrameInNewWindow:
        return EWK_CONTEXT_MENU_ITEM_TAG_OPEN_FRAME_IN_NEW_WINDOW;
    case WebCore::ContextMenuItemTagCopy:
        return EWK_CONTEXT_MENU_ITEM_TAG_COPY;
    case WebCore::ContextMenuItemTagGoBack:
        return EWK_CONTEXT_MENU_ITEM_TAG_GO_BACK;
    case WebCore::ContextMenuItemTagGoForward:
        return EWK_CONTEXT_MENU_ITEM_TAG_GO_FORWARD;
    case WebCore::ContextMenuItemTagStop:
        return EWK_CONTEXT_MENU_ITEM_TAG_STOP;
    case WebCore::ContextMenuItemTagReload:
        return EWK_CONTEXT_MENU_ITEM_TAG_RELOAD;
    case WebCore::ContextMenuItemTagCut:
        return EWK_CONTEXT_MENU_ITEM_TAG_CUT;
    case WebCore::ContextMenuItemTagPaste:
        return EWK_CONTEXT_MENU_ITEM_TAG_PASTE;
    case WebCore::ContextMenuItemTagSelectAll:
        return EWK_CONTEXT_MENU_ITEM_TAG_SELECT_ALL;
    case WebCore::ContextMenuItemTagSpellingGuess:
        return EWK_CONTEXT_MENU_ITEM_TAG_SPELLING_GUESS;
    case WebCore::ContextMenuItemTagNoGuessesFound:
        return EWK_CONTEXT_MENU_ITEM_TAG_NO_GUESSES_FOUND;
    case WebCore::ContextMenuItemTagIgnoreSpelling:
        return EWK_CONTEXT_MENU_ITEM_TAG_IGNORE_SPELLING;
    case WebCore::ContextMenuItemTagLearnSpelling:
        return EWK_CONTEXT_MENU_ITEM_TAG_LEARN_SPELLING;
    case WebCore::ContextMenuItemTagOther:
        return EWK_CONTEXT_MENU_ITEM_TAG_OTHER;
    case WebCore::ContextMenuItemTagSearchInSpotlight:
        return EWK_CONTEXT_MENU_ITEM_TAG_SEARCH_IN_SPOTLIGHT;
    case WebCore::ContextMenuItemTagSearchWeb:
        return EWK_CONTEXT_MENU_ITEM_TAG_SEARCH_WEB;
    case WebCore::ContextMenuItemTagLookUpInDictionary:
        return EWK_CONTEXT_MENU_ITEM_TAG_LOOK_UP_IN_DICTIONARY;
    case WebCore::ContextMenuItemTagOpenWithDefaultApplication:
        return EWK_CONTEXT_MENU_ITEM_TAG_OPEN_WITH_DEFAULT_APPLICATION;
    case WebCore::ContextMenuItemPDFActualSize:
        return EWK_CONTEXT_MENU_ITEM_PDFACTUAL_SIZE;
    case WebCore::ContextMenuItemPDFZoomIn:
        return EWK_CONTEXT_MENU_ITEM_PDFZOOM_IN;
    case WebCore::ContextMenuItemPDFZoomOut:
        return EWK_CONTEXT_MENU_ITEM_PDFZOOM_OUT;
    case WebCore::ContextMenuItemPDFAutoSize:
        return EWK_CONTEXT_MENU_ITEM_PDFAUTO_SIZE;
    case WebCore::ContextMenuItemPDFSinglePage:
        return EWK_CONTEXT_MENU_ITEM_PDFSINGLE_PAGE;
    case WebCore::ContextMenuItemPDFFacingPages:
        return EWK_CONTEXT_MENU_ITEM_PDFFACING_PAGES;
    case WebCore::ContextMenuItemPDFContinuous:
        return EWK_CONTEXT_MENU_ITEM_PDFCONTINUOUS;
    case WebCore::ContextMenuItemPDFNextPage:
        return EWK_CONTEXT_MENU_ITEM_PDFNEXT_PAGE;
    case WebCore::ContextMenuItemPDFPreviousPage:
        return EWK_CONTEXT_MENU_ITEM_PDFPREVIOUS_PAGE;
    case WebCore::ContextMenuItemTagOpenLink:
        return EWK_CONTEXT_MENU_ITEM_TAG_OPEN_LINK;
    case WebCore::ContextMenuItemTagIgnoreGrammar:
        return EWK_CONTEXT_MENU_ITEM_TAG_IGNORE_GRAMMAR;
    case WebCore::ContextMenuItemTagSpellingMenu:
        return EWK_CONTEXT_MENU_ITEM_TAG_SPELLING_MENU;
    case WebCore::ContextMenuItemTagShowSpellingPanel:
        return EWK_CONTEXT_MENU_ITEM_TAG_SHOW_SPELLING_PANEL;
    case WebCore::ContextMenuItemTagCheckSpelling:
        return EWK_CONTEXT_MENU_ITEM_TAG_CHECK_SPELLING;
    case WebCore::ContextMenuItemTagCheckSpellingWhileTyping:
        return EWK_CONTEXT_MENU_ITEM_TAG_CHECK_SPELLING_WHILE_TYPING;
    case WebCore::ContextMenuItemTagCheckGrammarWithSpelling:
        return EWK_CONTEXT_MENU_ITEM_TAG_CHECK_GRAMMAR_WITH_SPELLING;
    case WebCore::ContextMenuItemTagFontMenu:
        return EWK_CONTEXT_MENU_ITEM_TAG_FONT_MENU;
    case WebCore::ContextMenuItemTagShowFonts:
        return EWK_CONTEXT_MENU_ITEM_TAG_SHOW_FONTS;
    case WebCore::ContextMenuItemTagBold:
        return EWK_CONTEXT_MENU_ITEM_TAG_BOLD;
    case WebCore::ContextMenuItemTagItalic:
        return EWK_CONTEXT_MENU_ITEM_TAG_ITALIC;
    case WebCore::ContextMenuItemTagUnderline:
        return EWK_CONTEXT_MENU_ITEM_TAG_UNDERLINE;
    case WebCore::ContextMenuItemTagOutline:
        return EWK_CONTEXT_MENU_ITEM_TAG_OUTLINE;
    case WebCore::ContextMenuItemTagStyles:
        return EWK_CONTEXT_MENU_ITEM_TAG_STYLES;
    case WebCore::ContextMenuItemTagShowColors:
        return EWK_CONTEXT_MENU_ITEM_TAG_SHOW_COLORS;
    case WebCore::ContextMenuItemTagSpeechMenu:
        return EWK_CONTEXT_MENU_ITEM_TAG_SPEECH_MENU;
    case WebCore::ContextMenuItemTagStartSpeaking:
        return EWK_CONTEXT_MENU_ITEM_TAG_START_SPEAKING;
    case WebCore::ContextMenuItemTagStopSpeaking:
        return EWK_CONTEXT_MENU_ITEM_TAG_STOP_SPEAKING;
    case WebCore::ContextMenuItemTagWritingDirectionMenu:
        return EWK_CONTEXT_MENU_ITEM_TAG_WRITING_DIRECTION_MENU;
    case WebCore::ContextMenuItemTagDefaultDirection:
        return EWK_CONTEXT_MENU_ITEM_TAG_DEFAULT_DIRECTION;
    case WebCore::ContextMenuItemTagLeftToRight:
        return EWK_CONTEXT_MENU_ITEM_TAG_LEFT_TO_RIGHT;
    case WebCore::ContextMenuItemTagRightToLeft:
        return EWK_CONTEXT_MENU_ITEM_TAG_RIGHT_TO_LEFT;
    case WebCore::ContextMenuItemTagPDFSinglePageScrolling:
        return EWK_CONTEXT_MENU_ITEM_TAG_PDFSINGLE_PAGE_SCROLLING;
    case WebCore::ContextMenuItemTagPDFFacingPagesScrolling:
        return EWK_CONTEXT_MENU_ITEM_TAG_PDFFACING_PAGES_SCROLLING;
#if ENABLE(INSPECTOR)
    case WebCore::ContextMenuItemTagInspectElement:
        return EWK_CONTEXT_MENU_ITEM_TAG_INSPECT_ELEMENT;
#endif
    case WebCore::ContextMenuItemTagTextDirectionMenu:
        return EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_MENU;
    case WebCore::ContextMenuItemTagTextDirectionDefault:
        return EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_DEFAULT;
    case WebCore::ContextMenuItemTagTextDirectionLeftToRight:
        return EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_LEFT_TO_RIGHT;
    case WebCore::ContextMenuItemTagTextDirectionRightToLeft:
        return EWK_CONTEXT_MENU_ITEM_TAG_TEXT_DIRECTION_RIGHT_TO_LEFT;
    case WebCore::ContextMenuItemTagOpenMediaInNewWindow:
        return EWK_CONTEXT_MENU_ITEM_OPEN_MEDIA_IN_NEW_WINDOW;
    case WebCore::ContextMenuItemTagDownloadMediaToDisk:
        return EWK_CONTEXT_MENU_ITEM_TAG_DOWNLOAD_MEDIA_TO_DISK;
    case WebCore::ContextMenuItemTagCopyMediaLinkToClipboard:
        return EWK_CONTEXT_MENU_ITEM_TAG_COPY_MEDIA_LINK_TO_CLIPBOARD;
    case WebCore::ContextMenuItemTagToggleMediaControls:
        return EWK_CONTEXT_MENU_ITEM_TAG_TOGGLE_MEDIA_CONTROLS;
    case WebCore::ContextMenuItemTagToggleMediaLoop:
        return EWK_CONTEXT_MENU_ITEM_TAG_TOGGLE_MEDIA_LOOP;
    case WebCore::ContextMenuItemTagEnterVideoFullscreen:
        return EWK_CONTEXT_MENU_ITEM_TAG_ENTER_VIDEO_FULLSCREEN;
    case WebCore::ContextMenuItemTagMediaPlayPause:
        return EWK_CONTEXT_MENU_ITEM_TAG_MEDIA_PLAY_PAUSE;
    case WebCore::ContextMenuItemTagMediaMute:
        return EWK_CONTEXT_MENU_ITEM_TAG_MEDIA_MUTE;
    case WebCore::ContextMenuItemTagDictationAlternative:
        return EWK_CONTEXT_MENU_ITEM_TAG_DICTATION_ALTERNATIVE;
    case WebCore::ContextMenuItemTagOpenLinkInThisWindow:
        return EWK_CONTEXT_MENU_ITEM_TAG_OPEN_LINK_IN_THIS_WINDOW;
    case WebCore::ContextMenuItemTagToggleVideoFullscreen:
        return EWK_CONTEXT_MENU_ITEM_TAG_TOGGLE_VIDEO_FULL_SCREEN;
    case WebCore::ContextMenuItemBaseCustomTag:
        return EWK_CONTEXT_MENU_ITEM_BASE_CUSTOM_TAG;
    case WebCore::ContextMenuItemCustomTagNoAction:
        return EWK_CONTEXT_MENU_ITEM_CUSTOM_TAG_NO_ACTION;
    case WebCore::ContextMenuItemLastCustomTag:
        return EWK_CONTEXT_MENU_ITEM_LAST_CUSTOM_TAG;
    case WebCore::ContextMenuItemBaseApplicationTag:
        return EWK_CONTEXT_MENU_ITEM_BASE_APPLICATION_TAG;
    }
}
#endif

void ewk_context_menu_ref(Ewk_Context_Menu* menu)
{
    EINA_SAFETY_ON_NULL_RETURN(menu);
    menu->__ref++;
}

void ewk_context_menu_unref(Ewk_Context_Menu* menu)
{
    EINA_SAFETY_ON_NULL_RETURN(menu);
    void* item;

    if (--menu->__ref)
        return;

    EINA_LIST_FREE(menu->items, item)
        ewk_context_menu_item_free(static_cast<Ewk_Context_Menu_Item*>(item));

    delete menu;
}

Eina_Bool ewk_context_menu_destroy(Ewk_Context_Menu* menu)
{
#if ENABLE(CONTEXT_MENUS)
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu->controller, false);
    menu->controller->clearContextMenu();
    ewk_context_menu_free(menu);
    return true;
#else
    UNUSED_PARAM(menu);
    return false;
#endif
}

const Eina_List* ewk_context_menu_item_list_get(const Ewk_Context_Menu* menu)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, 0);

    return menu->items;
}

Ewk_Context_Menu_Item* ewk_context_menu_item_new(Ewk_Context_Menu_Item_Type type, Ewk_Context_Menu_Action action, Ewk_Context_Menu* parentMenu,
    Ewk_Context_Menu* submenu, const char* title, Eina_Bool checked, Eina_Bool enabled)
{
    Ewk_Context_Menu_Item* item = new Ewk_Context_Menu_Item;
    item->type = type;
    item->action = action;
    item->title = eina_stringshare_add(title);
    item->parentMenu = parentMenu;
    item->submenu = submenu;
    item->checked = checked;
    item->enabled = enabled;

    return item;
}

Eina_Bool ewk_context_menu_item_select(Ewk_Context_Menu* menu, Ewk_Context_Menu_Item* item)
{
#if ENABLE(CONTEXT_MENUS)
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);
    WebCore::ContextMenuAction action = static_cast<WebCore::ContextMenuAction>(item->action);
    WebCore::ContextMenuItemType type = static_cast<WebCore::ContextMenuItemType>(item->type);

    // Don't care about title and submenu as they're not used after this point.
    WebCore::ContextMenuItem core(type, action, WTF::String());
    menu->controller->contextMenuItemSelected(&core);
    return true;
#else
    UNUSED_PARAM(menu);
    UNUSED_PARAM(item);
    return false;
#endif
}

void ewk_context_menu_item_free(Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN(item);

    eina_stringshare_del(item->title);
    delete item;
}

Ewk_Context_Menu_Item_Type ewk_context_menu_item_type_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, EWK_ACTION_TYPE);
    return item->type;
}

Eina_Bool ewk_context_menu_item_type_set(Ewk_Context_Menu_Item* item, Ewk_Context_Menu_Item_Type type)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);
    item->type = type;
    return true;
}

Ewk_Context_Menu_Action ewk_context_menu_item_action_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, EWK_CONTEXT_MENU_ITEM_TAG_NO_ACTION);
    return item->action;
}

Eina_Bool ewk_context_menu_item_action_set(Ewk_Context_Menu_Item* item, Ewk_Context_Menu_Action action)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);
    item->action = action;
    return true;
}

const char* ewk_context_menu_item_title_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, 0);
    return item->title;
}

const char* ewk_context_menu_item_title_set(Ewk_Context_Menu_Item* item, const char* title)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, 0);
    eina_stringshare_replace(&item->title, title);
    return item->title;
}

Eina_Bool ewk_context_menu_item_checked_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);
    return item->checked;
}

Eina_Bool ewk_context_menu_item_checked_set(Ewk_Context_Menu_Item* item, Eina_Bool checked)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);
    item->checked = checked;
    return true;
}

Eina_Bool ewk_context_menu_item_enabled_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);
    return item->enabled;
}

Eina_Bool ewk_context_menu_item_enabled_set(Ewk_Context_Menu_Item* item, Eina_Bool enabled)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);
    item->enabled = enabled;
    return true;
}

Ewk_Context_Menu* ewk_context_menu_item_parent_get(const Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, 0);

    return item->parentMenu;
}

/* internal methods ****************************************************/

#if ENABLE(CONTEXT_MENUS)
/**
 * @internal
 *
 * Creates an empty context menu on view.
 *
 * @param view the view object
 * @param controller the WebCore's context menu controller
 * @return newly allocated the context menu on success or @c 0 on errors
 *
 * @note emits a signal "contextmenu,new"
 */
Ewk_Context_Menu* ewk_context_menu_new(Evas_Object* view, WebCore::ContextMenuController* controller, WebCore::ContextMenu* coreMenu)
{
    Ewk_Context_Menu* menu;
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(controller, 0);

    menu = new Ewk_Context_Menu;

    menu->__ref = 1;
    menu->view = view;
    menu->controller = controller;
    menu->items = 0;
    evas_object_smart_callback_call(menu->view, "contextmenu,new", menu);

    const Vector<WebCore::ContextMenuItem>& itemsList = coreMenu->items();
    for (Vector<WebCore::ContextMenuItem>::const_iterator iter = itemsList.begin(); iter != itemsList.end(); ++iter)
        ewk_context_menu_item_append(menu, *iter);

    return menu;
}

/**
 * @internal
 *
 * Frees the context menu.
 *
 * @param menu the view object
 * @return @c true on success, or @c false on failure
 *
 * @note emits a signal "contextmenu,free"
 *
 * @see ewk_context_menu_unref
 * @see ewk_context_menu_destroy
 */
bool ewk_context_menu_free(Ewk_Context_Menu* menu)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, false);
    evas_object_smart_callback_call(menu->view, "contextmenu,free", menu);
    ewk_context_menu_unref(menu);
    return true;
}

/**
 * @internal
 *
 * Appends the WebCore's item to the context menu object.
 *
 * @param menu the context menu object
 * @param core the WebCore's context menu item that will be added to the context menu
 * @note emits a signal "contextmenu,item,appended"
 *
 * @see ewk_context_menu_item_new
 */
void ewk_context_menu_item_append(Ewk_Context_Menu* menu, const WebCore::ContextMenuItem& core)
{
    Ewk_Context_Menu_Item_Type type = static_cast<Ewk_Context_Menu_Item_Type>(core.type());
    Ewk_Context_Menu_Action action = static_cast<Ewk_Context_Menu_Action>(core.action());

    Ewk_Context_Menu_Item* menu_item = ewk_context_menu_item_new(type, action, menu, 0, core.title().utf8().data(), core.checked(), core.enabled());
    EINA_SAFETY_ON_NULL_RETURN(menu_item);

    menu->items = eina_list_append(menu->items, menu_item);
    evas_object_smart_callback_call(menu->view, "contextmenu,item,appended", menu);
}

/**
 * @internal
 *
 * Emits a signal "contextmenu,show"
 *
 * @param menu the context menu object
 */
void ewk_context_menu_show(Ewk_Context_Menu* menu)
{
    EINA_SAFETY_ON_NULL_RETURN(menu);

    evas_object_smart_callback_call(menu->view, "contextmenu,show", menu);
}

#endif
