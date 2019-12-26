/*
 * Copyright (C) 2019 Igalia S.L.
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

#include "UserMessage.h"
#include "WebKitExtensionManager.h"
#include "WebKitUserMessage.h"
#include "WebKitWebExtension.h"
#include "WebKitWebPagePrivate.h"
#include "WebPageProxyMessages.h"

namespace WebKit {

void WebPage::sendMessageToWebExtensionWithReply(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    auto* extension = WebKitExtensionManager::singleton().extension();
    if (!extension) {
        completionHandler(UserMessage(message.name, WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE));
        return;
    }

    auto* page = webkit_web_extension_get_page(extension, m_identifier.toUInt64());
    if (!page) {
        completionHandler(UserMessage(message.name, WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE));
        return;
    }

    webkitWebPageDidReceiveUserMessage(page, WTFMove(message), WTFMove(completionHandler));
}

void WebPage::sendMessageToWebExtension(UserMessage&& message)
{
    sendMessageToWebExtensionWithReply(WTFMove(message), [](UserMessage&&) { });
}

void WebPage::setInputMethodState(bool enabled)
{
    if (m_inputMethodEnabled == enabled)
        return;

    m_inputMethodEnabled = enabled;
    send(Messages::WebPageProxy::SetInputMethodState(enabled));
}

} // namespace WebKit
