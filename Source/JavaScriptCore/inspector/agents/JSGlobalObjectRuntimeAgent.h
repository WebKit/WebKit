/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef JSGlobalObjectRuntimeAgent_h
#define JSGlobalObjectRuntimeAgent_h

#if ENABLE(INSPECTOR)

#include "InspectorRuntimeAgent.h"
#include <wtf/PassOwnPtr.h>

namespace JSC {
class JSGlobalObject;
}

namespace Inspector {

class JSGlobalObjectRuntimeAgent final : public InspectorRuntimeAgent {
public:
    JSGlobalObjectRuntimeAgent(InjectedScriptManager*, JSC::JSGlobalObject&);

    virtual void didCreateFrontendAndBackend(InspectorFrontendChannel*, InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(InspectorDisconnectReason) override;

    virtual JSC::VM& globalVM() override;
    virtual InjectedScript injectedScriptForEval(ErrorString*, const int* executionContextId) override;

    // NOTE: JavaScript inspector does not yet need to mute a console because no messages
    // are sent to the console outside of the API boundary or console object.
    virtual void muteConsole() override { }
    virtual void unmuteConsole() override { }

private:
    std::unique_ptr<InspectorRuntimeFrontendDispatcher> m_frontendDispatcher;
    RefPtr<InspectorRuntimeBackendDispatcher> m_backendDispatcher;
    JSC::JSGlobalObject& m_globalObject;
};

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // !defined(JSGlobalObjectRuntimeAgent_h)
