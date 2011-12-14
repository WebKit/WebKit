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
#include "EWebKit.h"
#include "ewk_private.h"

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

    Eina_Bool checked:1;
    Eina_Bool enabled:1;
};

/**
 * Increases the reference count of the given object.
 *
 * @param menu the context menu object to increase the reference count
 */
void ewk_context_menu_ref(Ewk_Context_Menu* menu)
{
    EINA_SAFETY_ON_NULL_RETURN(menu);
    menu->__ref++;
}

/**
 * Decreases the reference count of the given object, possibly freeing it.
 *
 * When the reference count it's reached 0, the menu with all items are freed.
 *
 * @param menu the context menu object to decrease the reference count
 */
void ewk_context_menu_unref(Ewk_Context_Menu* menu)
{
    EINA_SAFETY_ON_NULL_RETURN(menu);
    void* item;

    if (--menu->__ref)
        return;

    EINA_LIST_FREE(menu->items, item)
        ewk_context_menu_item_free(static_cast<Ewk_Context_Menu_Item*>(item));

    free(menu);
}

/**
 * Destroys the context menu object.
 *
 * @param menu the context menu object to destroy
 * @return @c EINA_TRUE on success, @c EINA_FALSE on failure
 *
 * @see ewk_context_menu_item_free
 */
Eina_Bool ewk_context_menu_destroy(Ewk_Context_Menu* menu)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, EINA_FALSE);
#if ENABLE(CONTEXT_MENUS)
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu->controller, EINA_FALSE);

    menu->controller->clearContextMenu();
#endif

    return EINA_TRUE;
}

/**
 * Gets the list of items.
 *
 * @param o the context menu object to get list of the items
 * @return the list of the items on success or @c 0 on failure
 */
const Eina_List* ewk_context_menu_item_list_get(Ewk_Context_Menu* o)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, 0);

    return o->items;
}

/**
 * Creates a new item of the context menu.
 *
 * @param type specifies a type of the item
 * @param action specifies a action of the item
 * @param submenu specifies a submenu of the item
 * @param title specifies a title of the item
 * @param checked
 * @param enabled @c EINA_TRUE to enable the item or @c EINA_FALSE to disable
 * @return the pointer to the new item on success or @c 0 on failure
 *
 * @note The return value @b should @b be freed after use.
 */
Ewk_Context_Menu_Item* ewk_context_menu_item_new(Ewk_Context_Menu_Item_Type type,
        Ewk_Context_Menu_Action action, Ewk_Context_Menu* submenu,
        const char* title, Eina_Bool checked, Eina_Bool enabled)
{
    Ewk_Context_Menu_Item* item = (Ewk_Context_Menu_Item*) malloc(sizeof(*item));
    if (!item)
        return 0;

    item->type = type;
    item->action = action;
    item->title = eina_stringshare_add(title);
    item->submenu = submenu;
    item->checked = checked;
    item->enabled = enabled;

    return item;
}

/**
 * Selects the item from the context menu object.
 *
 * @param menu the context menu object
 * @param item the item is selected
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
Eina_Bool ewk_context_menu_item_select(Ewk_Context_Menu* menu, Ewk_Context_Menu_Item* item)
{
#if ENABLE(CONTEXT_MENUS)
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, EINA_FALSE);
    WebCore::ContextMenuAction action = static_cast<WebCore::ContextMenuAction>(item->action);
    WebCore::ContextMenuItemType type = static_cast<WebCore::ContextMenuItemType>(item->type);

    // Don't care about title and submenu as they're not used after this point.
    WebCore::ContextMenuItem core(type, action, WTF::String());
    menu->controller->contextMenuItemSelected(&core);
    return EINA_TRUE;
#else
    return EINA_FALSE;
#endif
}

/**
 * Destroys the item of the context menu object.
 *
 * @param item the item to destroy
 *
 * @see ewk_context_menu_destroy
 * @see ewk_context_menu_unref
 */
void ewk_context_menu_item_free(Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN(item);

    eina_stringshare_del(item->title);
    free(item);
}

/**
 * Gets type of the item.
 *
 * @param o the item to get the type
 * @return type of the item on success or @c EWK_ACTION_TYPE on failure
 *
 * @see ewk_context_menu_item_type_set
 */
Ewk_Context_Menu_Item_Type ewk_context_menu_item_type_get(Ewk_Context_Menu_Item* o)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, EWK_ACTION_TYPE);
    return o->type;
}

/**
 * Sets the type of item.
 *
 * @param o the item to set the type
 * @param type a new type for the item object
 * @return @c EINA_TRUE on success, or @c EINA_FALSE on failure
 *
 * @see ewk_context_menu_item_type_get
 */
Eina_Bool ewk_context_menu_item_type_set(Ewk_Context_Menu_Item* o, Ewk_Context_Menu_Item_Type type)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, EINA_FALSE);
    o->type = type;
    return EINA_TRUE;
}

/**
 * Gets an action of the item.
 *
 * @param o the item to get the action
 * @return an action of the item on success or @c EWK_CONTEXT_MENU_ITEM_TAG_NO_ACTION on failure
 *
 * @see ewk_context_menu_item_action_set
 */
Ewk_Context_Menu_Action ewk_context_menu_item_action_get(Ewk_Context_Menu_Item* o)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, EWK_CONTEXT_MENU_ITEM_TAG_NO_ACTION);
    return o->action;
}

/**
 * Sets an action of the item.
 *
 * @param o the item to set the action
 * @param action a new action for the item object
 * @return @c EINA_TRUE on success, or @c EINA_FALSE on failure
 *
 * @see ewk_context_menu_item_action_get
 */
Eina_Bool ewk_context_menu_item_action_set(Ewk_Context_Menu_Item* o, Ewk_Context_Menu_Action action)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, EINA_FALSE);
    o->action = action;
    return EINA_TRUE;
}

/**
 * Gets a title of the item.
 *
 * @param o the item to get the title
 * @return a title of the item on success, or @c 0 on failure
 *
 * @see ewk_context_menu_item_title_set
 */
const char* ewk_context_menu_item_title_get(Ewk_Context_Menu_Item* o)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, 0);
    return o->title;
}

/**
 * Sets a title of the item.
 *
 * @param o the item to set the title
 * @param title a new title for the item object
 * @return a new title of the item on success or @c 0 on failure
 *
 * @see ewk_context_menu_item_title_get
 */
const char* ewk_context_menu_item_title_set(Ewk_Context_Menu_Item* o, const char* title)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, 0);
    eina_stringshare_replace(&o->title, title);
    return o->title;
}

Eina_Bool ewk_context_menu_item_checked_get(Ewk_Context_Menu_Item* o)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, EINA_FALSE);
    return o->checked;
}

Eina_Bool ewk_context_menu_item_checked_set(Ewk_Context_Menu_Item* o, Eina_Bool checked)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, EINA_FALSE);
    o->checked = checked;
    return EINA_TRUE;
}

/**
 * Gets if the item is enabled.
 *
 * @param o the item to get enabled state
 * @return @c EINA_TRUE if it's enabled, @c EINA_FALSE if not or on failure
 *
 * @see ewk_context_menu_item_enabled_set
 */
Eina_Bool ewk_context_menu_item_enabled_get(Ewk_Context_Menu_Item* o)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, EINA_FALSE);
    return o->enabled;
}

/**
 * Enables/disables the item.
 *
 * @param o the item to enable/disable
 * @param enabled @c EINA_TRUE to enable the item or @c EINA_FALSE to disable
 * @return @c EINA_TRUE on success, or @c EINA_FALSE on failure
 *
 * @see ewk_context_menu_item_enabled_get
 */
Eina_Bool ewk_context_menu_item_enabled_set(Ewk_Context_Menu_Item *o, Eina_Bool enabled)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, EINA_FALSE);
    o->enabled = enabled;
    return EINA_TRUE;
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
Ewk_Context_Menu* ewk_context_menu_new(Evas_Object* view, WebCore::ContextMenuController* controller)
{
    Ewk_Context_Menu* menu;
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(controller, 0);

    menu = static_cast<Ewk_Context_Menu*>(malloc(sizeof(*menu)));
    if (!menu) {
        CRITICAL("Could not allocate context menu memory.");
        return 0;
    }

    menu->__ref = 1;
    menu->view = view;
    menu->controller = controller;
    menu->items = 0;
    evas_object_smart_callback_call(menu->view, "contextmenu,new", menu);

    return menu;
}

/**
 * @internal
 *
 * Frees the context menu.
 *
 * @param o the view object
 * @return @c EINA_TRUE on success, or @c EINA_FALSE on failure
 *
 * @note emits a signal "contextmenu,free"
 *
 * @see ewk_context_menu_unref
 * @see ewk_context_menu_destroy
 */
Eina_Bool ewk_context_menu_free(Ewk_Context_Menu* o)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, EINA_FALSE);
    evas_object_smart_callback_call(o->view, "contextmenu,free", o);
    ewk_context_menu_unref(o);
    return EINA_TRUE;
}

/**
 * @internal
 *
 * Appends the WebCore's item to the context menu object.
 *
 * @param o the context menu object
 * @param core the WebCore's context menu item that will be added to the context menu
 * @note emits a signal "contextmenu,item,appended"
 *
 * @see ewk_context_menu_item_new
 */
void ewk_context_menu_item_append(Ewk_Context_Menu* o, WebCore::ContextMenuItem& core)
{
    Ewk_Context_Menu_Item_Type type = static_cast<Ewk_Context_Menu_Item_Type>(core.type());
    Ewk_Context_Menu_Action action = static_cast<Ewk_Context_Menu_Action>(core.action());

    Ewk_Context_Menu_Item* menu_item = ewk_context_menu_item_new
        (type, action, 0, core.title().utf8().data(), core.checked(),
         core.enabled());
    EINA_SAFETY_ON_NULL_RETURN(menu_item);

    o->items = eina_list_append(o->items, menu_item);
    evas_object_smart_callback_call(o->view, "contextmenu,item,appended", o);
}

/**
 * @internal
 *
 * Emits a signal with the items of the context menu.
 *
 * @param o the context menu object
 * @return the same context menu object that was given through parameter
 *
 * @note emits a signal "contextmenu,customize"
 *
 * @see ewk_context_menu_item_list_get
 */
Ewk_Context_Menu* ewk_context_menu_custom_get(Ewk_Context_Menu* o)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(o, 0);

    evas_object_smart_callback_call(o->view, "contextmenu,customize", o->items);
    return o;
}

/**
 * @internal
 *
 * Emits a signal "contextmenu,show"
 *
 * @param o the context menu object
 */
void ewk_context_menu_show(Ewk_Context_Menu* o)
{
    EINA_SAFETY_ON_NULL_RETURN(o);

    evas_object_smart_callback_call(o->view, "contextmenu,show", o);
}

#endif
