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

#include "config.h"
#include "JSGlobalObjectDebugger.h"

#include "JSGlobalObject.h"
#include "JSLock.h"
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

namespace Inspector {

using namespace JSC;

WTF_MAKE_TZONE_ALLOCATED_IMPL(JSGlobalObjectDebugger);

JSGlobalObjectDebugger::JSGlobalObjectDebugger(JSGlobalObject& globalObject)
    : Debugger(globalObject.vm())
    , m_globalObject(globalObject)
{
}

void JSGlobalObjectDebugger::attachDebugger()
{
    JSC::Debugger::attachDebugger();

    attach(&m_globalObject);
}

void JSGlobalObjectDebugger::detachDebugger(bool isBeingDestroyed)
{
    JSC::Debugger::detachDebugger(isBeingDestroyed);

    detach(&m_globalObject, isBeingDestroyed ? Debugger::GlobalObjectIsDestructing : Debugger::TerminatingDebuggingSession);
    if (!isBeingDestroyed)
        recompileAllJSFunctions();
}

void JSGlobalObjectDebugger::runEventLoopWhilePaused()
{
    JSC::Debugger::runEventLoopWhilePaused();

    // Drop all locks so another thread can work in the VM while we are nested.
    JSC::JSLock::DropAllLocks dropAllLocks(&m_globalObject.vm());

    while (!m_doneProcessingDebuggerEvents) {
        if (RunLoop::cycle(JSGlobalObjectDebugger::runLoopMode()) == RunLoop::CycleResult::Stop)
            break;
    }
}

RunLoopMode JSGlobalObjectDebugger::runLoopMode()
{
#if USE(CF) && !PLATFORM(WATCHOS)
    // Run the RunLoop in a custom run loop mode to prevent default observers
    // to run and potentially evaluate JavaScript in this context while we are
    // nested. Only the debugger should control things until we continue.
    // FIXME: This is not a perfect solution, as background threads are not
    // paused and can still access and evalute script in the JSContext.

    // FIXME: <rdar://problem/25972777>. On watchOS, in order for auto-attach to work,
    // we need to run in the default run loop mode otherwise we do not receive the XPC messages
    // necessary to setup the relay connection and negotiate an auto-attach debugger.
    return CFSTR("com.apple.JavaScriptCore.remote-inspector-runloop-mode");
#else
    return DefaultRunLoopMode;
#endif
}

} // namespace Inspector
