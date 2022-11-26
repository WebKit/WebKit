/*
 * Copyright (C) 2011, 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
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

#pragma once

#if ENABLE(CONTEXT_MENUS)

#include "WebContextMenuItemGlib.h"
#include "WebContextMenuProxy.h"
#include <WebCore/GtkVersioning.h>
#include <WebCore/IntPoint.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _GMenu GMenu;

namespace WebKit {

class WebContextMenuItem;
class WebContextMenuItemData;
class WebPageProxy;

class WebContextMenuProxyGtk final : public WebContextMenuProxy {
public:
    static auto create(GtkWidget* widget, WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
    {
        return adoptRef(*new WebContextMenuProxyGtk(widget, page, WTFMove(context), userData));
    }
    ~WebContextMenuProxyGtk();

    void populate(const Vector<WebContextMenuItemGlib>&);
    GtkWidget* gtkWidget() const { return m_menu; }
    static const char* widgetDismissedSignal;

private:
    WebContextMenuProxyGtk(GtkWidget*, WebPageProxy&, ContextMenuContextData&&, const UserData&);
    void show() override;
    Vector<Ref<WebContextMenuItem>> proposedItems() const override;
    void showContextMenuWithItems(Vector<Ref<WebContextMenuItem>>&&) override;
    void append(GMenu*, const WebContextMenuItemGlib&);
    GRefPtr<GMenu> buildMenu(const Vector<WebContextMenuItemGlib>&);
    void populate(const Vector<Ref<WebContextMenuItem>>&);
    Vector<WebContextMenuItemGlib> populateSubMenu(const WebContextMenuItemData&);

    GtkWidget* m_webView;
    GtkWidget* m_menu;
    HashMap<unsigned long, void*> m_signalHandlers;
    GRefPtr<GSimpleActionGroup> m_actionGroup { adoptGRef(g_simple_action_group_new()) };
};


} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
