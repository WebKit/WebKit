/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "SharedWorkerKey.h"
#include "SharedWorkerObjectIdentifier.h"
#include "URLKeepingBlobAlive.h"
#include <wtf/MonotonicTime.h>

namespace WebCore {

class MessagePort;
class ResourceError;

struct WorkerOptions;

class SharedWorker final : public AbstractWorker, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(SharedWorker);
public:
    static ExceptionOr<Ref<SharedWorker>> create(Document&, String&& scriptURL, std::optional<std::variant<String, WorkerOptions>>&&);
    ~SharedWorker();

    static SharedWorker* fromIdentifier(SharedWorkerObjectIdentifier);
    SharedWorkerObjectIdentifier identifier() const { return m_identifier; }
    MessagePort& port() const { return m_port.get(); }

    const String& identifierForInspector() const { return m_identifierForInspector; }

    void didFinishLoading(const ResourceError&);

    // EventTarget.
    ScriptExecutionContext* scriptExecutionContext() const final;

private:
    SharedWorker(Document&, const SharedWorkerKey&, Ref<MessagePort>&&);

    // EventTarget.
    EventTargetInterface eventTargetInterface() const final;

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    void stop() final;
    bool virtualHasPendingActivity() const final;
    void suspend(ReasonForSuspension) final;
    void resume() final;


    SharedWorkerKey m_key;
    SharedWorkerObjectIdentifier m_identifier;
    Ref<MessagePort> m_port;
    String m_identifierForInspector;
    URLKeepingBlobAlive m_blobURLExtension;
    bool m_isActive { true };
    bool m_isSuspendedForBackForwardCache { false };
};

} // namespace WebCore
