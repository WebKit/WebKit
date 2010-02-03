/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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
#include "V8TreeWalker.h"

#include "Node.h"
#include "ScriptState.h"
#include "TreeWalker.h"

#include "V8Binding.h"
#include "V8Node.h"
#include "V8Proxy.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

static inline v8::Handle<v8::Value> toV8Object(PassRefPtr<Node> object, ScriptState* state)
{
    if (state->hadException())
        return throwError(state->exception());

    if (!object)
        return v8::Null();

    return toV8(object);
}

v8::Handle<v8::Value> V8TreeWalker::parentNodeCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.TreeWalker.parentNode()");
    TreeWalker* treeWalker = V8TreeWalker::toNative(args.Holder());

    EmptyScriptState state;
    RefPtr<Node> result = treeWalker->parentNode(&state);
    return toV8Object(result.release(), &state);
}

v8::Handle<v8::Value> V8TreeWalker::firstChildCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.TreeWalker.firstChild()");
    TreeWalker* treeWalker = V8TreeWalker::toNative(args.Holder());

    EmptyScriptState state;
    RefPtr<Node> result = treeWalker->firstChild(&state);
    return toV8Object(result.release(), &state);
}

v8::Handle<v8::Value> V8TreeWalker::lastChildCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.TreeWalker.lastChild()");
    TreeWalker* treeWalker = V8TreeWalker::toNative(args.Holder());

    EmptyScriptState state;
    RefPtr<Node> result = treeWalker->lastChild(&state);
    return toV8Object(result.release(), &state);
}

v8::Handle<v8::Value> V8TreeWalker::nextNodeCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.TreeWalker.nextNode()");
    TreeWalker* treeWalker = V8TreeWalker::toNative(args.Holder());

    EmptyScriptState state;
    RefPtr<Node> result = treeWalker->nextNode(&state);
    return toV8Object(result.release(), &state);
}

v8::Handle<v8::Value> V8TreeWalker::previousNodeCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.TreeWalker.previousNode()");
    TreeWalker* treeWalker = V8TreeWalker::toNative(args.Holder());

    EmptyScriptState state;
    RefPtr<Node> result = treeWalker->previousNode(&state);
    return toV8Object(result.release(), &state);
}

v8::Handle<v8::Value> V8TreeWalker::nextSiblingCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.TreeWalker.nextSibling()");
    TreeWalker* treeWalker = V8TreeWalker::toNative(args.Holder());

    EmptyScriptState state;
    RefPtr<Node> result = treeWalker->nextSibling(&state);
    return toV8Object(result.release(), &state);
}

v8::Handle<v8::Value> V8TreeWalker::previousSiblingCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.TreeWalker.previousSibling()");
    TreeWalker* treeWalker = V8TreeWalker::toNative(args.Holder());

    EmptyScriptState state;
    RefPtr<Node> result = treeWalker->previousSibling(&state);
    return toV8Object(result.release(), &state);
}

} // namespace WebCore
