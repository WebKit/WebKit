/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
 * Copyright (c) 2011 Google Inc. All rights reserved.
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
#include "PageDebugger.h"

#include "CommonVM.h"
#include "Document.h"
#include "InspectorController.h"
#include "InspectorFrontendClient.h"
#include "JSDOMExceptionHandling.h"
#include "JSLocalDOMWindowCustom.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "PageGroup.h"
#include "ScriptController.h"
#include "Timer.h"
#include <JavaScriptCore/JSLock.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThreadInternal.h"
#endif


namespace WebCore {
using namespace JSC;
using namespace Inspector;

PageDebugger::PageDebugger(Page& page)
    : Debugger(WebCore::commonVM())
    , m_page(page)
{
}

void PageDebugger::attachDebugger()
{
    JSC::Debugger::attachDebugger();

    m_page.setDebugger(this);
}

void PageDebugger::detachDebugger(bool isBeingDestroyed)
{
    JSC::Debugger::detachDebugger(isBeingDestroyed);

    m_page.setDebugger(nullptr);
    if (!isBeingDestroyed)
        recompileAllJSFunctions();
}

void PageDebugger::recompileAllJSFunctions()
{
    JSLockHolder lock(vm());
    Debugger::recompileAllJSFunctions();
}

void PageDebugger::didPause(JSGlobalObject* globalObject)
{
    JSC::Debugger::didPause(globalObject);

    setJavaScriptPaused(m_page.group(), true);
}

void PageDebugger::didContinue(JSGlobalObject* globalObject)
{
    JSC::Debugger::didContinue(globalObject);

    setJavaScriptPaused(m_page.group(), false);
}

void PageDebugger::runEventLoopWhilePaused()
{
    JSC::Debugger::runEventLoopWhilePaused();

#if PLATFORM(IOS_FAMILY)
    // On iOS, running an EventLoop causes us to run a nested WebRunLoop.
    // Since the WebThread is autoreleased at the end of run loop iterations
    // we need to gracefully handle releasing and reacquiring the lock.
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

void PageDebugger::runEventLoopWhilePausedInternal()
{
    TimerBase::fireTimersInNestedEventLoop();

    // Protect the page during the execution of the nested run loop.
    Ref protectedPage = m_page;

    while (!m_doneProcessingDebuggerEvents) {
        if (!platformShouldContinueRunningEventLoopWhilePaused())
            break;
    }
}

bool PageDebugger::isContentScript(JSGlobalObject* state) const
{
    return &currentWorld(*state) != &mainThreadNormalWorld() || JSC::Debugger::isContentScript(state);
}

void PageDebugger::reportException(JSGlobalObject* state, JSC::Exception* exception) const
{
    JSC::Debugger::reportException(state, exception);

    WebCore::reportException(state, exception);
}

void PageDebugger::setJavaScriptPaused(const PageGroup& pageGroup, bool paused)
{
    for (auto& page : pageGroup.pages()) {
        for (Frame* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
            auto* localFrame = dynamicDowncast<LocalFrame>(frame);
            if (!localFrame)
                continue;
            setJavaScriptPaused(*localFrame, paused);
        }

        if (auto* frontendClient = page.inspectorController().inspectorFrontendClient()) {
            if (paused)
                frontendClient->pagePaused();
            else
                frontendClient->pageUnpaused();
        }
    }
}

void PageDebugger::setJavaScriptPaused(LocalFrame& frame, bool paused)
{
    if (!frame.script().canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
        return;

    frame.script().setPaused(paused);

    ASSERT(frame.document());
    auto& document = *frame.document();
    if (paused) {
        document.suspendScriptedAnimationControllerCallbacks();
        document.suspendActiveDOMObjects(ReasonForSuspension::JavaScriptDebuggerPaused);
    } else {
        document.resumeActiveDOMObjects(ReasonForSuspension::JavaScriptDebuggerPaused);
        document.resumeScriptedAnimationControllerCallbacks();
    }
}

#if !PLATFORM(MAC)
bool PageDebugger::platformShouldContinueRunningEventLoopWhilePaused()
{
    return RunLoop::cycle() != RunLoop::CycleResult::Stop;
}
#endif // !PLATFORM(MAC)

} // namespace WebCore
