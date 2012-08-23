/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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
#include "V8Proxy.h"

#include "BindingState.h"
#include "CachedMetadata.h"
#include "DateExtension.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "ExceptionHeaders.h"
#include "ExceptionInterfaces.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "IDBFactoryBackendInterface.h"
#include "InspectorInstrumentation.h"
#include "PlatformSupport.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include "ScriptRunner.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StylePropertySet.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8DOMCoreException.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8HiddenPropertyName.h"
#include "V8IsolatedContext.h"
#include "V8RecursionScope.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"

#include <algorithm>
#include <stdio.h>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

namespace WebCore {

V8Proxy::V8Proxy(Frame* frame)
    : m_frame(frame)
{
}

V8Proxy::~V8Proxy()
{
    windowShell()->destroyGlobal();
}

v8::Local<v8::Value> V8Proxy::evaluate(const ScriptSourceCode& source, Node* node)
{
    ASSERT(v8::Context::InContext());

    V8GCController::checkMemoryUsage();

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willEvaluateScript(m_frame, source.url().isNull() ? String() : source.url().string(), source.startLine());

    v8::Local<v8::Value> result;
    {
        // Isolate exceptions that occur when compiling and executing
        // the code. These exceptions should not interfere with
        // javascript code we might evaluate from C++ when returning
        // from here.
        v8::TryCatch tryCatch;
        tryCatch.SetVerbose(true);

        // Compile the script.
        v8::Local<v8::String> code = v8ExternalString(source.source());
#if PLATFORM(CHROMIUM)
        TRACE_EVENT_BEGIN0("v8", "v8.compile");
#endif
        OwnPtr<v8::ScriptData> scriptData = ScriptSourceCode::precompileScript(code, source.cachedScript());

        // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
        // 1, whereas v8 starts at 0.
        v8::Handle<v8::Script> script = ScriptSourceCode::compileScript(code, source.url(), source.startPosition(), scriptData.get());
#if PLATFORM(CHROMIUM)
        TRACE_EVENT_END0("v8", "v8.compile");
        TRACE_EVENT0("v8", "v8.run");
#endif

        // Keep Frame (and therefore ScriptController) alive.
        RefPtr<Frame> protect(frame());
        result = ScriptRunner::runCompiledScript(script, frame()->document());
    }

    InspectorInstrumentation::didEvaluateScript(cookie);

    return result;
}

V8DOMWindowShell* V8Proxy::windowShell() const
{
    return frame()->script()->windowShell();
}

}  // namespace WebCore
