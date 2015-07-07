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

#if ENABLE(INSPECTOR)

#include "Document.h"
#include "EventLoop.h"
#include "FrameView.h"
#include "JSDOMWindowCustom.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageGroup.h"
#include "PluginView.h"
#include "ScriptController.h"
#include "Timer.h"
#include "Widget.h"
#include <runtime/JSLock.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(IOS)
#include "JSDOMWindowBase.h"
#include "WebCoreThreadInternal.h"
#endif

using namespace JSC;
using namespace Inspector;

namespace WebCore {

PageScriptDebugServer::PageScriptDebugServer(Page& page)
    : ScriptDebugServer(false)
    , m_page(page)
{
}

void PageScriptDebugServer::addListener(ScriptDebugListener* listener)
{
    if (!listener)
        return;

    bool wasEmpty = m_listeners.isEmpty();
    m_listeners.add(listener);

    if (wasEmpty) {
        m_page.setDebugger(this);
        recompileAllJSFunctions();
    }
}

void PageScriptDebugServer::removeListener(ScriptDebugListener* listener, bool isBeingDestroyed)
{
    if (!listener)
        return;

    m_listeners.remove(listener);

    if (m_listeners.isEmpty()) {
        m_page.setDebugger(nullptr);
        if (!isBeingDestroyed)
            recompileAllJSFunctions();
    }
}

void PageScriptDebugServer::recompileAllJSFunctions()
{
    JSLockHolder lock(JSDOMWindow::commonVM());
    Debugger::recompileAllJSFunctions(&JSDOMWindow::commonVM());
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
#if PLATFORM(IOS)
    // On iOS, running an EventLoop causes us to run a nested WebRunLoop.
    // Since the WebThread is autoreleased at the end of run loop iterations
    // we need to gracefully handle releasing and reacquiring the lock.
    if (WebThreadIsEnabled()) {
        ASSERT(WebThreadIsLockedOrDisabled());
        JSC::JSLock::DropAllLocks dropAllLocks(WebCore::JSDOMWindowBase::commonVM());
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

    EventLoop loop;
    while (!m_doneProcessingDebuggerEvents && !loop.ended())
        loop.cycle();
}

bool PageScriptDebugServer::isContentScript(ExecState* exec) const
{
    return &currentWorld(exec) != &mainThreadNormalWorld();
}

void PageScriptDebugServer::reportException(ExecState* exec, JSValue exception) const
{
    WebCore::reportException(exec, exception);
}

void PageScriptDebugServer::setJavaScriptPaused(const PageGroup& pageGroup, bool paused)
{
    setMainThreadCallbacksPaused(paused);

    const HashSet<Page*>& pages = pageGroup.pages();

    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it)
        setJavaScriptPaused(*it, paused);
}

void PageScriptDebugServer::setJavaScriptPaused(Page* page, bool paused)
{
    ASSERT_ARG(page, page);

    page->setDefersLoading(paused);

    for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext())
        setJavaScriptPaused(frame, paused);
}

void PageScriptDebugServer::setJavaScriptPaused(Frame* frame, bool paused)
{
    ASSERT_ARG(frame, frame);

    if (!frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return;

    frame->script().setPaused(paused);

    Document* document = frame->document();
    if (paused) {
        document->suspendScriptedAnimationControllerCallbacks();
        document->suspendActiveDOMObjects(ActiveDOMObject::JavaScriptDebuggerPaused);
    } else {
        document->resumeActiveDOMObjects(ActiveDOMObject::JavaScriptDebuggerPaused);
        document->resumeScriptedAnimationControllerCallbacks();
    }

    setJavaScriptPaused(frame->view(), paused);
}

void PageScriptDebugServer::setJavaScriptPaused(FrameView* view, bool paused)
{
    if (!view)
        return;

    for (auto it = view->children().begin(), end = view->children().end(); it != end; ++it) {
        Widget* widget = (*it).get();
        if (!widget->isPluginView())
            continue;
        toPluginView(widget)->setJavaScriptPaused(paused);
    }
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
