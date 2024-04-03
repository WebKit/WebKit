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
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "WKWebViewInternal.h"
#import "WebExtensionContentWorldType.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebPageProxy.h"

namespace WebKit {

void WebExtensionContext::portPostMessage(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, std::optional<WebPageProxyIdentifier> sendingPageProxyIdentifier, WebExtensionPortChannelIdentifier channelIdentifier, const String& messageJSON)
{
    if (sendingPageProxyIdentifier && isBackgroundPage(sendingPageProxyIdentifier.value()))
        m_lastBackgroundPortActivityTime = MonotonicTime::now();

    if (!isPortConnected(sourceContentWorldType, targetContentWorldType, channelIdentifier)) {
        // The port might not be open on the other end yet. Queue the message until it does open.
        RELEASE_LOG_DEBUG(Extensions, "Enqueued message for port channel %{public}llu in %{public}@ world", channelIdentifier.toUInt64(), (NSString *)toDebugString(targetContentWorldType));

        auto& messages = m_portQueuedMessages.ensure({ targetContentWorldType, channelIdentifier }, [] {
            return Vector<MessagePageProxyIdentifierPair> { };
        }).iterator->value;

        messages.append({ messageJSON, sendingPageProxyIdentifier });

        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Sending message to port channel %{public}llu in %{public}@ world", channelIdentifier.toUInt64(), (NSString *)toDebugString(targetContentWorldType));

    constexpr auto type = WebExtensionEventListenerType::PortOnMessage;

    switch (targetContentWorldType) {
    case WebExtensionContentWorldType::Main:
#if ENABLE(INSPECTOR_EXTENSIONS)
    case WebExtensionContentWorldType::Inspector:
#endif
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPortMessageEvent(sendingPageProxyIdentifier, channelIdentifier, messageJSON));
        return;

    case WebExtensionContentWorldType::ContentScript:
        sendToContentScriptProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPortMessageEvent(sendingPageProxyIdentifier, channelIdentifier, messageJSON));
        return;

    case WebExtensionContentWorldType::Native:
        if (auto nativePort = m_nativePortMap.get(channelIdentifier))
            nativePort->receiveMessage(parseJSON(messageJSON, JSONOptions::FragmentsAllowed), std::nullopt);
        return;

    case WebExtensionContentWorldType::WebPage:
        sendToProcesses(processes(type, WebExtensionContentWorldType::WebPage), Messages::WebExtensionContextProxy::DispatchPortMessageEvent(sendingPageProxyIdentifier, channelIdentifier, messageJSON));
        return;
    }
}

void WebExtensionContext::portRemoved(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionPortChannelIdentifier channelIdentifier)
{
    removePort(sourceContentWorldType, targetContentWorldType, channelIdentifier, webPageProxyIdentifier);

    firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
}

bool WebExtensionContext::pageHasOpenPorts(WebPageProxy& page)
{
    return !m_pagePortMap.get(page.identifier()).isEmpty();
}

void WebExtensionContext::disconnectPortsForPage(WebPageProxy& page)
{
    auto portCounts = m_pagePortMap.take(page.identifier());
    if (portCounts.isEmpty())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Disconnecting ports for page %{public}llu", page.identifier().toUInt64());

    for (auto& entry : portCounts) {
        auto sourceContentWorldType = std::get<0>(entry.key);
        auto targetContentWorldType = std::get<1>(entry.key);
        auto channelIdentifier = std::get<WebExtensionPortChannelIdentifier>(entry.key);

        for (size_t i = 0; i < entry.value; ++i)
            removePort(sourceContentWorldType, targetContentWorldType, channelIdentifier, page.identifier());

        firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
    }
}

void WebExtensionContext::addPorts(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier, HashCountedSet<WebPageProxyIdentifier>&& addedPortCounts)
{
    ASSERT(!addedPortCounts.isEmpty());

    for (auto& entry : addedPortCounts) {
        RELEASE_LOG_DEBUG(Extensions, "Added %{public}u port(s) for channel %{public}llu in %{public}@ world for page %{public}llu", entry.value, channelIdentifier.toUInt64(), (NSString *)toDebugString(sourceContentWorldType), entry.key.toUInt64());

        if (isBackgroundPage(entry.key))
            m_lastBackgroundPortActivityTime = MonotonicTime::now();

        m_ports.add({ sourceContentWorldType, channelIdentifier }, entry.value);

        auto& pagePorts = m_pagePortMap.ensure(entry.key, [&] {
            return HashCountedSet<PortWorldTuple> { };
        }).iterator->value;

        pagePorts.add({ sourceContentWorldType, targetContentWorldType, channelIdentifier });
    }
}

void WebExtensionContext::removePort(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier, WebPageProxyIdentifier webPageProxyIdentifier)
{
    RELEASE_LOG_DEBUG(Extensions, "Removed 1 port for channel %{public}llu in %{public}@ world for page %{public}llu", channelIdentifier.toUInt64(), (NSString *)toDebugString(sourceContentWorldType), webPageProxyIdentifier.toUInt64());

    if (m_ports.remove({ sourceContentWorldType, channelIdentifier }))
        clearQueuedPortMessages(targetContentWorldType, channelIdentifier);

    auto entry = m_pagePortMap.find(webPageProxyIdentifier);
    if (entry == m_pagePortMap.end())
        return;

    auto& pagePorts = entry->value;
    if (pagePorts.remove({ sourceContentWorldType, targetContentWorldType, channelIdentifier }))
        m_pagePortMap.remove(webPageProxyIdentifier);
}

void WebExtensionContext::addNativePort(WebExtensionMessagePort& nativePort)
{
    constexpr auto contentWorldType = WebExtensionContentWorldType::Native;
    auto channelIdentifier = nativePort.channelIdentifier();

    RELEASE_LOG_DEBUG(Extensions, "Added 1 port for channel %{public}llu in %{public}@ world", channelIdentifier.toUInt64(), (NSString *)toDebugString(contentWorldType));

    m_ports.add({ contentWorldType, channelIdentifier });
    m_nativePortMap.add(channelIdentifier, nativePort);
}

void WebExtensionContext::removeNativePort(WebExtensionMessagePort& nativePort)
{
    constexpr auto contentWorldType = WebExtensionContentWorldType::Native;
    auto channelIdentifier = nativePort.channelIdentifier();

    RELEASE_LOG_DEBUG(Extensions, "Removed 1 port for channel %{public}llu in %{public}@ world", channelIdentifier.toUInt64(), (NSString *)toDebugString(contentWorldType));

    clearQueuedPortMessages(WebExtensionContentWorldType::Main, channelIdentifier);

    m_ports.remove({ contentWorldType, channelIdentifier });
    m_nativePortMap.remove(channelIdentifier);
}

unsigned WebExtensionContext::openPortCount(WebExtensionContentWorldType contentWorldType, WebExtensionPortChannelIdentifier channelIdentifier)
{
    auto count = m_ports.count({ contentWorldType, channelIdentifier });

#if ENABLE(INSPECTOR_EXTENSIONS)
    // Inspector content world is a special alias of Main. Include it when Main is requested (and vice versa).
    if (contentWorldType == WebExtensionContentWorldType::Main)
        count += m_ports.count({ WebExtensionContentWorldType::Inspector, channelIdentifier });
    else if (contentWorldType == WebExtensionContentWorldType::Inspector)
        count += m_ports.count({ WebExtensionContentWorldType::Main, channelIdentifier });
#endif

    return count;
}

bool WebExtensionContext::isPortConnected(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier)
{
    const auto sourceWorldCount = openPortCount(sourceContentWorldType, channelIdentifier);

    RELEASE_LOG_DEBUG(Extensions, "Port channel %{public}llu has %{public}u port(s) open in %{public}@ world", channelIdentifier.toUInt64(), sourceWorldCount, (NSString *)toDebugString(sourceContentWorldType));

    // When the worlds are the same, it is connected if there are 2 ports remaining.
    if (isEqual(sourceContentWorldType, targetContentWorldType))
        return sourceWorldCount >= 2;

    const auto targetWorldCount = openPortCount(targetContentWorldType, channelIdentifier);

    RELEASE_LOG_DEBUG(Extensions, "Port channel %{public}llu has %{public}u port(s) open in %{public}@ world", channelIdentifier.toUInt64(), targetWorldCount, (NSString *)toDebugString(targetContentWorldType));

    // When the worlds are different, it is connected if there is 1 port in each world remaining.
    return sourceWorldCount && targetWorldCount;
}

Vector<WebExtensionContext::MessagePageProxyIdentifierPair> WebExtensionContext::portQueuedMessages(WebExtensionContentWorldType contentWorldType, WebExtensionPortChannelIdentifier channelIdentifier)
{
    auto messages = m_portQueuedMessages.get({ contentWorldType, channelIdentifier });

#if ENABLE(INSPECTOR_EXTENSIONS)
    // Inspector content world is a special alias of Main. Include it when Main is requested (and vice versa).
    if (contentWorldType == WebExtensionContentWorldType::Main)
        messages.appendVector(m_portQueuedMessages.get({ WebExtensionContentWorldType::Inspector, channelIdentifier }));
    else if (contentWorldType == WebExtensionContentWorldType::Inspector)
        messages.appendVector(m_portQueuedMessages.get({ WebExtensionContentWorldType::Main, channelIdentifier }));
#endif

    return messages;
}

void WebExtensionContext::fireQueuedPortMessageEventsIfNeeded(WebProcessProxy& process, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier)
{
    auto messages = portQueuedMessages(targetContentWorldType, channelIdentifier);
    if (messages.isEmpty())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Sending %{public}zu queued message(s) to port channel %{public}llu in %{public}@ world", messages.size(), channelIdentifier.toUInt64(), (NSString *)toDebugString(targetContentWorldType));

    for (auto& entry : messages) {
        auto& sendingPageProxyIdentifier = std::get<std::optional<WebPageProxyIdentifier>>(entry);
        auto& messageJSON = std::get<String>(entry);

        process.send(Messages::WebExtensionContextProxy::DispatchPortMessageEvent(sendingPageProxyIdentifier, channelIdentifier, messageJSON), identifier());
    }
}

void WebExtensionContext::sendQueuedNativePortMessagesIfNeeded(WebExtensionPortChannelIdentifier channelIdentifier)
{
    constexpr auto targetContentWorldType = WebExtensionContentWorldType::Native;
    auto messages = portQueuedMessages(targetContentWorldType, channelIdentifier);
    if (messages.isEmpty())
        return;

    auto nativePort = m_nativePortMap.get(channelIdentifier);
    if (!nativePort)
        return;

    RELEASE_LOG_DEBUG(Extensions, "Sending %{public}zu queued message(s) to port channel %{public}llu in native world", messages.size(), channelIdentifier.toUInt64());

    for (auto& entry : messages) {
        id message = parseJSON(std::get<String>(entry), JSONOptions::FragmentsAllowed);

        nativePort->sendMessage(message, [=](WebExtensionMessagePort::Error error) {
            if (error)
                nativePort->disconnect(error);
        });
    }
}

void WebExtensionContext::clearQueuedPortMessages(WebExtensionContentWorldType contentWorldType, WebExtensionPortChannelIdentifier channelIdentifier)
{
    m_portQueuedMessages.remove({ contentWorldType, channelIdentifier });

#if ENABLE(INSPECTOR_EXTENSIONS)
    // Inspector content world is a special alias of Main. Include it when Main is requested (and vice versa).
    if (contentWorldType == WebExtensionContentWorldType::Main)
        m_portQueuedMessages.remove({ WebExtensionContentWorldType::Inspector, channelIdentifier });
    else if (contentWorldType == WebExtensionContentWorldType::Inspector)
        m_portQueuedMessages.remove({ WebExtensionContentWorldType::Main, channelIdentifier });
#endif
}

void WebExtensionContext::firePortDisconnectEventIfNeeded(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier)
{
    // Only fire the disconnect event if there are no ports connected on this channel.
    if (isPortConnected(sourceContentWorldType, targetContentWorldType, channelIdentifier))
        return;

    RELEASE_LOG_DEBUG(Extensions, "Sending disconnect event for port channel %{public}llu in %{public}@ world", channelIdentifier.toUInt64(), (NSString *)toDebugString(targetContentWorldType));

    constexpr auto type = WebExtensionEventListenerType::PortOnDisconnect;

    switch (targetContentWorldType) {
    case WebExtensionContentWorldType::Main:
#if ENABLE(INSPECTOR_EXTENSIONS)
    case WebExtensionContentWorldType::Inspector:
#endif
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPortDisconnectEvent(channelIdentifier));
        return;

    case WebExtensionContentWorldType::ContentScript:
        sendToContentScriptProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPortDisconnectEvent(channelIdentifier));
        return;

    case WebExtensionContentWorldType::Native:
        if (auto nativePort = m_nativePortMap.get(channelIdentifier))
            nativePort->reportDisconnection(std::nullopt);
        return;

    case WebExtensionContentWorldType::WebPage:
        sendToProcesses(processes(type, WebExtensionContentWorldType::WebPage), Messages::WebExtensionContextProxy::DispatchPortDisconnectEvent(channelIdentifier));
        return;
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
