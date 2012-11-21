/*
    Copyright (C) 2012 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "config.h"

#include "UnitTestUtils/EWK2UnitTestBase.h"
#include "UnitTestUtils/EWK2UnitTestEnvironment.h"
#include <EWebKit2.h>
#include <Ecore.h>

using namespace EWK2UnitTest;

extern EWK2UnitTestEnvironment* environment;

static inline void checkBasicContextMenuItem(Ewk_Context_Menu_Item* item, Ewk_Context_Menu_Item_Type type, Ewk_Context_Menu_Item_Action action, const char* title, Eina_Bool checked, Eina_Bool enabled)
{
    ASSERT_TRUE(item);

    EXPECT_EQ(type, ewk_context_menu_item_type_get(item));
    EXPECT_EQ(action, ewk_context_menu_item_action_get(item));
    EXPECT_STREQ(title, ewk_context_menu_item_title_get(item));
    EXPECT_EQ(checked, ewk_context_menu_item_checked_get(item));
    EXPECT_EQ(enabled, ewk_context_menu_item_enabled_get(item));
}

static Eina_Bool showContextMenu(Ewk_View_Smart_Data* smartData, Evas_Coord x, Evas_Coord y, Ewk_Context_Menu* contextMenu)
{
    const Eina_List* list = ewk_context_menu_items_get(contextMenu);
    EXPECT_EQ(4, eina_list_count(list));

    Ewk_Context_Menu_Item* item = static_cast<Ewk_Context_Menu_Item*>(eina_list_nth(list, 0));
    checkBasicContextMenuItem(item, EWK_ACTION_TYPE, EWK_CONTEXT_MENU_ITEM_TAG_GO_BACK, "Go Back", false, true);

    item = static_cast<Ewk_Context_Menu_Item*>(eina_list_nth(list, 1));
    checkBasicContextMenuItem(item, EWK_ACTION_TYPE, EWK_CONTEXT_MENU_ITEM_TAG_GO_FORWARD, "Go Forward", false, true);
    ewk_context_menu_item_enabled_set(item, false);
    EXPECT_FALSE(ewk_context_menu_item_enabled_get(item));

    item = static_cast<Ewk_Context_Menu_Item*>(eina_list_nth(list, 2));
    checkBasicContextMenuItem(item, EWK_ACTION_TYPE, EWK_CONTEXT_MENU_ITEM_TAG_STOP, "Stop", false, true);
    ewk_context_menu_item_checked_set(item, true);
    EXPECT_TRUE(ewk_context_menu_item_checked_get(item));

    item = static_cast<Ewk_Context_Menu_Item*>(eina_list_nth(list, 3));
    checkBasicContextMenuItem(item, EWK_ACTION_TYPE, EWK_CONTEXT_MENU_ITEM_TAG_RELOAD, "Reload", false, true);
    ewk_context_menu_item_title_set(item, "Refresh");
    EXPECT_STREQ("Refresh", ewk_context_menu_item_title_get(item));

    // Makes new context menu items.
    Ewk_Context_Menu_Item* newItem = ewk_context_menu_item_new(EWK_ACTION_TYPE, EWK_CONTEXT_MENU_ITEM_TAG_OTHER, "New Custom Item", false, true);
    ewk_context_menu_item_append(contextMenu, newItem);

    Eina_List* subMenuItemList = 0;
    Ewk_Context_Menu_Item* subMenuItem1 = ewk_context_menu_item_new(EWK_ACTION_TYPE, EWK_CONTEXT_MENU_ITEM_TAG_OTHER, "New SubMenu Item 1", false, true);
    Ewk_Context_Menu_Item* subMenuItem2 = ewk_context_menu_item_new(EWK_ACTION_TYPE, EWK_CONTEXT_MENU_ITEM_TAG_OTHER, "New SubMenu Item 2", false, true);
    Ewk_Context_Menu_Item* subMenuItem3 = ewk_context_menu_item_new(EWK_ACTION_TYPE, EWK_CONTEXT_MENU_ITEM_TAG_OTHER, "New SubMenu Item 3", false, true);
    subMenuItemList = eina_list_append(subMenuItemList, subMenuItem1);
    subMenuItemList = eina_list_append(subMenuItemList, subMenuItem2);
    subMenuItemList = eina_list_append(subMenuItemList, subMenuItem3);
    Ewk_Context_Menu* subMenu = ewk_context_menu_new_with_items(subMenuItemList);
    Ewk_Context_Menu_Item* newItem2 = ewk_context_menu_item_new_with_submenu(EWK_SUBMENU_TYPE, EWK_CONTEXT_MENU_ITEM_TAG_OTHER, "New Custom Item 2", false, true, subMenu);
    ewk_context_menu_item_append(contextMenu, newItem2);

    Ewk_Context_Menu* subMenu2 = ewk_context_menu_new();
    Ewk_Context_Menu_Item* newItem3 = ewk_context_menu_item_new_with_submenu(EWK_SUBMENU_TYPE, EWK_CONTEXT_MENU_ITEM_TAG_OTHER, "New Custom Item 3", false, true, subMenu2);
    ewk_context_menu_item_append(contextMenu, newItem3);

    list = ewk_context_menu_items_get(contextMenu);
    EXPECT_EQ(7, eina_list_count(list));

    ewk_context_menu_item_remove(contextMenu, newItem);
    ewk_context_menu_item_remove(contextMenu, newItem2);
    ewk_context_menu_item_remove(contextMenu, newItem3);
    list = ewk_context_menu_items_get(contextMenu);
    EXPECT_EQ(4, eina_list_count(list));

    EXPECT_TRUE(ewk_context_menu_item_select(contextMenu, item));

    return true;
}

TEST_F(EWK2UnitTestBase, ewk_context_menu_item_select)
{
    const char* itemSelectHTML =
        "<html>"
        "<body><a href=http://www.google.com>Test Link</a></body>"
        "</body></html>"; 

    ewkViewClass()->context_menu_show = showContextMenu;

    ewk_view_html_string_load(webView(), itemSelectHTML, "file:///", 0);
    mouseClick(20, 30, /*Right*/ 3);
    ASSERT_TRUE(waitUntilLoadFinished());
}
