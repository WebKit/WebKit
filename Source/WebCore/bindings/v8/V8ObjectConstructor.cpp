/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8ObjectConstructor.h"

#include "Frame.h"
#include "V8Binding.h"
#include "V8RecursionScope.h"

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

namespace WebCore {

v8::Local<v8::Object> V8ObjectConstructor::newInstance(v8::Handle<v8::Function> function)
{
    if (function.IsEmpty())
        return v8::Local<v8::Object>();
    ConstructorMode constructorMode;
    V8RecursionScope::MicrotaskSuppression scope;
    v8::Local<v8::Object> result = function->NewInstance();
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Object> V8ObjectConstructor::newInstance(v8::Handle<v8::ObjectTemplate> objectTemplate)
{
    if (objectTemplate.IsEmpty())
        return v8::Local<v8::Object>();
    ConstructorMode constructorMode;
    V8RecursionScope::MicrotaskSuppression scope;
    v8::Local<v8::Object> result = objectTemplate->NewInstance();
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Object> V8ObjectConstructor::newInstance(v8::Handle<v8::Function> function, int argc, v8::Handle<v8::Value> argv[])
{
    if (function.IsEmpty())
        return v8::Local<v8::Object>();
    ConstructorMode constructorMode;
    V8RecursionScope::MicrotaskSuppression scope;
    v8::Local<v8::Object> result = function->NewInstance(argc, argv);
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Object> V8ObjectConstructor::newInstanceInDocument(v8::Handle<v8::Function> function, int argc, v8::Handle<v8::Value> argv[], Document* document)
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT0("v8", "v8.newInstance");
#endif

    // No artificial limitations on the depth of recursion, see comment in
    // V8Proxy::callFunction.
    V8RecursionScope recursionScope(document);
    v8::Local<v8::Object> result = function->NewInstance(argc, argv);
    crashIfV8IsDead();
    return result;
}

v8::Handle<v8::Value> V8ObjectConstructor::isValidConstructorMode(const v8::Arguments& args)
{
    if (ConstructorMode::current() == ConstructorMode::CreateNewObject)
        return throwTypeError("Illegal constructor", args.GetIsolate());
    return args.This();
}

} // namespace WebCore
