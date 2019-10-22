/*
 * Copyright (c) 2008, 2011 Google Inc.
 * All rights reserved.
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

#pragma once

namespace JSC {
class CallFrame;
class JSGlobalObject;
}

namespace WebCore {

class DOMWindow;
class DOMWrapperWorld;
class Frame;
class Node;
class Page;
class ScriptExecutionContext;
class WorkerGlobalScope;
#if ENABLE(CSS_PAINTING_API)
class WorkletGlobalScope;
#endif

DOMWindow* domWindowFromExecState(JSC::JSGlobalObject*);
Frame* frameFromExecState(JSC::JSGlobalObject*);
ScriptExecutionContext* scriptExecutionContextFromExecState(JSC::JSGlobalObject*);

JSC::JSGlobalObject* mainWorldExecState(Frame*);

JSC::JSGlobalObject* execStateFromNode(DOMWrapperWorld&, Node*);
WEBCORE_EXPORT JSC::JSGlobalObject* execStateFromPage(DOMWrapperWorld&, Page*);
JSC::JSGlobalObject* execStateFromWorkerGlobalScope(WorkerGlobalScope&);
#if ENABLE(CSS_PAINTING_API)
JSC::JSGlobalObject* execStateFromWorkletGlobalScope(WorkletGlobalScope&);
#endif

} // namespace WebCore
