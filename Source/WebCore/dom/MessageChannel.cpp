/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "MessageChannel.h"

#include "MessagePort.h"
#include "MessagePortChannelProvider.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

Ref<MessageChannel> MessageChannel::create(ScriptExecutionContext& context)
{
    return adoptRef(*new MessageChannel(context));
}

MessageChannel::MessageChannel(ScriptExecutionContext& context)
{
    MessagePortIdentifier id1 = { Process::identifier(), ObjectIdentifier<MessagePortIdentifier::PortIdentifierType>::generate() };
    MessagePortIdentifier id2 = { Process::identifier(), ObjectIdentifier<MessagePortIdentifier::PortIdentifierType>::generate() };

    m_port1 = MessagePort::create(context, id1, id2);
    m_port2 = MessagePort::create(context, id2, id1);

    if (!context.activeDOMObjectsAreStopped()) {
        ASSERT(!m_port1->closed());
        ASSERT(!m_port2->closed());
        MessagePortChannelProvider::singleton().createNewMessagePortChannel(id1, id2);
    } else {
        ASSERT(m_port1->closed());
        ASSERT(m_port2->closed());
    }
}

MessageChannel::~MessageChannel() = default;

} // namespace WebCore
