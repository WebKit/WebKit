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

#ifndef JSGlobalObjectScriptDebugServer_h
#define JSGlobalObjectScriptDebugServer_h

#if ENABLE(INSPECTOR)

#include "ScriptDebugServer.h"
#include <wtf/Forward.h>

namespace Inspector {

class JSGlobalObjectScriptDebugServer final : public ScriptDebugServer {
    WTF_MAKE_NONCOPYABLE(JSGlobalObjectScriptDebugServer);
public:
    JSGlobalObjectScriptDebugServer(JSC::JSGlobalObject&);
    virtual ~JSGlobalObjectScriptDebugServer() { }

    void addListener(ScriptDebugListener*);
    void removeListener(ScriptDebugListener*, bool isBeingDestroyed);

    JSC::JSGlobalObject& globalObject() const { return m_globalObject; }

    virtual void recompileAllJSFunctions() override;

private:
    virtual ListenerSet* getListenersForGlobalObject(JSC::JSGlobalObject*) override { return &m_listeners; }
    virtual void didPause(JSC::JSGlobalObject*) override { }
    virtual void didContinue(JSC::JSGlobalObject*) override { }
    virtual void runEventLoopWhilePaused() override;
    virtual bool isContentScript(JSC::ExecState*) const override { return false; }

    // NOTE: Currently all exceptions are reported at the API boundary through reportAPIException.
    // Until a time comes where an exception can be caused outside of the API (e.g. setTimeout
    // or some other async operation in a pure JSContext) we can ignore exceptions reported here.
    virtual void reportException(JSC::ExecState*, JSC::JSValue) const override { }

    ListenerSet m_listeners;
    JSC::JSGlobalObject& m_globalObject;
};

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // JSGlobalObjectScriptDebugServer_h
