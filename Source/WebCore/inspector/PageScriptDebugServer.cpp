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
#include "PageScriptDebugServer.h"

#include "CommonVM.h"
#include "Document.h"
#include "Frame.h"
#include "FrameView.h"
#include "InspectorController.h"
#include "InspectorFrontendClient.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMWindowCustom.h"
#include "Page.h"
#include "PageGroup.h"
#include "PluginViewBase.h"
#include "ScriptController.h"
#include "Timer.h"
#include <JavaScriptCore/JSLock.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThreadInternal.h"
#endif


namespace WebCore {
using namespace JSC;
using namespace Inspector;

PageScriptDebugServer::PageScriptDebugServer(Page& page)
    : ScriptDebugServer(WebCore::commonVM())
    , m_page(page)
{
}

void PageScriptDebugServer::attachDebugger()
{
    m_page.setDebugger(this);
}

void PageScriptDebugServer::detachDebugger(bool isBeingDestroyed)
{
    m_page.setDebugger(nullptr);
    if (!isBeingDestroyed)
        recompileAllJSFunctions();
}

void PageScriptDebugServer::recompileAllJSFunctions()
{
    JSLockHolder lock(vm());
    Debugger::recompileAllJSFunctions();
}

void PageScriptDebugServer::didPause(JSGlobalObject*)
{
    setJavaScriptPaused(m_page.group(), true);
}

void PageScriptDebugServer::didContinue(JSGlobalObject*)
{
    setJavaScriptPaused(m_page.group(), false);
}

void PageScriptDebugServer::runEventLoopWhilePaused()
{
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

void PageScriptDebugServer::runEventLoopWhilePausedInternal()
{
    TimerBase::fireTimersInNestedEventLoop();

    m_page.incrementNestedRunLoopCount();

    while (!m_doneProcessingDebuggerEvents) {
        if (!platformShouldContinueRunningEventLoopWhilePaused())
            break;
    }

    m_page.decrementNestedRunLoopCount();
}

bool PageScriptDebugServer::isContentScript(JSGlobalObject* state) const
{
    return &currentWorld(*state) != &mainThreadNormalWorld();
}

void PageScriptDebugServer::reportException(JSGlobalObject* state, JSC::Exception* exception) const
{
    WebCore::reportException(state, exception);
}

void PageScriptDebugServer::setJavaScriptPaused(const PageGroup& pageGroup, bool paused)
{
    for (auto& page : pageGroup.pages()) {
        for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext())
            setJavaScriptPaused(*frame, paused);

        if (auto* frontendClient = page->inspectorController().inspectorFrontendClient()) {
            if (paused)
                frontendClient->pagePaused();
            else
                frontendClient->pageUnpaused();
        }
    }
}

void PageScriptDebugServer::setJavaScriptPaused(Frame& frame, bool paused)
{
    if (!frame.script().canExecuteScripts(NotAboutToExecuteScript))
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

    if (auto* view = frame.view()) {
        for (auto& child : view->children()) {
            if (!is<PluginViewBase>(child))
                continue;
            downcast<PluginViewBase>(child.get()).setJavaScriptPaused(paused);
        }
    }
}

#if !PLATFORM(MAC)
bool PageScriptDebugServer::platformShouldContinueRunningEventLoopWhilePaused()
{
    return RunLoop::cycle() != RunLoop::CycleResult::Stop;
}
#endif // !PLATFORM(MAC)

} // namespace WebCore
