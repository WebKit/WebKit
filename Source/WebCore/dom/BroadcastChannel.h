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

#pragma once

#include "ActiveDOMObject.h"
#include "BroadcastChannelIdentifier.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "SecurityOriginData.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace JSC {
class JSGlobalObject;
class JSValue;
}

namespace WebCore {

class SerializedScriptValue;

class BroadcastChannel : public RefCounted<BroadcastChannel>, public EventTargetWithInlineData, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(BroadcastChannel);
public:
    static Ref<BroadcastChannel> create(ScriptExecutionContext& context, const String& name)
    {
        auto channel = adoptRef(*new BroadcastChannel(context, name));
        channel->suspendIfNeeded();
        return channel;
    }
    ~BroadcastChannel();

    using RefCounted<BroadcastChannel>::ref;
    using RefCounted<BroadcastChannel>::deref;

    BroadcastChannelIdentifier identifier() const { return m_identifier; }
    const String& name() const { return m_name; }

    ExceptionOr<void> postMessage(JSC::JSGlobalObject&, JSC::JSValue message);
    void close();

    WEBCORE_EXPORT static void dispatchMessageTo(BroadcastChannelIdentifier, Ref<SerializedScriptValue>&&, CompletionHandler<void()>&&);

private:
    BroadcastChannel(ScriptExecutionContext&, const String& name);

    void dispatchMessage(Ref<SerializedScriptValue>&&);
    void ensureOnMainThread(Function<void(Document&)>&&);

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return BroadcastChannelEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { RefCounted<BroadcastChannel>::ref(); }
    void derefEventTarget() final { RefCounted<BroadcastChannel>::deref(); }
    void eventListenersDidChange() final;

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;
    void stop() final { close(); }

    const String m_name;
    const SecurityOriginData m_origin;
    const BroadcastChannelIdentifier m_identifier;
    bool m_isClosed { false };
    bool m_hasRelevantEventListener { false };
};

} // namespace WebCore
