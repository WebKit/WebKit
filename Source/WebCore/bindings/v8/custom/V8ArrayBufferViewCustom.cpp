
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
const char fastSetFlagName[] = "TypedArray::FastSet::8NkZVq";

bool fastSetInstalled(v8::Handle<v8::Object> array)
{
    // Use a hidden flag in the global object an indicator of whether the fast
    // 'set' is installed or not.
    v8::Handle<v8::Object> global = array->CreationContext()->Global();
    v8::Handle<v8::String> key = v8::String::New(fastSetFlagName);
    v8::Handle<v8::Value> fastSetFlag = global->GetHiddenValue(key);
    return !fastSetFlag.IsEmpty();
}

void installFastSet(v8::Handle<v8::Object> array)
{
    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true);
    v8::Handle<v8::Object> global = array->CreationContext()->Global();
    v8::Handle<v8::String> key = v8::String::New(fastSetFlagName);
    global->SetHiddenValue(key, v8::Boolean::New(true));

    String source(reinterpret_cast<const char*>(V8ArrayBufferViewCustomScript_js),
                  sizeof(V8ArrayBufferViewCustomScript_js));
    v8::Handle<v8::Script> script = v8::Script::Compile(v8String(source));
    script->Run();
}


void copyElements(v8::Handle<v8::Object> destArray, v8::Handle<v8::Object> srcArray, uint32_t offset)
{
    v8::Handle<v8::String> key = v8::String::New("set");
    v8::Handle<v8::Function> set = destArray->Get(key).As<v8::Function>();
    v8::Handle<v8::Value> arguments[2];
    int numberOfArguments = 1;
    arguments[0] = srcArray;
    if (offset) {
        arguments[1] = v8::Uint32::New(offset);
        numberOfArguments = 2;
    }
    set->Call(destArray, numberOfArguments, arguments);
}

}
