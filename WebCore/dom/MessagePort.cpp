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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "MessagePort.h"

#include "AtomicString.h"
#include "DOMWindow.h"
#include "Document.h"
#include "EventException.h"
#include "EventNames.h"
#include "MessageEvent.h"
#include "SecurityOrigin.h"
#include "Timer.h"

namespace WebCore {

MessagePort::MessagePort(ScriptExecutionContext& scriptExecutionContext)
    : m_entangledChannel(0)
    , m_started(false)
    , m_scriptExecutionContext(&scriptExecutionContext)
{
    m_scriptExecutionContext->createdMessagePort(this);

    // Don't need to call processMessagePortMessagesSoon() here, because the port will not be opened until start() is invoked.
}

MessagePort::~MessagePort()
{
    close();
    if (m_scriptExecutionContext)
        m_scriptExecutionContext->destroyedMessagePort(this);
}

// FIXME: remove this when we update the ObjC bindings (bug #28774).
void MessagePort::postMessage(const String& message, MessagePort* port, ExceptionCode& ec)
{
    MessagePortArray ports;
    if (port)
        ports.append(port);
    postMessage(message, &ports, ec);
}

void MessagePort::postMessage(const String& message, ExceptionCode& ec)
{
    postMessage(message, static_cast<MessagePortArray*>(0), ec);
}

void MessagePort::postMessage(const String& message, const MessagePortArray* ports, ExceptionCode& ec)
{
    if (!m_entangledChannel)
        return;
    ASSERT(m_scriptExecutionContext);

    OwnPtr<MessagePortChannelArray> channels;
    // Make sure we aren't connected to any of the passed-in ports.
    if (ports) {
        for (unsigned int i = 0; i < ports->size(); ++i) {
            MessagePort* dataPort = (*ports)[i].get();
            if (dataPort == this || m_entangledChannel->isConnectedTo(dataPort)) {
                ec = INVALID_STATE_ERR;
                return;
            }
        }
        channels = MessagePort::disentanglePorts(ports, ec);
        if (ec)
            return;
    }
    m_entangledChannel->postMessageToRemote(MessagePortChannel::EventData::create(message, channels.release()));
}

PassOwnPtr<MessagePortChannel> MessagePort::disentangle(ExceptionCode& ec)
{
    if (!m_entangledChannel)
        ec = INVALID_STATE_ERR;
    else {
        m_entangledChannel->disentangle();

        // We can't receive any messages or generate any events, so remove ourselves from the list of active ports.
        ASSERT(m_scriptExecutionContext);
        m_scriptExecutionContext->destroyedMessagePort(this);
        m_scriptExecutionContext = 0;
    }
    return m_entangledChannel.release();
}

// Invoked to notify us that there are messages available for this port.
// This code may be called from another thread, and so should not call any non-threadsafe APIs (i.e. should not call into the entangled channel or access mutable variables).
void MessagePort::messageAvailable()
{
    ASSERT(m_scriptExecutionContext);
    m_scriptExecutionContext->processMessagePortMessagesSoon();
}

void MessagePort::start()
{
    // Do nothing if we've been cloned
    if (!m_entangledChannel)
        return;

    ASSERT(m_scriptExecutionContext);
    if (m_started)
        return;

    m_started = true;
    m_scriptExecutionContext->processMessagePortMessagesSoon();
}

void MessagePort::close()
{
    if (!m_entangledChannel)
        return;
    m_entangledChannel->close();
}

void MessagePort::entangle(PassOwnPtr<MessagePortChannel> remote)
{
    // Only invoked to set our initial entanglement.
    ASSERT(!m_entangledChannel);
    ASSERT(m_scriptExecutionContext);

    // Don't entangle the ports if the channel is closed.
    if (remote->entangleIfOpen(this))
        m_entangledChannel = remote;
}

void MessagePort::contextDestroyed()
{
    ASSERT(m_scriptExecutionContext);
    // Must close port before blowing away the cached context, to ensure that we get no more calls to messageAvailable().
    close();
    m_scriptExecutionContext = 0;
}

ScriptExecutionContext* MessagePort::scriptExecutionContext() const
{
    return m_scriptExecutionContext;
}

void MessagePort::dispatchMessages()
{
    // Messages for contexts that are not fully active get dispatched too, but JSAbstractEventListener::handleEvent() doesn't call handlers for these.
    // The HTML5 spec specifies that any messages sent to a document that is not fully active should be dropped, so this behavior is OK.
    ASSERT(started());

    OwnPtr<MessagePortChannel::EventData> eventData;
    while (m_entangledChannel && m_entangledChannel->tryGetMessageFromRemote(eventData)) {
        OwnPtr<MessagePortArray> ports = MessagePort::entanglePorts(*m_scriptExecutionContext, eventData->channels());
        RefPtr<Event> evt = MessageEvent::create(ports.release(), eventData->message());

        ExceptionCode ec = 0;
        dispatchEvent(evt.release(), ec);
        ASSERT(!ec);
    }
}

bool MessagePort::hasPendingActivity()
{
    // The spec says that entangled message ports should always be treated as if they have a strong reference.
    // We'll also stipulate that the queue needs to be open (if the app drops its reference to the port before start()-ing it, then it's not really entangled as it's unreachable).
    return m_started && m_entangledChannel && m_entangledChannel->hasPendingActivity();
}

MessagePort* MessagePort::locallyEntangledPort()
{
    return m_entangledChannel ? m_entangledChannel->locallyEntangledPort(m_scriptExecutionContext) : 0;
}

PassOwnPtr<MessagePortChannelArray> MessagePort::disentanglePorts(const MessagePortArray* ports, ExceptionCode& ec)
{
    if (!ports || !ports->size())
        return 0;

    // HashSet used to efficiently check for duplicates in the passed-in array.
    HashSet<MessagePort*> portSet;

    // Walk the incoming array - if there are any duplicate ports, or null ports or cloned ports, throw an error (per section 8.3.3 of the HTML5 spec).
    for (unsigned int i = 0; i < ports->size(); ++i) {
        MessagePort* port = (*ports)[i].get();
        if (!port || !port->isEntangled() || portSet.contains(port)) {
            ec = INVALID_STATE_ERR;
            return 0;
        }
        portSet.add(port);
    }

    // Passed-in ports passed validity checks, so we can disentangle them.
    MessagePortChannelArray* portArray = new MessagePortChannelArray(ports->size());
    for (unsigned int i = 0 ; i < ports->size() ; ++i) {
        OwnPtr<MessagePortChannel> channel = (*ports)[i]->disentangle(ec);
        ASSERT(!ec); // Can't generate exception here if passed above checks.
        (*portArray)[i] = channel.release();
    }
    return portArray;
}

PassOwnPtr<MessagePortArray> MessagePort::entanglePorts(ScriptExecutionContext& context, PassOwnPtr<MessagePortChannelArray> channels)
{
    if (!channels || !channels->size())
        return 0;

    MessagePortArray* portArray = new MessagePortArray(channels->size());
    for (unsigned int i = 0; i < channels->size(); ++i) {
        RefPtr<MessagePort> port = MessagePort::create(context);
        port->entangle((*channels)[i].release());
        (*portArray)[i] = port.release();
    }
    return portArray;
}

EventTargetData* MessagePort::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* MessagePort::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore
