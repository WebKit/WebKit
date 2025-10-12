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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIPort.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPIEvent.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPIRuntime.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionControllerProxy.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"

namespace WebKit {

using PortChannelPortMap = HashMap<WebExtensionPortChannelIdentifier, HashSet<WeakRef<WebExtensionAPIPort>>>;

static PortChannelPortMap& webExtensionPorts()
{
    static MainRunLoopNeverDestroyed<PortChannelPortMap> ports;
    return ports;
}

WebExtensionAPIPort::PortSet WebExtensionAPIPort::get(WebExtensionPortChannelIdentifier identifier)
{
    PortSet result;

    auto entry = webExtensionPorts().find(identifier);
    if (entry == webExtensionPorts().end())
        return result;

    for (auto& port : entry->value)
        result.add(port.get());

    return result;
}

void WebExtensionAPIPort::add()
{
    ASSERT(!isQuarantined());

    auto addResult = webExtensionPorts().ensure(channelIdentifier(), [&] {
        return HashSet<WeakRef<WebExtensionAPIPort>> { };
    });

    addResult.iterator->value.add(*this);

    RELEASE_LOG_DEBUG(Extensions, "Added port for channel %{public}llu in %{public}@ world", channelIdentifier().toUInt64(), toDebugString(contentWorldType()).createNSString().get());
}

void WebExtensionAPIPort::remove()
{
    disconnect();

    if (isQuarantined())
        return;

    auto entry = webExtensionPorts().find(channelIdentifier());
    if (entry == webExtensionPorts().end())
        return;

    WebProcess::singleton().send(Messages::WebExtensionContext::PortRemoved(contentWorldType(), targetContentWorldType(), *owningPageProxyIdentifier(), channelIdentifier()), extensionContext().identifier());

    entry->value.remove(*this);

    if (!entry->value.isEmpty())
        return;

    webExtensionPorts().remove(entry);
}

NSString *WebExtensionAPIPort::name()
{
    return m_name.createNSString().autorelease();
}

NSDictionary *WebExtensionAPIPort::sender()
{
    return m_senderParameters ? toWebAPI(m_senderParameters.value()) : nil;
}

JSValue *WebExtensionAPIPort::error()
{
    return m_error.get();
}

void WebExtensionAPIPort::setError(JSValue *error)
{
    m_error = error;
}

void WebExtensionAPIPort::postMessage(WebFrame& frame, NSString *message, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port#postmessage

    if (isDisconnected()) {
        *outExceptionString = toErrorString(nullString(), nullString(), @"the port is disconnected").createNSString().autorelease();
        return;
    }

    if (message.length > webExtensionMaxMessageLength) {
        *outExceptionString = toErrorString(nullString(), @"message", @"it exceeded the maximum allowed length").createNSString().autorelease();
        return;
    }

    if (isQuarantined())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Sent port message for channel %{public}llu from %{public}@ world", channelIdentifier().toUInt64(), toDebugString(contentWorldType()).createNSString().get());

    WebProcess::singleton().send(Messages::WebExtensionContext::PortPostMessage(contentWorldType(), targetContentWorldType(), owningPageProxyIdentifier(), channelIdentifier(), message), extensionContext().identifier());
}

void WebExtensionAPIPort::disconnect()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port#disconnect

    fireDisconnectEventIfNeeded();
}

void WebExtensionAPIPort::fireMessageEventIfNeeded(id message)
{
    if (isDisconnected() || isQuarantined() || !m_onMessage || m_onMessage->listeners().isEmpty())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Fired port message event for channel %{public}llu in %{public}@ world", channelIdentifier().toUInt64(), toDebugString(contentWorldType()).createNSString().get());

    for (auto& listener : m_onMessage->listeners()) {
        auto globalContext = listener->globalContext();
        auto *port = toJSValue(globalContext, toJS(globalContext, this));

        listener->call(message, port);
    }
}

void WebExtensionAPIPort::fireDisconnectEventIfNeeded()
{
    if (isDisconnected())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Port channel %{public}llu disconnected in %{public}@ world", m_channelIdentifier ? m_channelIdentifier->toUInt64() : 0, toDebugString(contentWorldType()).createNSString().get());

    m_disconnected = true;

    if (m_onMessage)
        m_onMessage->removeAllListeners();

    remove();

    if (!m_onDisconnect || m_onDisconnect->listeners().isEmpty())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Fired port disconnect event for channel %{public}llu in %{public}@ world", m_channelIdentifier ? m_channelIdentifier->toUInt64() : 0, toDebugString(contentWorldType()).createNSString().get());

    for (auto& listener : m_onDisconnect->listeners()) {
        auto globalContext = listener->globalContext();
        auto *port = toJSValue(globalContext, toJS(globalContext, this));

        listener->call(port);
    }

    m_onDisconnect->removeAllListeners();
}

WebExtensionAPIEvent& WebExtensionAPIPort::onMessage()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port#onmessage

    if (!m_onMessage)
        m_onMessage = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::PortOnMessage);

    return *m_onMessage;
}

WebExtensionAPIEvent& WebExtensionAPIPort::onDisconnect()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port#ondisconnect

    if (!m_onDisconnect)
        m_onDisconnect = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::PortOnDisconnect);

    return *m_onDisconnect;
}

void WebExtensionContextProxy::dispatchPortMessageEvent(std::optional<WebPageProxyIdentifier> sendingPageProxyIdentifier, WebExtensionPortChannelIdentifier channelIdentifier, const String& messageJSON)
{
    auto ports = WebExtensionAPIPort::get(channelIdentifier);
    if (ports.isEmpty())
        return;

    id message = parseJSON(messageJSON.createNSString().get(), { JSONOptions::FragmentsAllowed });

    for (Ref port : ports) {
        // Don't send the message to other ports in the same page as the sender.
        if (sendingPageProxyIdentifier && sendingPageProxyIdentifier == port->owningPageProxyIdentifier())
            continue;

        port->fireMessageEventIfNeeded(message);
    }
}

void WebExtensionContextProxy::dispatchPortDisconnectEvent(WebExtensionPortChannelIdentifier channelIdentifier)
{
    auto ports = WebExtensionAPIPort::get(channelIdentifier);
    if (ports.isEmpty())
        return;

    for (Ref port : ports)
        port->fireDisconnectEventIfNeeded();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
