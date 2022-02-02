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

#pragma once

#include "AbstractWorker.h"
#include "ActiveDOMObject.h"
#include <JavaScriptCore/RuntimeFlags.h>
#include <wtf/MonotonicTime.h>

namespace WebCore {

class MessagePort;
class SharedWorkerThreadProxy;

struct WorkerOptions;

class SharedWorker final : public AbstractWorker, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(SharedWorker);
public:
    static ExceptionOr<Ref<SharedWorker>> create(Document&, JSC::RuntimeFlags, String&& scriptURL, std::optional<std::variant<String, WorkerOptions>>&&);
    ~SharedWorker();

    MessagePort& port() const { return m_port.get(); }
    JSC::RuntimeFlags runtimeFlags() { return m_runtimeFlags; }

    const String& identifierForInspector() const { return m_identifierForInspector; }
    MonotonicTime creationTime() const { return m_creationTime; }

    SharedWorkerThreadProxy& proxy() { return m_proxy; }

    void setIsLoading(bool isLoading) { m_isLoading = isLoading; }

    // EventTarget.
    ScriptExecutionContext* scriptExecutionContext() const final;

private:
    SharedWorker(Document&, Ref<MessagePort>&&, JSC::RuntimeFlags);

    void terminate();

    // EventTarget.
    EventTargetInterface eventTargetInterface() const final;

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    void stop() final;
    bool virtualHasPendingActivity() const final;


    Ref<MessagePort> m_port;
    String m_identifierForInspector;
    JSC::RuntimeFlags m_runtimeFlags;
    MonotonicTime m_creationTime;
    SharedWorkerThreadProxy& m_proxy; // The proxy outlives the worker to perform thread shutdown.
    bool m_isLoading { false };
};

} // namespace WebCore
