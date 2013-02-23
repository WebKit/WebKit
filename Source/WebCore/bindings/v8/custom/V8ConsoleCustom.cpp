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

#include "V8Console.h"

#include "Console.h"
#include "ScriptArguments.h"
#include "ScriptCallStackFactory.h"
#include "V8Binding.h"
#include "V8MemoryInfo.h"

namespace WebCore {

v8::Handle<v8::Value> V8Console::traceMethodCustom(const v8::Arguments& args)
{
    Console* imp = V8Console::toNative(args.Holder());
    RefPtr<ScriptArguments> scriptArguments(createScriptArguments(args, 0));
    imp->trace(ScriptState::current(), scriptArguments.release());
    return v8Undefined();
}

v8::Handle<v8::Value> V8Console::assertMethodCustom(const v8::Arguments& args)
{
    Console* imp = V8Console::toNative(args.Holder());
    bool condition = args[0]->BooleanValue();
    RefPtr<ScriptArguments> scriptArguments(createScriptArguments(args, 1));
    imp->assertCondition(ScriptState::current(), scriptArguments.release(), condition);
    return v8Undefined();
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
v8::Handle<v8::Value> V8Console::profileMethodCustom(const v8::Arguments& args)
{
    Console* imp = V8Console::toNative(args.Holder());
    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<WithUndefinedOrNullCheck>, title, args[0]);
    imp->profile(title, ScriptState::current());
    return v8Undefined();
}

v8::Handle<v8::Value> V8Console::profileEndMethodCustom(const v8::Arguments& args)
{
    Console* imp = V8Console::toNative(args.Holder());
    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<WithUndefinedOrNullCheck>, title, args[0]);
    imp->profileEnd(title, ScriptState::current());
    return v8Undefined();
}
#endif

} // namespace WebCore
