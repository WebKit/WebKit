/*
 * Copyright (C) 2015 Igalia S.L.
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

#ifndef WebContextMenuItemGtk_h
#define WebContextMenuItemGtk_h

#include "APIObject.h"
#include "WebContextMenuItemData.h"
#include <wtf/glib/GRefPtr.h>

typedef struct _GtkAction GtkAction;
typedef struct _GAction GAction;
typedef struct _GSimpleAction GSimpleAction;

namespace WebKit {

class WebContextMenuItemGtk : public WebContextMenuItemData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebContextMenuItemGtk(WebCore::ContextMenuItemType, WebCore::ContextMenuAction, const String& title, bool enabled = true, bool checked = false);
    WebContextMenuItemGtk(const WebContextMenuItemData&);
    WebContextMenuItemGtk(const WebContextMenuItemGtk&, Vector<WebContextMenuItemGtk>&& submenu);
    WebContextMenuItemGtk(GtkAction*);
    ~WebContextMenuItemGtk();

    // We don't use the SubmenuType internally, so check if we have submenu items.
    WebCore::ContextMenuItemType type() const { return m_submenuItems.isEmpty() ? WebContextMenuItemData::type() : WebCore::SubmenuType; }
    GtkAction* gtkAction() const { return m_gtkAction; }
    GAction* gAction() const { return reinterpret_cast<GAction*>(m_gAction.get()); }
    const Vector<WebContextMenuItemGtk>& submenuItems() const { return m_submenuItems; }

private:
    void createActionIfNeeded();

    GRefPtr<GSimpleAction> m_gAction;
    GtkAction* m_gtkAction { nullptr };
    Vector<WebContextMenuItemGtk> m_submenuItems;
};

} // namespace WebKit

#endif // WebContextMenuItemGtk_h
