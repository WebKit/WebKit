/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIObject.h"
#include "WebExtensionPortChannelIdentifier.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>

OBJC_CLASS NSError;
OBJC_CLASS WKWebExtensionMessagePort;

namespace WebKit {

class WebExtensionContext;

class WebExtensionMessagePort : public API::ObjectImpl<API::Object::Type::WebExtensionMessagePort>, public CanMakeWeakPtr<WebExtensionMessagePort> {
    WTF_MAKE_NONCOPYABLE(WebExtensionMessagePort);

public:
    template<typename... Args>
    static Ref<WebExtensionMessagePort> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionMessagePort(std::forward<Args>(args)...));
    }

    explicit WebExtensionMessagePort(WebExtensionContext&, String applicationIdentifier, WebExtensionPortChannelIdentifier);

    ~WebExtensionMessagePort();

    enum class ErrorType : uint8_t {
        Unknown = 1,
        NotConnected,
        MessageInvalid,
    };

    using Error = std::optional<std::pair<ErrorType, std::optional<String>>>;

    bool operator==(const WebExtensionMessagePort&) const;

    const String& applicationIdentifier() const { return m_applicationIdentifier; }
    WebExtensionPortChannelIdentifier channelIdentifier() const { return m_channelIdentifier; }
    WebExtensionContext* extensionContext() const;

    void disconnect(Error = std::nullopt);
    void reportDisconnection(Error);
    bool isDisconnected() const;

#if PLATFORM(COCOA)
    void sendMessage(id message, CompletionHandler<void(Error)>&& = { });
    void receiveMessage(id message, Error);
#endif

#ifdef __OBJC__
    WKWebExtensionMessagePort *wrapper() const { return (WKWebExtensionMessagePort *)API::ObjectImpl<API::Object::Type::WebExtensionMessagePort>::wrapper(); }
#endif

private:
    void remove();

    WeakPtr<WebExtensionContext> m_extensionContext;
    String m_applicationIdentifier;
    WebExtensionPortChannelIdentifier m_channelIdentifier;
};

NSError *toAPI(WebExtensionMessagePort::Error);
WebExtensionMessagePort::Error toWebExtensionMessagePortError(NSError *);

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
