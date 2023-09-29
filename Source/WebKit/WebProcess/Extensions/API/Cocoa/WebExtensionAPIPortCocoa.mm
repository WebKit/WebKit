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
#import "WebExtensionUtilities.h"
#import "WebProcess.h"

namespace WebKit {

using CheckedPortSet = HashSet<CheckedPtr<WebExtensionAPIPort>>;
using PortChannelPortMap = HashMap<WebExtensionPortChannelIdentifier, CheckedPortSet>;

static PortChannelPortMap& webExtensionPorts()
{
    static MainThreadNeverDestroyed<PortChannelPortMap> ports;
    return ports;
}

WebExtensionAPIPort::PortSet WebExtensionAPIPort::get(WebExtensionPortChannelIdentifier identifier)
{
    PortSet result;

    auto entry = webExtensionPorts().find(identifier);
    if (entry == webExtensionPorts().end())
        return result;

    for (auto& port : entry->value)
        result.add(*port);

    return result;
}

void WebExtensionAPIPort::add()
{
    auto addResult = webExtensionPorts().ensure(channelIdentifier(), [&] {
        return CheckedPortSet { };
    });

    addResult.iterator->value.add(this);

    RELEASE_LOG_DEBUG(Extensions, "Added port for channel %{public}llu in %{public}@ world", channelIdentifier().toUInt64(), (NSString *)toDebugString(contentWorldType()));
}

void WebExtensionAPIPort::remove()
{
    disconnect();

    auto entry = webExtensionPorts().find(channelIdentifier());
    if (entry == webExtensionPorts().end())
        return;

    entry->value.remove(this);

    if (!entry->value.isEmpty())
        return;

    webExtensionPorts().remove(entry);
}

NSString *WebExtensionAPIPort::name()
{
    return m_name;
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

void WebExtensionAPIPort::postMessage(WebFrame *frame, NSString *message, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port#postmessage

    if (disconnected()) {
        *outExceptionString = toErrorString(nil, nil, @"the port is disconnected");
        return;
    }

    if (message.length > webExtensionMaxMessageLength) {
        *outExceptionString = toErrorString(nil, @"message", @"it exceeded the maximum allowed length");
        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Sent port message for channel %{public}llu from %{public}@ world", channelIdentifier().toUInt64(), (NSString *)toDebugString(contentWorldType()));

    WebProcess::singleton().send(Messages::WebExtensionContext::PortPostMessage(targetContentWorldType(), channelIdentifier(), message), extensionContext().identifier());
}

void WebExtensionAPIPort::disconnect()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port#disconnect

    fireDisconnectEventIfNeeded();
}

void WebExtensionAPIPort::fireMessageEventIfNeeded(id message)
{
    if (disconnected() || !m_onMessage || m_onMessage->listeners().isEmpty())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Fired port message event for channel %{public}llu in %{public}@ world", channelIdentifier().toUInt64(), (NSString *)toDebugString(contentWorldType()));

    for (auto& listener : m_onMessage->listeners()) {
        auto globalContext = listener->globalContext();
        auto *port = toJSValue(globalContext, toJS(globalContext, this));

        listener->call(message, port);
    }
}

void WebExtensionAPIPort::fireDisconnectEventIfNeeded()
{
    if (disconnected())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Port channel %{public}llu disconnected in %{public}@ world", channelIdentifier().toUInt64(), (NSString *)toDebugString(contentWorldType()));

    m_disconnected = true;

    if (m_onMessage)
        m_onMessage->removeAllListeners();

    remove();

    WebProcess::singleton().send(Messages::WebExtensionContext::PortDisconnect(contentWorldType(), targetContentWorldType(), channelIdentifier()), extensionContext().identifier());

    if (!m_onDisconnect || m_onDisconnect->listeners().isEmpty())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Fired port disconnect event for channel %{public}llu in %{public}@ world", channelIdentifier().toUInt64(), (NSString *)toDebugString(contentWorldType()));

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
        m_onMessage = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::PortOnMessage);

    return *m_onMessage;
}

WebExtensionAPIEvent& WebExtensionAPIPort::onDisconnect()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port#ondisconnect

    if (!m_onDisconnect)
        m_onDisconnect = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::PortOnDisconnect);

    return *m_onDisconnect;
}

void WebExtensionContextProxy::dispatchPortMessageEvent(WebExtensionPortChannelIdentifier channelIdentifier, const String& messageJSON)
{
    auto ports = WebExtensionAPIPort::get(channelIdentifier);
    if (ports.isEmpty())
        return;

    id message = parseJSON(messageJSON, { JSONOptions::FragmentsAllowed });

    for (auto& port : ports)
        port->fireMessageEventIfNeeded(message);
}

void WebExtensionContextProxy::dispatchPortDisconnectEvent(WebExtensionPortChannelIdentifier channelIdentifier)
{
    auto ports = WebExtensionAPIPort::get(channelIdentifier);
    if (ports.isEmpty())
        return;

    for (auto& port : ports)
        port->fireDisconnectEventIfNeeded();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
