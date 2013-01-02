/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "V8JavaScriptCallFrame.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "V8Binding.h"

namespace WebCore {

v8::Handle<v8::Value> V8JavaScriptCallFrame::evaluateCallback(const v8::Arguments& args)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(args.Holder());
    String expression = toWebCoreStringWithUndefinedOrNullCheck(args[0]);
    return impl->evaluate(expression);
}

v8::Handle<v8::Value> V8JavaScriptCallFrame::restartCallback(const v8::Arguments& args)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(args.Holder());
    return impl->restart();
}

v8::Handle<v8::Value> V8JavaScriptCallFrame::scopeChainAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(info.Holder());
    return impl->scopeChain();
}

v8::Handle<v8::Value> V8JavaScriptCallFrame::scopeTypeCallback(const v8::Arguments& args)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(args.Holder());
    int scopeIndex = args[0]->Int32Value();
    return v8::Int32::New(impl->scopeType(scopeIndex));
}

v8::Handle<v8::Value> V8JavaScriptCallFrame::thisObjectAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toNative(info.Holder());
    return impl->thisObject();
}

v8::Handle<v8::Value> V8JavaScriptCallFrame::typeAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    return v8::String::NewSymbol("function");
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
