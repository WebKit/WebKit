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

#ifndef ewk_context_menu_private_h
#define ewk_context_menu_private_h

#include "WebContextMenuItemData.h"
#include "ewk_context_menu_item.h"
#include <Eina.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebKit {
class WebContextMenuItemData;
class WebContextMenuProxyEfl;
}

class EwkView;

class EwkContextMenu {
public:
    static PassOwnPtr<EwkContextMenu> create(EwkView* viewImpl, WebKit::WebContextMenuProxyEfl* contextMenuProxy, const Vector<WebKit::WebContextMenuItemData>& items)
    {
        return adoptPtr(new EwkContextMenu(viewImpl, contextMenuProxy, items));
    }

    static PassOwnPtr<EwkContextMenu> create()
    {
        return adoptPtr(new EwkContextMenu());
    }

    static PassOwnPtr<EwkContextMenu> create(Eina_List* items)
    {
        return adoptPtr(new EwkContextMenu(items));
    }

    ~EwkContextMenu();

    void hide();
    void appendItem(EwkContextMenuItem*);
    void removeItem(EwkContextMenuItem*);

    const Eina_List* items() const { return m_contextMenuItems; }
    void contextMenuItemSelected(const WebKit::WebContextMenuItemData& item);

private:
    EwkContextMenu();
    EwkContextMenu(Eina_List* items);
    EwkContextMenu(EwkView* viewImpl, WebKit::WebContextMenuProxyEfl*, const Vector<WebKit::WebContextMenuItemData>& items);

    EwkView* m_viewImpl;
    WebKit::WebContextMenuProxyEfl* m_contextMenuProxy;
    Eina_List* m_contextMenuItems;
};

#endif // ewk_context_menu_private_h

