/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebContextMenuProxyWin_h
#define WebContextMenuProxyWin_h

#include "WebContextMenuItemData.h"
#include "WebContextMenuProxy.h"
#include "WebPageProxy.h"
#include <wtf/HashMap.h>

namespace WebKit {

class WebContextMenuProxyWin : public WebContextMenuProxy {
public:
    static PassRefPtr<WebContextMenuProxyWin> create(HWND parentWindow, WebPageProxy* page)
    {
        return adoptRef(new WebContextMenuProxyWin(parentWindow, page));
    }

private:
    WebContextMenuProxyWin(HWND parentWindow, WebPageProxy* page);

    virtual void showContextMenu(const WebCore::IntPoint&, const Vector<WebContextMenuItemData>&);
    virtual void hideContextMenu();

    void populateMenu(HMENU, const Vector<WebContextMenuItemData>&);

    HMENU m_menu;
    HWND m_window;
    WebPageProxy* m_page;

    // Creates a map from the context menu item's action to the context menu item itself.
    HashMap<int, WebContextMenuItemData> m_actionMap;
};

} // namespace WebKit

#endif // WebContextMenuProxyWin_h
