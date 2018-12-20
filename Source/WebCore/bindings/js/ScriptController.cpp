/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ScriptController.h"

#include "BridgeJSC.h"
#include "CachedScriptFetcher.h"
#include "CommonVM.h"
#include "ContentSecurityPolicy.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLPlugInElement.h"
#include "InspectorInstrumentation.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMWindow.h"
#include "JSDocument.h"
#include "JSExecState.h"
#include "LoadableModuleScript.h"
#include "ModuleFetchFailureKind.h"
#include "ModuleFetchParameters.h"
#include "NP_jsobject.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageGroup.h"
#include "PluginViewBase.h"
#include "RuntimeApplicationChecks.h"
#include "ScriptDisallowedScope.h"
#include "ScriptSourceCode.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include "WebCoreJSClientData.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSFunction.h>
#include <JavaScriptCore/JSInternalPromise.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSModuleRecord.h>
#include <JavaScriptCore/JSNativeStdFunction.h>
#include <JavaScriptCore/JSScriptFetchParameters.h>
#include <JavaScriptCore/JSScriptFetcher.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/Threading.h>
#include <wtf/text/TextPosition.h>

namespace WebCore {
using namespace JSC;

void ScriptController::initializeThreading()
{
#if !PLATFORM(IOS_FAMILY)
    JSC::initializeThreading();
    WTF::initializeMainThread();
#endif
}

ScriptController::ScriptController(Frame& frame)
    : m_frame(frame)
    , m_sourceURL(0)
    , m_paused(false)
#if ENABLE(NETSCAPE_PLUGIN_API)
    , m_windowScriptNPObject(0)
#endif
#if PLATFORM(COCOA)
    , m_windowScriptObject(0)
#endif
{
}

ScriptController::~ScriptController()
{
    disconnectPlatformScriptObjects();

    if (m_cacheableBindingRootObject) {
        JSLockHolder lock(commonVM());
        m_cacheableBindingRootObject->invalidate();
        m_cacheableBindingRootObject = nullptr;
    }
}

JSValue ScriptController::evaluateInWorld(const ScriptSourceCode& sourceCode, DOMWrapperWorld& world, ExceptionDetails* exceptionDetails)
{
    JSLockHolder lock(world.vm());

    const SourceCode& jsSourceCode = sourceCode.jsSourceCode();
    String sourceURL = jsSourceCode.provider()->url();

    // evaluate code. Returns the JS return value or 0
    // if there was none, an error occurred or the type couldn't be converted.

    // inlineCode is true for <a href="javascript:doSomething()">
    // and false for <script>doSomething()</script>. Check if it has the
    // expected value in all cases.
    // See smart window.open policy for where this is used.
    auto& proxy = jsWindowProxy(world);
    auto& exec = *proxy.window()->globalExec();
    const String* savedSourceURL = m_sourceURL;
    m_sourceURL = &sourceURL;

    Ref<Frame> protector(m_frame);

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willEvaluateScript(m_frame, sourceURL, sourceCode.startLine());

    NakedPtr<JSC::Exception> evaluationException;
    JSValue returnValue = JSExecState::profiledEvaluate(&exec, JSC::ProfilingReason::Other, jsSourceCode, &proxy, evaluationException);

    InspectorInstrumentation::didEvaluateScript(cookie, m_frame);

    if (evaluationException) {
        reportException(&exec, evaluationException, sourceCode.cachedScript(), exceptionDetails);
        m_sourceURL = savedSourceURL;
        return { };
    }

    m_sourceURL = savedSourceURL;
    return returnValue;
}

JSValue ScriptController::evaluate(const ScriptSourceCode& sourceCode, ExceptionDetails* exceptionDetails)
{
    return evaluateInWorld(sourceCode, mainThreadNormalWorld(), exceptionDetails);
}

void ScriptController::loadModuleScriptInWorld(LoadableModuleScript& moduleScript, const String& moduleName, Ref<ModuleFetchParameters>&& topLevelFetchParameters, DOMWrapperWorld& world)
{
    JSLockHolder lock(world.vm());

    auto& proxy = jsWindowProxy(world);
    auto& state = *proxy.window()->globalExec();

    auto& promise = JSExecState::loadModule(state, moduleName, JSC::JSScriptFetchParameters::create(state.vm(), WTFMove(topLevelFetchParameters)), JSC::JSScriptFetcher::create(state.vm(), { &moduleScript }));
    setupModuleScriptHandlers(moduleScript, promise, world);
}

void ScriptController::loadModuleScript(LoadableModuleScript& moduleScript, const String& moduleName, Ref<ModuleFetchParameters>&& topLevelFetchParameters)
{
    loadModuleScriptInWorld(moduleScript, moduleName, WTFMove(topLevelFetchParameters), mainThreadNormalWorld());
}

void ScriptController::loadModuleScriptInWorld(LoadableModuleScript& moduleScript, const ScriptSourceCode& sourceCode, DOMWrapperWorld& world)
{
    JSLockHolder lock(world.vm());

    auto& proxy = jsWindowProxy(world);
    auto& state = *proxy.window()->globalExec();

    auto& promise = JSExecState::loadModule(state, sourceCode.jsSourceCode(), JSC::JSScriptFetcher::create(state.vm(), { &moduleScript }));
    setupModuleScriptHandlers(moduleScript, promise, world);
}

void ScriptController::loadModuleScript(LoadableModuleScript& moduleScript, const ScriptSourceCode& sourceCode)
{
    loadModuleScriptInWorld(moduleScript, sourceCode, mainThreadNormalWorld());
}

JSC::JSValue ScriptController::linkAndEvaluateModuleScriptInWorld(LoadableModuleScript& moduleScript, DOMWrapperWorld& world)
{
    JSLockHolder lock(world.vm());

    auto& proxy = jsWindowProxy(world);
    auto& state = *proxy.window()->globalExec();

    // FIXME: Preventing Frame from being destroyed is essentially unnecessary.
    // https://bugs.webkit.org/show_bug.cgi?id=164763
    Ref<Frame> protector(m_frame);

    NakedPtr<JSC::Exception> evaluationException;
    auto returnValue = JSExecState::linkAndEvaluateModule(state, Identifier::fromUid(&state.vm(), moduleScript.moduleKey()), jsUndefined(), evaluationException);
    if (evaluationException) {
        // FIXME: Give a chance to dump the stack trace if the "crossorigin" attribute allows.
        // https://bugs.webkit.org/show_bug.cgi?id=164539
        reportException(&state, evaluationException, nullptr);
        return jsUndefined();
    }
    return returnValue;
}

JSC::JSValue ScriptController::linkAndEvaluateModuleScript(LoadableModuleScript& moduleScript)
{
    return linkAndEvaluateModuleScriptInWorld(moduleScript, mainThreadNormalWorld());
}

JSC::JSValue ScriptController::evaluateModule(const URL& sourceURL, JSModuleRecord& moduleRecord, DOMWrapperWorld& world)
{
    JSLockHolder lock(world.vm());

    const auto& jsSourceCode = moduleRecord.sourceCode();

    auto& proxy = jsWindowProxy(world);
    auto& state = *proxy.window()->globalExec();
    SetForScope<const String*> sourceURLScope(m_sourceURL, &sourceURL.string());

    Ref<Frame> protector(m_frame);

    auto cookie = InspectorInstrumentation::willEvaluateScript(m_frame, sourceURL, jsSourceCode.firstLine().oneBasedInt());

    auto returnValue = moduleRecord.evaluate(&state);
    InspectorInstrumentation::didEvaluateScript(cookie, m_frame);

    return returnValue;
}

JSC::JSValue ScriptController::evaluateModule(const URL& sourceURL, JSModuleRecord& moduleRecord)
{
    return evaluateModule(sourceURL, moduleRecord, mainThreadNormalWorld());
}

Ref<DOMWrapperWorld> ScriptController::createWorld()
{
    return DOMWrapperWorld::create(commonVM());
}

void ScriptController::getAllWorlds(Vector<Ref<DOMWrapperWorld>>& worlds)
{
    static_cast<JSVMClientData*>(commonVM().clientData)->getAllWorlds(worlds);
}

void ScriptController::initScriptForWindowProxy(JSWindowProxy& windowProxy)
{
    auto& world = windowProxy.world();

    jsCast<JSDOMWindow*>(windowProxy.window())->updateDocument();

    if (Document* document = m_frame.document())
        document->contentSecurityPolicy()->didCreateWindowProxy(windowProxy);

    if (Page* page = m_frame.page()) {
        windowProxy.attachDebugger(page->debugger());
        windowProxy.window()->setProfileGroup(page->group().identifier());
        windowProxy.window()->setConsoleClient(&page->console());
    }

    m_frame.loader().dispatchDidClearWindowObjectInWorld(world);
}

static Identifier jsValueToModuleKey(ExecState* exec, JSValue value)
{
    if (value.isSymbol())
        return Identifier::fromUid(jsCast<Symbol*>(value)->privateName());
    ASSERT(value.isString());
    return asString(value)->toIdentifier(exec);
}

void ScriptController::setupModuleScriptHandlers(LoadableModuleScript& moduleScriptRef, JSInternalPromise& promise, DOMWrapperWorld& world)
{
    auto& proxy = jsWindowProxy(world);
    auto& state = *proxy.window()->globalExec();

    // It is not guaranteed that either fulfillHandler or rejectHandler is eventually called.
    // For example, if the page load is canceled, the DeferredPromise used in the module loader pipeline will stop executing JS code.
    // Thus the promise returned from this function could remain unresolved.

    RefPtr<LoadableModuleScript> moduleScript(&moduleScriptRef);

    auto& fulfillHandler = *JSNativeStdFunction::create(state.vm(), proxy.window(), 1, String(), [moduleScript](ExecState* exec) {
        Identifier moduleKey = jsValueToModuleKey(exec, exec->argument(0));
        moduleScript->notifyLoadCompleted(*moduleKey.impl());
        return JSValue::encode(jsUndefined());
    });

    auto& rejectHandler = *JSNativeStdFunction::create(state.vm(), proxy.window(), 1, String(), [moduleScript](ExecState* exec) {
        VM& vm = exec->vm();
        JSValue errorValue = exec->argument(0);
        if (errorValue.isObject()) {
            auto* object = JSC::asObject(errorValue);
            if (JSValue failureKindValue = object->getDirect(vm, static_cast<JSVMClientData&>(*vm.clientData).builtinNames().failureKindPrivateName())) {
                // This is host propagated error in the module loader pipeline.
                switch (static_cast<ModuleFetchFailureKind>(failureKindValue.asInt32())) {
                case ModuleFetchFailureKind::WasErrored:
                    moduleScript->notifyLoadFailed(LoadableScript::Error {
                        LoadableScript::ErrorType::CachedScript,
                        WTF::nullopt
                    });
                    break;
                case ModuleFetchFailureKind::WasCanceled:
                    moduleScript->notifyLoadWasCanceled();
                    break;
                }
                return JSValue::encode(jsUndefined());
            }
        }

        auto scope = DECLARE_CATCH_SCOPE(vm);
        moduleScript->notifyLoadFailed(LoadableScript::Error {
            LoadableScript::ErrorType::CachedScript,
            LoadableScript::ConsoleMessage {
                MessageSource::JS,
                MessageLevel::Error,
                retrieveErrorMessage(*exec, vm, errorValue, scope),
            }
        });
        return JSValue::encode(jsUndefined());
    });

    promise.then(&state, &fulfillHandler, &rejectHandler);
}

WindowProxy& ScriptController::windowProxy()
{
    return m_frame.windowProxy();
}

JSWindowProxy& ScriptController::jsWindowProxy(DOMWrapperWorld& world)
{
    auto* jsWindowProxy = m_frame.windowProxy().jsWindowProxy(world);
    ASSERT_WITH_MESSAGE(jsWindowProxy, "The JSWindowProxy can only be null if the frame has been destroyed");
    return *jsWindowProxy;
}

TextPosition ScriptController::eventHandlerPosition() const
{
    // FIXME: If we are not currently parsing, we should use our current location
    // in JavaScript, to cover cases like "element.setAttribute('click', ...)".

    // FIXME: This location maps to the end of the HTML tag, and not to the
    // exact column number belonging to the event handler attribute.
    auto* parser = m_frame.document()->scriptableDocumentParser();
    if (parser)
        return parser->textPosition();
    return TextPosition();
}

void ScriptController::enableEval()
{
    auto* jsWindowProxy = windowProxy().existingJSWindowProxy(mainThreadNormalWorld());
    if (!jsWindowProxy)
        return;
    jsWindowProxy->window()->setEvalEnabled(true);
}

void ScriptController::enableWebAssembly()
{
    auto* jsWindowProxy = windowProxy().existingJSWindowProxy(mainThreadNormalWorld());
    if (!jsWindowProxy)
        return;
    jsWindowProxy->window()->setWebAssemblyEnabled(true);
}

void ScriptController::disableEval(const String& errorMessage)
{
    auto* jsWindowProxy = windowProxy().existingJSWindowProxy(mainThreadNormalWorld());
    if (!jsWindowProxy)
        return;
    jsWindowProxy->window()->setEvalEnabled(false, errorMessage);
}

void ScriptController::disableWebAssembly(const String& errorMessage)
{
    auto* jsWindowProxy = windowProxy().existingJSWindowProxy(mainThreadNormalWorld());
    if (!jsWindowProxy)
        return;
    jsWindowProxy->window()->setWebAssemblyEnabled(false, errorMessage);
}

bool ScriptController::canAccessFromCurrentOrigin(Frame* frame)
{
    auto* state = JSExecState::currentState();

    // If the current state is null we're in a call path where the DOM security check doesn't apply (eg. parser).
    if (!state)
        return true;

    return BindingSecurity::shouldAllowAccessToFrame(state, frame);
}

void ScriptController::updateDocument()
{
    for (auto& jsWindowProxy : windowProxy().jsWindowProxiesAsVector()) {
        JSLockHolder lock(jsWindowProxy->world().vm());
        jsCast<JSDOMWindow*>(jsWindowProxy->window())->updateDocument();
    }
}

Bindings::RootObject* ScriptController::cacheableBindingRootObject()
{
    if (!canExecuteScripts(NotAboutToExecuteScript))
        return nullptr;

    if (!m_cacheableBindingRootObject) {
        JSLockHolder lock(commonVM());
        m_cacheableBindingRootObject = Bindings::RootObject::create(nullptr, globalObject(pluginWorld()));
    }
    return m_cacheableBindingRootObject.get();
}

Bindings::RootObject* ScriptController::bindingRootObject()
{
    if (!canExecuteScripts(NotAboutToExecuteScript))
        return nullptr;

    if (!m_bindingRootObject) {
        JSLockHolder lock(commonVM());
        m_bindingRootObject = Bindings::RootObject::create(nullptr, globalObject(pluginWorld()));
    }
    return m_bindingRootObject.get();
}

Ref<Bindings::RootObject> ScriptController::createRootObject(void* nativeHandle)
{
    auto it = m_rootObjects.find(nativeHandle);
    if (it != m_rootObjects.end())
        return it->value.copyRef();

    auto rootObject = Bindings::RootObject::create(nativeHandle, globalObject(pluginWorld()));

    m_rootObjects.set(nativeHandle, rootObject.copyRef());
    return rootObject;
}

void ScriptController::collectIsolatedContexts(Vector<std::pair<JSC::ExecState*, SecurityOrigin*>>& result)
{
    for (auto& jsWindowProxy : windowProxy().jsWindowProxiesAsVector()) {
        auto* exec = jsWindowProxy->window()->globalExec();
        auto* origin = &downcast<DOMWindow>(jsWindowProxy->wrapped()).document()->securityOrigin();
        result.append(std::make_pair(exec, origin));
    }
}

#if ENABLE(NETSCAPE_PLUGIN_API)
NPObject* ScriptController::windowScriptNPObject()
{
    if (!m_windowScriptNPObject) {
        JSLockHolder lock(commonVM());
        if (canExecuteScripts(NotAboutToExecuteScript)) {
            // JavaScript is enabled, so there is a JavaScript window object.
            // Return an NPObject bound to the window object.
            auto* window = jsWindowProxy(pluginWorld()).window();
            ASSERT(window);
            Bindings::RootObject* root = bindingRootObject();
            m_windowScriptNPObject = _NPN_CreateScriptObject(0, window, root);
        } else {
            // JavaScript is not enabled, so we cannot bind the NPObject to the JavaScript window object.
            // Instead, we create an NPObject of a different class, one which is not bound to a JavaScript object.
            m_windowScriptNPObject = _NPN_CreateNoScriptObject();
        }
    }

    return m_windowScriptNPObject;
}
#endif

#if !PLATFORM(COCOA)
RefPtr<JSC::Bindings::Instance> ScriptController::createScriptInstanceForWidget(Widget* widget)
{
    if (!is<PluginViewBase>(*widget))
        return nullptr;

    return downcast<PluginViewBase>(*widget).bindingInstance();
}
#endif

JSObject* ScriptController::jsObjectForPluginElement(HTMLPlugInElement* plugin)
{
    // Can't create JSObjects when JavaScript is disabled
    if (!canExecuteScripts(NotAboutToExecuteScript))
        return nullptr;

    JSLockHolder lock(commonVM());

    // Create a JSObject bound to this element
    auto* globalObj = globalObject(pluginWorld());
    // FIXME: is normal okay? - used for NP plugins?
    JSValue jsElementValue = toJS(globalObj->globalExec(), globalObj, plugin);
    if (!jsElementValue || !jsElementValue.isObject())
        return nullptr;
    
    return jsElementValue.getObject();
}

#if !PLATFORM(COCOA)

void ScriptController::updatePlatformScriptObjects()
{
}

void ScriptController::disconnectPlatformScriptObjects()
{
}

#endif

void ScriptController::cleanupScriptObjectsForPlugin(void* nativeHandle)
{
    auto it = m_rootObjects.find(nativeHandle);
    if (it == m_rootObjects.end())
        return;

    it->value->invalidate();
    m_rootObjects.remove(it);
}

void ScriptController::clearScriptObjects()
{
    JSLockHolder lock(commonVM());

    for (auto& rootObject : m_rootObjects.values())
        rootObject->invalidate();

    m_rootObjects.clear();

    if (m_bindingRootObject) {
        m_bindingRootObject->invalidate();
        m_bindingRootObject = nullptr;
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (m_windowScriptNPObject) {
        // Call _NPN_DeallocateObject() instead of _NPN_ReleaseObject() so that we don't leak if a plugin fails to release the window
        // script object properly.
        // This shouldn't cause any problems for plugins since they should have already been stopped and destroyed at this point.
        _NPN_DeallocateObject(m_windowScriptNPObject);
        m_windowScriptNPObject = nullptr;
    }
#endif
}

JSValue ScriptController::executeScriptInWorld(DOMWrapperWorld& world, const String& script, bool forceUserGesture, ExceptionDetails* exceptionDetails)
{
    UserGestureIndicator gestureIndicator(forceUserGesture ? Optional<ProcessingUserGestureState>(ProcessingUserGesture) : WTF::nullopt);
    ScriptSourceCode sourceCode(script, m_frame.document()->url(), TextPosition(), JSC::SourceProviderSourceType::Program, CachedScriptFetcher::create(m_frame.document()->charset()));

    if (!canExecuteScripts(AboutToExecuteScript) || isPaused())
        return { };

    return evaluateInWorld(sourceCode, world, exceptionDetails);
}

bool ScriptController::canExecuteScripts(ReasonForCallingCanExecuteScripts reason)
{
    if (reason == AboutToExecuteScript)
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed() || !isInWebProcess());

    if (m_frame.document() && m_frame.document()->isSandboxed(SandboxScripts)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        if (reason == AboutToExecuteScript || reason == AboutToCreateEventListener)
            m_frame.document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked script execution in '" + m_frame.document()->url().stringCenterEllipsizedToLength() + "' because the document's frame is sandboxed and the 'allow-scripts' permission is not set.");
        return false;
    }

    if (!m_frame.page())
        return false;

    return m_frame.loader().client().allowScript(m_frame.settings().isScriptEnabled());
}

JSValue ScriptController::executeScript(const String& script, bool forceUserGesture, ExceptionDetails* exceptionDetails)
{
    UserGestureIndicator gestureIndicator(forceUserGesture ? Optional<ProcessingUserGestureState>(ProcessingUserGesture) : WTF::nullopt);
    return executeScript(ScriptSourceCode(script, m_frame.document()->url(), TextPosition(), JSC::SourceProviderSourceType::Program, CachedScriptFetcher::create(m_frame.document()->charset())), exceptionDetails);
}

JSValue ScriptController::executeScript(const ScriptSourceCode& sourceCode, ExceptionDetails* exceptionDetails)
{
    if (!canExecuteScripts(AboutToExecuteScript) || isPaused())
        return { }; // FIXME: Would jsNull be better?

    // FIXME: Preventing Frame from being destroyed is essentially unnecessary.
    // https://bugs.webkit.org/show_bug.cgi?id=164763
    Ref<Frame> protector(m_frame); // Script execution can destroy the frame, and thus the ScriptController.

    return evaluate(sourceCode, exceptionDetails);
}

bool ScriptController::executeIfJavaScriptURL(const URL& url, ShouldReplaceDocumentIfJavaScriptURL shouldReplaceDocumentIfJavaScriptURL)
{
    if (!WTF::protocolIsJavaScript(url))
        return false;

    if (!m_frame.page() || !m_frame.document()->contentSecurityPolicy()->allowJavaScriptURLs(m_frame.document()->url(), eventHandlerPosition().m_line))
        return true;

    // We need to hold onto the Frame here because executing script can
    // destroy the frame.
    Ref<Frame> protector(m_frame);
    RefPtr<Document> ownerDocument(m_frame.document());

    const int javascriptSchemeLength = sizeof("javascript:") - 1;

    String decodedURL = decodeURLEscapeSequences(url.string());
    auto result = executeScript(decodedURL.substring(javascriptSchemeLength));

    // If executing script caused this frame to be removed from the page, we
    // don't want to try to replace its document!
    if (!m_frame.page())
        return true;

    String scriptResult;
    if (!result || !result.getString(jsWindowProxy(mainThreadNormalWorld()).window()->globalExec(), scriptResult))
        return true;

    // FIXME: We should always replace the document, but doing so
    //        synchronously can cause crashes:
    //        http://bugs.webkit.org/show_bug.cgi?id=16782
    if (shouldReplaceDocumentIfJavaScriptURL == ReplaceDocumentIfJavaScriptURL) {
        // We're still in a frame, so there should be a DocumentLoader.
        ASSERT(m_frame.document()->loader());
        
        // DocumentWriter::replaceDocument can cause the DocumentLoader to get deref'ed and possible destroyed,
        // so protect it with a RefPtr.
        if (RefPtr<DocumentLoader> loader = m_frame.document()->loader())
            loader->writer().replaceDocument(scriptResult, ownerDocument.get());
    }
    return true;
}

} // namespace WebCore
