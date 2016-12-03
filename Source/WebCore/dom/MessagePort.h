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

#pragma once

#include "EventTarget.h"
#include "ExceptionOr.h"
#include "MessagePortChannel.h"

namespace JSC {
class ExecState;
class JSObject;
class JSValue;
}

namespace WebCore {

class Frame;

class MessagePort final : public RefCounted<MessagePort>, public EventTargetWithInlineData {
public:
    static Ref<MessagePort> create(ScriptExecutionContext& scriptExecutionContext) { return adoptRef(*new MessagePort(scriptExecutionContext)); }
    virtual ~MessagePort();

    ExceptionOr<void> postMessage(JSC::ExecState&, JSC::JSValue message, Vector<JSC::Strong<JSC::JSObject>>&&);

    void start();
    void close();

    void entangle(std::unique_ptr<MessagePortChannel>&&);

    // Returns nullptr if the passed-in vector is empty.
    static ExceptionOr<std::unique_ptr<MessagePortChannelArray>> disentanglePorts(Vector<RefPtr<MessagePort>>&&);

    static Vector<RefPtr<MessagePort>> entanglePorts(ScriptExecutionContext&, std::unique_ptr<MessagePortChannelArray>&&);

    void messageAvailable();
    bool started() const { return m_started; }

    void contextDestroyed();

    ScriptExecutionContext* scriptExecutionContext() const final { return m_scriptExecutionContext; }

    void dispatchMessages();

    bool hasPendingActivity();

    // Returns null if there is no entangled port, or if the entangled port is run by a different thread.
    // This is used solely to enable a GC optimization. Some platforms may not be able to determine ownership
    // of the remote port (since it may live cross-process) - those platforms may always return null.
    MessagePort* locallyEntangledPort();

    using RefCounted::ref;
    using RefCounted::deref;

private:
    explicit MessagePort(ScriptExecutionContext&);

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    bool isMessagePort() const final { return true; }
    EventTargetInterface eventTargetInterface() const final { return MessagePortEventTargetInterfaceType; }

    bool addEventListener(const AtomicString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) final;

    std::unique_ptr<MessagePortChannel> disentangle();

    // A port starts out its life entangled, and remains entangled until it is closed or is cloned.
    bool isEntangled() const { return !m_closed && !isNeutered(); }

    // A port gets neutered when it is transferred to a new owner via postMessage().
    bool isNeutered() const { return !m_entangledChannel; }

    std::unique_ptr<MessagePortChannel> m_entangledChannel;
    bool m_started { false };
    bool m_closed { false };
    ScriptExecutionContext* m_scriptExecutionContext;
};

} // namespace WebCore
