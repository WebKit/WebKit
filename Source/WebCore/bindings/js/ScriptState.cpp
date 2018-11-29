/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptState.h"

#include "Document.h"
#include "Frame.h"
#include "JSDOMWindowBase.h"
#include "JSWorkerGlobalScope.h"
#include "JSWorkletGlobalScope.h"
#include "Node.h"
#include "Page.h"
#include "ScriptController.h"
#include "WorkerGlobalScope.h"
#include "WorkerScriptController.h"
#include "WorkletGlobalScope.h"
#include "WorkletScriptController.h"
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/StrongInlines.h>

namespace WebCore {

DOMWindow* domWindowFromExecState(JSC::ExecState* scriptState)
{
    JSC::JSGlobalObject* globalObject = scriptState->lexicalGlobalObject();
    JSC::VM& vm = globalObject->vm();
    if (!globalObject->inherits<JSDOMWindowBase>(vm))
        return nullptr;
    return &JSC::jsCast<JSDOMWindowBase*>(globalObject)->wrapped();
}

Frame* frameFromExecState(JSC::ExecState* scriptState)
{
    ScriptExecutionContext* context = scriptExecutionContextFromExecState(scriptState);
    Document* document = is<Document>(context) ? downcast<Document>(context) : nullptr;
    return document ? document->frame() : nullptr;
}

ScriptExecutionContext* scriptExecutionContextFromExecState(JSC::ExecState* scriptState)
{
    JSC::JSGlobalObject* globalObject = scriptState->lexicalGlobalObject();
    JSC::VM& vm = globalObject->vm();
    if (!globalObject->inherits<JSDOMGlobalObject>(vm))
        return nullptr;
    return JSC::jsCast<JSDOMGlobalObject*>(globalObject)->scriptExecutionContext();
}

JSC::ExecState* mainWorldExecState(Frame* frame)
{
    if (!frame)
        return nullptr;
    return frame->windowProxy().jsWindowProxy(mainThreadNormalWorld())->window()->globalExec();
}

JSC::ExecState* execStateFromNode(DOMWrapperWorld& world, Node* node)
{
    if (!node)
        return nullptr;
    Frame* frame = node->document().frame();
    if (!frame)
        return nullptr;
    if (!frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return nullptr;
    return frame->script().globalObject(world)->globalExec();
}

JSC::ExecState* execStateFromPage(DOMWrapperWorld& world, Page* page)
{
    return page ? page->mainFrame().script().globalObject(world)->globalExec() : nullptr;
}

JSC::ExecState* execStateFromWorkerGlobalScope(WorkerGlobalScope& workerGlobalScope)
{
    return workerGlobalScope.script()->workerGlobalScopeWrapper()->globalExec();
}

#if ENABLE(CSS_PAINTING_API)
JSC::ExecState* execStateFromWorkletGlobalScope(WorkletGlobalScope& workletGlobalScope)
{
    if (!workletGlobalScope.script())
        return nullptr;
    return workletGlobalScope.script()->workletGlobalScopeWrapper()->globalExec();
}
#endif

}
