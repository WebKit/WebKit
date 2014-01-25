/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#include "config.h"
#include "WebPage.h"

#include "NotImplemented.h"
#include "WebEvent.h"
#include "WebPageAccessibilityObject.h"
#include "WebPageProxyMessages.h"
#include "WindowsKeyboardCodes.h"
#include <WebCore/BackForwardController.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PasteboardHelper.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/Settings.h>
#include <wtf/gobject/GUniquePtr.h>

using namespace WebCore;

namespace WebKit {

void WebPage::platformInitialize()
{
#if HAVE(ACCESSIBILITY)
    // Create the accessible object (the plug) that will serve as the
    // entry point to the Web process, and send a message to the UI
    // process to connect the two worlds through the accessibility
    // object there specifically placed for that purpose (the socket).
    m_accessibilityObject = adoptGRef(webPageAccessibilityObjectNew(this));
    GUniquePtr<gchar> plugID(atk_plug_get_id(ATK_PLUG(m_accessibilityObject.get())));
    send(Messages::WebPageProxy::BindAccessibilityTree(String(plugID.get())));
#endif

#if USE(TEXTURE_MAPPER_GL)
    m_nativeWindowHandle = 0;
#endif
}

#if HAVE(ACCESSIBILITY)
void WebPage::updateAccessibilityTree()
{
    if (!m_accessibilityObject)
        return;

    webPageAccessibilityObjectRefresh(m_accessibilityObject.get());
}
#endif

void WebPage::platformPreferencesDidChange(const WebPreferencesStore&)
{
    notImplemented();
}

static inline void scroll(Page* page, ScrollDirection direction, ScrollGranularity granularity)
{
    page->focusController().focusedOrMainFrame().eventHandler().scrollRecursively(direction, granularity);
}

bool WebPage::performDefaultBehaviorForKeyEvent(const WebKeyboardEvent& keyboardEvent)
{
    if (keyboardEvent.type() != WebEvent::KeyDown && keyboardEvent.type() != WebEvent::RawKeyDown)
        return false;

    switch (keyboardEvent.windowsVirtualKeyCode()) {
    case VK_BACK:
        if (keyboardEvent.shiftKey())
            m_page->backForward().goForward();
        else
            m_page->backForward().goBack();
        break;
    case VK_SPACE:
        scroll(m_page.get(), keyboardEvent.shiftKey() ? ScrollUp : ScrollDown, ScrollByPage);
        break;
    case VK_LEFT:
        scroll(m_page.get(), ScrollLeft, ScrollByLine);
        break;
    case VK_RIGHT:
        scroll(m_page.get(), ScrollRight, ScrollByLine);
        break;
    case VK_UP:
        scroll(m_page.get(), ScrollUp, ScrollByLine);
        break;
    case VK_DOWN:
        scroll(m_page.get(), ScrollDown, ScrollByLine);
        break;
    case VK_HOME:
        scroll(m_page.get(), ScrollUp, ScrollByDocument);
        break;
    case VK_END:
        scroll(m_page.get(), ScrollDown, ScrollByDocument);
        break;
    case VK_PRIOR:
        scroll(m_page.get(), ScrollUp, ScrollByPage);
        break;
    case VK_NEXT:
        scroll(m_page.get(), ScrollDown, ScrollByPage);
        break;
    default:
        return false;
    }

    return true;
}

bool WebPage::platformHasLocalDataForURL(const URL&)
{
    notImplemented();
    return false;
}

String WebPage::cachedResponseMIMETypeForURL(const URL&)
{
    notImplemented();
    return String();
}

bool WebPage::platformCanHandleRequest(const ResourceRequest&)
{
    notImplemented();
    return false;
}

String WebPage::cachedSuggestedFilenameForURL(const URL&)
{
    notImplemented();
    return String();
}

PassRefPtr<SharedBuffer> WebPage::cachedResponseDataForURL(const URL&)
{
    notImplemented();
    return 0;
}

#if USE(TEXTURE_MAPPER_GL)
void WebPage::setAcceleratedCompositingWindowId(int64_t nativeWindowHandle)
{
    m_nativeWindowHandle = nativeWindowHandle;
}
#endif

} // namespace WebKit
