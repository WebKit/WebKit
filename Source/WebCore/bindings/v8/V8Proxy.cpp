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

#include "CSSMutableStyleDeclaration.h"
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
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "V8Binding.h"
#include "V8BindingState.h"
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

namespace WebCore {

static V8Extensions& staticExtensionsList()
{
    DEFINE_STATIC_LOCAL(V8Extensions, extensions, ());
    return extensions;
}

void batchConfigureAttributes(v8::Handle<v8::ObjectTemplate> instance, 
                              v8::Handle<v8::ObjectTemplate> proto, 
                              const BatchedAttribute* attributes, 
                              size_t attributeCount)
{
    for (size_t i = 0; i < attributeCount; ++i)
        configureAttribute(instance, proto, attributes[i]);
}

void batchConfigureCallbacks(v8::Handle<v8::ObjectTemplate> proto, 
                             v8::Handle<v8::Signature> signature, 
                             v8::PropertyAttribute attributes,
                             const BatchedCallback* callbacks,
                             size_t callbackCount)
{
    for (size_t i = 0; i < callbackCount; ++i) {
        proto->Set(v8::String::New(callbacks[i].name),
                   v8::FunctionTemplate::New(callbacks[i].callback, 
                                             v8::Handle<v8::Value>(),
                                             signature),
                   attributes);
    }
}

void batchConfigureConstants(v8::Handle<v8::FunctionTemplate> functionDescriptor,
                             v8::Handle<v8::ObjectTemplate> proto,
                             const BatchedConstant* constants,
                             size_t constantCount)
{
    for (size_t i = 0; i < constantCount; ++i) {
        const BatchedConstant* constant = &constants[i];
        functionDescriptor->Set(v8::String::New(constant->name), v8::Integer::New(constant->value), v8::ReadOnly);
        proto->Set(v8::String::New(constant->name), v8::Integer::New(constant->value), v8::ReadOnly);
    }
}

typedef HashMap<Node*, v8::Object*> DOMNodeMap;
typedef HashMap<void*, v8::Object*> DOMObjectMap;
typedef HashMap<int, v8::FunctionTemplate*> FunctionTemplateMap;

void V8Proxy::reportUnsafeAccessTo(Frame* target)
{
    ASSERT(target);
    Document* targetDocument = target->document();
    if (!targetDocument)
        return;

    Frame* source = V8Proxy::retrieveFrameForEnteredContext();
    if (!source)
        return;

    Document* sourceDocument = source->document();
    if (!sourceDocument)
        return; // Ignore error if the source document is gone.

    // FIXME: This error message should contain more specifics of why the same
    // origin check has failed.
    String str = "Unsafe JavaScript attempt to access frame with URL " + targetDocument->url().string() +
                 " from frame with URL " + sourceDocument->url().string() + ". Domains, protocols and ports must match.\n";

    // NOTE: Safari prints the message in the target page, but it seems like
    // it should be in the source page. Even for delayed messages, we put it in
    // the source page.
    sourceDocument->addConsoleMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, str);
}

static void handleFatalErrorInV8()
{
    // FIXME: We temporarily deal with V8 internal error situations
    // such as out-of-memory by crashing the renderer.
    CRASH();
}

static v8::Local<v8::Value> handleMaxRecursionDepthExceeded()
{
    throwError("Maximum call stack size exceeded.", V8Proxy::RangeError);
    return v8::Local<v8::Value>();
}

V8Proxy::V8Proxy(Frame* frame)
    : m_frame(frame)
    , m_windowShell(V8DOMWindowShell::create(frame))
{
}

V8Proxy::~V8Proxy()
{
    clearForClose();
    windowShell()->destroyGlobal();
}

v8::Handle<v8::Script> V8Proxy::compileScript(v8::Handle<v8::String> code, const String& fileName, const TextPosition& scriptStartPosition, v8::ScriptData* scriptData)
{
    const uint16_t* fileNameString = fromWebCoreString(fileName);
    v8::Handle<v8::String> name = v8::String::New(fileNameString, fileName.length());
    v8::Handle<v8::Integer> line = v8::Integer::New(scriptStartPosition.m_line.zeroBasedInt());
    v8::Handle<v8::Integer> column = v8::Integer::New(scriptStartPosition.m_column.zeroBasedInt());
    v8::ScriptOrigin origin(name, line, column);
    v8::Handle<v8::Script> script = v8::Script::Compile(code, &origin, scriptData);
    return script;
}

bool V8Proxy::handleOutOfMemory()
{
    v8::Local<v8::Context> context = v8::Context::GetCurrent();

    if (!context->HasOutOfMemoryException())
        return false;

    // Warning, error, disable JS for this frame?
    Frame* frame = V8Proxy::retrieveFrame(context);

    V8Proxy* proxy = V8Proxy::retrieve(frame);
    if (proxy && frame->script()->canExecuteScripts(NotAboutToExecuteScript)) {
        // Clean m_context, and event handlers.
        proxy->clearForClose();

        proxy->windowShell()->destroyGlobal();
    }

#if PLATFORM(CHROMIUM)
    PlatformSupport::notifyJSOutOfMemory(frame);
#endif

    // Disable JS.
    Settings* settings = frame->settings();
    ASSERT(settings);
    settings->setScriptEnabled(false);

    return true;
}

void V8Proxy::evaluateInIsolatedWorld(int worldID, const Vector<ScriptSourceCode>& sources, int extensionGroup)
{
    // FIXME: This will need to get reorganized once we have a windowShell for the isolated world.
    if (!windowShell()->initContextIfNeeded())
        return;

    v8::HandleScope handleScope;
    V8IsolatedContext* isolatedContext = 0;

    if (worldID > 0) {
        IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(worldID);
        if (iter != m_isolatedWorlds.end()) {
            isolatedContext = iter->second;
        } else {
            isolatedContext = new V8IsolatedContext(this, extensionGroup, worldID);
            if (isolatedContext->context().IsEmpty()) {
                delete isolatedContext;
                return;
            }

            // FIXME: We should change this to using window shells to match JSC.
            m_isolatedWorlds.set(worldID, isolatedContext);

            // Setup context id for JS debugger.
            if (!setInjectedScriptContextDebugId(isolatedContext->context())) {
                m_isolatedWorlds.take(worldID);
                delete isolatedContext;
                return;
            }
        }
        
        IsolatedWorldSecurityOriginMap::iterator securityOriginIter = m_isolatedWorldSecurityOrigins.find(worldID);
        if (securityOriginIter != m_isolatedWorldSecurityOrigins.end())
            isolatedContext->setSecurityOrigin(securityOriginIter->second);
    } else {
        isolatedContext = new V8IsolatedContext(this, extensionGroup, worldID);
        if (isolatedContext->context().IsEmpty()) {
            delete isolatedContext;
            return;
        }
    }

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolatedContext->context());
    v8::Context::Scope context_scope(context);
    for (size_t i = 0; i < sources.size(); ++i)
      evaluate(sources[i], 0);

    if (worldID == 0)
      isolatedContext->destroy();
}

void V8Proxy::setIsolatedWorldSecurityOrigin(int worldID, PassRefPtr<SecurityOrigin> prpSecurityOriginIn)
{
    ASSERT(worldID);
    RefPtr<SecurityOrigin> securityOrigin = prpSecurityOriginIn;
    m_isolatedWorldSecurityOrigins.set(worldID, securityOrigin);
    IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(worldID);
    if (iter != m_isolatedWorlds.end())
        iter->second->setSecurityOrigin(securityOrigin);
}

bool V8Proxy::setInjectedScriptContextDebugId(v8::Handle<v8::Context> targetContext)
{
    // Setup context id for JS debugger.
    v8::Context::Scope contextScope(targetContext);
    v8::Handle<v8::Context> context = windowShell()->context();
    if (context.IsEmpty())
        return false;
    int debugId = contextDebugId(context);

    char buffer[32];
    if (debugId == -1)
        snprintf(buffer, sizeof(buffer), "injected");
    else
        snprintf(buffer, sizeof(buffer), "injected,%d", debugId);
    targetContext->SetData(v8::String::New(buffer));

    return true;
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
        PlatformSupport::traceEventBegin("v8.compile", node, "");
#endif
        OwnPtr<v8::ScriptData> scriptData = precompileScript(code, source.cachedScript());

        // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
        // 1, whereas v8 starts at 0.
        v8::Handle<v8::Script> script = compileScript(code, source.url(), source.startPosition(), scriptData.get());
#if PLATFORM(CHROMIUM)
        PlatformSupport::traceEventEnd("v8.compile", node, "");

        PlatformSupport::traceEventBegin("v8.run", node, "");
#endif
        result = runScript(script);
    }
#if PLATFORM(CHROMIUM)
    PlatformSupport::traceEventEnd("v8.run", node, "");
#endif

    InspectorInstrumentation::didEvaluateScript(cookie);

    return result;
}

v8::Local<v8::Value> V8Proxy::runScript(v8::Handle<v8::Script> script)
{
    if (script.IsEmpty())
        return notHandledByInterceptor();

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
        V8RecursionScope recursionScope;
        result = script->Run();
    }

    if (handleOutOfMemory())
        ASSERT(result.IsEmpty());

    // Handle V8 internal error situation (Out-of-memory).
    if (tryCatch.HasCaught()) {
        ASSERT(result.IsEmpty());
        return notHandledByInterceptor();
    }

    if (result.IsEmpty())
        return notHandledByInterceptor();

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

    return result;
}

v8::Local<v8::Value> V8Proxy::callFunction(v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> args[])
{
    // Keep Frame (and therefore ScriptController and V8Proxy) alive.
    RefPtr<Frame> protect(frame());
    return V8Proxy::instrumentedCallFunction(m_frame->page(), function, receiver, argc, args);
}

v8::Local<v8::Value> V8Proxy::instrumentedCallFunction(Page* page, v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> args[])
{
    V8GCController::checkMemoryUsage();

    if (V8RecursionScope::recursionLevel() >= kMaxRecursionDepth)
        return handleMaxRecursionDepthExceeded();

    InspectorInstrumentationCookie cookie;
    if (InspectorInstrumentation::hasFrontends()) {
        String resourceName("undefined");
        int lineNumber = 1;
        v8::ScriptOrigin origin = function->GetScriptOrigin();
        if (!origin.ResourceName().IsEmpty()) {
            resourceName = toWebCoreString(origin.ResourceName());
            lineNumber = function->GetScriptLineNumber() + 1;
        }
        cookie = InspectorInstrumentation::willCallFunction(page, resourceName, lineNumber);
    }

    v8::Local<v8::Value> result;
    {
        V8RecursionScope recursionScope;
        result = function->Call(receiver, argc, args);
    }

    InspectorInstrumentation::didCallFunction(cookie);

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

    return result;
}

v8::Local<v8::Value> V8Proxy::newInstance(v8::Handle<v8::Function> constructor, int argc, v8::Handle<v8::Value> args[])
{
    // No artificial limitations on the depth of recursion, see comment in
    // V8Proxy::callFunction.
    v8::Local<v8::Value> result;
    {
        result = constructor->NewInstance(argc, args);
    }

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

    return result;
}

DOMWindow* V8Proxy::retrieveWindow(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    global = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), global);
    ASSERT(!global.IsEmpty());
    return V8DOMWindow::toNative(global);
}

Frame* V8Proxy::retrieveFrame(v8::Handle<v8::Context> context)
{
    DOMWindow* window = retrieveWindow(context);
    Frame* frame = window->frame();
    if (frame && frame->domWindow() == window)
        return frame;
    // We return 0 here because |context| is detached from the Frame.  If we
    // did return |frame| we could get in trouble because the frame could be
    // navigated to another security origin.
    return 0;
}

Frame* V8Proxy::retrieveFrameForEnteredContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetEntered();
    if (context.IsEmpty())
        return 0;
    return retrieveFrame(context);
}

Frame* V8Proxy::retrieveFrameForCurrentContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    if (context.IsEmpty())
        return 0;
    return retrieveFrame(context);
}

Frame* V8Proxy::retrieveFrameForCallingContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetCalling();
    if (context.IsEmpty())
        return 0;
    return retrieveFrame(context);
}

V8Proxy* V8Proxy::retrieve()
{
    DOMWindow* window = retrieveWindow(currentContext());
    ASSERT(window);
    return retrieve(window->frame());
}

V8Proxy* V8Proxy::retrieve(Frame* frame)
{
    return frame ? frame->script()->proxy() : 0;
}

V8Proxy* V8Proxy::retrieve(ScriptExecutionContext* context)
{
    if (!context || !context->isDocument())
        return 0;
    return retrieve(static_cast<Document*>(context)->frame());
}

void V8Proxy::resetIsolatedWorlds()
{
    for (IsolatedWorldMap::iterator iter = m_isolatedWorlds.begin();
         iter != m_isolatedWorlds.end(); ++iter) {
        iter->second->destroy();
    }
    m_isolatedWorlds.clear();
    m_isolatedWorldSecurityOrigins.clear();
}

void V8Proxy::clearForClose()
{
    resetIsolatedWorlds();
    windowShell()->clearForClose();
}

void V8Proxy::clearForNavigation()
{
    resetIsolatedWorlds();
    windowShell()->clearForNavigation();
}

#define TRY_TO_CREATE_EXCEPTION(interfaceName) \
    case interfaceName##Type: \
        exception = toV8(interfaceName::create(description)); \
        break;

void V8Proxy::setDOMException(int ec)
{
    if (ec <= 0)
        return;

    ExceptionCodeDescription description(ec);

    v8::Handle<v8::Value> exception;
    switch (description.type) {
        DOM_EXCEPTION_INTERFACES_FOR_EACH(TRY_TO_CREATE_EXCEPTION)
    }

    if (!exception.IsEmpty())
        v8::ThrowException(exception);
}

#undef TRY_TO_CREATE_EXCEPTION

v8::Handle<v8::Value> V8Proxy::throwError(ErrorType type, const char* message)
{
    switch (type) {
    case RangeError:
        return v8::ThrowException(v8::Exception::RangeError(v8String(message)));
    case ReferenceError:
        return v8::ThrowException(v8::Exception::ReferenceError(v8String(message)));
    case SyntaxError:
        return v8::ThrowException(v8::Exception::SyntaxError(v8String(message)));
    case TypeError:
        return v8::ThrowException(v8::Exception::TypeError(v8String(message)));
    case GeneralError:
        return v8::ThrowException(v8::Exception::Error(v8String(message)));
    default:
        ASSERT_NOT_REACHED();
        return notHandledByInterceptor();
    }
}

v8::Handle<v8::Value> V8Proxy::throwTypeError()
{
    return throwError(TypeError, "Type error");
}

v8::Handle<v8::Value> V8Proxy::throwSyntaxError()
{
    return throwError(SyntaxError, "Syntax error");
}

v8::Local<v8::Context> V8Proxy::context(Frame* frame)
{
    v8::Local<v8::Context> context = V8Proxy::mainWorldContext(frame);
    if (context.IsEmpty())
        return v8::Local<v8::Context>();

    if (V8IsolatedContext* isolatedContext = V8IsolatedContext::getEntered()) {
        context = v8::Local<v8::Context>::New(isolatedContext->context());
        if (frame != V8Proxy::retrieveFrame(context))
            return v8::Local<v8::Context>();
    }

    return context;
}

v8::Local<v8::Context> V8Proxy::context()
{
    if (V8IsolatedContext* isolatedContext = V8IsolatedContext::getEntered()) {
        RefPtr<SharedPersistent<v8::Context> > context = isolatedContext->sharedContext();
        if (m_frame != V8Proxy::retrieveFrame(context->get()))
            return v8::Local<v8::Context>();
        return v8::Local<v8::Context>::New(context->get());
    }
    return mainWorldContext();
}

v8::Local<v8::Context> V8Proxy::mainWorldContext()
{
    windowShell()->initContextIfNeeded();
    return v8::Local<v8::Context>::New(windowShell()->context());
}

v8::Local<v8::Context> V8Proxy::mainWorldContext(Frame* frame)
{
    V8Proxy* proxy = retrieve(frame);
    if (!proxy)
        return v8::Local<v8::Context>();

    return proxy->mainWorldContext();
}

v8::Local<v8::Context> V8Proxy::currentContext()
{
    return v8::Context::GetCurrent();
}

v8::Handle<v8::Value> V8Proxy::checkNewLegal(const v8::Arguments& args)
{
    if (ConstructorMode::current() == ConstructorMode::CreateNewObject)
        return throwError(TypeError, "Illegal constructor");

    return args.This();
}

void V8Proxy::registerExtensionWithV8(v8::Extension* extension)
{
    // If the extension exists in our list, it was already registered with V8.
    if (!registeredExtensionWithV8(extension))
        v8::RegisterExtension(extension);
}

bool V8Proxy::registeredExtensionWithV8(v8::Extension* extension)
{
    const V8Extensions& registeredExtensions = extensions();
    for (size_t i = 0; i < registeredExtensions.size(); ++i) {
        if (registeredExtensions[i] == extension)
            return true;
    }

    return false;
}

void V8Proxy::registerExtension(v8::Extension* extension)
{
    registerExtensionWithV8(extension);
    staticExtensionsList().append(extension);
}

const V8Extensions& V8Proxy::extensions()
{
    return staticExtensionsList();
}

bool V8Proxy::setContextDebugId(int debugId)
{
    ASSERT(debugId > 0);
    v8::HandleScope scope;
    v8::Handle<v8::Context> context = windowShell()->context();
    if (context.IsEmpty())
        return false;
    if (!context->GetData()->IsUndefined())
        return false;

    v8::Context::Scope contextScope(context);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "page,%d", debugId);
    context->SetData(v8::String::New(buffer));

    return true;
}

int V8Proxy::contextDebugId(v8::Handle<v8::Context> context)
{
    v8::HandleScope scope;
    if (!context->GetData()->IsString())
        return -1;
    v8::String::AsciiValue ascii(context->GetData());
    char* comma = strnstr(*ascii, ",", ascii.length());
    if (!comma)
        return -1;
    return atoi(comma + 1);
}

v8::Local<v8::Context> toV8Context(ScriptExecutionContext* context, const WorldContextHandle& worldContext)
{
    if (context->isDocument()) {
        if (V8Proxy* proxy = V8Proxy::retrieve(context)) {
            Frame* frame = static_cast<Document*>(context)->frame();
            if (frame->script()->canExecuteScripts(NotAboutToExecuteScript))
                return worldContext.adjustedContext(proxy);
        }
#if ENABLE(WORKERS)
    } else if (context->isWorkerContext()) {
        if (WorkerContextExecutionProxy* proxy = static_cast<WorkerContext*>(context)->script()->proxy())
            return proxy->context();
#endif
    }
    return v8::Local<v8::Context>();
}

}  // namespace WebCore
