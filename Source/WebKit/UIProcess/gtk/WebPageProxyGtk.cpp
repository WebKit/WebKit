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
#include "MessageSenderInlines.h"
#include "PageClientImpl.h"
#include "WebKitUserMessage.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebPageMessages.h"
#include "WebPasteboardProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/PlatformDisplay.h>
#include <WebCore/PlatformEvent.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

void WebPageProxy::platformInitialize()
{
}

GtkWidget* WebPageProxy::viewWidget()
{
    return static_cast<PageClientImpl&>(pageClient()).viewWidget();
}

void WebPageProxy::bindAccessibilityTree(const String& plugID)
{
#if USE(GTK4)
    // FIXME: We need a way to override accessible interface of WebView and send the atspi reference to the web process.
    ASSERT_NOT_IMPLEMENTED_YET();
#else
    auto* accessible = gtk_widget_get_accessible(viewWidget());
    atk_socket_embed(ATK_SOCKET(accessible), const_cast<char*>(plugID.utf8().data()));
    atk_object_notify_state_change(accessible, ATK_STATE_TRANSIENT, FALSE);
#endif
}

void WebPageProxy::didUpdateEditorState(const EditorState&, const EditorState& newEditorState)
{
    if (newEditorState.shouldIgnoreSelectionChanges)
        return;
    if (newEditorState.selectionIsRange)
        WebPasteboardProxy::singleton().setPrimarySelectionOwner(focusedFrame());
    pageClient().selectionDidChange();
}

void WebPageProxy::setInputMethodState(std::optional<InputMethodState>&& state)
{
    webkitWebViewBaseSetInputMethodState(WEBKIT_WEB_VIEW_BASE(viewWidget()), WTFMove(state));
}

bool WebPageProxy::makeGLContextCurrent()
{
    return webkitWebViewBaseMakeGLContextCurrent(WEBKIT_WEB_VIEW_BASE(viewWidget()));
}

void WebPageProxy::showEmojiPicker(const WebCore::IntRect& caretRect, CompletionHandler<void(String)>&& completionHandler)
{
    webkitWebViewBaseShowEmojiChooser(WEBKIT_WEB_VIEW_BASE(viewWidget()), caretRect, WTFMove(completionHandler));
}

void WebPageProxy::showValidationMessage(const WebCore::IntRect& anchorClientRect, const String& message)
{
    m_validationBubble = pageClient().createValidationBubble(message, { m_preferences->minimumFontSize() });
    m_validationBubble->showRelativeTo(anchorClientRect);
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

void WebPageProxy::accentColorDidChange()
{
    if (!hasRunningProcess())
        return;

    WebCore::Color accentColor = pageClient().accentColor();

    send(Messages::WebPage::SetAccentColor(accentColor));
}

OptionSet<WebCore::PlatformEvent::Modifier> WebPageProxy::currentStateOfModifierKeys()
{
#if USE(GTK4)
    auto* device = gdk_seat_get_keyboard(gdk_display_get_default_seat(gtk_widget_get_display(viewWidget())));
    auto gdkModifiers = gdk_device_get_modifier_state(device);
    bool capsLockActive = gdk_device_get_caps_lock_state(device);
#else
    auto* keymap = gdk_keymap_get_for_display(gtk_widget_get_display(viewWidget()));
    auto gdkModifiers = gdk_keymap_get_modifier_state(keymap);
    bool capsLockActive = gdk_keymap_get_caps_lock_state(keymap);
#endif

    OptionSet<WebCore::PlatformEvent::Modifier> modifiers;
    if (gdkModifiers & GDK_SHIFT_MASK)
        modifiers.add(WebCore::PlatformEvent::Modifier::ShiftKey);
    if (gdkModifiers & GDK_CONTROL_MASK)
        modifiers.add(WebCore::PlatformEvent::Modifier::ControlKey);
    if (gdkModifiers & GDK_MOD1_MASK)
        modifiers.add(WebCore::PlatformEvent::Modifier::AltKey);
    if (gdkModifiers & GDK_META_MASK)
        modifiers.add(WebCore::PlatformEvent::Modifier::MetaKey);
    if (capsLockActive)
        modifiers.add(WebCore::PlatformEvent::Modifier::CapsLockKey);
    return modifiers;
}

} // namespace WebKit
