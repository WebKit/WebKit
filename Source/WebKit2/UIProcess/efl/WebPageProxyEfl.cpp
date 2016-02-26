/*
 * Copyright (C) 2011 Samsung Electronics
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
#include "WebPageProxy.h"

#include "EwkView.h"
#include "NativeWebKeyboardEvent.h"
#include "NotImplemented.h"
#include "UserAgentEfl.h"
#include "WebPageMessages.h"
#include "WebProcessProxy.h"
#include "WebView.h"
#include "WebsiteDataStore.h"

namespace WebKit {

void WebPageProxy::platformInitialize()
{
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return WebCore::standardUserAgent(applicationNameForUserAgent);
}

void WebPageProxy::getEditorCommandsForKeyEvent(Vector<WTF::String>& /*commandsList*/)
{
    notImplemented();
}

void WebPageProxy::saveRecentSearches(const String&, const Vector<WebCore::RecentSearch>&)
{
    notImplemented();
}

void WebPageProxy::loadRecentSearches(const String&, Vector<WebCore::RecentSearch>&)
{
    notImplemented();
}

void WebsiteDataStore::platformRemoveRecentSearches(std::chrono::system_clock::time_point oldestTimeToRemove)
{
    notImplemented();
}

void WebPageProxy::editorStateChanged(const EditorState& editorState)
{
    m_editorState = editorState;
    
    if (editorState.shouldIgnoreCompositionSelectionChange)
        return;
    m_pageClient.updateTextInputState();
}

void WebPageProxy::setThemePath(const String& themePath)
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::SetThemePath(themePath), m_pageID, 0);
}

void WebPageProxy::createPluginContainer(uint64_t&)
{
    notImplemented();
}

void WebPageProxy::windowedPluginGeometryDidChange(const WebCore::IntRect&, const WebCore::IntRect&, uint64_t)
{
    notImplemented();
}

void WebPageProxy::windowedPluginVisibilityDidChange(bool, uint64_t)
{
    notImplemented();
}

void WebPageProxy::handleInputMethodKeydown(bool& handled)
{
    handled = m_keyEventQueue.first().isFiltered();
}

void WebPageProxy::confirmComposition(const String& compositionString)
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::ConfirmComposition(compositionString), m_pageID, 0);
}

void WebPageProxy::setComposition(const String& compositionString, Vector<WebCore::CompositionUnderline>& underlines, int cursorPosition)
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::SetComposition(compositionString, underlines, cursorPosition), m_pageID, 0);
}

void WebPageProxy::cancelComposition()
{
    if (!isValid())
        return;

    process().send(Messages::WebPage::CancelComposition(), m_pageID, 0);
}

#if HAVE(ACCESSIBILITY) && defined(HAVE_ECORE_X)

bool WebPageProxy::accessibilityObjectReadByPoint(const WebCore::IntPoint& point)
{
    UNUSED_PARAM(point);
    notImplemented();
    return false;
}

bool WebPageProxy::accessibilityObjectReadPrevious()
{
    notImplemented();
    return false;
}

bool WebPageProxy::accessibilityObjectReadNext()
{
    notImplemented();
    return false;
}

#endif

} // namespace WebKit
