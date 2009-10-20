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

#include "ChromiumBridge.h"
#include "CSSMutableStyleDeclaration.h"
#include "DateExtension.h"
#include "DOMObjectsInclude.h"
#include "DocumentLoader.h"
#include "FrameLoaderClient.h"
#include "Page.h"
#include "PageGroup.h"
#include "ScriptController.h"
#include "StorageNamespace.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8ConsoleMessage.h"
#include "V8CustomBinding.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8HiddenPropertyName.h"
#include "V8Index.h"
#include "V8IsolatedWorld.h"

#include <algorithm>
#include <utility>
#include <v8.h>
#include <v8-debug.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

v8::Persistent<v8::Context> V8Proxy::m_utilityContext;

// Static list of registered extensions
V8Extensions V8Proxy::m_extensions;

const char* V8Proxy::kContextDebugDataType = "type";
const char* V8Proxy::kContextDebugDataValue = "value";

void batchConfigureAttributes(v8::Handle<v8::ObjectTemplate> instance, v8::Handle<v8::ObjectTemplate> proto, const BatchedAttribute* attributes, size_t attributeCount)
{
    for (size_t i = 0; i < attributeCount; ++i)
        configureAttribute(instance, proto, attributes[i]);
}

void batchConfigureConstants(v8::Handle<v8::FunctionTemplate> functionDescriptor, v8::Handle<v8::ObjectTemplate> proto, const BatchedConstant* constants, size_t constantCount)
{
    for (size_t i = 0; i < constantCount; ++i) {
        const BatchedConstant* constant = &constants[i];
        functionDescriptor->Set(v8::String::New(constant->name), v8::Integer::New(constant->value), v8::ReadOnly);
        proto->Set(v8::String::New(constant->name), v8::Integer::New(constant->value), v8::ReadOnly);
    }
}

typedef HashMap<Node*, v8::Object*> DOMNodeMap;
typedef HashMap<void*, v8::Object*> DOMObjectMap;

#if ENABLE(SVG)
// Map of SVG objects with contexts to their contexts
static HashMap<void*, SVGElement*>& svgObjectToContextMap()
{
    typedef HashMap<void*, SVGElement*> SvgObjectToContextMap;
    DEFINE_STATIC_LOCAL(SvgObjectToContextMap, staticSvgObjectToContextMap, ());
    return staticSvgObjectToContextMap;
}

void V8Proxy::setSVGContext(void* object, SVGElement* context)
{
    if (!object)
        return;

    SVGElement* oldContext = svgObjectToContextMap().get(object);

    if (oldContext == context)
        return;

    if (oldContext)
        oldContext->deref();

    if (context)
        context->ref();

    svgObjectToContextMap().set(object, context);
}

SVGElement* V8Proxy::svgContext(void* object)
{
    return svgObjectToContextMap().get(object);
}

#endif

typedef HashMap<int, v8::FunctionTemplate*> FunctionTemplateMap;

bool AllowAllocation::m_current = false;

void logInfo(Frame* frame, const String& message, const String& url)
{
    Page* page = frame->page();
    if (!page)
        return;
    V8ConsoleMessage consoleMessage(message, url, 0);
    consoleMessage.dispatchNow(page);
}

enum DelayReporting {
    ReportLater,
    ReportNow
};

static void reportUnsafeAccessTo(Frame* target, DelayReporting delay)
{
    ASSERT(target);
    Document* targetDocument = target->document();
    if (!targetDocument)
        return;

    Frame* source = V8Proxy::retrieveFrameForEnteredContext();
    if (!source || !source->document())
        return; // Ignore error if the source document is gone.

    Document* sourceDocument = source->document();

    // FIXME: This error message should contain more specifics of why the same
    // origin check has failed.
    String str = String::format("Unsafe JavaScript attempt to access frame "
                                "with URL %s from frame with URL %s. "
                                "Domains, protocols and ports must match.\n",
                                targetDocument->url().string().utf8().data(),
                                sourceDocument->url().string().utf8().data());

    // Build a console message with fake source ID and line number.
    const String kSourceID = "";
    const int kLineNumber = 1;
    V8ConsoleMessage message(str, kSourceID, kLineNumber);

    if (delay == ReportNow) {
        // NOTE: Safari prints the message in the target page, but it seems like
        // it should be in the source page. Even for delayed messages, we put it in
        // the source page; see V8ConsoleMessage::processDelayed().
        message.dispatchNow(source->page());
    } else {
        ASSERT(delay == ReportLater);
        // We cannot safely report the message eagerly, because this may cause
        // allocations and GCs internally in V8 and we cannot handle that at this
        // point. Therefore we delay the reporting.
        message.dispatchLater();
    }
}

static void reportUnsafeJavaScriptAccess(v8::Local<v8::Object> host, v8::AccessType type, v8::Local<v8::Value> data)
{
    Frame* target = V8Custom::GetTargetFrame(host, data);
    if (target)
        reportUnsafeAccessTo(target, ReportLater);
}

static void handleFatalErrorInV8()
{
    // FIXME: We temporarily deal with V8 internal error situations
    // such as out-of-memory by crashing the renderer.
    CRASH();
}

static void reportFatalErrorInV8(const char* location, const char* message)
{
    // V8 is shutdown, we cannot use V8 api.
    // The only thing we can do is to disable JavaScript.
    // FIXME: clean up V8Proxy and disable JavaScript.
    printf("V8 error: %s (%s)\n", message, location);
    handleFatalErrorInV8();
}

V8Proxy::V8Proxy(Frame* frame)
    : m_frame(frame),
      m_context(SharedPersistent<v8::Context>::create()),
      m_listenerGuard(V8ListenerGuard::create()),
      m_inlineCode(false),
      m_timerCallback(false),
      m_recursion(0) { }

V8Proxy::~V8Proxy()
{
    clearForClose();
    destroyGlobal();
}

void V8Proxy::destroyGlobal()
{
    if (!m_global.IsEmpty()) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_global);
#endif
        m_global.Dispose();
        m_global.Clear();
    }
}

v8::Handle<v8::Script> V8Proxy::compileScript(v8::Handle<v8::String> code, const String& fileName, int baseLine)
{
    const uint16_t* fileNameString = fromWebCoreString(fileName);
    v8::Handle<v8::String> name = v8::String::New(fileNameString, fileName.length());
    v8::Handle<v8::Integer> line = v8::Integer::New(baseLine);
    v8::ScriptOrigin origin(name, line);
    v8::Handle<v8::Script> script = v8::Script::Compile(code, &origin);
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
    if (proxy) {
        // Clean m_context, and event handlers.
        proxy->clearForClose();

        proxy->destroyGlobal();
    }

    ChromiumBridge::notifyJSOutOfMemory(frame);

    // Disable JS.
    Settings* settings = frame->settings();
    ASSERT(settings);
    settings->setJavaScriptEnabled(false);

    return true;
}

void V8Proxy::evaluateInIsolatedWorld(int worldID, const Vector<ScriptSourceCode>& sources, int extensionGroup)
{
    initContextIfNeeded();

    v8::HandleScope handleScope;
    V8IsolatedWorld* world = 0;

    if (worldID > 0) {
        IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(worldID);
        if (iter != m_isolatedWorlds.end()) {
            world = iter->second;
        } else {
            world = new V8IsolatedWorld(this, extensionGroup);
            m_isolatedWorlds.set(worldID, world);

            // Setup context id for JS debugger.
            setInjectedScriptContextDebugId(world->context());
        }
    } else {
        world = new V8IsolatedWorld(this, extensionGroup);
    }

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(world->context());
    v8::Context::Scope context_scope(context);
    for (size_t i = 0; i < sources.size(); ++i)
      evaluate(sources[i], 0);

    if (worldID == 0)
      world->destroy();
}

void V8Proxy::evaluateInNewContext(const Vector<ScriptSourceCode>& sources, int extensionGroup)
{
    initContextIfNeeded();

    v8::HandleScope handleScope;

    // Set up the DOM window as the prototype of the new global object.
    v8::Handle<v8::Context> windowContext = context();
    v8::Handle<v8::Object> windowGlobal = windowContext->Global();
    v8::Handle<v8::Object> windowWrapper = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, windowGlobal);

    ASSERT(V8DOMWrapper::convertDOMWrapperToNative<DOMWindow>(windowWrapper) == m_frame->domWindow());

    v8::Persistent<v8::Context> context = createNewContext(v8::Handle<v8::Object>(), extensionGroup);
    v8::Context::Scope contextScope(context);

    // Setup context id for JS debugger.
    setInjectedScriptContextDebugId(context);

    v8::Handle<v8::Object> global = context->Global();

    v8::Handle<v8::String> implicitProtoString = v8::String::New("__proto__");
    global->Set(implicitProtoString, windowWrapper);

    // Give the code running in the new context a way to get access to the
    // original context.
    global->Set(v8::String::New("contentWindow"), windowGlobal);

    m_frame->loader()->client()->didCreateIsolatedScriptContext();

    // Run code in the new context.
    for (size_t i = 0; i < sources.size(); ++i)
        evaluate(sources[i], 0);

    // Using the default security token means that the canAccess is always
    // called, which is slow.
    // FIXME: Use tokens where possible. This will mean keeping track of all
    // created contexts so that they can all be updated when the document domain
    // changes.
    context->UseDefaultSecurityToken();
    context.Dispose();
}

void V8Proxy::setInjectedScriptContextDebugId(v8::Handle<v8::Context> targetContext)
{
    // Setup context id for JS debugger.
    v8::Context::Scope contextScope(targetContext);
    v8::Handle<v8::Object> contextData = v8::Object::New();

    v8::Handle<v8::Value> windowContextData = context()->GetData();
    if (windowContextData->IsObject()) {
        v8::Handle<v8::String> propertyName = v8::String::New(kContextDebugDataValue);
        contextData->Set(propertyName, v8::Object::Cast(*windowContextData)->Get(propertyName));
    }
    contextData->Set(v8::String::New(kContextDebugDataType), v8::String::New("injected"));
    targetContext->SetData(contextData);
}

v8::Local<v8::Value> V8Proxy::evaluate(const ScriptSourceCode& source, Node* node)
{
    ASSERT(v8::Context::InContext());

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
        ChromiumBridge::traceEventBegin("v8.compile", node, "");

        // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
        // 1, whereas v8 starts at 0.
        v8::Handle<v8::Script> script = compileScript(code, source.url(), source.startLine() - 1);
        ChromiumBridge::traceEventEnd("v8.compile", node, "");

        ChromiumBridge::traceEventBegin("v8.run", node, "");
        // Set inlineCode to true for <a href="javascript:doSomething()">
        // and false for <script>doSomething</script>. We make a rough guess at
        // this based on whether the script source has a URL.
        result = runScript(script, source.url().string().isNull());
    }
    ChromiumBridge::traceEventEnd("v8.run", node, "");
    return result;
}

v8::Local<v8::Value> V8Proxy::runScript(v8::Handle<v8::Script> script, bool isInlineCode)
{
    if (script.IsEmpty())
        return notHandledByInterceptor();

    // Compute the source string and prevent against infinite recursion.
    if (m_recursion >= kMaxRecursionDepth) {
        v8::Local<v8::String> code = v8ExternalString("throw RangeError('Recursion too deep')");
        // FIXME: Ideally, we should be able to re-use the origin of the
        // script passed to us as the argument instead of using an empty string
        // and 0 baseLine.
        script = compileScript(code, "", 0);
    }

    if (handleOutOfMemory())
        ASSERT(script.IsEmpty());

    if (script.IsEmpty())
        return notHandledByInterceptor();

    // Save the previous value of the inlineCode flag and update the flag for
    // the duration of the script invocation.
    bool previousInlineCode = inlineCode();
    setInlineCode(isInlineCode);

    // Run the script and keep track of the current recursion depth.
    v8::Local<v8::Value> result;
    {
        V8ConsoleMessage::Scope scope;

        // See comment in V8Proxy::callFunction.
        m_frame->keepAlive();

        m_recursion++;
        result = script->Run();
        m_recursion--;
    }

    // Release the storage mutex if applicable.
    releaseStorageMutex();

    if (handleOutOfMemory())
        ASSERT(result.IsEmpty());

    // Handle V8 internal error situation (Out-of-memory).
    if (result.IsEmpty())
        return notHandledByInterceptor();

    // Restore inlineCode flag.
    setInlineCode(previousInlineCode);

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

    return result;
}

v8::Local<v8::Value> V8Proxy::callFunction(v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> args[])
{
    v8::Local<v8::Value> result;
    {
        V8ConsoleMessage::Scope scope;

        if (m_recursion >= kMaxRecursionDepth) {
            v8::Local<v8::String> code = v8::String::New("throw new RangeError('Maximum call stack size exceeded.')");
            if (code.IsEmpty())
                return result;
            v8::Local<v8::Script> script = v8::Script::Compile(code);
            if (script.IsEmpty())
                return result;
            script->Run();
            return result;
        }

        // Evaluating the JavaScript could cause the frame to be deallocated,
        // so we start the keep alive timer here.
        // Frame::keepAlive method adds the ref count of the frame and sets a
        // timer to decrease the ref count. It assumes that the current JavaScript
        // execution finishs before firing the timer.
        m_frame->keepAlive();

        m_recursion++;
        result = function->Call(receiver, argc, args);
        m_recursion--;
    }

    // Release the storage mutex if applicable.
    releaseStorageMutex();

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
        V8ConsoleMessage::Scope scope;

        // See comment in V8Proxy::callFunction.
        m_frame->keepAlive();

        result = constructor->NewInstance(argc, args);
    }

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

    return result;
}

v8::Local<v8::Object> V8Proxy::createWrapperFromCacheSlowCase(V8ClassIndex::V8WrapperType type)
{
    // Not in cache.
    int classIndex = V8ClassIndex::ToInt(type);
    initContextIfNeeded();
    v8::Context::Scope scope(context());
    v8::Local<v8::Function> function = V8DOMWrapper::getConstructor(type, getHiddenObjectPrototype(context()));
    v8::Local<v8::Object> instance = SafeAllocation::newInstance(function);
    if (!instance.IsEmpty()) {
        m_wrapperBoilerplates->Set(v8::Integer::New(classIndex), instance);
        return instance->Clone();
    }
    return notHandledByInterceptor();
}

bool V8Proxy::isContextInitialized()
{
    // m_context, m_global, and m_wrapperBoilerplates should
    // all be non-empty if if m_context is non-empty.
    ASSERT(context().IsEmpty() || !m_global.IsEmpty());
    ASSERT(context().IsEmpty() || !m_wrapperBoilerplates.IsEmpty());
    return !context().IsEmpty();
}

DOMWindow* V8Proxy::retrieveWindow(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    global = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, global);
    ASSERT(!global.IsEmpty());
    return V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, global);
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
    if (!frame)
        return 0;
    return frame->script()->isEnabled() ? frame->script()->proxy() : 0;
}

V8Proxy* V8Proxy::retrieve(ScriptExecutionContext* context)
{
    if (!context->isDocument())
        return 0;
    return retrieve(static_cast<Document*>(context)->frame());
}

void V8Proxy::disconnectFrame()
{
    disconnectEventListeners();
}

bool V8Proxy::isEnabled()
{
    Settings* settings = m_frame->settings();
    if (!settings)
        return false;

    // In the common case, JavaScript is enabled and we're done.
    if (settings->isJavaScriptEnabled())
        return true;

    // If JavaScript has been disabled, we need to look at the frame to tell
    // whether this script came from the web or the embedder. Scripts from the
    // embedder are safe to run, but scripts from the other sources are
    // disallowed.
    Document* document = m_frame->document();
    if (!document)
        return false;

    SecurityOrigin* origin = document->securityOrigin();
    if (origin->protocol().isEmpty())
        return false; // Uninitialized document

    if (origin->protocol() == "http" || origin->protocol() == "https")
        return false; // Web site

    // FIXME: the following are application decisions, and they should
    // not be made at this layer. instead, we should bridge out to the
    // embedder to allow them to override policy here.

    if (origin->protocol() == ChromiumBridge::uiResourceProtocol())
        return true;   // Embedder's scripts are ok to run

    // If the scheme is ftp: or file:, an empty file name indicates a directory
    // listing, which requires JavaScript to function properly.
    const char* kDirProtocols[] = { "ftp", "file" };
    for (size_t i = 0; i < arraysize(kDirProtocols); ++i) {
        if (origin->protocol() == kDirProtocols[i]) {
            const KURL& url = document->url();
            return url.pathAfterLastSlash() == url.pathEnd();
        }
    }

    return false; // Other protocols fall through to here
}

void V8Proxy::updateDocumentWrapper(v8::Handle<v8::Value> wrapper)
{
    clearDocumentWrapper();

    ASSERT(m_document.IsEmpty());
    m_document = v8::Persistent<v8::Value>::New(wrapper);
#ifndef NDEBUG
    V8GCController::registerGlobalHandle(PROXY, this, m_document);
#endif
}

void V8Proxy::clearDocumentWrapper()
{
    if (!m_document.IsEmpty()) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_document);
#endif
        m_document.Dispose();
        m_document.Clear();
    }
}

void V8Proxy::updateDocumentWrapperCache()
{
    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(context());

    // If the document has no frame, NodeToV8Object might get the
    // document wrapper for a document that is about to be deleted.
    // If the ForceSet below causes a garbage collection, the document
    // might get deleted and the global handle for the document
    // wrapper cleared. Using the cleared global handle will lead to
    // crashes. In this case we clear the cache and let the DOMWindow
    // accessor handle access to the document.
    if (!m_frame->document()->frame()) {
        clearDocumentWrapperCache();
        return;
    }

    v8::Handle<v8::Value> documentWrapper = V8DOMWrapper::convertNodeToV8Object(m_frame->document());

    // If instantiation of the document wrapper fails, clear the cache
    // and let the DOMWindow accessor handle access to the document.
    if (documentWrapper.IsEmpty()) {
        clearDocumentWrapperCache();
        return;
    }
    context()->Global()->ForceSet(v8::String::New("document"), documentWrapper, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));
}

void V8Proxy::clearDocumentWrapperCache()
{
    ASSERT(!context().IsEmpty());
    context()->Global()->ForceDelete(v8::String::New("document"));
}

void V8Proxy::disposeContextHandles()
{
    if (!context().IsEmpty()) {
        m_frame->loader()->client()->didDestroyScriptContextForFrame();
        shared_context()->disposeHandle();
    }

    if (!m_wrapperBoilerplates.IsEmpty()) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_wrapperBoilerplates);
#endif
        m_wrapperBoilerplates.Dispose();
        m_wrapperBoilerplates.Clear();
    }
}

void V8Proxy::releaseStorageMutex()
{
    // If we've just left a top level script context and local storage has been
    // instantiated, we must ensure that any storage locks have been freed.
    // Per http://dev.w3.org/html5/spec/Overview.html#storage-mutex
    if (m_recursion != 0)
        return;
    Page* page = m_frame->page();
    if (!page)
        return;
    if (page->group().hasLocalStorage())
        page->group().localStorage()->unlock();
}

void V8Proxy::disconnectEventListeners()
{
    m_listenerGuard->disconnectListeners();
    m_listenerGuard = V8ListenerGuard::create();
}
    
void V8Proxy::resetIsolatedWorlds()
{
    for (IsolatedWorldMap::iterator iter = m_isolatedWorlds.begin();
         iter != m_isolatedWorlds.end(); ++iter) {
        iter->second->destroy();
    }
    m_isolatedWorlds.clear();
}

void V8Proxy::clearForClose()
{
    resetIsolatedWorlds();

    if (!context().IsEmpty()) {
        v8::HandleScope handleScope;

        clearDocumentWrapper();
        disposeContextHandles();
    }
}

void V8Proxy::clearForNavigation()
{
    disconnectEventListeners();
    resetIsolatedWorlds();

    if (!context().IsEmpty()) {
        v8::HandleScope handle;
        clearDocumentWrapper();

        v8::Context::Scope contextScope(context());

        // Clear the document wrapper cache before turning on access checks on
        // the old DOMWindow wrapper. This way, access to the document wrapper
        // will be protected by the security checks on the DOMWindow wrapper.
        clearDocumentWrapperCache();

        // Turn on access check on the old DOMWindow wrapper.
        v8::Handle<v8::Object> wrapper = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, m_global);
        ASSERT(!wrapper.IsEmpty());
        wrapper->TurnOnAccessCheck();

        // Separate the context from its global object.
        context()->DetachGlobal();

        disposeContextHandles();
    }
}

void V8Proxy::setSecurityToken()
{
    Document* document = m_frame->document();
    // Setup security origin and security token.
    if (!document) {
        context()->UseDefaultSecurityToken();
        return;
    }

    // Ask the document's SecurityOrigin to generate a security token.
    // If two tokens are equal, then the SecurityOrigins canAccess each other.
    // If two tokens are not equal, then we have to call canAccess.
    // Note: we can't use the HTTPOrigin if it was set from the DOM.
    SecurityOrigin* origin = document->securityOrigin();
    String token;
    if (!origin->domainWasSetInDOM())
        token = document->securityOrigin()->toString();

    // An empty or "null" token means we always have to call
    // canAccess. The toString method on securityOrigins returns the
    // string "null" for empty security origins and for security
    // origins that should only allow access to themselves. In this
    // case, we use the global object as the security token to avoid
    // calling canAccess when a script accesses its own objects.
    if (token.isEmpty() || token == "null") {
        context()->UseDefaultSecurityToken();
        return;
    }

    CString utf8Token = token.utf8();
    // NOTE: V8 does identity comparison in fast path, must use a symbol
    // as the security token.
    context()->SetSecurityToken(v8::String::NewSymbol(utf8Token.data(), utf8Token.length()));
}

void V8Proxy::updateDocument()
{
    if (!m_frame->document())
        return;

    if (m_global.IsEmpty())
        return;

    // There is an existing JavaScript wrapper for the global object
    // of this frame. JavaScript code in other frames might hold a
    // reference to this wrapper. We eagerly initialize the JavaScript
    // context for the new document to make property access on the
    // global object wrapper succeed.
    initContextIfNeeded();

    // Bail out if context initialization failed.
    if (context().IsEmpty())
        return;

    // We have a new document and we need to update the cache.
    updateDocumentWrapperCache();

    updateSecurityOrigin();
}

void V8Proxy::updateSecurityOrigin()
{
    v8::HandleScope scope;
    setSecurityToken();
}

// Same origin policy implementation:
//
// Same origin policy prevents JS code from domain A access JS & DOM objects
// in a different domain B. There are exceptions and several objects are
// accessible by cross-domain code. For example, the window.frames object is
// accessible by code from a different domain, but window.document is not.
//
// The binding code sets security check callbacks on a function template,
// and accessing instances of the template calls the callback function.
// The callback function checks same origin policy.
//
// Callback functions are expensive. V8 uses a security token string to do
// fast access checks for the common case where source and target are in the
// same domain. A security token is a string object that represents
// the protocol/url/port of a domain.
//
// There are special cases where a security token matching is not enough.
// For example, JavaScript can set its domain to a super domain by calling
// document.setDomain(...). In these cases, the binding code can reset
// a context's security token to its global object so that the fast access
// check will always fail.

// Check if the current execution context can access a target frame.
// First it checks same domain policy using the lexical context
//
// This is equivalent to KJS::Window::allowsAccessFrom(ExecState*, String&).
bool V8Proxy::canAccessPrivate(DOMWindow* targetWindow)
{
    ASSERT(targetWindow);

    String message;

    DOMWindow* originWindow = retrieveWindow(currentContext());
    if (originWindow == targetWindow)
        return true;

    if (!originWindow)
        return false;

    const SecurityOrigin* activeSecurityOrigin = originWindow->securityOrigin();
    const SecurityOrigin* targetSecurityOrigin = targetWindow->securityOrigin();

    // We have seen crashes were the security origin of the target has not been
    // initialized. Defend against that.
    if (!targetSecurityOrigin)
        return false;

    if (activeSecurityOrigin->canAccess(targetSecurityOrigin))
        return true;

    // Allow access to a "about:blank" page if the dynamic context is a
    // detached context of the same frame as the blank page.
    if (targetSecurityOrigin->isEmpty() && originWindow->frame() == targetWindow->frame())
        return true;

    return false;
}

bool V8Proxy::canAccessFrame(Frame* target, bool reportError)
{
    // The subject is detached from a frame, deny accesses.
    if (!target)
        return false;

    if (!canAccessPrivate(target->domWindow())) {
        if (reportError)
            reportUnsafeAccessTo(target, ReportNow);
        return false;
    }
    return true;
}

bool V8Proxy::checkNodeSecurity(Node* node)
{
    if (!node)
        return false;

    Frame* target = node->document()->frame();

    if (!target)
        return false;

    return canAccessFrame(target, true);
}

v8::Persistent<v8::Context> V8Proxy::createNewContext(v8::Handle<v8::Object> global, int extensionGroup)
{
    v8::Persistent<v8::Context> result;

    // The activeDocumentLoader pointer could be NULL during frame shutdown.
    if (!m_frame->loader()->activeDocumentLoader())
        return result;

    // Create a new environment using an empty template for the shadow
    // object. Reuse the global object if one has been created earlier.
    v8::Persistent<v8::ObjectTemplate> globalTemplate = V8DOMWindow::GetShadowObjectTemplate();
    if (globalTemplate.IsEmpty())
        return result;

    // Install a security handler with V8.
    globalTemplate->SetAccessCheckCallbacks(V8Custom::v8DOMWindowNamedSecurityCheck, V8Custom::v8DOMWindowIndexedSecurityCheck, v8::Integer::New(V8ClassIndex::DOMWINDOW));
    globalTemplate->SetInternalFieldCount(V8Custom::kDOMWindowInternalFieldCount);

    // Used to avoid sleep calls in unload handlers.
    if (!registeredExtensionWithV8(DateExtension::get()))
        registerExtension(DateExtension::get(), String());

    // Dynamically tell v8 about our extensions now.
    OwnArrayPtr<const char*> extensionNames(new const char*[m_extensions.size()]);
    int index = 0;
    for (size_t i = 0; i < m_extensions.size(); ++i) {
        if (m_extensions[i].group && m_extensions[i].group != extensionGroup)
            continue;

        // Note: we check the loader URL here instead of the document URL
        // because we might be currently loading an URL into a blank page.
        // See http://code.google.com/p/chromium/issues/detail?id=10924
        if (m_extensions[i].scheme.length() > 0 && (m_extensions[i].scheme != m_frame->loader()->activeDocumentLoader()->url().protocol() || m_extensions[i].scheme != m_frame->page()->mainFrame()->loader()->activeDocumentLoader()->url().protocol()))
            continue;

        extensionNames[index++] = m_extensions[i].extension->name();
    }
    v8::ExtensionConfiguration extensions(index, extensionNames.get());
    result = v8::Context::New(&extensions, globalTemplate, global);

    return result;
}

bool V8Proxy::installDOMWindow(v8::Handle<v8::Context> context, DOMWindow* window)
{
    v8::Handle<v8::String> implicitProtoString = v8::String::New("__proto__");
    if (implicitProtoString.IsEmpty())
        return false;

    // Create a new JS window object and use it as the prototype for the  shadow global object.
    v8::Handle<v8::Function> windowConstructor = V8DOMWrapper::getConstructor(V8ClassIndex::DOMWINDOW, getHiddenObjectPrototype(context));
    v8::Local<v8::Object> jsWindow = SafeAllocation::newInstance(windowConstructor);
    // Bail out if allocation failed.
    if (jsWindow.IsEmpty())
        return false;

    // Wrap the window.
    V8DOMWrapper::setDOMWrapper(jsWindow, V8ClassIndex::ToInt(V8ClassIndex::DOMWINDOW), window);
    V8DOMWrapper::setDOMWrapper(v8::Handle<v8::Object>::Cast(jsWindow->GetPrototype()), V8ClassIndex::ToInt(V8ClassIndex::DOMWINDOW), window);

    window->ref();
    V8DOMWrapper::setJSWrapperForDOMObject(window, v8::Persistent<v8::Object>::New(jsWindow));

    // Insert the window instance as the prototype of the shadow object.
    v8::Handle<v8::Object> v8Global = context->Global();
    V8DOMWrapper::setDOMWrapper(v8::Handle<v8::Object>::Cast(v8Global->GetPrototype()), V8ClassIndex::ToInt(V8ClassIndex::DOMWINDOW), window);
    v8Global->Set(implicitProtoString, jsWindow);
    return true;
}

// Create a new environment and setup the global object.
//
// The global object corresponds to a DOMWindow instance. However, to
// allow properties of the JS DOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS DOMWindow
// instance as the prototype for that shadow object. The JS DOMWindow
// instance is undetectable from javascript code because the __proto__
// accessors skip that object.
//
// The shadow object and the DOMWindow instance are seen as one object
// from javascript. The javascript object that corresponds to a
// DOMWindow instance is the shadow object. When mapping a DOMWindow
// instance to a V8 object, we return the shadow object.
//
// To implement split-window, see
//   1) https://bugs.webkit.org/show_bug.cgi?id=17249
//   2) https://wiki.mozilla.org/Gecko:SplitWindow
//   3) https://bugzilla.mozilla.org/show_bug.cgi?id=296639
// we need to split the shadow object further into two objects:
// an outer window and an inner window. The inner window is the hidden
// prototype of the outer window. The inner window is the default
// global object of the context. A variable declared in the global
// scope is a property of the inner window.
//
// The outer window sticks to a Frame, it is exposed to JavaScript
// via window.window, window.self, window.parent, etc. The outer window
// has a security token which is the domain. The outer window cannot
// have its own properties. window.foo = 'x' is delegated to the
// inner window.
//
// When a frame navigates to a new page, the inner window is cut off
// the outer window, and the outer window identify is preserved for
// the frame. However, a new inner window is created for the new page.
// If there are JS code holds a closure to the old inner window,
// it won't be able to reach the outer window via its global object.
void V8Proxy::initContextIfNeeded()
{
    // Bail out if the context has already been initialized.
    if (!context().IsEmpty())
        return;

    // Create a handle scope for all local handles.
    v8::HandleScope handleScope;

    // Setup the security handlers and message listener. This only has
    // to be done once.
    static bool isV8Initialized = false;
    if (!isV8Initialized) {
        // Tells V8 not to call the default OOM handler, binding code
        // will handle it.
        v8::V8::IgnoreOutOfMemoryException();
        v8::V8::SetFatalErrorHandler(reportFatalErrorInV8);

        v8::V8::SetGlobalGCPrologueCallback(&V8GCController::gcPrologue);
        v8::V8::SetGlobalGCEpilogueCallback(&V8GCController::gcEpilogue);

        v8::V8::AddMessageListener(&V8ConsoleMessage::handler);

        v8::V8::SetFailedAccessCheckCallbackFunction(reportUnsafeJavaScriptAccess);

        isV8Initialized = true;
    }


    v8::Persistent<v8::Context> context = createNewContext(m_global, 0);
    if (context.IsEmpty())
        return;
    m_context->set(context);


    v8::Context::Scope contextScope(context);

    // Store the first global object created so we can reuse it.
    if (m_global.IsEmpty()) {
        m_global = v8::Persistent<v8::Object>::New(context->Global());
        // Bail out if allocation of the first global objects fails.
        if (m_global.IsEmpty()) {
            disposeContextHandles();
            return;
        }
#ifndef NDEBUG
        V8GCController::registerGlobalHandle(PROXY, this, m_global);
#endif
    }

    installHiddenObjectPrototype(context);
    m_wrapperBoilerplates = v8::Persistent<v8::Array>::New(v8::Array::New(V8ClassIndex::WRAPPER_TYPE_COUNT));
    // Bail out if allocation failed.
    if (m_wrapperBoilerplates.IsEmpty()) {
        disposeContextHandles();
        return;
    }
#ifndef NDEBUG
    V8GCController::registerGlobalHandle(PROXY, this, m_wrapperBoilerplates);
#endif

    if (!installDOMWindow(context, m_frame->domWindow()))
        disposeContextHandles();

    updateDocument();

    setSecurityToken();

    m_frame->loader()->client()->didCreateScriptContextForFrame();
    m_frame->loader()->dispatchWindowObjectAvailable();
}

void V8Proxy::setDOMException(int exceptionCode)
{
    if (exceptionCode <= 0)
        return;

    ExceptionCodeDescription description;
    getExceptionCodeDescription(exceptionCode, description);

    v8::Handle<v8::Value> exception;
    switch (description.type) {
    case DOMExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::DOMCOREEXCEPTION, DOMCoreException::create(description));
        break;
    case RangeExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::RANGEEXCEPTION, RangeException::create(description));
        break;
    case EventExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::EVENTEXCEPTION, EventException::create(description));
        break;
    case XMLHttpRequestExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::XMLHTTPREQUESTEXCEPTION, XMLHttpRequestException::create(description));
        break;
#if ENABLE(SVG)
    case SVGExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::SVGEXCEPTION, SVGException::create(description));
        break;
#endif
#if ENABLE(XPATH)
    case XPathExceptionType:
        exception = V8DOMWrapper::convertToV8Object(V8ClassIndex::XPATHEXCEPTION, XPathException::create(description));
        break;
#endif
    }

    ASSERT(!exception.IsEmpty());
    v8::ThrowException(exception);
}

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

v8::Local<v8::Context> V8Proxy::context(Frame* frame)
{
    v8::Local<v8::Context> context = V8Proxy::mainWorldContext(frame);
    if (context.IsEmpty())
        return v8::Local<v8::Context>();

    if (V8IsolatedWorld* world = V8IsolatedWorld::getEntered()) {
        context = v8::Local<v8::Context>::New(world->context());
        if (frame != V8Proxy::retrieveFrame(context))
            return v8::Local<v8::Context>();
    }

    return context;
}

PassRefPtr<SharedPersistent<v8::Context> > V8Proxy::shared_context(Frame* frame)
{
    V8Proxy *proxy = V8Proxy::retrieve(frame);
    if (!proxy)
        return 0;

    proxy->initContextIfNeeded();
    RefPtr<SharedPersistent<v8::Context> > context = proxy->shared_context();
    if (V8IsolatedWorld* world = V8IsolatedWorld::getEntered()) {
        context = world->shared_context();
        if (frame != V8Proxy::retrieveFrame(context->get()))
            return 0;
    }

    return context;
}

v8::Local<v8::Context> V8Proxy::mainWorldContext(Frame* frame)
{
    V8Proxy* proxy = retrieve(frame);
    if (!proxy)
        return v8::Local<v8::Context>();

    proxy->initContextIfNeeded();
    return v8::Local<v8::Context>::New(proxy->context());
}

v8::Local<v8::Context> V8Proxy::currentContext()
{
    return v8::Context::GetCurrent();
}

v8::Handle<v8::Value> V8Proxy::checkNewLegal(const v8::Arguments& args)
{
    if (!AllowAllocation::m_current)
        return throwError(TypeError, "Illegal constructor");

    return args.This();
}

void V8Proxy::bindJsObjectToWindow(Frame* frame, const char* name, int type, v8::Handle<v8::FunctionTemplate> descriptor, void* impl)
{
    // Get environment.
    v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(frame);
    if (v8Context.IsEmpty())
        return; // JS not enabled.

    v8::Context::Scope scope(v8Context);
    v8::Handle<v8::Object> instance = descriptor->GetFunction();
    V8DOMWrapper::setDOMWrapper(instance, type, impl);

    v8::Handle<v8::Object> global = v8Context->Global();
    global->Set(v8::String::New(name), instance);
}

void V8Proxy::processConsoleMessages()
{
    V8ConsoleMessage::processDelayed();
}

// Create the utility context for holding JavaScript functions used internally
// which are not visible to JavaScript executing on the page.
void V8Proxy::createUtilityContext()
{
    ASSERT(m_utilityContext.IsEmpty());

    v8::HandleScope scope;
    v8::Handle<v8::ObjectTemplate> globalTemplate = v8::ObjectTemplate::New();
    m_utilityContext = v8::Context::New(0, globalTemplate);
    v8::Context::Scope contextScope(m_utilityContext);

    // Compile JavaScript function for retrieving the source line of the top
    // JavaScript stack frame.
    DEFINE_STATIC_LOCAL(const char*, frameSourceLineSource,
        ("function frameSourceLine(exec_state) {"
        "  return exec_state.frame(0).sourceLine();"
        "}"));
    v8::Script::Compile(v8::String::New(frameSourceLineSource))->Run();

    // Compile JavaScript function for retrieving the source name of the top
    // JavaScript stack frame.
    DEFINE_STATIC_LOCAL(const char*, frameSourceNameSource,
        ("function frameSourceName(exec_state) {"
        "  var frame = exec_state.frame(0);"
        "  if (frame.func().resolved() && "
        "      frame.func().script() && "
        "      frame.func().script().name()) {"
        "    return frame.func().script().name();"
        "  }"
        "}"));
    v8::Script::Compile(v8::String::New(frameSourceNameSource))->Run();
}

int V8Proxy::sourceLineNumber()
{
    v8::HandleScope scope;
    v8::Handle<v8::Context> v8UtilityContext = V8Proxy::utilityContext();
    if (v8UtilityContext.IsEmpty())
        return 0;
    v8::Context::Scope contextScope(v8UtilityContext);
    v8::Handle<v8::Function> frameSourceLine;
    frameSourceLine = v8::Local<v8::Function>::Cast(v8UtilityContext->Global()->Get(v8::String::New("frameSourceLine")));
    if (frameSourceLine.IsEmpty())
        return 0;
    v8::Handle<v8::Value> result = v8::Debug::Call(frameSourceLine);
    if (result.IsEmpty())
        return 0;
    return result->Int32Value();
}

String V8Proxy::sourceName()
{
    v8::HandleScope scope;
    v8::Handle<v8::Context> v8UtilityContext = utilityContext();
    if (v8UtilityContext.IsEmpty())
        return String();
    v8::Context::Scope contextScope(v8UtilityContext);
    v8::Handle<v8::Function> frameSourceName;
    frameSourceName = v8::Local<v8::Function>::Cast(v8UtilityContext->Global()->Get(v8::String::New("frameSourceName")));
    if (frameSourceName.IsEmpty())
        return String();
    return toWebCoreString(v8::Debug::Call(frameSourceName));
}

void V8Proxy::registerExtensionWithV8(v8::Extension* extension)
{
    // If the extension exists in our list, it was already registered with V8.
    if (!registeredExtensionWithV8(extension))
        v8::RegisterExtension(extension);
}

bool V8Proxy::registeredExtensionWithV8(v8::Extension* extension)
{
    for (size_t i = 0; i < m_extensions.size(); ++i) {
        if (m_extensions[i].extension == extension)
            return true;
    }

    return false;
}

void V8Proxy::registerExtension(v8::Extension* extension, const String& schemeRestriction)
{
    registerExtensionWithV8(extension);
    V8ExtensionInfo info = {schemeRestriction, 0, extension};
    m_extensions.append(info);
}

void V8Proxy::registerExtension(v8::Extension* extension, int extensionGroup)
{
    registerExtensionWithV8(extension);
    V8ExtensionInfo info = {String(), extensionGroup, extension};
    m_extensions.append(info);
}

bool V8Proxy::setContextDebugId(int debugId)
{
    ASSERT(debugId > 0);
    if (context().IsEmpty())
        return false;
    v8::HandleScope scope;
    if (!context()->GetData()->IsUndefined())
        return false;

    v8::Context::Scope contextScope(context());
    v8::Handle<v8::Object> contextData = v8::Object::New();
    contextData->Set(v8::String::New(kContextDebugDataType), v8::String::New("page"));
    contextData->Set(v8::String::New(kContextDebugDataValue), v8::Integer::New(debugId));
    context()->SetData(contextData);
    return true;
}

int V8Proxy::contextDebugId(v8::Handle<v8::Context> context)
{
    v8::HandleScope scope;
    if (!context->GetData()->IsObject())
        return -1;
    v8::Handle<v8::Value> data = context->GetData()->ToObject()->Get( v8::String::New(kContextDebugDataValue));
    return data->IsInt32() ? data->Int32Value() : -1;
}

v8::Handle<v8::Value> V8Proxy::getHiddenObjectPrototype(v8::Handle<v8::Context> context)
{
    return context->Global()->GetHiddenValue(V8HiddenPropertyName::objectPrototype());
}

void V8Proxy::installHiddenObjectPrototype(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::String> objectString = v8::String::New("Object");
    v8::Handle<v8::String> prototypeString = v8::String::New("prototype");
    v8::Handle<v8::String> hiddenObjectPrototypeString = V8HiddenPropertyName::objectPrototype();
    // Bail out if allocation failed.
    if (objectString.IsEmpty() || prototypeString.IsEmpty() || hiddenObjectPrototypeString.IsEmpty())
        return;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(context->Global()->Get(objectString));
    v8::Handle<v8::Value> objectPrototype = object->Get(prototypeString);

    context->Global()->SetHiddenValue(hiddenObjectPrototypeString, objectPrototype);
}

}  // namespace WebCore
