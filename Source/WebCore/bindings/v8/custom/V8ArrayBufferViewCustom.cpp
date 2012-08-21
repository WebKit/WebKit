/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "V8ArrayBufferViewCustom.h"

#include "V8ArrayBufferViewCustomScript.h"

#include <v8.h>


namespace WebCore {

// The random suffix helps to avoid name collision.
const char hiddenCopyMethodName[] = "TypedArray::HiddenCopy::8NkZVq";

v8::Handle<v8::Value> getHiddenCopyMethod(v8::Handle<v8::Object> prototype)
{
    v8::Handle<v8::String> key = v8::String::New(hiddenCopyMethodName);
    return prototype->GetHiddenValue(key);
}

v8::Handle<v8::Value> installHiddenCopyMethod(v8::Handle<v8::Object> prototype) {
    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true);
    String source(reinterpret_cast<const char*>(V8ArrayBufferViewCustomScript_js),
                  sizeof(V8ArrayBufferViewCustomScript_js));
    v8::Handle<v8::Script> script = v8::Script::Compile(v8String(source));
    v8::Handle<v8::Value> value = script->Run();
    v8::Handle<v8::String> key = v8::String::New(hiddenCopyMethodName);
    prototype->SetHiddenValue(key, value);
    return value;
}

bool copyElements(v8::Handle<v8::Object> destArray, v8::Handle<v8::Object> srcArray, uint32_t length, uint32_t offset, v8::Isolate* isolate)
{
    v8::Handle<v8::Value> prototype_value = destArray->GetPrototype();
    if (prototype_value.IsEmpty() || !prototype_value->IsObject())
        return false;
    v8::Handle<v8::Object> prototype = prototype_value.As<v8::Object>();
    v8::Handle<v8::Value> value = getHiddenCopyMethod(prototype);
    if (value.IsEmpty())
        value = installHiddenCopyMethod(prototype);
    if (value.IsEmpty() || !value->IsFunction())
        return false;
    v8::Handle<v8::Function> copy_method = value.As<v8::Function>();
    v8::Handle<v8::Value> arguments[3];
    arguments[0] = srcArray;
    arguments[1] = v8::Uint32::New(length);
    arguments[2] = v8::Uint32::New(offset);
    copy_method->Call(destArray, 3, arguments);
    return true;
}

}
