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

#include "WebEvent.h"
#include "WebFrame.h"
#include "WebKitWebPageAccessibilityObject.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/BackForwardController.h>
#include <WebCore/Editor.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PasteboardHelper.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/Settings.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/UserAgent.h>
#include <WebCore/WindowsKeyboardCodes.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {
using namespace WebCore;

void WebPage::platformInitialize()
{
#if ENABLE(ACCESSIBILITY)
    // Create the accessible object (the plug) that will serve as the
    // entry point to the Web process, and send a message to the UI
    // process to connect the two worlds through the accessibility
    // object there specifically placed for that purpose (the socket).
    m_accessibilityObject = adoptGRef(webkitWebPageAccessibilityObjectNew(this));
    GUniquePtr<gchar> plugID(atk_plug_get_id(ATK_PLUG(m_accessibilityObject.get())));
    send(Messages::WebPageProxy::BindAccessibilityTree(String(plugID.get())));
#endif
}

void WebPage::platformReinitialize()
{
}

void WebPage::platformDetach()
{
}

bool WebPage::performDefaultBehaviorForKeyEvent(const WebKeyboardEvent& keyboardEvent)
{
    if (keyboardEvent.type() != WebEvent::KeyDown && keyboardEvent.type() != WebEvent::RawKeyDown)
        return false;

    switch (keyboardEvent.windowsVirtualKeyCode()) {
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

bool WebPage::platformCanHandleRequest(const ResourceRequest&)
{
    notImplemented();
    return false;
}

String WebPage::platformUserAgent(const URL& url) const
{
    if (url.isNull() || !m_page->settings().needsSiteSpecificQuirks())
        return String();

    return WebCore::standardUserAgentForURL(url);
}

void WebPage::getCenterForZoomGesture(const IntPoint& centerInViewCoordinates, CompletionHandler<void(WebCore::IntPoint&&)>&& completionHandler)
{
    IntPoint result = mainFrameView()->rootViewToContents(centerInViewCoordinates);
    double scale = m_page->pageScaleFactor();
    result.scale(1 / scale, 1 / scale);
    completionHandler(WTFMove(result));
}

void WebPage::collapseSelectionInFrame(FrameIdentifier frameID)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    if (!frame || !frame->coreFrame())
        return;

    // Collapse the selection without clearing it.
    const VisibleSelection& selection = frame->coreFrame()->selection().selection();
    frame->coreFrame()->selection().setBase(selection.extent(), selection.affinity());
}

void WebPage::showEmojiPicker(Frame& frame)
{
    CompletionHandler<void(String)> completionHandler = [frame = makeRef(frame)](String result) {
        if (!result.isEmpty())
            frame->editor().insertText(result, nullptr);
    };
    sendWithAsyncReply(Messages::WebPageProxy::ShowEmojiPicker(frame.view()->contentsToRootView(frame.selection().absoluteCaretBounds())), WTFMove(completionHandler));
}

void WebPage::themeDidChange(String&& themeName)
{
    if (m_themeName == themeName)
        return;

    m_themeName = WTFMove(themeName);
    g_object_set(gtk_settings_get_default(), "gtk-theme-name", m_themeName.utf8().data(), nullptr);
    Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
}

} // namespace WebKit
