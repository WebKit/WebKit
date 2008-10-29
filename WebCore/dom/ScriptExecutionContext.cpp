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
#include "ScriptExecutionContext.h"

#include "ActiveDOMObject.h"
#include "MessagePort.h"
#include "Timer.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

class MessagePortTimer : public TimerBase {
public:
    MessagePortTimer(PassRefPtr<ScriptExecutionContext> context)
        : m_context(context)
    {
    }

private:
    virtual void fired()
    {
        m_context->dispatchMessagePortEvents();
        delete this;
    }

    RefPtr<ScriptExecutionContext> m_context;
};

ScriptExecutionContext::ScriptExecutionContext()
    : m_firedMessagePortTimer(false)
{
}

ScriptExecutionContext::~ScriptExecutionContext()
{
    HashMap<ActiveDOMObject*, void*>::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (HashMap<ActiveDOMObject*, void*>::iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT(iter->first->scriptExecutionContext() == this);
        iter->first->contextDestroyed();
    }

    HashSet<MessagePort*>::iterator messagePortsEnd = m_messagePorts.end();
    for (HashSet<MessagePort*>::iterator iter = m_messagePorts.begin(); iter != messagePortsEnd; ++iter) {
        ASSERT((*iter)->scriptExecutionContext() == this);
        (*iter)->contextDestroyed();
    }
}

void ScriptExecutionContext::processMessagePortMessagesSoon()
{
    if (m_firedMessagePortTimer)
        return;

    MessagePortTimer* timer = new MessagePortTimer(this);
    timer->startOneShot(0);

    m_firedMessagePortTimer = true;
}

void ScriptExecutionContext::dispatchMessagePortEvents()
{
    RefPtr<ScriptExecutionContext> protect(this);

    // Make a frozen copy.
    Vector<MessagePort*> ports;
    copyToVector(m_messagePorts, ports);

    m_firedMessagePortTimer = false;

    unsigned portCount = ports.size();
    for (unsigned i = 0; i < portCount; ++i) {
        MessagePort* port = ports[i];
        if (m_messagePorts.contains(port) && port->queueIsOpen())
            port->dispatchMessages();
    }
}

void ScriptExecutionContext::createdMessagePort(MessagePort* port)
{
    ASSERT(port);
    m_messagePorts.add(port);
}

void ScriptExecutionContext::destroyedMessagePort(MessagePort* port)
{
    ASSERT(port);
    m_messagePorts.remove(port);
}

void ScriptExecutionContext::stopActiveDOMObjects()
{
    HashMap<ActiveDOMObject*, void*>::iterator activeObjectsEnd = m_activeDOMObjects.end();
    for (HashMap<ActiveDOMObject*, void*>::iterator iter = m_activeDOMObjects.begin(); iter != activeObjectsEnd; ++iter) {
        ASSERT(iter->first->scriptExecutionContext() == this);
        iter->first->stop();
    }
}

void ScriptExecutionContext::createdActiveDOMObject(ActiveDOMObject* object, void* upcastPointer)
{
    ASSERT(object);
    ASSERT(upcastPointer);
    m_activeDOMObjects.add(object, upcastPointer);
}

void ScriptExecutionContext::destroyedActiveDOMObject(ActiveDOMObject* object)
{
    ASSERT(object);
    m_activeDOMObjects.remove(object);
}


} // namespace WebCore
