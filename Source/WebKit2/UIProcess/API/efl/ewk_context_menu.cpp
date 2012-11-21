/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#include "config.h"
#include "ewk_context_menu.h"

#include "APIObject.h"
#include "EwkViewImpl.h"
#include "WebContextMenuItemData.h"
#include "WebContextMenuProxyEfl.h"
#include "ewk_context_menu_item.h"
#include "ewk_context_menu_item_private.h"
#include "ewk_context_menu_private.h"

using namespace WebKit;

EwkContextMenu::EwkContextMenu(EwkViewImpl* viewImpl, WebContextMenuProxyEfl* contextMenuProxy, const Vector<WebKit::WebContextMenuItemData>& items)
    : m_viewImpl(viewImpl)
    , m_contextMenuProxy(contextMenuProxy)
    , m_contextMenuItems(0)
{
    const size_t size = items.size();
    for (size_t i = 0; i < size; ++i)
        m_contextMenuItems = eina_list_append(m_contextMenuItems, Ewk_Context_Menu_Item::create(items[i]).leakPtr());
}

EwkContextMenu::EwkContextMenu()
    : m_viewImpl(0)
    , m_contextMenuProxy(0)
    , m_contextMenuItems(0)
{
}

EwkContextMenu::EwkContextMenu(Eina_List* items)
    : m_viewImpl(0)
    , m_contextMenuProxy(0)
    , m_contextMenuItems(0)
{
    Eina_List* l;
    void* data;
    EINA_LIST_FOREACH(items, l, data)
        m_contextMenuItems = eina_list_append(m_contextMenuItems, static_cast<EwkContextMenuItem*>(data));
}

EwkContextMenu::~EwkContextMenu()
{
    void* data;
    EINA_LIST_FREE(m_contextMenuItems, data)
        delete static_cast<Ewk_Context_Menu_Item*>(data);
}

void EwkContextMenu::hide()
{
    m_viewImpl->hideContextMenu();
}

void Ewk_Context_Menu::appendItem(EwkContextMenuItem* item)
{
    m_contextMenuItems = eina_list_append(m_contextMenuItems, item);
}

void Ewk_Context_Menu::removeItem(EwkContextMenuItem* item)
{
    m_contextMenuItems = eina_list_remove(m_contextMenuItems, item);
}

void EwkContextMenu::contextMenuItemSelected(const WebKit::WebContextMenuItemData& item)
{
    m_contextMenuProxy->contextMenuItemSelected(item); 
} 

Ewk_Context_Menu* ewk_context_menu_new()
{
    return EwkContextMenu::create().leakPtr();
}

Ewk_Context_Menu* ewk_context_menu_new_with_items(Eina_List* items)
{
    return EwkContextMenu::create(items).leakPtr();
}

Eina_Bool ewk_context_menu_item_append(Ewk_Context_Menu* menu, Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, false);

    menu->appendItem(item);

    return true;
}

Eina_Bool ewk_context_menu_item_remove(Ewk_Context_Menu* menu, Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, false);

    menu->removeItem(item);

    return true;
}

Eina_Bool ewk_context_menu_hide(Ewk_Context_Menu* menu)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, false);

    menu->hide();

    return true;
}

const Eina_List* ewk_context_menu_items_get(const Ewk_Context_Menu* menu)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, 0);

    return menu->items();
}

Eina_Bool ewk_context_menu_item_select(Ewk_Context_Menu* menu, Ewk_Context_Menu_Item* item)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(menu, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(item, false);

    WebCore::ContextMenuItemType type = static_cast<WebCore::ContextMenuItemType>(item->type());
    WebCore::ContextMenuAction action = static_cast<WebCore::ContextMenuAction>(item->action());

    menu->contextMenuItemSelected(WebContextMenuItemData(type, action, item->title(), item->enabled(), item->checked()));

    return true;
}
