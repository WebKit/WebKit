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

#include "BindingState.h"
#include "ContentSecurityPolicy.h"
#include "DateExtension.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "MemoryUsageSupport.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformSupport.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include "ScriptController.h"
#include "ScriptProfiler.h"
#include "SecurityOrigin.h"
#include "StorageNamespace.h"
#include "StylePropertySet.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8Document.h"
#include "V8GCController.h"
#include "V8GCForContextDispose.h"
#include "V8HTMLDocument.h"
#include "V8HiddenPropertyName.h"
#include "V8History.h"
#include "V8Location.h"
#include "V8ObjectConstructor.h"
#include "V8PerContextData.h"
#include "WorkerContextExecutionProxy.h"
#include <algorithm>
#include <stdio.h>
#include <utility>
#include <v8-debug.h>
#include <v8.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

#if ENABLE(JAVASCRIPT_I18N_API)
#include <v8-i18n/include/extension.h>
#endif

namespace WebCore {

static void reportFatalError(const char* location, const char* message)
{
    int memoryUsageMB = MemoryUsageSupport::actualMemoryUsageMB();
    printf("V8 error: %s (%s).  Current memory usage: %d MB\n", message, location, memoryUsageMB);
    CRASH();
}

static void reportUncaughtException(v8::Handle<v8::Message> message, v8::Handle<v8::Value> data)
{
    DOMWindow* firstWindow = firstDOMWindow(BindingState::instance());
    if (!firstWindow->isCurrentlyDisplayedInFrame())
        return;

    String errorMessage = toWebCoreString(message->Get());

    v8::Handle<v8::StackTrace> stackTrace = message->GetStackTrace();
    RefPtr<ScriptCallStack> callStack;
    // Currently stack trace is only collected when inspector is open.
    if (!stackTrace.IsEmpty() && stackTrace->GetFrameCount() > 0)
        callStack = createScriptCallStack(stackTrace, ScriptCallStack::maxCallStackSizeToCapture);

    v8::Handle<v8::Value> resourceName = message->GetScriptResourceName();
    bool shouldUseDocumentURL = resourceName.IsEmpty() || !resourceName->IsString();
    String resource = shouldUseDocumentURL ? firstWindow->document()->url() : toWebCoreString(resourceName);
    firstWindow->document()->reportException(errorMessage, message->GetLineNumber(), resource, callStack);
}

static Frame* findFrame(v8::Local<v8::Object> host, v8::Local<v8::Value> data)
{
    WrapperTypeInfo* type = WrapperTypeInfo::unwrap(data);

    if (V8DOMWindow::info.equals(type)) {
        v8::Handle<v8::Object> windowWrapper = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), host);
        if (windowWrapper.IsEmpty())
            return 0;
        return V8DOMWindow::toNative(windowWrapper)->frame();
    }

    if (V8History::info.equals(type))
        return V8History::toNative(host)->frame();

    if (V8Location::info.equals(type))
        return V8Location::toNative(host)->frame();

    // This function can handle only those types listed above.
    ASSERT_NOT_REACHED();
    return 0;
}

static void reportUnsafeJavaScriptAccess(v8::Local<v8::Object> host, v8::AccessType type, v8::Local<v8::Value> data)
{
    Frame* target = findFrame(host, data);
    if (!target)
        return;
    DOMWindow* targetWindow = target->document()->domWindow();
    targetWindow->printErrorMessage(targetWindow->crossDomainAccessErrorMessage(activeDOMWindow(BindingState::instance())));
}

static void initializeV8IfNeeded()
{
    ASSERT(isMainThread());

    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    v8::V8::IgnoreOutOfMemoryException();
    v8::V8::SetFatalErrorHandler(reportFatalError);
    v8::V8::SetGlobalGCPrologueCallback(&V8GCController::gcPrologue);
    v8::V8::SetGlobalGCEpilogueCallback(&V8GCController::gcEpilogue);
    v8::V8::AddMessageListener(&reportUncaughtException);
    v8::V8::SetFailedAccessCheckCallbackFunction(reportUnsafeJavaScriptAccess);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    ScriptProfiler::initialize();
#endif
    V8PerIsolateData::ensureInitialized(v8::Isolate::GetCurrent());

    // FIXME: Remove the following 2 lines when V8 default has changed.
    const char es5ReadonlyFlag[] = "--es5_readonly";
    v8::V8::SetFlagsFromString(es5ReadonlyFlag, sizeof(es5ReadonlyFlag));
}

static void checkDocumentWrapper(v8::Handle<v8::Object> wrapper, Document* document)
{
    ASSERT(V8Document::toNative(wrapper) == document);
    ASSERT(!document->isHTMLDocument() || (V8Document::toNative(v8::Handle<v8::Object>::Cast(wrapper->GetPrototype())) == document));
}

PassOwnPtr<V8DOMWindowShell> V8DOMWindowShell::create(Frame* frame)
{
    return adoptPtr(new V8DOMWindowShell(frame));
}

V8DOMWindowShell::V8DOMWindowShell(Frame* frame)
    : m_frame(frame)
{
}

bool V8DOMWindowShell::isContextInitialized()
{
    ASSERT(m_context.isEmpty() || !m_global.isEmpty());
    return !m_context.isEmpty();
}

void V8DOMWindowShell::disposeContext()
{
    m_perContextData.clear();

    if (!m_context.isEmpty()) {
        m_frame->loader()->client()->willReleaseScriptContext(m_context.get(), 0);
        m_context.clear();

        // It's likely that disposing the context has created a lot of
        // garbage. Notify V8 about this so it'll have a chance of cleaning
        // it up when idle.
        bool isMainFrame = m_frame->page() && (m_frame->page()->mainFrame() == m_frame); 
        V8GCForContextDispose::instance().notifyContextDisposed(isMainFrame);
    }
}

void V8DOMWindowShell::destroyGlobal()
{
    m_global.clear();
}

void V8DOMWindowShell::clearForClose()
{
    if (m_context.isEmpty())
        return;

    v8::HandleScope handleScope;
    m_document.clear();
    disposeContext();
}

void V8DOMWindowShell::clearForNavigation()
{
    if (m_context.isEmpty())
        return;

    v8::HandleScope handleScope;
    m_document.clear();

    // FIXME: Should we create a new Local handle here?
    v8::Context::Scope contextScope(m_context.get());

    // Clear the document wrapper cache before turning on access checks on
    // the old DOMWindow wrapper. This way, access to the document wrapper
    // will be protected by the security checks on the DOMWindow wrapper.
    clearDocumentProperty();

    v8::Handle<v8::Object> windowWrapper = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), m_global.get());
    ASSERT(!windowWrapper.IsEmpty());
    windowWrapper->TurnOnAccessCheck();
    m_context->DetachGlobal();
    disposeContext();
}

// Create a new environment and setup the global object.
//
// The global object corresponds to a DOMWindow instance. However, to
// allow properties of the JS DOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS DOMWindow
// instance as the prototype for that shadow object. The JS DOMWindow
// instance is undetectable from JavaScript code because the __proto__
// accessors skip that object.
//
// The shadow object and the DOMWindow instance are seen as one object
// from JavaScript. The JavaScript object that corresponds to a
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
bool V8DOMWindowShell::initializeIfNeeded()
{
    if (!m_context.isEmpty())
        return true;

    v8::HandleScope handleScope;

    initializeV8IfNeeded();

    m_context.adopt(createNewContext(m_global.get(), 0, 0));
    if (m_context.isEmpty())
        return false;

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(m_context.get());
    v8::Context::Scope contextScope(context);

    if (m_global.isEmpty()) {
        m_global.set(context->Global());
        if (m_global.isEmpty()) {
            disposeContext();
            return false;
        }
    }

    m_perContextData = V8PerContextData::create(m_context.get());
    if (!m_perContextData->init()) {
        disposeContext();
        return false;
    }

    if (!installDOMWindow(context, m_frame->document()->domWindow())) {
        disposeContext();
        return false;
    }

    updateDocument();

    setSecurityToken();

    if (m_frame->document())
        context->AllowCodeGenerationFromStrings(m_frame->document()->contentSecurityPolicy()->allowEval(0, ContentSecurityPolicy::SuppressReport));

    m_frame->loader()->client()->didCreateScriptContext(m_context.get(), 0, 0);

    // FIXME: This is wrong. We should actually do this for the proper world once
    // we do isolated worlds the WebCore way.
    m_frame->loader()->dispatchDidClearWindowObjectInWorld(0);

    return true;
}

v8::Persistent<v8::Context> V8DOMWindowShell::createNewContext(v8::Handle<v8::Object> global, int extensionGroup, int worldId)
{
    v8::Persistent<v8::Context> result;

    // The activeDocumentLoader pointer could be 0 during frame shutdown.
    // FIXME: Can we remove this check?
    if (!m_frame->loader()->activeDocumentLoader())
        return result;

    // Create a new environment using an empty template for the shadow
    // object. Reuse the global object if one has been created earlier.
    v8::Persistent<v8::ObjectTemplate> globalTemplate = V8DOMWindow::GetShadowObjectTemplate();
    if (globalTemplate.IsEmpty())
        return result;

    // Used to avoid sleep calls in unload handlers.
    ScriptController::registerExtensionIfNeeded(DateExtension::get());

#if ENABLE(JAVASCRIPT_I18N_API)
    // Enables experimental i18n API in V8.
    if (RuntimeEnabledFeatures::javaScriptI18NAPIEnabled())
        ScriptController::registerExtensionIfNeeded(v8_i18n::Extension::get());
#endif

    // Dynamically tell v8 about our extensions now.
    const V8Extensions& extensions = ScriptController::registeredExtensions();
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

bool V8DOMWindowShell::installDOMWindow(v8::Handle<v8::Context> context, DOMWindow* window)
{
    v8::Local<v8::Object> windowWrapper = V8ObjectConstructor::newInstance(V8DOMWrapper::constructorForType(&V8DOMWindow::info, window));
    if (windowWrapper.IsEmpty())
        return false;

    V8DOMWindow::installPerContextProperties(windowWrapper, window);

    V8DOMWrapper::setDOMWrapper(windowWrapper, &V8DOMWindow::info, window);
    V8DOMWrapper::setDOMWrapper(v8::Handle<v8::Object>::Cast(windowWrapper->GetPrototype()), &V8DOMWindow::info, window);
    V8DOMWrapper::setJSWrapperForDOMObject(PassRefPtr<DOMWindow>(window), windowWrapper);

    // Install the windowWrapper as the prototype of the innerGlobalObject.
    // The full structure of the global object is as follows:
    //
    // outerGlobalObject (Empty object, remains after navigation)
    //   -- has prototype --> innerGlobalObject (Holds global variables, changes during navigation)
    //   -- has prototype --> DOMWindow instance
    //   -- has prototype --> Window.prototype
    //   -- has prototype --> Object.prototype
    //
    // Note: Much of this prototype structure is hidden from web content. The
    //       outer, inner, and DOMWindow instance all appear to be the same
    //       JavaScript object.
    //
    v8::Handle<v8::Object> innerGlobalObject = toInnerGlobalObject(context);
    V8DOMWrapper::setDOMWrapper(innerGlobalObject, &V8DOMWindow::info, window);
    innerGlobalObject->SetPrototype(windowWrapper);
    return true;
}

void V8DOMWindowShell::updateDocumentWrapper(v8::Handle<v8::Object> wrapper)
{
    m_document.set(wrapper);
}

void V8DOMWindowShell::updateDocumentProperty()
{
    v8::HandleScope handleScope;
    // FIXME: Should we use a new Local handle here?
    v8::Context::Scope contextScope(m_context.get());

    v8::Handle<v8::Value> documentWrapper = toV8(m_frame->document());
    ASSERT(documentWrapper == m_document.get() || m_document.isEmpty());
    if (m_document.isEmpty())
        updateDocumentWrapper(v8::Handle<v8::Object>::Cast(documentWrapper));
    checkDocumentWrapper(m_document.get(), m_frame->document());

    // If instantiation of the document wrapper fails, clear the cache
    // and let the DOMWindow accessor handle access to the document.
    if (documentWrapper.IsEmpty()) {
        clearDocumentProperty();
        return;
    }
    ASSERT(documentWrapper->IsObject());
    m_context->Global()->ForceSet(v8::String::New("document"), documentWrapper, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));

    // We also stash a reference to the document on the inner global object so that
    // DOMWindow objects we obtain from JavaScript references are guaranteed to have
    // live Document objects.
    toInnerGlobalObject(m_context.get())->SetHiddenValue(V8HiddenPropertyName::document(), documentWrapper);
}

void V8DOMWindowShell::clearDocumentProperty()
{
    ASSERT(!m_context.isEmpty());
    m_context->Global()->ForceDelete(v8::String::New("document"));
}

void V8DOMWindowShell::setSecurityToken()
{
    Document* document = m_frame->document();

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
    if (m_global.isEmpty())
        return;
    if (!initializeIfNeeded())
        return;
    updateDocumentProperty();
    updateSecurityOrigin();
}

static v8::Handle<v8::Value> getter(v8::Local<v8::String> property, const v8::AccessorInfo& info)
{
    // FIXME: Consider passing AtomicStringImpl directly.
    AtomicString name = toWebCoreAtomicString(property);
    HTMLDocument* htmlDocument = V8HTMLDocument::toNative(info.Holder());
    ASSERT(htmlDocument);
    v8::Handle<v8::Value> result = V8HTMLDocument::getNamedProperty(htmlDocument, name, info.Holder(), info.GetIsolate());
    if (!result.IsEmpty())
        return result;
    v8::Handle<v8::Value> prototype = info.Holder()->GetPrototype();
    if (prototype->IsObject())
        return prototype.As<v8::Object>()->Get(property);
    return v8::Undefined();
}

void V8DOMWindowShell::namedItemAdded(HTMLDocument* document, const AtomicString& name)
{
    if (!initializeIfNeeded())
        return;

    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(m_context.get());

    ASSERT(!m_document.isEmpty());
    checkDocumentWrapper(m_document.get(), document);
    m_document->SetAccessor(v8String(name), getter);
}

void V8DOMWindowShell::namedItemRemoved(HTMLDocument* document, const AtomicString& name)
{
    if (document->hasNamedItem(name.impl()) || document->hasExtraNamedItem(name.impl()))
        return;

    if (!initializeIfNeeded())
        return;

    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(m_context.get());

    ASSERT(!m_document.isEmpty());
    checkDocumentWrapper(m_document.get(), document);
    m_document->Delete(v8String(name));
}

void V8DOMWindowShell::updateSecurityOrigin()
{
    if (m_context.isEmpty())
        return;
    v8::HandleScope handleScope;
    setSecurityToken();
}

} // WebCore
