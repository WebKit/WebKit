/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "EditorState.h"
#include "InputMethodState.h"
#include "PageClientImpl.h"

#if USE(ATK)
#include <atk/atk.h>
#endif

namespace WebKit {

void WebPageProxy::platformInitialize()
{
}

struct wpe_view_backend* WebPageProxy::viewBackend()
{
    return static_cast<PageClientImpl&>(pageClient()).viewBackend();
}

void WebPageProxy::bindAccessibilityTree(const String& plugID, CompletionHandler<void(String&&)>&& completionHandler)
{
#if USE(ATK)
    auto* accessible = static_cast<PageClientImpl&>(pageClient()).accessible();
    atk_socket_embed(ATK_SOCKET(accessible), const_cast<char*>(plugID.utf8().data()));
    atk_object_notify_state_change(accessible, ATK_STATE_TRANSIENT, FALSE);
    completionHandler({ });
#endif
}

void WebPageProxy::didUpdateEditorState(const EditorState&, const EditorState& newEditorState)
{
    if (!newEditorState.shouldIgnoreSelectionChanges)
        pageClient().selectionDidChange();
}

void WebPageProxy::sendMessageToWebViewWithReply(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    static_cast<PageClientImpl&>(pageClient()).sendMessageToWebView(WTFMove(message), WTFMove(completionHandler));
}

void WebPageProxy::sendMessageToWebView(UserMessage&& message)
{
    sendMessageToWebViewWithReply(WTFMove(message), [](UserMessage&&) { });
}

void WebPageProxy::setInputMethodState(std::optional<InputMethodState>&& state)
{
    static_cast<PageClientImpl&>(pageClient()).setInputMethodState(WTFMove(state));
}

} // namespace WebKit
