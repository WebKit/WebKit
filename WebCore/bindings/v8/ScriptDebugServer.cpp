/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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
#include "ScriptDebugServer.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "V8Binding.h"
#include <v8-debug.h>

#include <wtf/StdLibExtras.h>

namespace WebCore {

v8::Persistent<v8::Context> ScriptDebugServer::s_utilityContext;

ScriptDebugServer& ScriptDebugServer::shared()
{
    DEFINE_STATIC_LOCAL(ScriptDebugServer, server, ());
    return server;
}

// Create the utility context for holding JavaScript functions used internally
// which are not visible to JavaScript executing on the page.
void ScriptDebugServer::createUtilityContext()
{
    ASSERT(s_utilityContext.IsEmpty());

    v8::HandleScope scope;
    v8::Handle<v8::ObjectTemplate> globalTemplate = v8::ObjectTemplate::New();
    s_utilityContext = v8::Context::New(0, globalTemplate);
    v8::Context::Scope contextScope(s_utilityContext);

    // Compile JavaScript function for retrieving the source line, the source
    // name and the symbol name for the top JavaScript stack frame.
    DEFINE_STATIC_LOCAL(const char*, topStackFrame,
        ("function topStackFrame(exec_state) {"
        "  if (!exec_state.frameCount())"
        "      return undefined;"
        "  var frame = exec_state.frame(0);"
        "  var func = frame.func();"
        "  var scriptName;"
        "  if (func.resolved() && func.script())"
        "      scriptName = func.script().name();"
        "  return [scriptName, frame.sourceLine(), (func.name() || func.inferredName())];"
        "}"));
    v8::Script::Compile(v8::String::New(topStackFrame))->Run();
}

bool ScriptDebugServer::topStackFrame(String& sourceName, int& lineNumber, String& functionName)
{
    v8::HandleScope scope;
    v8::Handle<v8::Context> v8UtilityContext = utilityContext();
    if (v8UtilityContext.IsEmpty())
        return false;
    v8::Context::Scope contextScope(v8UtilityContext);
    v8::Handle<v8::Function> topStackFrame;
    topStackFrame = v8::Local<v8::Function>::Cast(v8UtilityContext->Global()->Get(v8::String::New("topStackFrame")));
    if (topStackFrame.IsEmpty())
        return false;
    v8::Handle<v8::Value> value = v8::Debug::Call(topStackFrame);
    if (value.IsEmpty())
        return false;    
    if (!value->IsArray())
        return false;
    v8::Local<v8::Object> jsArray = value->ToObject();
    v8::Local<v8::Value> sourceNameValue = jsArray->Get(0);
    v8::Local<v8::Value> lineNumberValue = jsArray->Get(1);
    v8::Local<v8::Value> functionNameValue = jsArray->Get(2);
    if (sourceNameValue.IsEmpty() || lineNumberValue.IsEmpty() || functionNameValue.IsEmpty())
        return false;  
    sourceName = toWebCoreString(sourceNameValue);
    lineNumber = lineNumberValue->Int32Value();
    functionName = toWebCoreString(functionNameValue);
    return true;
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
