/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "IPCTesterReceiver.h"

#include <wtf/CompletionHandler.h>

#if ENABLE(IPC_TESTING_API) && !ENABLE(IPC_TESTING_SWIFT)

#include "Connection.h"
#include "Logging.h"
#include <wtf/CompletionHandler.h>

namespace WebKit {

Ref<IPCTesterReceiver> IPCTesterReceiver::create()
{
    return adoptRef(*new IPCTesterReceiver);
}

void IPCTesterReceiver::asyncMessage(uint32_t arg0, CompletionHandler<void(uint32_t)>&& completionHandler)
{
    completionHandler(arg0 + 1u);
}

void IPCTesterReceiver::deferredReplyMessage(uint32_t arg0, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    m_deferredReplyArgument = arg0;
    m_deferredReply = WTF::move(completionHandler);
}

void IPCTesterReceiver::completeDeferredReply(uint32_t arg0)
{
    if (auto completionHandler = std::exchange(m_deferredReply, nullptr))
        completionHandler(static_cast<uint64_t>(m_deferredReplyArgument) + arg0 + 1u);
}

void IPCTesterReceiver::transformInner(const IPCTesterInner& inner, CompletionHandler<void(IPCTesterInner&&)>&& completionHandler)
{
    completionHandler({ static_cast<int32_t>(0u - static_cast<uint32_t>(inner.value)), inner.label });
}

void IPCTesterReceiver::transformInnerSwift(const IPCTesterInner& inner, CompletionHandler<void(IPCTesterInner&&)>&& completionHandler)
{
    completionHandler(ipcTesterTransformed(inner));
}

void IPCTesterReceiver::transformPayloadSwift(const IPCTesterPayload& payload, CompletionHandler<void(IPCTesterPayload&&)>&& completionHandler)
{
    completionHandler(ipcTesterTransformed(payload));
}

void IPCTesterReceiver::transformPayloadSynchronouslySwift(const IPCTesterPayload& payload, CompletionHandler<void(IPCTesterPayload&&)>&& completionHandler)
{
    completionHandler(ipcTesterTransformed(payload));
}

void IPCTesterReceiver::transformNodeSwift(Ref<IPCTesterNode>&& node, RefPtr<IPCTesterNode>&& link, CompletionHandler<void(RefPtr<IPCTesterNode>&&, uint32_t)>&& completionHandler)
{
    uint32_t nodeCount = ipcTesterNodeCount(node) + (link ? ipcTesterNodeCount(*link) : 0);
    completionHandler(link ? RefPtr { ipcTesterMirrored(node) } : nullptr, nodeCount);
}

void IPCTesterReceiver::describeSwift(IPCTesterColor color, OptionSet<IPCTesterFlag> flags, IPCTesterChoice choice, IPCConnectionTesterIdentifier identifier, const IPCTesterVariant& variant, bool flag, CompletionHandler<void(String&&)>&& completionHandler)
{
    completionHandler(ipcTesterDescription(color, flags, choice, identifier, variant, flag));
}

void IPCTesterReceiver::halveEvenNumberSwift(IPC::Connection& connection, uint32_t value, CompletionHandler<void(uint32_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION_BASE(!(value % 2), connection, completionHandler(0));
    completionHandler(value / 2);
}

void IPCTesterReceiver::recordSwift(const String& text)
{
    m_recordedTexts.append(text);
}

void IPCTesterReceiver::takeRecordedSwift(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler(std::exchange(m_recordedTexts, { }));
}

void IPCTesterReceiver::clearRecordedSwift(CompletionHandler<void()>&& completionHandler)
{
    m_recordedTexts.clear();
    completionHandler();
}

}

#endif
