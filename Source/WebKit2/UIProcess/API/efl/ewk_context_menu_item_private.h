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

#ifndef ewk_context_menu_item_private_h
#define ewk_context_menu_item_private_h

#include "WKEinaSharedString.h"
#include "WebContextMenuItemData.h"
#include "ewk_context_menu.h"
#include "ewk_context_menu_item.h"
#include <wtf/PassOwnPtr.h>

/**
 * \struct  Ewk_Context_Menu_Item
 * @brief   Contains the context menu item data.
 */
class EwkContextMenuItem {
public:
    static PassOwnPtr<EwkContextMenuItem> create(const WebKit::WebContextMenuItemData& item)
    {
        return adoptPtr(new EwkContextMenuItem(item));
    }

    static PassOwnPtr<EwkContextMenuItem> create(Ewk_Context_Menu_Item_Type type, Ewk_Context_Menu_Item_Action action, const char* title, Eina_Bool checked, Eina_Bool enabled, EwkContextMenu* subMenu)
    {
        return adoptPtr(new EwkContextMenuItem(type, action, title, checked, enabled, subMenu));
    }

    Ewk_Context_Menu_Item_Action action() const { return m_action; }
    void setAction(Ewk_Context_Menu_Item_Action action) { m_action = action; }

    const char* title() const { return m_title; }
    void setTitle(const char* title) { m_title = title; }

    Ewk_Context_Menu_Item_Type type() const { return m_type; }
    void setType(Ewk_Context_Menu_Item_Type type) { m_type = type; }

    bool checked() const { return m_isChecked; }
    void setChecked(bool checked) { m_isChecked = checked; }

    bool enabled() const { return m_isEnabled; }
    void setEnabled(bool enabled) { m_isEnabled = enabled; }

private:
    explicit EwkContextMenuItem(const WebKit::WebContextMenuItemData&);
    EwkContextMenuItem(Ewk_Context_Menu_Item_Type type, Ewk_Context_Menu_Item_Action action, const char* title, Eina_Bool checked, Eina_Bool enabled, EwkContextMenu* subMenu);

    Ewk_Context_Menu_Item_Type m_type;
    Ewk_Context_Menu_Item_Action m_action;

    WKEinaSharedString m_title;
    
    bool m_isChecked;
    bool m_isEnabled;

    Ewk_Context_Menu* m_parentMenu;
    Ewk_Context_Menu* m_subMenu;
};

#endif // ewk_context_menu_item_private_h
