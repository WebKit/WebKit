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
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include "ScriptProfile.h"
#include "V8Binding.h"
#include "V8Proxy.h"
#include "V8ScriptProfile.h"

namespace WebCore {

typedef Vector<RefPtr<ScriptProfile> > ProfilesArray;

#if ENABLE(JAVASCRIPT_DEBUGGER)
v8::Handle<v8::Value> V8Console::profilesAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Console.profilesAccessorGetter");
    Console* imp = V8Console::toNative(info.Holder());
    const ProfilesArray& profiles = imp->profiles();
    v8::Handle<v8::Array> result = v8::Array::New(profiles.size());
    int index = 0;
    ProfilesArray::const_iterator end = profiles.end();
    for (ProfilesArray::const_iterator iter = profiles.begin(); iter != end; ++iter)
        result->Set(v8::Integer::New(index++), toV8(iter->get()));
    return result;
}
#endif

v8::Handle<v8::Value> V8Console::traceCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Console.traceCallback");
    Console* imp = V8Console::toNative(args.Holder());
    OwnPtr<ScriptCallStack> callStack(createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture));
    OwnPtr<ScriptArguments> scriptArguments(createScriptArguments(args, 0));
    imp->trace(scriptArguments.release(), callStack.release());
    return v8::Handle<v8::Value>();
}

v8::Handle<v8::Value> V8Console::assertCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Console.assertCallback");
    Console* imp = V8Console::toNative(args.Holder());
    OwnPtr<ScriptCallStack> callStack(createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture));
    bool condition = args[0]->BooleanValue();
    OwnPtr<ScriptArguments> scriptArguments(createScriptArguments(args, 1));
    imp->assertCondition(condition, scriptArguments.release(), callStack.release());
    return v8::Handle<v8::Value>();
}

} // namespace WebCore
