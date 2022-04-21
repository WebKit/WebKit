/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebBroadcastChannelRegistry.h"

#include <WebCore/BroadcastChannel.h>
#include <WebCore/SerializedScriptValue.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

Ref<WebBroadcastChannelRegistry> WebBroadcastChannelRegistry::getOrCreate(bool privateSession)
{
    static NeverDestroyed<WeakPtr<WebBroadcastChannelRegistry>> defaultSessionRegistry;
    static NeverDestroyed<WeakPtr<WebBroadcastChannelRegistry>> privateSessionRegistry;
    auto& existingRegistry = privateSession ? privateSessionRegistry : defaultSessionRegistry;
    if (existingRegistry.get())
        return *existingRegistry.get();

    auto registry = adoptRef(*new WebBroadcastChannelRegistry);
    existingRegistry.get() = registry;
    return registry;
}

void WebBroadcastChannelRegistry::registerChannel(const WebCore::ClientOrigin& origin, const String& name, WebCore::BroadcastChannelIdentifier identifier)
{
    ASSERT(isMainThread());
    auto& channelsForOrigin = m_channels.ensure(origin, [] { return NameToChannelIdentifiersMap { }; }).iterator->value;
    auto& channelsForName = channelsForOrigin.ensure(name, [] { return Vector<WebCore::BroadcastChannelIdentifier> { }; }).iterator->value;
    ASSERT(!channelsForName.contains(identifier));
    channelsForName.append(identifier);
}

void WebBroadcastChannelRegistry::unregisterChannel(const WebCore::ClientOrigin& origin, const String& name, WebCore::BroadcastChannelIdentifier identifier)
{
    ASSERT(isMainThread());
    auto channelsForOriginIterator = m_channels.find(origin);
    ASSERT(channelsForOriginIterator != m_channels.end());
    if (channelsForOriginIterator == m_channels.end())
        return;
    auto channelsForNameIterator = channelsForOriginIterator->value.find(name);
    ASSERT(channelsForNameIterator != channelsForOriginIterator->value.end());
    ASSERT(channelsForNameIterator->value.contains(identifier));
    channelsForNameIterator->value.removeFirst(identifier);
}

void WebBroadcastChannelRegistry::postMessage(const WebCore::ClientOrigin& origin, const String& name, WebCore::BroadcastChannelIdentifier source, Ref<WebCore::SerializedScriptValue>&& message, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainThread());
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    auto channelsForOriginIterator = m_channels.find(origin);
    ASSERT(channelsForOriginIterator != m_channels.end());
    if (channelsForOriginIterator == m_channels.end())
        return;

    auto channelsForNameIterator = channelsForOriginIterator->value.find(name);
    ASSERT(channelsForNameIterator != channelsForOriginIterator->value.end());
    for (auto& channelIdentifier : channelsForNameIterator->value) {
        if (channelIdentifier == source)
            continue;
        WebCore::BroadcastChannel::dispatchMessageTo(channelIdentifier, message.copyRef(), [callbackAggregator] { });
    }
}
