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
#include "NodeIterator.h"

#include "ScriptState.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

static inline v8::Handle<v8::Value> toV8(PassRefPtr<Node> object, ExceptionCode ec, ScriptState* state)
{
    if (ec)
        return throwError(ec);

    if (state->hadException())
        return throwError(state->exception());

    if (!object)
        return v8::Null();

    return V8Proxy::NodeToV8Object(object.get());
}

CALLBACK_FUNC_DECL(NodeIteratorNextNode)
{
    INC_STATS("DOM.NodeIterator.nextNode()");
    NodeIterator* nodeIterator = V8Proxy::ToNativeObject<NodeIterator>(V8ClassIndex::NODEITERATOR, args.Holder());

    ExceptionCode ec = 0;
    ScriptState state;
    RefPtr<Node> result = nodeIterator->nextNode(&state, ec);
    return toV8(result.release(), ec, &state);
}

CALLBACK_FUNC_DECL(NodeIteratorPreviousNode)
{
    INC_STATS("DOM.NodeIterator.previousNode()");
    NodeIterator* nodeIterator = V8Proxy::ToNativeObject<NodeIterator>(V8ClassIndex::NODEITERATOR, args.Holder());

    ExceptionCode ec = 0;
    ScriptState state;
    RefPtr<Node> result = nodeIterator->previousNode(&state, ec);
    return toV8(result.release(), ec, &state);
}

} // namespace WebCore
