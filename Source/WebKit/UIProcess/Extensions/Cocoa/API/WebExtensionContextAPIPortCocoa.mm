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

#import "Logging.h"
#import "WebExtensionContentWorldType.h"
#import "WebExtensionContextProxyMessages.h"

namespace WebKit {

void WebExtensionContext::portPostMessage(WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier, const String& messageJSON)
{
    if (!m_ports.contains({ targetContentWorldType, channelIdentifier })) {
        // The port might not be open on the other end yet. Queue the message until it does open.
        RELEASE_LOG_DEBUG(Extensions, "Enqueued message for port channel %{public}llu in %{public}@ world", channelIdentifier.toUInt64(), (NSString *)toDebugString(targetContentWorldType));
        auto& messages = m_portQueuedMessages.ensure({ targetContentWorldType, channelIdentifier }, [] {
            return Vector<String> { };
        }).iterator->value;
        messages.append(messageJSON);
        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Sending message to port channel %{public}llu in %{public}@ world", channelIdentifier.toUInt64(), (NSString *)toDebugString(targetContentWorldType));

    constexpr auto type = WebExtensionEventListenerType::PortOnMessage;

    switch (targetContentWorldType) {
    case WebExtensionContentWorldType::Main:
        wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
            sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPortMessageEvent(channelIdentifier, messageJSON));
        });

        return;

    case WebExtensionContentWorldType::ContentScript:
        sendToContentScriptProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPortMessageEvent(channelIdentifier, messageJSON));
        return;
    }
}

void WebExtensionContext::portDisconnect(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier)
{
    RELEASE_LOG_DEBUG(Extensions, "Port for channel %{public}llu disconnected in %{public}@ world", channelIdentifier.toUInt64(), (NSString *)toDebugString(sourceContentWorldType));

    m_portQueuedMessages.remove({ sourceContentWorldType, channelIdentifier });
    m_ports.remove({ sourceContentWorldType, channelIdentifier });

    firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
}

void WebExtensionContext::addPorts(WebExtensionContentWorldType contentWorldType, WebExtensionPortChannelIdentifier channelIdentifier, size_t totalPortObjects)
{
    ASSERT(totalPortObjects);

    RELEASE_LOG_DEBUG(Extensions, "Added %{public}zu port(s) for channel %{public}llu in %{public}@ world", totalPortObjects, channelIdentifier.toUInt64(), (NSString *)toDebugString(contentWorldType));

    for (size_t i = 0; i < totalPortObjects; ++i)
        m_ports.add({ contentWorldType, channelIdentifier });
}

bool WebExtensionContext::isPortConnected(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier)
{
    const auto sourceWorldCount = m_ports.count({ sourceContentWorldType, channelIdentifier });

    RELEASE_LOG_DEBUG(Extensions, "Port channel %{public}llu has %{public}u port(s) open in %{public}@ world", channelIdentifier.toUInt64(), sourceWorldCount, (NSString *)toDebugString(sourceContentWorldType));

    // When the worlds are the same, it is connected if there are 2 ports remaining.
    if (sourceContentWorldType == targetContentWorldType)
        return sourceWorldCount >= 2;

    const auto targetWorldCount = m_ports.count({ targetContentWorldType, channelIdentifier });

    RELEASE_LOG_DEBUG(Extensions, "Port channel %{public}llu has %{public}u port(s) open in %{public}@ world", channelIdentifier.toUInt64(), targetWorldCount, (NSString *)toDebugString(targetContentWorldType));

    // When the worlds are different, it is connected if there is 1 port in each world remaining.
    return sourceWorldCount && targetWorldCount;
}

void WebExtensionContext::fireQueuedPortMessageEventIfNeeded(WebProcessProxy& process, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier)
{
    auto messages = m_portQueuedMessages.get({ targetContentWorldType, channelIdentifier });
    if (messages.isEmpty())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Sending %{public}zu queued message(s) to port channel %{public}llu in %{public}@ world", messages.size(), channelIdentifier.toUInt64(), (NSString *)toDebugString(targetContentWorldType));

    for (auto& messageJSON : messages)
        process.send(Messages::WebExtensionContextProxy::DispatchPortMessageEvent(channelIdentifier, messageJSON), identifier());
}

void WebExtensionContext::clearQueuedPortMessages(WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier channelIdentifier)
{
    m_portQueuedMessages.remove({ targetContentWorldType, channelIdentifier });
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
        wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
            sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPortDisconnectEvent(channelIdentifier));
        });

        return;

    case WebExtensionContentWorldType::ContentScript:
        sendToContentScriptProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPortDisconnectEvent(channelIdentifier));
        return;
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
