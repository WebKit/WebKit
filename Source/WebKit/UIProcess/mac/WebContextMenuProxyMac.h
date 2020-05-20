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

#pragma once

#if PLATFORM(MAC)

#include "WKArray.h"
#include "WebContextMenuProxy.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS NSMenu;
OBJC_CLASS NSMenuItem;
OBJC_CLASS NSPopUpButtonCell;
OBJC_CLASS NSView;
OBJC_CLASS NSWindow;

namespace WebKit {

class ShareableBitmap;
class UserData;
class WebContextMenuItemData;
class WebContextMenuListenerProxy;
class WebPageProxy;

class WebContextMenuProxyMac : public WebContextMenuProxy {
public:
    static auto create(NSView* view, WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
    {
        return adoptRef(*new WebContextMenuProxyMac(view, page, WTFMove(context), userData));
    }
    ~WebContextMenuProxyMac();

    void contextMenuItemSelected(const WebContextMenuItemData&);
    void showContextMenuWithItems(Vector<Ref<WebContextMenuItem>>&&) override;

#if ENABLE(SERVICE_CONTROLS)
    void clearServicesMenu();
#endif

    WebPageProxy& page() const { return m_page; }
    NSWindow *window() const;

private:
    WebContextMenuProxyMac(NSView*, WebPageProxy&, ContextMenuContextData&&, const UserData&);
    void show() override;

    RefPtr<WebContextMenuListenerProxy> m_contextMenuListener;
    void getContextMenuItem(const WebContextMenuItemData&, CompletionHandler<void(NSMenuItem *)>&&);
    void getContextMenuFromItems(const Vector<WebContextMenuItemData>&, CompletionHandler<void(NSMenu *)>&&);
    void showContextMenu();

#if ENABLE(SERVICE_CONTROLS)
    void getShareMenuItem(CompletionHandler<void(NSMenuItem *)>&&);
    void showServicesMenu();
    void setupServicesMenu();
#endif

    RetainPtr<NSMenu> m_menu;

    NSView* m_webView;
    WebPageProxy& m_page;
};

} // namespace WebKit

#endif // PLATFORM(MAC)
