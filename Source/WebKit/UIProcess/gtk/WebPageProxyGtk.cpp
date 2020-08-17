/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#include "InputMethodState.h"
#include "PageClientImpl.h"
#include "WebKitUserMessage.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebPageMessages.h"
#include "WebPasteboardProxy.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/UserAgent.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

void WebPageProxy::platformInitialize()
{
}

GtkWidget* WebPageProxy::viewWidget()
{
    return static_cast<PageClientImpl&>(pageClient()).viewWidget();
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return WebCore::standardUserAgent(applicationNameForUserAgent);
}

void WebPageProxy::bindAccessibilityTree(const String& plugID)
{
    auto* accessible = gtk_widget_get_accessible(viewWidget());
    atk_socket_embed(ATK_SOCKET(accessible), const_cast<char*>(plugID.utf8().data()));
    atk_object_notify_state_change(accessible, ATK_STATE_TRANSIENT, FALSE);
}

void WebPageProxy::saveRecentSearches(const String&, const Vector<WebCore::RecentSearch>&)
{
    notImplemented();
}

void WebPageProxy::loadRecentSearches(const String&, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

void WebsiteDataStore::platformRemoveRecentSearches(WallTime oldestTimeToRemove)
{
    UNUSED_PARAM(oldestTimeToRemove);
    notImplemented();
}

void WebPageProxy::updateEditorState(const EditorState& editorState)
{
    m_editorState = editorState;
    
    if (editorState.shouldIgnoreSelectionChanges)
        return;
    if (m_editorState.selectionIsRange)
        WebPasteboardProxy::singleton().setPrimarySelectionOwner(focusedFrame());
    pageClient().selectionDidChange();
}

void WebPageProxy::setInputMethodState(Optional<InputMethodState>&& state)
{
    webkitWebViewBaseSetInputMethodState(WEBKIT_WEB_VIEW_BASE(viewWidget()), WTFMove(state));
}

void WebPageProxy::getCenterForZoomGesture(const WebCore::IntPoint& centerInViewCoordinates, WebCore::IntPoint& center)
{
    sendSync(Messages::WebPage::GetCenterForZoomGesture(centerInViewCoordinates), Messages::WebPage::GetCenterForZoomGesture::Reply(center));
}

bool WebPageProxy::makeGLContextCurrent()
{
    return webkitWebViewBaseMakeGLContextCurrent(WEBKIT_WEB_VIEW_BASE(viewWidget()));
}

void WebPageProxy::showEmojiPicker(const WebCore::IntRect& caretRect, CompletionHandler<void(String)>&& completionHandler)
{
    webkitWebViewBaseShowEmojiChooser(WEBKIT_WEB_VIEW_BASE(viewWidget()), caretRect, WTFMove(completionHandler));
}

void WebPageProxy::sendMessageToWebViewWithReply(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    if (!WEBKIT_IS_WEB_VIEW(viewWidget())) {
        completionHandler(UserMessage(message.name, WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE));
        return;
    }

    webkitWebViewDidReceiveUserMessage(WEBKIT_WEB_VIEW(viewWidget()), WTFMove(message), WTFMove(completionHandler));
}

void WebPageProxy::sendMessageToWebView(UserMessage&& message)
{
    sendMessageToWebViewWithReply(WTFMove(message), [](UserMessage&&) { });
}

void WebPageProxy::themeDidChange()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::ThemeDidChange(pageClient().themeName()));
    effectiveAppearanceDidChange();
}

} // namespace WebKit
