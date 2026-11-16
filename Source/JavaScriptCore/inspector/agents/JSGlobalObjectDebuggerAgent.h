/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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

#include "InspectorDebuggerAgent.h"
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>

namespace Inspector {

class InspectorConsoleAgent;

// Note: This uses CanMakeThreadSafeCheckedPtr because a JSC::JSGlobalObject (and thus its
// JSGlobalObjectInspectorController, which keeps CheckedPtrs to its agents) may get destroyed
// on a different thread than the one it got created on, when the VM is torn down.
class JSGlobalObjectDebuggerAgent final : public InspectorDebuggerAgent, public CanMakeThreadSafeCheckedPtr<JSGlobalObjectDebuggerAgent> {
    WTF_MAKE_NONCOPYABLE(JSGlobalObjectDebuggerAgent);
    WTF_MAKE_TZONE_ALLOCATED(JSGlobalObjectDebuggerAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(JSGlobalObjectDebuggerAgent);
public:
    OVERRIDE_ABSTRACT_CAN_MAKE_CHECKEDPTR(CanMakeThreadSafeCheckedPtr);

    JSGlobalObjectDebuggerAgent(JSAgentContext&, InspectorConsoleAgent*);
    ~JSGlobalObjectDebuggerAgent() final;

    // JSC::Debugger::Observer
    void breakpointActionLog(JSC::JSGlobalObject*, const String& data) final;

private:
    InjectedScript injectedScriptForEval(Protocol::ErrorString&, std::optional<Protocol::Runtime::ExecutionContextId>&&) final;

    // NOTE: JavaScript inspector does not yet need to mute a console because no messages
    // are sent to the console outside of the API boundary or console object.
    void muteConsole() final { }
    void unmuteConsole() final { }

    const CheckedPtr<InspectorConsoleAgent> m_consoleAgent;
};

} // namespace Inspector
