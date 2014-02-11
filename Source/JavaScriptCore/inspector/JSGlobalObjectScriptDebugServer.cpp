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

#include "config.h"
#include "JSGlobalObjectScriptDebugServer.h"

#if ENABLE(INSPECTOR)

#include "EventLoop.h"
#include "JSCInlines.h"
#include "JSLock.h"

using namespace JSC;

namespace Inspector {

JSGlobalObjectScriptDebugServer::JSGlobalObjectScriptDebugServer(JSGlobalObject& globalObject)
    : ScriptDebugServer(false)
    , m_globalObject(globalObject)
{
}

void JSGlobalObjectScriptDebugServer::addListener(ScriptDebugListener* listener)
{
    if (!listener)
        return;

    bool wasEmpty = m_listeners.isEmpty();
    m_listeners.add(listener);

    // First listener. Attach the debugger to the JSGlobalObject.
    if (wasEmpty) {
        attach(&m_globalObject);
        recompileAllJSFunctions();
    }
}

void JSGlobalObjectScriptDebugServer::removeListener(ScriptDebugListener* listener, bool isBeingDestroyed)
{
    if (!listener)
        return;

    m_listeners.remove(listener);

    // Last listener. Detach the debugger from the JSGlobalObject.
    if (m_listeners.isEmpty()) {
        detach(&m_globalObject, isBeingDestroyed ? Debugger::GlobalObjectIsDestructing : Debugger::TerminatingDebuggingSession);
        if (!isBeingDestroyed)
            recompileAllJSFunctions();
    }
}

void JSGlobalObjectScriptDebugServer::recompileAllJSFunctions()
{
    JSC::Debugger::recompileAllJSFunctions(&m_globalObject.vm());
}

void JSGlobalObjectScriptDebugServer::runEventLoopWhilePaused()
{
    // Drop all locks so another thread can work in the VM while we are nested.
    JSC::JSLock::DropAllLocks dropAllLocks(&m_globalObject.vm());

    EventLoop loop;
    while (!m_doneProcessingDebuggerEvents && !loop.ended())
        loop.cycle();
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
