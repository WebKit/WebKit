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
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerOrWorkletScriptController.h"
#include "WorkletGlobalScope.h"
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/StrongInlines.h>

namespace WebCore {

DOMWindow* domWindowFromExecState(JSC::JSGlobalObject* lexicalGlobalObject)
{
    JSC::JSGlobalObject* globalObject = lexicalGlobalObject;
    JSC::VM& vm = globalObject->vm();
    if (!globalObject->inherits<JSDOMWindowBase>(vm))
        return nullptr;
    return &JSC::jsCast<JSDOMWindowBase*>(globalObject)->wrapped();
}

Frame* frameFromExecState(JSC::JSGlobalObject* lexicalGlobalObject)
{
    ScriptExecutionContext* context = scriptExecutionContextFromExecState(lexicalGlobalObject);
    Document* document = is<Document>(context) ? downcast<Document>(context) : nullptr;
    return document ? document->frame() : nullptr;
}

ScriptExecutionContext* scriptExecutionContextFromExecState(JSC::JSGlobalObject* lexicalGlobalObject)
{
    JSC::JSGlobalObject* globalObject = lexicalGlobalObject;
    JSC::VM& vm = globalObject->vm();
    if (!globalObject->inherits<JSDOMGlobalObject>(vm))
        return nullptr;
    return JSC::jsCast<JSDOMGlobalObject*>(globalObject)->scriptExecutionContext();
}

JSC::JSGlobalObject* mainWorldExecState(Frame* frame)
{
    if (!frame)
        return nullptr;
    return frame->windowProxy().jsWindowProxy(mainThreadNormalWorld())->window();
}

JSC::JSGlobalObject* globalObject(DOMWrapperWorld& world, Node* node)
{
    if (!node)
        return nullptr;
    Frame* frame = node->document().frame();
    if (!frame)
        return nullptr;
    if (!frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return nullptr;
    return frame->script().globalObject(world);
}

JSC::JSGlobalObject* globalObject(DOMWrapperWorld& world, Frame* frame)
{
    return frame ? frame->script().globalObject(world) : nullptr;
}

JSC::JSGlobalObject* globalObject(WorkerOrWorkletGlobalScope& workerOrWorkletGlobalScope)
{
    if (auto* scriptController = workerOrWorkletGlobalScope.script())
        return scriptController->globalScopeWrapper();
    return nullptr;
}

}
