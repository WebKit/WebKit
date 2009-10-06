/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "DateExtension.h"

#include "V8Proxy.h"
#include "V8HiddenPropertyName.h"

namespace WebCore {

DateExtension* DateExtension::extension;

static const char* dateExtensionName = "v8/DateExtension";
static const char* dateExtensionScript =
    "(function () {"
    "  var counter;"
    "  var orig_getTime;"
    "  function getTimeOverride() {"
    "    if (++counter > 1000)"
    "      OnSleepDetected();"
    "    return orig_getTime.call(this);"
    "  };"
    "  function enableSleepDetection(enable) {"
    "    if (enable) {"
    "      counter = 0;"
    "      orig_getTime = Date.prototype.getTime;"
    "      Date.prototype.getTime = getTimeOverride;"
    "    } else {"
    "      Date.prototype.getTime = orig_getTime;"
    "    }"
    "  };"
    "  native function Setup();"
    "  native function OnSleepDetected();"
    "  Setup(Date, enableSleepDetection);"
    "})()";

DateExtension::DateExtension() : v8::Extension(dateExtensionName, dateExtensionScript)
{
}

DateExtension* DateExtension::get()
{
    if (!extension)
        extension = new DateExtension();
    return extension;
}

void DateExtension::setAllowSleep(bool allow)
{
    v8::Local<v8::Value> result = V8Proxy::retrieve()->context()->Global()->Get(v8::String::New("Date"));
    if (result.IsEmpty())
        return;

    v8::Handle<v8::Object> dateObject = v8::Handle<v8::Object>::Cast(result);
    if (dateObject.IsEmpty())
        return;

    v8::Local<v8::Value> sleepFunctionHandle = dateObject->GetHiddenValue(V8HiddenPropertyName::sleepFunction());
    if (sleepFunctionHandle.IsEmpty() || !sleepFunctionHandle->IsFunction())
        return;

    v8::Handle<v8::Value> argv[1];
    argv[0] = v8::String::New(allow ? "false" : "true");
    v8::Handle<v8::Function>::Cast(sleepFunctionHandle)->Call(v8::Object::New(), 1, argv);
}

v8::Handle<v8::FunctionTemplate> DateExtension::GetNativeFunction(v8::Handle<v8::String> name)
{
    if (name->Equals(v8::String::New("Setup")))
        return v8::FunctionTemplate::New(Setup);
    if (name->Equals(v8::String::New("OnSleepDetected")))
        return v8::FunctionTemplate::New(OnSleepDetected);

    return v8::Handle<v8::FunctionTemplate>();
}

v8::Handle<v8::Value> DateExtension::Setup(const v8::Arguments& args)
{
    if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsFunction())
        return v8::Undefined();

    v8::Handle<v8::Object> dateObject = v8::Handle<v8::Object>::Cast(args[0]);
    v8::Handle<v8::Function> enableSleepDetectionFunction = v8::Handle<v8::Function>::Cast(args[1]);

    dateObject->SetHiddenValue(V8HiddenPropertyName::sleepFunction(), enableSleepDetectionFunction);
    return v8::Undefined();
}

v8::Handle<v8::Value> DateExtension::OnSleepDetected(const v8::Arguments&)
{
    v8::V8::TerminateExecution();
    return v8::Undefined();
}

}  // namespace WebCore
