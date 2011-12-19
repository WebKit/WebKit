/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
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
#include "V8DOMWindowShell.h"

#include "PlatformSupport.h"
#include "CSSMutableStyleDeclaration.h"
#include "DateExtension.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "Page.h"
#include "PageGroup.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include "ScriptProfiler.h"
#include "SecurityOrigin.h"
#include "StorageNamespace.h"
#include "V8Binding.h"
#include "V8BindingState.h"
#include "V8Collection.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8Document.h"
#include "V8GCForContextDispose.h"
#include "V8HTMLDocument.h"
#include "V8HiddenPropertyName.h"
#include "V8History.h"
#include "V8Location.h"
#include "V8Proxy.h"
#include "WorkerContextExecutionProxy.h"

#include <algorithm>
#include <stdio.h>
#include <utility>
#include <v8-debug.h>
#include <v8.h>

#if ENABLE(JAVASCRIPT_I18N_API)
#include <v8-i18n/include/extension.h>
#endif

#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

namespace WebCore {

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
    int memoryUsageMB = -1;
#if PLATFORM(CHROMIUM)
    memoryUsageMB = PlatformSupport::actualMemoryUsageMB();
#endif
    printf("V8 error: %s (%s).  Current memory usage: %d MB\n", message, location, memoryUsageMB);
    handleFatalErrorInV8();
}

static void v8UncaughtExceptionHandler(v8::Handle<v8::Message> message, v8::Handle<v8::Value> data)
{
    // Use the frame where JavaScript is called from.
    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    if (!frame)
        return;

    v8::Handle<v8::String> errorMessageString = message->Get();
    ASSERT(!errorMessageString.IsEmpty());
    String errorMessage = toWebCoreString(errorMessageString);

    v8::Handle<v8::StackTrace> stackTrace = message->GetStackTrace();
    RefPtr<ScriptCallStack> callStack;
    // Currently stack trace is only collected when inspector is open.
    if (!stackTrace.IsEmpty() && stackTrace->GetFrameCount() > 0)
        callStack = createScriptCallStack(stackTrace, ScriptCallStack::maxCallStackSizeToCapture);

    v8::Handle<v8::Value> resourceName = message->GetScriptResourceName();
    bool useURL = resourceName.IsEmpty() || !resourceName->IsString();
    Document* document = frame->document();
    String resourceNameString = useURL ? document->url() : toWebCoreString(resourceName);
    document->reportException(errorMessage, message->GetLineNumber(), resourceNameString, callStack);
}

// Returns the owner frame pointer of a DOM wrapper object. It only works for
// these DOM objects requiring cross-domain access check.
static Frame* getTargetFrame(v8::Local<v8::Object> host, v8::Local<v8::Value> data)
{
    Frame* target = 0;
    WrapperTypeInfo* type = WrapperTypeInfo::unwrap(data);
    if (V8DOMWindow::info.equals(type)) {
        v8::Handle<v8::Object> window = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), host);
        if (window.IsEmpty())
            return target;

        DOMWindow* targetWindow = V8DOMWindow::toNative(window);
        target = targetWindow->frame();
    } else if (V8History::info.equals(type)) {
        History* history = V8History::toNative(host);
        target = history->frame();
    } else if (V8Location::info.equals(type)) {
        Location* location = V8Location::toNative(host);
        target = location->frame();
    }
    return target;
}

static void reportUnsafeJavaScriptAccess(v8::Local<v8::Object> host, v8::AccessType type, v8::Local<v8::Value> data)
{
    Frame* target = getTargetFrame(host, data);
    if (target)
        V8Proxy::reportUnsafeAccessTo(target);
}

PassRefPtr<V8DOMWindowShell> V8DOMWindowShell::create(Frame* frame)
{
    return adoptRef(new V8DOMWindowShell(frame));
}

V8DOMWindowShell::V8DOMWindowShell(Frame* frame)
    : m_frame(frame)
{
}

bool V8DOMWindowShell::isContextInitialized()
{
    // m_context, m_global, and m_wrapperBoilerplates should
    // all be non-empty if if m_context is non-empty.
    ASSERT(m_context.IsEmpty() || !m_global.IsEmpty());
    return !m_context.IsEmpty();
}

void V8DOMWindowShell::disposeContextHandles()
{
    if (!m_context.IsEmpty()) {
        m_frame->loader()->client()->willReleaseScriptContext(m_context, 0);
        m_context.Dispose();
        m_context.Clear();

        // It's likely that disposing the context has created a lot of
        // garbage. Notify V8 about this so it'll have a chance of cleaning
        // it up when idle.
        V8GCForContextDispose::instance().notifyContextDisposed();
    }

    WrapperBoilerplateMap::iterator it = m_wrapperBoilerplates.begin();
    for (; it != m_wrapperBoilerplates.end(); ++it) {
        v8::Persistent<v8::Object> wrapper = it->second;
        wrapper.Dispose();
        wrapper.Clear();
    }
    m_wrapperBoilerplates.clear();
}

void V8DOMWindowShell::destroyGlobal()
{
    if (!m_global.IsEmpty()) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_global);
#endif
        m_global.Dispose();
        m_global.Clear();
    }
}

void V8DOMWindowShell::clearForClose()
{
    if (!m_context.IsEmpty()) {
        v8::HandleScope handleScope;

        clearDocumentWrapper();
        disposeContextHandles();
    }
}

void V8DOMWindowShell::clearForNavigation()
{
    if (!m_context.IsEmpty()) {
        v8::HandleScope handle;
        clearDocumentWrapper();

        v8::Context::Scope contextScope(m_context);

        // Clear the document wrapper cache before turning on access checks on
        // the old DOMWindow wrapper. This way, access to the document wrapper
        // will be protected by the security checks on the DOMWindow wrapper.
        clearDocumentWrapperCache();

        // Turn on access check on the old DOMWindow wrapper.
        v8::Handle<v8::Object> wrapper = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), m_global);
        ASSERT(!wrapper.IsEmpty());
        wrapper->TurnOnAccessCheck();

        // Separate the context from its global object.
        m_context->DetachGlobal();

        disposeContextHandles();
    }
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
bool V8DOMWindowShell::initContextIfNeeded()
{
    // Bail out if the context has already been initialized.
    if (!m_context.IsEmpty())
        return true;

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

        v8::V8::AddMessageListener(&v8UncaughtExceptionHandler);

        v8::V8::SetFailedAccessCheckCallbackFunction(reportUnsafeJavaScriptAccess);

        ScriptProfiler::initialize();

        V8BindingPerIsolateData::ensureInitialized(v8::Isolate::GetCurrent());

        isV8Initialized = true;
    }

    m_context = createNewContext(m_global, 0, 0);
    if (m_context.IsEmpty())
        return false;

    v8::Local<v8::Context> v8Context = v8::Local<v8::Context>::New(m_context);
    v8::Context::Scope contextScope(v8Context);

    // Store the first global object created so we can reuse it.
    if (m_global.IsEmpty()) {
        m_global = v8::Persistent<v8::Object>::New(v8Context->Global());
        // Bail out if allocation of the first global objects fails.
        if (m_global.IsEmpty()) {
            disposeContextHandles();
            return false;
        }
#ifndef NDEBUG
        V8GCController::registerGlobalHandle(PROXY, this, m_global);
#endif
    }

    if (!installHiddenObjectPrototype(v8Context)) {
        disposeContextHandles();
        return false;
    }

    if (!installDOMWindow(v8Context, m_frame->domWindow())) {
        disposeContextHandles();
        return false;
    }

    updateDocument();

    setSecurityToken();

    m_frame->loader()->client()->didCreateScriptContext(m_context, 0);

    // FIXME: This is wrong. We should actually do this for the proper world once
    // we do isolated worlds the WebCore way.
    m_frame->loader()->dispatchDidClearWindowObjectInWorld(0);

    return true;
}

v8::Persistent<v8::Context> V8DOMWindowShell::createNewContext(v8::Handle<v8::Object> global, int extensionGroup, int worldId)
{
    v8::Persistent<v8::Context> result;

    // The activeDocumentLoader pointer could be 0 during frame shutdown.
    if (!m_frame->loader()->activeDocumentLoader())
        return result;

    // Create a new environment using an empty template for the shadow
    // object. Reuse the global object if one has been created earlier.
    v8::Persistent<v8::ObjectTemplate> globalTemplate = V8DOMWindow::GetShadowObjectTemplate();
    if (globalTemplate.IsEmpty())
        return result;

    // Used to avoid sleep calls in unload handlers.
    if (!V8Proxy::registeredExtensionWithV8(DateExtension::get()))
        V8Proxy::registerExtension(DateExtension::get());

#if ENABLE(JAVASCRIPT_I18N_API)
    // Enables experimental i18n API in V8.
    if (RuntimeEnabledFeatures::javaScriptI18NAPIEnabled() && !V8Proxy::registeredExtensionWithV8(v8_i18n::Extension::get()))
        V8Proxy::registerExtension(v8_i18n::Extension::get());
#endif

    // Dynamically tell v8 about our extensions now.
    const V8Extensions& extensions = V8Proxy::extensions();
    OwnArrayPtr<const char*> extensionNames = adoptArrayPtr(new const char*[extensions.size()]);
    int index = 0;
    for (size_t i = 0; i < extensions.size(); ++i) {
        // Ensure our date extension is always allowed.
        if (extensions[i] != DateExtension::get()
            && !m_frame->loader()->client()->allowScriptExtension(extensions[i]->name(), extensionGroup, worldId))
            continue;

        extensionNames[index++] = extensions[i]->name();
    }
    v8::ExtensionConfiguration extensionConfiguration(index, extensionNames.get());
    result = v8::Context::New(&extensionConfiguration, globalTemplate, global);

    return result;
}

void V8DOMWindowShell::setContext(v8::Handle<v8::Context> context)
{
    // if we already have a context, clear it before setting the new one.
    if (!m_context.IsEmpty()) {
        m_context.Dispose();
        m_context.Clear();
    }
    m_context = v8::Persistent<v8::Context>::New(context);
}

bool V8DOMWindowShell::installDOMWindow(v8::Handle<v8::Context> context, DOMWindow* window)
{
    // Create a new JS window object and use it as the prototype for the  shadow global object.
    v8::Handle<v8::Function> windowConstructor = V8DOMWrapper::getConstructor(&V8DOMWindow::info, getHiddenObjectPrototype(context));
    v8::Local<v8::Object> jsWindow = SafeAllocation::newInstance(windowConstructor);
    // Bail out if allocation failed.
    if (jsWindow.IsEmpty())
        return false;

    // Wrap the window.
    V8DOMWrapper::setDOMWrapper(jsWindow, &V8DOMWindow::info, window);
    V8DOMWrapper::setDOMWrapper(v8::Handle<v8::Object>::Cast(jsWindow->GetPrototype()), &V8DOMWindow::info, window);

    window->ref();
    V8DOMWrapper::setJSWrapperForDOMObject(window, v8::Persistent<v8::Object>::New(jsWindow));

    // Insert the window instance as the prototype of the shadow object.
    v8::Handle<v8::Object> v8RealGlobal = v8::Handle<v8::Object>::Cast(context->Global()->GetPrototype());
    V8DOMWrapper::setDOMWrapper(v8RealGlobal, &V8DOMWindow::info, window);
    v8RealGlobal->SetPrototype(jsWindow);
    return true;
}

void V8DOMWindowShell::updateDocumentWrapper(v8::Handle<v8::Object> wrapper)
{
    clearDocumentWrapper();

    ASSERT(m_document.IsEmpty());
    m_document = v8::Persistent<v8::Object>::New(wrapper);
#ifndef NDEBUG
    V8GCController::registerGlobalHandle(PROXY, this, m_document);
#endif
}

void V8DOMWindowShell::clearDocumentWrapper()
{
    if (!m_document.IsEmpty()) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_document);
#endif
        m_document.Dispose();
        m_document.Clear();
    }
}

static void checkDocumentWrapper(v8::Handle<v8::Object> wrapper, Document* document)
{
    ASSERT(V8Document::toNative(wrapper) == document);
    ASSERT(!document->isHTMLDocument() || (V8Document::toNative(v8::Handle<v8::Object>::Cast(wrapper->GetPrototype())) == document));
}

void V8DOMWindowShell::updateDocumentWrapperCache()
{
    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(m_context);

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

    v8::Handle<v8::Value> documentWrapper = toV8(m_frame->document());
    ASSERT(documentWrapper == m_document || m_document.IsEmpty());
    if (m_document.IsEmpty())
        updateDocumentWrapper(v8::Handle<v8::Object>::Cast(documentWrapper));
    checkDocumentWrapper(m_document, m_frame->document());

    // If instantiation of the document wrapper fails, clear the cache
    // and let the DOMWindow accessor handle access to the document.
    if (documentWrapper.IsEmpty()) {
        clearDocumentWrapperCache();
        return;
    }
    ASSERT(documentWrapper->IsObject());
    m_context->Global()->ForceSet(v8::String::New("document"), documentWrapper, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));
}

void V8DOMWindowShell::clearDocumentWrapperCache()
{
    ASSERT(!m_context.IsEmpty());
    m_context->Global()->ForceDelete(v8::String::New("document"));
}

void V8DOMWindowShell::setSecurityToken()
{
    Document* document = m_frame->document();
    // Setup security origin and security token.
    if (!document) {
        m_context->UseDefaultSecurityToken();
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
        m_context->UseDefaultSecurityToken();
        return;
    }

    CString utf8Token = token.utf8();
    // NOTE: V8 does identity comparison in fast path, must use a symbol
    // as the security token.
    m_context->SetSecurityToken(v8::String::NewSymbol(utf8Token.data(), utf8Token.length()));
}

void V8DOMWindowShell::updateDocument()
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
    if (!initContextIfNeeded())
        return;

    // We have a new document and we need to update the cache.
    updateDocumentWrapperCache();

    updateSecurityOrigin();
}

v8::Handle<v8::Value> getter(v8::Local<v8::String> property, const v8::AccessorInfo& info)
{
    // FIXME(antonm): consider passing AtomicStringImpl directly.
    AtomicString name = v8StringToAtomicWebCoreString(property);
    HTMLDocument* htmlDocument = V8HTMLDocument::toNative(info.Holder());
    ASSERT(htmlDocument);
    v8::Handle<v8::Value> result = V8HTMLDocument::GetNamedProperty(htmlDocument, name);
    if (!result.IsEmpty())
        return result;
    v8::Handle<v8::Value> prototype = info.Holder()->GetPrototype();
    if (prototype->IsObject())
        return prototype.As<v8::Object>()->Get(property);
    return v8::Undefined();
}

void V8DOMWindowShell::namedItemAdded(HTMLDocument* doc, const AtomicString& name)
{
    if (!initContextIfNeeded())
        return;

    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(m_context);

    ASSERT(!m_document.IsEmpty());
    checkDocumentWrapper(m_document, doc);
    m_document->SetAccessor(v8String(name), getter);
}

void V8DOMWindowShell::namedItemRemoved(HTMLDocument* doc, const AtomicString& name)
{
}

void V8DOMWindowShell::updateSecurityOrigin()
{
    v8::HandleScope scope;
    setSecurityToken();
}

v8::Handle<v8::Value> V8DOMWindowShell::getHiddenObjectPrototype(v8::Handle<v8::Context> context)
{
    return context->Global()->GetHiddenValue(V8HiddenPropertyName::objectPrototype());
}

bool V8DOMWindowShell::installHiddenObjectPrototype(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::String> objectString = v8::String::New("Object");
    v8::Handle<v8::String> prototypeString = v8::String::New("prototype");
    v8::Handle<v8::String> hiddenObjectPrototypeString = V8HiddenPropertyName::objectPrototype();
    // Bail out if allocation failed.
    if (objectString.IsEmpty() || prototypeString.IsEmpty() || hiddenObjectPrototypeString.IsEmpty())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(context->Global()->Get(objectString));
    // Bail out if fetching failed.
    if (object.IsEmpty())
        return false;
    v8::Handle<v8::Value> objectPrototype = object->Get(prototypeString);
    // Bail out if fetching failed.
    if (objectPrototype.IsEmpty())
        return false;

    context->Global()->SetHiddenValue(hiddenObjectPrototypeString, objectPrototype);

    return true;
}

v8::Local<v8::Object> V8DOMWindowShell::createWrapperFromCacheSlowCase(WrapperTypeInfo* type)
{
    // Not in cache.
    if (!initContextIfNeeded())
        return notHandledByInterceptor();

    v8::Context::Scope scope(m_context);
    v8::Local<v8::Function> function = V8DOMWrapper::getConstructor(type, getHiddenObjectPrototype(m_context));
    v8::Local<v8::Object> instance = SafeAllocation::newInstance(function);
    if (!instance.IsEmpty()) {
        m_wrapperBoilerplates.set(type, v8::Persistent<v8::Object>::New(instance));
        return instance->Clone();
    }
    return notHandledByInterceptor();
}

void V8DOMWindowShell::setLocation(DOMWindow* window, const String& locationString)
{
    State<V8Binding>* state = V8BindingState::Only();
    window->setLocation(locationString, state->activeWindow(), state->firstWindow());
}

} // WebCore
