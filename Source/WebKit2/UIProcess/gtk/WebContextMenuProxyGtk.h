/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef WebContextMenuProxyGtk_h
#define WebContextMenuProxyGtk_h

#if ENABLE(CONTEXT_MENUS)

#include "WebContextMenuItemGtk.h"
#include "WebContextMenuProxy.h"
#include <WebCore/IntPoint.h>
#include <wtf/HashMap.h>

namespace WebKit {

class WebContextMenuItem;
class WebContextMenuItemData;
class WebPageProxy;

class WebContextMenuProxyGtk : public WebContextMenuProxy {
public:
    WebContextMenuProxyGtk(GtkWidget*, WebPageProxy&, const ContextMenuContextData&, const UserData&);
    ~WebContextMenuProxyGtk();

    void populate(Vector<WebContextMenuItemGtk>&);
    GtkMenu* gtkMenu() const { return m_menu; }

private:
    void show() override;
    void append(GtkMenu*, const WebContextMenuItemGtk&);
    void populate(const Vector<RefPtr<WebContextMenuItem>>&);
    static void menuPositionFunction(GtkMenu*, gint*, gint*, gboolean*, WebContextMenuProxyGtk*);

    GtkWidget* m_webView;
    WebPageProxy* m_page;
    GtkMenu* m_menu;
    WebCore::IntPoint m_popupPosition;
    HashMap<unsigned long, GtkAction*> m_signalHandlers;
};


} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
#endif // WebContextMenuProxyGtk_h
