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

#if USE(APPKIT)

#include "WebPopupMenuProxy.h"
#include <wtf/RetainPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/WeakObjCPtr.h>

OBJC_CLASS NSPopUpButtonCell;
OBJC_CLASS WKView;

namespace WebKit {

class WebPageProxy;

class WebPopupMenuProxyMac : public WebPopupMenuProxy {
public:
    static Ref<WebPopupMenuProxyMac> create(NSView *webView, WebPopupMenuProxy::Client& client)
    {
        return adoptRef(*new WebPopupMenuProxyMac(webView, client));
    }
    ~WebPopupMenuProxyMac();

    void showPopupMenu(const WebCore::IntRect&, WebCore::TextDirection, double pageScaleFactor, const Vector<WebPopupItem>&, const PlatformPopupMenuData&, int32_t selectedIndex) override;
    void hidePopupMenu() override;
    void cancelTracking() override;

    NSPopUpButtonCell *popup() const;
    bool isVisible() const { return m_isVisible; }

private:
    WebPopupMenuProxyMac(NSView *, WebPopupMenuProxy::Client&);

    void populate(const Vector<WebPopupItem>&, NSFont *, WebCore::TextDirection);
    bool isWebPopupMenuProxyMac() const final { return true; }
    NSMenu *menu() const;

    RetainPtr<NSPopUpButtonCell> m_popup;
    WeakObjCPtr<NSView> m_webView;
    bool m_wasCanceled { false };
    bool m_isVisible { false };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebPopupMenuProxyMac) \
    static bool isType(const WebKit::WebPopupMenuProxy& menu) { return menu.isWebPopupMenuProxyMac(); } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // USE(APPKIT)

