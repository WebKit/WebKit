/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "JSWebExtensionAPIPort.h"
#include "WebExtensionAPIEvent.h"
#include "WebExtensionAPIObject.h"
#include "WebExtensionMessageSenderParameters.h"
#include "WebExtensionPortChannelIdentifier.h"
#include <wtf/WeakPtr.h>

OBJC_CLASS NSDictionary;
OBJC_CLASS NSObject;
OBJC_CLASS NSString;

namespace WebKit {

class WebExtensionAPIPort : public WebExtensionAPIObject, public JSWebExtensionWrappable, public CanMakeWeakPtr<WebExtensionAPIPort> {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIPort, port, port);

public:
#if PLATFORM(COCOA)
    using PortSet = HashSet<Ref<WebExtensionAPIPort>>;

    static PortSet get(WebExtensionPortChannelIdentifier);

    WebExtensionContentWorldType targetContentWorldType() const { return m_targetContentWorldType; }
    WebExtensionPortChannelIdentifier channelIdentifier() const { return m_channelIdentifier; }
    WebPageProxyIdentifier owningPageProxyIdentifier() const { return m_owningPageProxyIdentifier; }
    const std::optional<WebExtensionMessageSenderParameters>& senderParameters() const { return m_senderParameters; }

    void postMessage(WebFrame&, NSString *, NSString **outExceptionString);
    void disconnect();

    bool isDisconnected() const { return m_disconnected; }
    bool isQuarantined() const { return !m_channelIdentifier; }

    NSString *name();
    NSDictionary *sender();

    JSValue *error();
    void setError(JSValue *);

    WebExtensionAPIEvent& onMessage();
    WebExtensionAPIEvent& onDisconnect();

    virtual ~WebExtensionAPIPort()
    {
        // Don't fire the disconnect event, since this port is being finalized
        // and we can't call into JavaScript during garbage collection.
        m_disconnected = true;

        remove();
    }

private:
    friend class WebExtensionContextProxy;

    explicit WebExtensionAPIPort(const WebExtensionAPIObject& parentObject, const String& name)
        : WebExtensionAPIObject(parentObject)
        , m_targetContentWorldType(WebExtensionContentWorldType::Main)
        , m_name(name)
    {
        ASSERT(isQuarantined());
    }

    explicit WebExtensionAPIPort(const WebExtensionAPIObject& parentObject, WebPage& page, WebExtensionContentWorldType targetContentWorldType, const String& name)
        : WebExtensionAPIObject(parentObject)
        , m_targetContentWorldType(targetContentWorldType)
        , m_owningPageProxyIdentifier(page.webPageProxyIdentifier())
        , m_channelIdentifier(WebExtensionPortChannelIdentifier::generate())
        , m_name(name)
    {
        add();
    }

    explicit WebExtensionAPIPort(WebExtensionContentWorldType contentWorldType, WebExtensionAPIRuntimeBase& runtime, WebExtensionContextProxy& context, WebPage& page, WebExtensionContentWorldType targetContentWorldType, const String& name)
        : WebExtensionAPIObject(contentWorldType, runtime, context)
        , m_targetContentWorldType(targetContentWorldType)
        , m_owningPageProxyIdentifier(page.webPageProxyIdentifier())
        , m_channelIdentifier(WebExtensionPortChannelIdentifier::generate())
        , m_name(name)
    {
        add();
    }

    explicit WebExtensionAPIPort(const WebExtensionAPIObject& parentObject, WebPage& page, WebExtensionContentWorldType targetContentWorldType, const String& name, const WebExtensionMessageSenderParameters& senderParameters)
        : WebExtensionAPIObject(parentObject)
        , m_targetContentWorldType(targetContentWorldType)
        , m_owningPageProxyIdentifier(page.webPageProxyIdentifier())
        , m_channelIdentifier(WebExtensionPortChannelIdentifier::generate())
        , m_name(name)
        , m_senderParameters(senderParameters)
    {
        add();
    }

    explicit WebExtensionAPIPort(const WebExtensionAPIObject& parentObject, WebPage& page, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier, const String& name, const WebExtensionMessageSenderParameters& senderParameters)
        : WebExtensionAPIObject(parentObject)
        , m_targetContentWorldType(targetContentWorldType)
        , m_owningPageProxyIdentifier(page.webPageProxyIdentifier())
        , m_channelIdentifier(channelIdentifier)
        , m_name(name)
        , m_senderParameters(senderParameters)
    {
        add();
    }

    void add();
    void remove();

    void fireMessageEventIfNeeded(id message);
    void fireDisconnectEventIfNeeded();

    WebExtensionContentWorldType m_targetContentWorldType;
    WebPageProxyIdentifier m_owningPageProxyIdentifier;
    WebExtensionPortChannelIdentifier m_channelIdentifier;
    bool m_disconnected { false };

    String m_name;
    RetainPtr<JSValue> m_error;
    std::optional<WebExtensionMessageSenderParameters> m_senderParameters;

    RefPtr<WebExtensionAPIEvent> m_onMessage;
    RefPtr<WebExtensionAPIEvent> m_onDisconnect;
#endif
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
