/*
 * Copyright (C) 2012 Intel Corporation
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

#include "config.h"
#include "ewk_popup_menu.h"

#include "EwkViewImpl.h"
#include "WebPopupMenuProxyEfl.h"
#include "ewk_popup_menu_item_private.h"
#include "ewk_popup_menu_private.h"

using namespace WebKit;

Ewk_Popup_Menu::Ewk_Popup_Menu(EwkViewImpl* viewImpl, WebPopupMenuProxyEfl* popupMenuProxy, const Vector<WebKit::WebPopupItem>& items, unsigned selectedIndex)
    : m_viewImpl(viewImpl)
    , m_popupMenuProxy(popupMenuProxy)
    , m_popupMenuItems(0)
    , m_selectedIndex(selectedIndex)
{
    const size_t size = items.size();
    for (size_t i = 0; i < size; ++i)
        m_popupMenuItems = eina_list_append(m_popupMenuItems, Ewk_Popup_Menu_Item::create(items[i]).leakPtr());
}

Ewk_Popup_Menu::~Ewk_Popup_Menu()
{
    void* item;
    EINA_LIST_FREE(m_popupMenuItems, item)
        delete static_cast<Ewk_Popup_Menu_Item*>(item);
}

void Ewk_Popup_Menu::close()
{
    m_viewImpl->closePopupMenu();
}

const Eina_List* Ewk_Popup_Menu::items() const
{
    return m_popupMenuItems;
}

unsigned Ewk_Popup_Menu::selectedIndex() const
{
    return m_selectedIndex;
}

bool Ewk_Popup_Menu::setSelectedIndex(unsigned selectedIndex)
{
    if (!m_popupMenuProxy)
        return false;

    if (selectedIndex >= eina_list_count(m_popupMenuItems))
        return false;

    if (m_selectedIndex == selectedIndex)
        return true;

    m_selectedIndex = selectedIndex;
    m_popupMenuProxy->valueChanged(selectedIndex);

    return true;
}

Eina_Bool ewk_popup_menu_close(Ewk_Popup_Menu* popupMenu)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(popupMenu, false);

    popupMenu->close();

    return true;
}

Eina_Bool ewk_popup_menu_selected_index_set(Ewk_Popup_Menu* popupMenu, unsigned selectedIndex)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(popupMenu, false);

    return popupMenu->setSelectedIndex(selectedIndex);
}

unsigned ewk_popup_menu_selected_index_get(const Ewk_Popup_Menu* popupMenu)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(popupMenu, 0);

    return popupMenu->selectedIndex();
}

const Eina_List* ewk_popup_menu_items_get(const Ewk_Popup_Menu* popupMenu)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(popupMenu, 0);

    return popupMenu->items();
}
