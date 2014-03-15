/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "ScriptProfiler.h"

#include "GCController.h"
#include "JSDOMBinding.h"
#include "JSDOMWindow.h"
#include "MainFrame.h"
#include "Page.h"
#include "ScriptState.h"
#include <bindings/ScriptObject.h>
#include <bindings/ScriptValue.h>
#include <profiler/LegacyProfiler.h>
#include <wtf/Forward.h>

namespace WebCore {

void ScriptProfiler::collectGarbage()
{
    gcController().garbageCollectSoon();
}

Deprecated::ScriptObject ScriptProfiler::objectByHeapObjectId(unsigned)
{
    return Deprecated::ScriptObject();
}

unsigned ScriptProfiler::getHeapObjectId(const Deprecated::ScriptValue&)
{
    return 0;
}

void ScriptProfiler::start(JSC::ExecState* state, const String& title)
{
    JSC::LegacyProfiler::profiler()->startProfiling(state, title);
}

void ScriptProfiler::startForPage(Page* inspectedPage, const String& title)
{
    JSC::ExecState* scriptState = toJSDOMWindow(&inspectedPage->mainFrame(), debuggerWorld())->globalExec();
    start(scriptState, title);
}

void ScriptProfiler::startForWorkerGlobalScope(WorkerGlobalScope* context, const String& title)
{
    start(execStateFromWorkerGlobalScope(context), title);
}

PassRefPtr<ScriptProfile> ScriptProfiler::stop(JSC::ExecState* state, const String& title)
{
    RefPtr<JSC::Profile> profile = JSC::LegacyProfiler::profiler()->stopProfiling(state, title);
    return ScriptProfile::create(profile);
}

PassRefPtr<ScriptProfile> ScriptProfiler::stopForPage(Page* inspectedPage, const String& title)
{
    JSC::ExecState* scriptState = toJSDOMWindow(&inspectedPage->mainFrame(), debuggerWorld())->globalExec();
    return stop(scriptState, title);
}

PassRefPtr<ScriptProfile> ScriptProfiler::stopForWorkerGlobalScope(WorkerGlobalScope* context, const String& title)
{
    return stop(execStateFromWorkerGlobalScope(context), title);
}

} // namespace WebCore
