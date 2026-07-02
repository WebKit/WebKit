/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FrameDebugger.h"

#include "ActiveDOMObject.h"
#include "CommonVM.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMWindowCustom.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "ScriptController.h"
#include "Timer.h"
#include <JavaScriptCore/JSLock.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThreadInternal.h"
#endif

namespace WebCore {
using namespace JSC;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameDebugger);

FrameDebugger::FrameDebugger(LocalFrame& frame)
    : Debugger(WebCore::commonVM())
    , m_frame(frame)
{
}

void FrameDebugger::attachDebugger()
{
    JSC::Debugger::attachDebugger();

    RefPtr frame = m_frame.get();
    if (!frame)
        return;

    Ref world = mainThreadNormalWorldSingleton();
    CheckedRef script = frame->script();
    auto* globalObject = script->globalObject(world);
    // globalObject() may lazily create the JSWindowProxy, which fires didClearWindowObjectInWorld
    // and attaches us via FrameDebuggerAgent::didClearWindowObjectInWorld. Guard against double-attach.
    if (globalObject && !globalObject->debugger())
        attach(globalObject);
}

void FrameDebugger::detachDebugger(bool isBeingDestroyed)
{
    JSC::Debugger::detachDebugger(isBeingDestroyed);

    if (!isBeingDestroyed)
        recompileAllJSFunctions();
}

void FrameDebugger::recompileAllJSFunctions()
{
    // NOTE: Debugger::recompileAllJSFunctions() is VM-wide — it recompiles all JS functions in the VM,
    // not just the frame's. Under site isolation each cross-origin iframe is alone in its process, so
    // this is effectively correct. If process reuse is ever introduced for multiple cross-origin iframes,
    // this would over-recompile.
    JSLockHolder lock(vm());
    Debugger::recompileAllJSFunctions();
}

void FrameDebugger::didPause(JSGlobalObject* globalObject)
{
    JSC::Debugger::didPause(globalObject);

    RefPtr frame = m_frame.get();
    if (frame)
        setJavaScriptPaused(*frame, true);
}

void FrameDebugger::didContinue(JSGlobalObject* globalObject)
{
    JSC::Debugger::didContinue(globalObject);

    RefPtr frame = m_frame.get();
    if (frame)
        setJavaScriptPaused(*frame, false);
}

void FrameDebugger::runEventLoopWhilePaused()
{
    JSC::Debugger::runEventLoopWhilePaused();

#if PLATFORM(IOS_FAMILY)
    if (WebThreadIsEnabled()) {
        ASSERT(WebThreadIsLockedOrDisabled());
        JSC::JSLock::DropAllLocks dropAllLocks(vm());
        WebRunLoopEnableNested();

        runEventLoopWhilePausedInternal();

        WebRunLoopDisableNested();
        ASSERT(WebThreadIsLockedOrDisabled());
        return;
    }
#endif

    runEventLoopWhilePausedInternal();
}

void FrameDebugger::runEventLoopWhilePausedInternal()
{
    TimerBase::fireTimersInNestedEventLoop();

    RefPtr protectedFrame = m_frame.get();
    if (!protectedFrame)
        return;

    while (!m_doneProcessingDebuggerEvents) {
        if (!platformShouldContinueRunningEventLoopWhilePaused())
            break;
    }
}

bool FrameDebugger::isContentScript(JSGlobalObject* state) const
{
    return &currentWorld(*state) != &mainThreadNormalWorldSingleton() || JSC::Debugger::isContentScript(state);
}

void FrameDebugger::reportException(JSGlobalObject* state, JSC::Exception* exception) const
{
    JSC::Debugger::reportException(state, exception);

    WebCore::reportException(state, exception);
}

// Unlike PageDebugger which pauses all frames in the page group, FrameDebugger only pauses
// its single frame. Under site isolation, cross-origin iframes run in their own process where
// this frame is the only web content — other frames are in different processes and cannot be
// paused from here.
void FrameDebugger::setJavaScriptPaused(LocalFrame& frame, bool paused)
{
    Ref protectedFrame = frame;
    CheckedRef script = protectedFrame->script();
    if (!script->canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
        return;

    script->setPaused(paused);

    ASSERT(protectedFrame->document());
    Ref document = *protectedFrame->document();
    if (paused) {
        document->suspendScriptedAnimationControllerCallbacks();
        document->suspendActiveDOMObjects(ReasonForSuspension::JavaScriptDebuggerPaused);
    } else {
        document->resumeActiveDOMObjects(ReasonForSuspension::JavaScriptDebuggerPaused);
        document->resumeScriptedAnimationControllerCallbacks();
    }
}

#if !PLATFORM(MAC)
bool FrameDebugger::platformShouldContinueRunningEventLoopWhilePaused()
{
    return RunLoop::cycle() != RunLoop::CycleResult::Stop;
}
#endif // !PLATFORM(MAC)

} // namespace WebCore
