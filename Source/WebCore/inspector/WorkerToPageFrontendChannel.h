/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "WorkerDebuggerProxy.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerThread.h"
#include <JavaScriptCore/InspectorFrontendChannel.h>

namespace WebCore {

class WorkerToPageFrontendChannel final : public Inspector::FrontendChannel {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WorkerToPageFrontendChannel(WorkerOrWorkletGlobalScope& globalScope)
        : m_globalScope(globalScope)
    {
    }
    ~WorkerToPageFrontendChannel() override = default;

private:
    ConnectionType connectionType() const override { return ConnectionType::Local; }

    void sendMessageToFrontend(const String& message) override
    {
        if (auto* workerDebuggerProxy = m_globalScope.workerOrWorkletThread()->workerDebuggerProxy())
            workerDebuggerProxy->postMessageToDebugger(message);
    }

    WorkerOrWorkletGlobalScope& m_globalScope;
};

} // namespace WebCore
