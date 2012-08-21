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

void V8Proxy::reportUnsafeAccessTo(Document* targetDocument)
{
    if (!targetDocument)
        return;

    // FIXME: We should pass both the active and target documents in as arguments.
    Frame* source = firstFrame(BindingState::instance());
    if (!source)
        return;

    Document* sourceDocument = source->document();
    if (!sourceDocument)
        return; // Ignore error if the source document is gone.

    // FIXME: This error message should contain more specifics of why the same
    // origin check has failed.
    String str = "Unsafe JavaScript attempt to access frame with URL " + targetDocument->url().string() +
                 " from frame with URL " + sourceDocument->url().string() + ". Domains, protocols and ports must match.\n";

    RefPtr<ScriptCallStack> stackTrace = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true);

    // NOTE: Safari prints the message in the target page, but it seems like
    // it should be in the source page. Even for delayed messages, we put it in
    // the source page.
    sourceDocument->addConsoleMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, str, stackTrace.release());
}

// FIXME: This will be soon removed when we move runScript() to ScriptController.
static v8::Local<v8::Value> handleMaxRecursionDepthExceeded()
{
    throwError(RangeError, "Maximum call stack size exceeded.");
    return v8::Local<v8::Value>();
}

V8Proxy::V8Proxy(Frame* frame)
    : m_frame(frame)
{
}

V8Proxy::~V8Proxy()
{
    windowShell()->destroyGlobal();
}

v8::Handle<v8::Script> V8Proxy::compileScript(v8::Handle<v8::String> code, const String& fileName, const TextPosition& scriptStartPosition, v8::ScriptData* scriptData)
{
    const uint16_t* fileNameString = fromWebCoreString(fileName);
    v8::Handle<v8::String> name = v8::String::New(fileNameString, fileName.length());
    v8::Handle<v8::Integer> line = v8Integer(scriptStartPosition.m_line.zeroBasedInt());
    v8::Handle<v8::Integer> column = v8Integer(scriptStartPosition.m_column.zeroBasedInt());
    v8::ScriptOrigin origin(name, line, column);
    v8::Handle<v8::Script> script = v8::Script::Compile(code, &origin, scriptData);
    return script;
}

PassOwnPtr<v8::ScriptData> V8Proxy::precompileScript(v8::Handle<v8::String> code, CachedScript* cachedScript)
{
    // A pseudo-randomly chosen ID used to store and retrieve V8 ScriptData from
    // the CachedScript. If the format changes, this ID should be changed too.
    static const unsigned dataTypeID = 0xECC13BD7;

    // Very small scripts are not worth the effort to preparse.
    static const int minPreparseLength = 1024;

    if (!cachedScript || code->Length() < minPreparseLength)
        return nullptr;

    CachedMetadata* cachedMetadata = cachedScript->cachedMetadata(dataTypeID);
    if (cachedMetadata)
        return adoptPtr(v8::ScriptData::New(cachedMetadata->data(), cachedMetadata->size()));

    OwnPtr<v8::ScriptData> scriptData = adoptPtr(v8::ScriptData::PreCompile(code));
    cachedScript->setCachedMetadata(dataTypeID, scriptData->Data(), scriptData->Length());

    return scriptData.release();
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
        OwnPtr<v8::ScriptData> scriptData = precompileScript(code, source.cachedScript());

        // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
        // 1, whereas v8 starts at 0.
        v8::Handle<v8::Script> script = compileScript(code, source.url(), source.startPosition(), scriptData.get());
#if PLATFORM(CHROMIUM)
        TRACE_EVENT_END0("v8", "v8.compile");
        TRACE_EVENT0("v8", "v8.run");
#endif
        result = runScript(script);
    }

    InspectorInstrumentation::didEvaluateScript(cookie);

    return result;
}

v8::Local<v8::Value> V8Proxy::runScript(v8::Handle<v8::Script> script)
{
    if (script.IsEmpty())
        return v8::Local<v8::Value>();

    V8GCController::checkMemoryUsage();
    if (V8RecursionScope::recursionLevel() >= kMaxRecursionDepth)
        return handleMaxRecursionDepthExceeded();

    if (handleOutOfMemory())
        ASSERT(script.IsEmpty());

    // Keep Frame (and therefore ScriptController and V8Proxy) alive.
    RefPtr<Frame> protect(frame());

    // Run the script and keep track of the current recursion depth.
    v8::Local<v8::Value> result;
    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true);
    {
        V8RecursionScope recursionScope(frame()->document());
        result = script->Run();
    }

    if (handleOutOfMemory())
        ASSERT(result.IsEmpty());

    // Handle V8 internal error situation (Out-of-memory).
    if (tryCatch.HasCaught()) {
        ASSERT(result.IsEmpty());
        return v8::Local<v8::Value>();
    }

    if (result.IsEmpty())
        return v8::Local<v8::Value>();

    crashIfV8IsDead();
    return result;
}

V8DOMWindowShell* V8Proxy::windowShell() const
{
    return frame()->script()->windowShell();
}

v8::Local<v8::Context> V8Proxy::context(Frame* frame)
{
    v8::Local<v8::Context> context = ScriptController::mainWorldContext(frame);
    if (context.IsEmpty())
        return v8::Local<v8::Context>();

    if (V8IsolatedContext* isolatedContext = V8IsolatedContext::getEntered()) {
        context = v8::Local<v8::Context>::New(isolatedContext->context());
        if (frame != toFrameIfNotDetached(context))
            return v8::Local<v8::Context>();
    }

    return context;
}

v8::Local<v8::Context> V8Proxy::context()
{
    if (V8IsolatedContext* isolatedContext = V8IsolatedContext::getEntered()) {
        RefPtr<SharedPersistent<v8::Context> > context = isolatedContext->sharedContext();
        if (m_frame != toFrameIfNotDetached(context->get()))
            return v8::Local<v8::Context>();
        return v8::Local<v8::Context>::New(context->get());
    }
    return frame()->script()->mainWorldContext();
}

v8::Local<v8::Context> V8Proxy::isolatedWorldContext(int worldId)
{
    IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(worldId);
    if (iter == m_isolatedWorlds.end())
        return v8::Local<v8::Context>();
    return v8::Local<v8::Context>::New(iter->second->context());
}

bool V8Proxy::matchesCurrentContext()
{
    v8::Handle<v8::Context> context;
    if (V8IsolatedContext* isolatedContext = V8IsolatedContext::getEntered()) {
        context = isolatedContext->sharedContext()->get();
        if (m_frame != toFrameIfNotDetached(context))
            return false;
    } else {
        windowShell()->initContextIfNeeded();
        context = windowShell()->context();
    }
    return context == context->GetCurrent();
}

v8::Local<v8::Context> toV8Context(ScriptExecutionContext* context, const WorldContextHandle& worldContext)
{
    if (context->isDocument()) {
        if (Frame* frame = static_cast<Document*>(context)->frame())
            return worldContext.adjustedContext(frame->script());
#if ENABLE(WORKERS)
    } else if (context->isWorkerContext()) {
        if (WorkerContextExecutionProxy* proxy = static_cast<WorkerContext*>(context)->script()->proxy())
            return proxy->context();
#endif
    }
    return v8::Local<v8::Context>();
}

}  // namespace WebCore
