/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "BroadcastChannel.h"

#include "BroadcastChannelRegistry.h"
#include "EventNames.h"
#include "MessageEvent.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include <wtf/CallbackAggregator.h>
#include <wtf/HashMap.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(BroadcastChannel);

static Lock allBroadcastChannelsLock;
static HashMap<BroadcastChannelIdentifier, BroadcastChannel*>& allBroadcastChannels() WTF_REQUIRES_LOCK(allBroadcastChannelsLock)
{
    static NeverDestroyed<HashMap<BroadcastChannelIdentifier, BroadcastChannel*>> map;
    return map;
}

static HashMap<BroadcastChannelIdentifier, ScriptExecutionContextIdentifier>& channelToContextIdentifier()
{
    ASSERT(isMainThread());
    static NeverDestroyed<HashMap<BroadcastChannelIdentifier, ScriptExecutionContextIdentifier>> map;
    return map;
}

BroadcastChannel::BroadcastChannel(ScriptExecutionContext& context, const String& name)
    : ActiveDOMObject(&context)
    , m_name(name)
    , m_origin(context.securityOrigin()->data())
    , m_identifier(BroadcastChannelIdentifier::generateThreadSafe())
{
    {
        Locker locker { allBroadcastChannelsLock };
        allBroadcastChannels().add(m_identifier, this);
    }

    ensureOnMainThread([origin = crossThreadCopy(m_origin), name = crossThreadCopy(m_name), contextIdentifier = context.contextIdentifier(), channelIdentifier = m_identifier](auto& document) {
        if (auto* page = document.page())
            page->broadcastChannelRegistry().registerChannel(origin, name, channelIdentifier);
        channelToContextIdentifier().add(channelIdentifier, contextIdentifier);
    });
}

BroadcastChannel::~BroadcastChannel()
{
    close();
    {
        Locker locker { allBroadcastChannelsLock };
        allBroadcastChannels().remove(m_identifier);
    }
}

ExceptionOr<void> BroadcastChannel::postMessage(JSC::JSGlobalObject& globalObject, JSC::JSValue message)
{
    if (m_isClosed)
        return Exception { InvalidStateError, "This BroadcastChannel is closed" };

    Vector<RefPtr<MessagePort>> ports;
    auto messageData = SerializedScriptValue::create(globalObject, message, { }, ports);
    if (messageData.hasException())
        return messageData.releaseException();
    ASSERT(ports.isEmpty());

    ensureOnMainThread([origin = crossThreadCopy(m_origin), name = crossThreadCopy(m_name), identifier = m_identifier, messageData = messageData.releaseReturnValue()](auto& document) mutable {
        auto* page = document.page();
        if (!page)
            return;

        auto blobHandles = messageData->blobHandles();
        page->broadcastChannelRegistry().postMessage(origin, name, identifier, WTFMove(messageData), [blobHandles = WTFMove(blobHandles)] {
            // Keeps Blob data inside messageData alive until the message has been delivered.
        });
    });

    return { };
}

void BroadcastChannel::close()
{
    if (m_isClosed)
        return;

    m_isClosed = true;
    ensureOnMainThread([origin = crossThreadCopy(m_origin), name = crossThreadCopy(m_name), channelIdentifier = m_identifier](auto& document) {
        if (auto* page = document.page())
            page->broadcastChannelRegistry().unregisterChannel(origin, name, channelIdentifier);
        channelToContextIdentifier().remove(channelIdentifier);
    });
}

void BroadcastChannel::dispatchMessageTo(BroadcastChannelIdentifier channelIdentifier, Ref<SerializedScriptValue>&& message, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainThread());
    auto contextIdentifier = channelToContextIdentifier().get(channelIdentifier);
    if (!contextIdentifier)
        return completionHandler();

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    ScriptExecutionContext::postTaskTo(contextIdentifier, [channelIdentifier, message = WTFMove(message), callbackAggregator = WTFMove(callbackAggregator)](auto&) mutable {
        RefPtr<BroadcastChannel> channel;
        {
            Locker locker { allBroadcastChannelsLock };
            channel = allBroadcastChannels().get(channelIdentifier);
        }
        if (channel)
            channel->dispatchMessage(WTFMove(message));

        callOnMainThread([callbackAggregator = WTFMove(callbackAggregator)] { });
    });
}

void BroadcastChannel::dispatchMessage(Ref<SerializedScriptValue>&& message)
{
    if (m_isClosed)
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::PostedMessageQueue, [this, message = WTFMove(message)]() mutable {
        if (!m_isClosed)
            dispatchEvent(MessageEvent::create({ }, WTFMove(message), m_origin.toString()));
    });
}

void BroadcastChannel::ensureOnMainThread(Function<void(Document&)>&& task)
{
    auto* context = scriptExecutionContext();
    if (!context)
        return;

    if (is<Document>(*context))
        task(downcast<Document>(*context));
    else {
        downcast<WorkerGlobalScope>(*context).thread().workerLoaderProxy().postTaskToLoader([task = WTFMove(task)](auto& context) {
            task(downcast<Document>(context));
        });
    }
}

const char* BroadcastChannel::activeDOMObjectName() const
{
    return "BroadcastChannel";
}

void BroadcastChannel::eventListenersDidChange()
{
    m_hasRelevantEventListener = hasEventListeners(eventNames().messageEvent);
}

bool BroadcastChannel::virtualHasPendingActivity() const
{
    return !m_isClosed && m_hasRelevantEventListener;
}

} // namespace WebCore
