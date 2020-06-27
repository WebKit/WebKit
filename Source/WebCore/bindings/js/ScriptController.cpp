/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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
#include "DOMWrapperWorld.h"
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
#include "Logging.h"
#include "ModuleFetchFailureKind.h"
#include "ModuleFetchParameters.h"
#include "NP_jsobject.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageGroup.h"
#include "PaymentCoordinator.h"
#include "PluginViewBase.h"
#include "RunJavaScriptParameters.h"
#include "RuntimeApplicationChecks.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptDisallowedScope.h"
#include "ScriptSourceCode.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include "WebCoreJSClientData.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/Heap.h>
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
#include <JavaScriptCore/WeakGCMapInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/SharedTask.h>
#include <wtf/Threading.h>
#include <wtf/text/TextPosition.h>

#define RELEASE_LOG_ERROR_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_ERROR_IF(m_frame.isAlwaysOnLoggingAllowed(), channel, "%p - ScriptController::" fmt, this, ##__VA_ARGS__)

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

JSC::JSValue ScriptController::evaluateInWorldIgnoringException(const ScriptSourceCode& sourceCode, DOMWrapperWorld& world)
{
    auto result = evaluateInWorld(sourceCode, world);
    return result ? result.value() : JSC::JSValue { };
}

ValueOrException ScriptController::evaluateInWorld(const ScriptSourceCode& sourceCode, DOMWrapperWorld& world)
{
    JSLockHolder lock(world.vm());

    const SourceCode& jsSourceCode = sourceCode.jsSourceCode();
    String sourceURL = jsSourceCode.provider()->url().string();

    // evaluate code. Returns the JS return value or 0
    // if there was none, an error occurred or the type couldn't be converted.

    // inlineCode is true for <a href="javascript:doSomething()">
    // and false for <script>doSomething()</script>. Check if it has the
    // expected value in all cases.
    // See smart window.open policy for where this is used.
    auto& proxy = jsWindowProxy(world);
    auto& globalObject = *proxy.window();
    const String* savedSourceURL = m_sourceURL;
    m_sourceURL = &sourceURL;

    Ref<Frame> protector(m_frame);

    InspectorInstrumentation::willEvaluateScript(m_frame, sourceURL, sourceCode.startLine(), sourceCode.startColumn());

    NakedPtr<JSC::Exception> evaluationException;
    JSValue returnValue = JSExecState::profiledEvaluate(&globalObject, JSC::ProfilingReason::Other, jsSourceCode, &proxy, evaluationException);

    InspectorInstrumentation::didEvaluateScript(m_frame);

    Optional<ExceptionDetails> optionalDetails;
    if (evaluationException) {
        ExceptionDetails details;
        reportException(&globalObject, evaluationException, sourceCode.cachedScript(), &details);
        optionalDetails = WTFMove(details);
    }

    m_sourceURL = savedSourceURL;
    if (optionalDetails)
        return makeUnexpected(*optionalDetails);

    return returnValue;
}

JSC::JSValue ScriptController::evaluateIgnoringException(const ScriptSourceCode& sourceCode)
{
    return evaluateInWorldIgnoringException(sourceCode, mainThreadNormalWorld());
}

void ScriptController::loadModuleScriptInWorld(LoadableModuleScript& moduleScript, const String& moduleName, Ref<ModuleFetchParameters>&& topLevelFetchParameters, DOMWrapperWorld& world)
{
    JSLockHolder lock(world.vm());

    auto& proxy = jsWindowProxy(world);
    auto& lexicalGlobalObject = *proxy.window();

    auto& promise = JSExecState::loadModule(lexicalGlobalObject, moduleName, JSC::JSScriptFetchParameters::create(lexicalGlobalObject.vm(), WTFMove(topLevelFetchParameters)), JSC::JSScriptFetcher::create(lexicalGlobalObject.vm(), { &moduleScript }));
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
    auto& lexicalGlobalObject = *proxy.window();

    auto& promise = JSExecState::loadModule(lexicalGlobalObject, sourceCode.jsSourceCode(), JSC::JSScriptFetcher::create(lexicalGlobalObject.vm(), { &moduleScript }));
    setupModuleScriptHandlers(moduleScript, promise, world);
}

void ScriptController::loadModuleScript(LoadableModuleScript& moduleScript, const ScriptSourceCode& sourceCode)
{
    loadModuleScriptInWorld(moduleScript, sourceCode, mainThreadNormalWorld());
}

JSC::JSValue ScriptController::linkAndEvaluateModuleScriptInWorld(LoadableModuleScript& moduleScript, DOMWrapperWorld& world)
{
    JSC::VM& vm = world.vm();
    JSLockHolder lock(vm);

    auto& proxy = jsWindowProxy(world);
    auto& lexicalGlobalObject = *proxy.window();

    // FIXME: Preventing Frame from being destroyed is essentially unnecessary.
    // https://bugs.webkit.org/show_bug.cgi?id=164763
    Ref<Frame> protector(m_frame);

    NakedPtr<JSC::Exception> evaluationException;
    auto returnValue = JSExecState::linkAndEvaluateModule(lexicalGlobalObject, Identifier::fromUid(vm, moduleScript.moduleKey()), jsUndefined(), evaluationException);
    if (evaluationException) {
        // FIXME: Give a chance to dump the stack trace if the "crossorigin" attribute allows.
        // https://bugs.webkit.org/show_bug.cgi?id=164539
        reportException(&lexicalGlobalObject, evaluationException, nullptr);
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
    auto& lexicalGlobalObject = *proxy.window();
    SetForScope<const String*> sourceURLScope(m_sourceURL, &sourceURL.string());

    Ref<Frame> protector(m_frame);

    InspectorInstrumentation::willEvaluateScript(m_frame, sourceURL.string(), jsSourceCode.firstLine().oneBasedInt(), jsSourceCode.startColumn().oneBasedInt());
    auto returnValue = moduleRecord.evaluate(&lexicalGlobalObject);
    InspectorInstrumentation::didEvaluateScript(m_frame);

    return returnValue;
}

JSC::JSValue ScriptController::evaluateModule(const URL& sourceURL, JSModuleRecord& moduleRecord)
{
    return evaluateModule(sourceURL, moduleRecord, mainThreadNormalWorld());
}

Ref<DOMWrapperWorld> ScriptController::createWorld(const String& name, WorldType type)
{
    return DOMWrapperWorld::create(commonVM(), type == WorldType::User ? DOMWrapperWorld::Type::User : DOMWrapperWorld::Type::Internal, name);
}

void ScriptController::getAllWorlds(Vector<Ref<DOMWrapperWorld>>& worlds)
{
    static_cast<JSVMClientData*>(commonVM().clientData)->getAllWorlds(worlds);
}

void ScriptController::initScriptForWindowProxy(JSWindowProxy& windowProxy)
{
    auto& world = windowProxy.world();
    JSC::VM& vm = world.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    jsCast<JSDOMWindow*>(windowProxy.window())->updateDocument();
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception());

    if (Document* document = m_frame.document())
        document->contentSecurityPolicy()->didCreateWindowProxy(windowProxy);

    if (Page* page = m_frame.page()) {
        windowProxy.attachDebugger(page->debugger());
        windowProxy.window()->setProfileGroup(page->group().identifier());
        windowProxy.window()->setConsoleClient(&page->console());
    }

    m_frame.loader().dispatchDidClearWindowObjectInWorld(world);
}

static Identifier jsValueToModuleKey(JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    if (value.isSymbol())
        return Identifier::fromUid(jsCast<Symbol*>(value)->privateName());
    ASSERT(value.isString());
    return asString(value)->toIdentifier(lexicalGlobalObject);
}

void ScriptController::setupModuleScriptHandlers(LoadableModuleScript& moduleScriptRef, JSInternalPromise& promise, DOMWrapperWorld& world)
{
    auto& proxy = jsWindowProxy(world);
    auto& lexicalGlobalObject = *proxy.window();

    // It is not guaranteed that either fulfillHandler or rejectHandler is eventually called.
    // For example, if the page load is canceled, the DeferredPromise used in the module loader pipeline will stop executing JS code.
    // Thus the promise returned from this function could remain unresolved.

    RefPtr<LoadableModuleScript> moduleScript(&moduleScriptRef);

    auto& fulfillHandler = *JSNativeStdFunction::create(lexicalGlobalObject.vm(), proxy.window(), 1, String(), [moduleScript](JSGlobalObject* globalObject, CallFrame* callFrame) -> JSC::EncodedJSValue {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        Identifier moduleKey = jsValueToModuleKey(globalObject, callFrame->argument(0));
        RETURN_IF_EXCEPTION(scope, { });
        moduleScript->notifyLoadCompleted(*moduleKey.impl());
        return JSValue::encode(jsUndefined());
    });

    auto& rejectHandler = *JSNativeStdFunction::create(lexicalGlobalObject.vm(), proxy.window(), 1, String(), [moduleScript](JSGlobalObject* globalObject, CallFrame* callFrame) {
        VM& vm = globalObject->vm();
        JSValue errorValue = callFrame->argument(0);
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
                retrieveErrorMessage(*globalObject, vm, errorValue, scope),
            }
        });
        return JSValue::encode(jsUndefined());
    });

    promise.then(&lexicalGlobalObject, &fulfillHandler, &rejectHandler);
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

bool ScriptController::canAccessFromCurrentOrigin(Frame* frame, Document& accessingDocument)
{
    auto* lexicalGlobalObject = JSExecState::currentState();

    // If the current lexicalGlobalObject is null we should use the accessing document for the security check.
    if (!lexicalGlobalObject) {
        auto* targetDocument = frame ? frame->document() : nullptr;
        return targetDocument && accessingDocument.securityOrigin().canAccess(targetDocument->securityOrigin());
    }

    return BindingSecurity::shouldAllowAccessToFrame(lexicalGlobalObject, frame);
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

void ScriptController::collectIsolatedContexts(Vector<std::pair<JSC::JSGlobalObject*, SecurityOrigin*>>& result)
{
    for (auto& jsWindowProxy : windowProxy().jsWindowProxiesAsVector()) {
        auto* lexicalGlobalObject = jsWindowProxy->window();
        auto* origin = &downcast<DOMWindow>(jsWindowProxy->wrapped()).document()->securityOrigin();
        result.append(std::make_pair(lexicalGlobalObject, origin));
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
    JSValue jsElementValue = toJS(globalObj, globalObj, plugin);
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

JSC::JSValue ScriptController::executeScriptIgnoringException(const String& script, bool forceUserGesture)
{
    return executeScriptInWorldIgnoringException(mainThreadNormalWorld(), script, forceUserGesture);
}

JSC::JSValue ScriptController::executeScriptInWorldIgnoringException(DOMWrapperWorld& world, const String& script, bool forceUserGesture)
{
    auto result = executeScriptInWorld(world, { script, URL { }, false, WTF::nullopt, forceUserGesture });
    return result ? result.value() : JSC::JSValue { };
}

ValueOrException ScriptController::executeScriptInWorld(DOMWrapperWorld& world, RunJavaScriptParameters&& parameters)
{
    if (m_frame.loader().client().shouldEnableInAppBrowserPrivacyProtections()) {
        if (auto* document = m_frame.document())
            document->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "Ignoring user script injection for non-app bound domain.");
        RELEASE_LOG_ERROR_IF_ALLOWED(Loading, "executeScriptInWorld: Ignoring user script injection for non app-bound domain");
        return makeUnexpected(ExceptionDetails { "Ignoring user script injection for non-app bound domain"_s });
    }
    m_frame.loader().client().notifyPageOfAppBoundBehavior();

    UserGestureIndicator gestureIndicator(parameters.forceUserGesture == ForceUserGesture::Yes ? Optional<ProcessingUserGestureState>(ProcessingUserGesture) : WTF::nullopt);

    if (!canExecuteScripts(AboutToExecuteScript) || isPaused())
        return makeUnexpected(ExceptionDetails { "Cannot execute JavaScript in this document"_s });

    auto sourceURL = parameters.sourceURL;
    if (!sourceURL.isValid()) {
        // FIXME: This is gross, but when setTimeout() and setInterval() are passed JS strings, the thrown errors should use the frame document URL (according to WPT).
        sourceURL = m_frame.document()->url();
    }

    switch (parameters.runAsAsyncFunction) {
    case RunAsAsyncFunction::No:
        return evaluateInWorld({ parameters.source, WTFMove(sourceURL), TextPosition(), JSC::SourceProviderSourceType::Program, CachedScriptFetcher::create(m_frame.document()->charset()) }, world);
    case RunAsAsyncFunction::Yes:
        return callInWorld(WTFMove(parameters), world);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

ValueOrException ScriptController::callInWorld(RunJavaScriptParameters&& parameters, DOMWrapperWorld& world)
{
    ASSERT(parameters.runAsAsyncFunction == RunAsAsyncFunction::Yes);
    ASSERT(parameters.arguments);

    auto& proxy = jsWindowProxy(world);
    auto& globalObject = *proxy.window();
    MarkedArgumentBuffer markedArguments;
    StringBuilder functionStringBuilder;
    String errorMessage;

    // Build up a new script string that is an async function with arguments, and deserialize those arguments.
    functionStringBuilder.append("(async function(");
    for (auto argument = parameters.arguments->begin(); argument != parameters.arguments->end();) {
        functionStringBuilder.append(argument->key);
        auto serializedArgument = SerializedScriptValue::createFromWireBytes(WTFMove(argument->value));

        auto scope = DECLARE_CATCH_SCOPE(globalObject.vm());
        auto jsArgument = serializedArgument->deserialize(globalObject, &globalObject);
        if (UNLIKELY(scope.exception())) {
            errorMessage = "Unable to deserialize argument to execute asynchronous JavaScript function";
            break;
        }

        markedArguments.append(jsArgument);

        ++argument;
        if (argument != parameters.arguments->end())
            functionStringBuilder.append(',');
    }

    if (!errorMessage.isEmpty())
        return makeUnexpected(ExceptionDetails { errorMessage });

    functionStringBuilder.append("){", parameters.source, "})");

    auto sourceCode = ScriptSourceCode { functionStringBuilder.toString(), WTFMove(parameters.sourceURL), TextPosition(), JSC::SourceProviderSourceType::Program, CachedScriptFetcher::create(m_frame.document()->charset()) };
    const auto& jsSourceCode = sourceCode.jsSourceCode();

    String sourceURL = jsSourceCode.provider()->url().string();
    const String* savedSourceURL = m_sourceURL;
    m_sourceURL = &sourceURL;

    Ref<Frame> protector(m_frame);

    InspectorInstrumentation::willEvaluateScript(m_frame, sourceURL, sourceCode.startLine(), sourceCode.startColumn());

    NakedPtr<JSC::Exception> evaluationException;
    Optional<ExceptionDetails> optionalDetails;
    JSValue returnValue;
    do {
        JSValue functionObject = JSExecState::profiledEvaluate(&globalObject, JSC::ProfilingReason::Other, jsSourceCode, &proxy, evaluationException);

        if (evaluationException)
            break;

        if (!functionObject || !functionObject.isCallable(world.vm())) {
            optionalDetails = { { "Unable to create JavaScript async function to call"_s } };
            break;
        }

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=205562
        // Getting CallData shouldn't be required to call into JS.
        auto callData = getCallData(world.vm(), functionObject);
        if (callData.type == CallData::Type::None) {
            optionalDetails = { { "Unable to prepare JavaScript async function to be called"_s } };
            break;
        }

        returnValue = JSExecState::profiledCall(&globalObject, JSC::ProfilingReason::Other, functionObject, callData, &proxy, markedArguments, evaluationException);
    } while (false);

    InspectorInstrumentation::didEvaluateScript(m_frame);

    if (evaluationException && !optionalDetails) {
        ExceptionDetails details;
        reportException(&globalObject, evaluationException, sourceCode.cachedScript(), &details);
        optionalDetails = WTFMove(details);
    }

    m_sourceURL = savedSourceURL;

    if (optionalDetails)
        return makeUnexpected(*optionalDetails);
    return returnValue;
}

JSC::JSValue ScriptController::executeUserAgentScriptInWorldIgnoringException(DOMWrapperWorld& world, const String& script, bool forceUserGesture)
{
    auto result = executeUserAgentScriptInWorld(world, script, forceUserGesture);
    return result ? result.value() : JSC::JSValue { };
}
ValueOrException ScriptController::executeUserAgentScriptInWorld(DOMWrapperWorld& world, const String& script, bool forceUserGesture)
{
    return executeUserAgentScriptInWorldInternal(world, { script, URL { }, false, WTF::nullopt, forceUserGesture });
}

ValueOrException ScriptController::executeUserAgentScriptInWorldInternal(DOMWrapperWorld& world, RunJavaScriptParameters&& parameters)
{
    auto& document = *m_frame.document();
    auto allowed = shouldAllowUserAgentScripts(document);
    if (!allowed)
        return makeUnexpected(allowed.error());

    document.setHasEvaluatedUserAgentScripts();
    return executeScriptInWorld(world, WTFMove(parameters));
}

void ScriptController::executeAsynchronousUserAgentScriptInWorld(DOMWrapperWorld& world, RunJavaScriptParameters&& parameters, ResolveFunction&& resolveCompletionHandler)
{
    auto result = executeUserAgentScriptInWorldInternal(world, WTFMove(parameters));
    
    if (parameters.runAsAsyncFunction == RunAsAsyncFunction::No || !result || !result.value().isObject()) {
        resolveCompletionHandler(result);
        return;
    }

    // When running JavaScript as an async function, any "thenable" object gets promise-like behavior of deferred completion.
    auto thenIdentifier = world.vm().propertyNames->then;
    auto& proxy = jsWindowProxy(world);
    auto& globalObject = *proxy.window();

    auto thenFunction = result.value().get(&globalObject, thenIdentifier);
    if (!thenFunction.isObject()) {
        resolveCompletionHandler(result);
        return;
    }

    auto callData = getCallData(world.vm(), thenFunction);
    if (callData.type == CallData::Type::None) {
        resolveCompletionHandler(result);
        return;
    }

    auto sharedResolveFunction = createSharedTask<void(ValueOrException)>([resolveCompletionHandler = WTFMove(resolveCompletionHandler)](ValueOrException result) mutable {
        if (resolveCompletionHandler)
            resolveCompletionHandler(result);
        resolveCompletionHandler = nullptr;
    });

    auto* fulfillHandler = JSC::JSNativeStdFunction::create(world.vm(), &globalObject, 1, String { }, [sharedResolveFunction] (JSGlobalObject*, CallFrame* callFrame) mutable {
        sharedResolveFunction->run(callFrame->argument(0));
        return JSValue::encode(jsUndefined());
    });

    auto* rejectHandler = JSC::JSNativeStdFunction::create(world.vm(), &globalObject, 1, String { }, [sharedResolveFunction] (JSGlobalObject* globalObject, CallFrame* callFrame) mutable {
        sharedResolveFunction->run(makeUnexpected(ExceptionDetails { callFrame->argument(0).toWTFString(globalObject) }));
        return JSValue::encode(jsUndefined());
    });

    auto finalizeCount = makeUniqueWithoutFastMallocCheck<unsigned>(0);
    auto finalizeGuard = createSharedTask<void()>([sharedResolveFunction = WTFMove(sharedResolveFunction), finalizeCount = WTFMove(finalizeCount)]() {
        if (++(*finalizeCount) == 2)
            sharedResolveFunction->run(makeUnexpected(ExceptionDetails { "Completion handler for function call is no longer reachable"_s }));
    });

    world.vm().heap.addFinalizer(fulfillHandler, [finalizeGuard](JSCell*) {
        finalizeGuard->run();
    });
    world.vm().heap.addFinalizer(rejectHandler, [finalizeGuard](JSCell*) {
        finalizeGuard->run();
    });

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(fulfillHandler);
    arguments.append(rejectHandler);

    call(&globalObject, thenFunction, callData, result.value(), arguments);
}

Expected<void, ExceptionDetails> ScriptController::shouldAllowUserAgentScripts(Document& document) const
{
#if ENABLE(APPLE_PAY)
    if (auto page = m_frame.page())
        return page->paymentCoordinator().shouldAllowUserAgentScripts(document);
#else
    UNUSED_PARAM(document);
#endif
    return { };
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

void ScriptController::executeJavaScriptURL(const URL& url, RefPtr<SecurityOrigin> requesterSecurityOrigin, ShouldReplaceDocumentIfJavaScriptURL shouldReplaceDocumentIfJavaScriptURL)
{
    ASSERT(url.protocolIsJavaScript());

    if (requesterSecurityOrigin && !requesterSecurityOrigin->canAccess(m_frame.document()->securityOrigin()))
        return;

    if (!m_frame.page() || !m_frame.document()->contentSecurityPolicy()->allowJavaScriptURLs(m_frame.document()->url().string(), eventHandlerPosition().m_line))
        return;

    // We need to hold onto the Frame here because executing script can
    // destroy the frame.
    Ref<Frame> protector(m_frame);
    RefPtr<Document> ownerDocument(m_frame.document());

    const int javascriptSchemeLength = sizeof("javascript:") - 1;

    JSDOMGlobalObject* globalObject = jsWindowProxy(mainThreadNormalWorld()).window();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    String decodedURL = decodeURLEscapeSequences(url.string());
    auto result = executeScriptIgnoringException(decodedURL.substring(javascriptSchemeLength));
    RELEASE_ASSERT(&vm == &jsWindowProxy(mainThreadNormalWorld()).window()->vm());

    // If executing script caused this frame to be removed from the page, we
    // don't want to try to replace its document!
    if (!m_frame.page())
        return;

    if (!result)
        return;

    String scriptResult;
    bool isString = result.getString(globalObject, scriptResult);
    RETURN_IF_EXCEPTION(throwScope, void());

    if (!isString)
        return;

    // FIXME: We should always replace the document, but doing so
    //        synchronously can cause crashes:
    //        http://bugs.webkit.org/show_bug.cgi?id=16782
    if (shouldReplaceDocumentIfJavaScriptURL == ReplaceDocumentIfJavaScriptURL) {
        // We're still in a frame, so there should be a DocumentLoader.
        ASSERT(m_frame.document()->loader());

        // Signal to FrameLoader to disable navigations within this frame while replacing it with the result of executing javascript
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=200523
        // The only reason we do a nestable save/restore of this flag here is because we sometimes nest javascript: url loads as
        // some will load synchronously. We'd like to remove those synchronous loads and then change this.
        SetForScope<bool> willBeReplaced(m_willReplaceWithResultOfExecutingJavascriptURL, true);

        // DocumentWriter::replaceDocumentWithResultOfExecutingJavascriptURL can cause the DocumentLoader to get deref'ed and possible destroyed,
        // so protect it with a RefPtr.
        if (RefPtr<DocumentLoader> loader = m_frame.document()->loader())
            loader->writer().replaceDocumentWithResultOfExecutingJavascriptURL(scriptResult, ownerDocument.get());
    }
}

} // namespace WebCore

#undef RELEASE_LOG_ERROR_IF_ALLOWED
