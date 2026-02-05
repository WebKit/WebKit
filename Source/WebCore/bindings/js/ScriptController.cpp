/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006-2025 Apple Inc. All rights reserved.
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
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "DocumentQuirks.h"
#include "DocumentSecurityOrigin.h"
#include "Event.h"
#include "FrameConsoleClient.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTMLPlugInElement.h"
#include "HistoryController.h"
#include "InspectorInstrumentation.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMWindow.h"
#include "JSDocument.h"
#include "JSExecState.h"
#include "LoadableModuleScript.h"
#include "LocalFrameInlines.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "ModuleFetchFailureKind.h"
#include "ModuleFetchParameters.h"
#include "NavigationAction.h"
#include "Page.h"
#include "PageGroup.h"
#include "PaymentCoordinator.h"
#include "RunJavaScriptParameters.h"
#include "ScriptDisallowedScope.h"
#include "ScriptSourceCode.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include "SpeculationRules.h"
#include "TrustedType.h"
#include "UserGestureIndicator.h"
#include "WebCoreJITOperations.h"
#include "WebCoreJSClientData.h"
#include "runtime_root.h"
#include <JavaScriptCore/AbstractModuleRecord.h>
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/ImportMap.h>
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
#include <JavaScriptCore/SyntheticModuleRecord.h>
#include <JavaScriptCore/WeakGCMapInlines.h>
#include <JavaScriptCore/WebAssemblyModuleRecord.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/SetForScope.h>
#include <wtf/SharedTask.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Threading.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextPosition.h>

#define SCRIPTCONTROLLER_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - ScriptController::" fmt, this, ##__VA_ARGS__)

#if ENABLE(LLVM_PROFILE_GENERATION)
#if PLATFORM(IOS_FAMILY)
#include <wtf/LLVMProfilingUtils.h>
extern "C" char __llvm_profile_filename[] = "%t/WebKitPGO/WebCore_%m_pid%p%c.profraw";
#else
extern "C" char __llvm_profile_filename[] = "/private/tmp/WebKitPGO/WebCore_%m_pid%p%c.profraw";
#endif
#endif

namespace WebCore {
using namespace JSC;

enum class WebCoreProfileTag { };

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScriptController);

void ScriptController::initializeMainThread()
{
#if !PLATFORM(IOS_FAMILY)
    JSC::initialize();
    WTF::initializeMainThread();
    WebCore::populateJITOperations();
#endif
}

ScriptController::ScriptController(LocalFrame& frame)
    : m_frame(frame)
{
}

ScriptController::~ScriptController()
{
    disconnectPlatformScriptObjects();

    if (RefPtr cacheableBindingRootObject = m_cacheableBindingRootObject) {
        JSLockHolder lock(commonVM());
        cacheableBindingRootObject->invalidate();
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
    auto& vm = world.vm();
    JSLockHolder lock(vm);

    if (vm.hasPendingTerminationException()) {
        ExceptionDetails details;
        return makeUnexpected(details);
    }

    const SourceCode& jsSourceCode = sourceCode.jsSourceCode();
    const URL& sourceURL = jsSourceCode.provider()->sourceOrigin().url();

    // evaluate code. Returns the JS return value or 0
    // if there was none, an error occurred or the type couldn't be converted.

    // inlineCode is true for <a href="javascript:doSomething()">
    // and false for <script>doSomething()</script>. Check if it has the
    // expected value in all cases.
    // See smart window.open policy for where this is used.
    auto& proxy = jsWindowProxy(world);
    auto& globalObject = *proxy.window();

    Ref protector { m_frame.get() };
    SetForScope sourceURLScope(m_sourceURL, &sourceURL);

    if (RefPtr document = m_frame->document()) {
        if (auto script = document->quirks().scriptToEvaluateBeforeRunningScriptFromURL(sourceURL); !script.isEmpty())
            evaluateIgnoringException({ WTF::move(script), JSC::SourceTaintedOrigin::Untainted });
    }

    InspectorInstrumentation::willEvaluateScript(protectedFrame(), sourceURL.string(), sourceCode.startLine(), sourceCode.startColumn());

    NakedPtr<JSC::Exception> evaluationException;
    JSValue returnValue = JSExecState::profiledEvaluate(&globalObject, JSC::ProfilingReason::Other, jsSourceCode, &proxy, evaluationException);

    InspectorInstrumentation::didEvaluateScript(protectedFrame());

    std::optional<ExceptionDetails> optionalDetails;
    if (evaluationException) {
        ExceptionDetails details;
        reportException(&globalObject, evaluationException, sourceCode.cachedScript(), false, &details);
        optionalDetails = WTF::move(details);
    }

    if (optionalDetails)
        return makeUnexpected(*optionalDetails);

    return returnValue;
}

JSC::JSValue ScriptController::evaluateIgnoringException(const ScriptSourceCode& sourceCode)
{
    return evaluateInWorldIgnoringException(sourceCode, mainThreadNormalWorldSingleton());
}

void ScriptController::loadModuleScriptInWorld(LoadableModuleScript& moduleScript, const URL& topLevelModuleURL, Ref<JSC::ScriptFetchParameters>&& topLevelFetchParameters, DOMWrapperWorld& world)
{
    JSLockHolder lock(world.vm());

    auto& proxy = jsWindowProxy(world);
    auto& lexicalGlobalObject = *proxy.window();

    auto* promise = JSExecState::loadModule(lexicalGlobalObject, topLevelModuleURL, JSC::JSScriptFetchParameters::create(lexicalGlobalObject.vm(), WTF::move(topLevelFetchParameters)), JSC::JSScriptFetcher::create(lexicalGlobalObject.vm(), { &moduleScript }));
    if (!promise) [[unlikely]]
        return;
    setupModuleScriptHandlers(moduleScript, *promise, world);
}

void ScriptController::loadModuleScript(LoadableModuleScript& moduleScript, const URL& topLevelModuleURL, Ref<JSC::ScriptFetchParameters>&& topLevelFetchParameters)
{
    loadModuleScriptInWorld(moduleScript, topLevelModuleURL, WTF::move(topLevelFetchParameters), mainThreadNormalWorldSingleton());
}

void ScriptController::loadModuleScriptInWorld(LoadableModuleScript& moduleScript, const ScriptSourceCode& sourceCode, DOMWrapperWorld& world)
{
    JSLockHolder lock(world.vm());

    auto& proxy = jsWindowProxy(world);
    auto& lexicalGlobalObject = *proxy.window();

    auto* promise = JSExecState::loadModule(lexicalGlobalObject, sourceCode.jsSourceCode(), JSC::JSScriptFetcher::create(lexicalGlobalObject.vm(), { &moduleScript }));
    if (!promise) [[unlikely]]
        return;
    setupModuleScriptHandlers(moduleScript, *promise, world);
}

void ScriptController::loadModuleScript(LoadableModuleScript& moduleScript, const ScriptSourceCode& sourceCode)
{
    loadModuleScriptInWorld(moduleScript, sourceCode, mainThreadNormalWorldSingleton());
}

JSC::JSValue ScriptController::linkAndEvaluateModuleScriptInWorld(LoadableModuleScript& moduleScript, DOMWrapperWorld& world)
{
    JSC::VM& vm = world.vm();
    JSLockHolder lock(vm);

    auto& proxy = jsWindowProxy(world);
    auto& lexicalGlobalObject = *proxy.window();

    // FIXME: Preventing Frame from being destroyed is essentially unnecessary.
    // https://bugs.webkit.org/show_bug.cgi?id=164763
    Ref protectedFrame { m_frame.get() };

    NakedPtr<JSC::Exception> evaluationException;
    auto returnValue = JSExecState::linkAndEvaluateModule(lexicalGlobalObject, Identifier::fromUid(vm, moduleScript.protectedModuleKey().get()), jsUndefined(), evaluationException);
    if (evaluationException) {
        // FIXME: Give a chance to dump the stack trace if the "crossorigin" attribute allows.
        // https://bugs.webkit.org/show_bug.cgi?id=164539
        constexpr bool fromModule = true;
        reportException(&lexicalGlobalObject, evaluationException, nullptr, fromModule);
        return jsUndefined();
    }
    return returnValue;
}

JSC::JSValue ScriptController::linkAndEvaluateModuleScript(LoadableModuleScript& moduleScript)
{
    return linkAndEvaluateModuleScriptInWorld(moduleScript, mainThreadNormalWorldSingleton());
}

JSC::JSValue ScriptController::evaluateModule(const URL& sourceURL, AbstractModuleRecord& moduleRecord, DOMWrapperWorld& world, JSC::JSValue awaitedValue, JSC::JSValue resumeMode)
{
    JSC::VM& vm = world.vm();
    JSLockHolder lock(vm);

    auto& proxy = jsWindowProxy(world);
    auto& lexicalGlobalObject = *proxy.window();

    Ref frame = m_frame.get();
    SetForScope sourceURLScope(m_sourceURL, &sourceURL);

#if ENABLE(WEBASSEMBLY)
    const bool isWasmModule = moduleRecord.inherits<WebAssemblyModuleRecord>();
#else
    constexpr bool isWasmModule = false;
#endif
    if (isWasmModule) {
        // FIXME: Provide better inspector support for Wasm scripts.
        InspectorInstrumentation::willEvaluateScript(protectedFrame(), sourceURL.string(), 1, 1);
    } else if (moduleRecord.inherits<JSC::SyntheticModuleRecord>())
        InspectorInstrumentation::willEvaluateScript(frame.get(), sourceURL.string(), 1, 1);
    else {
        auto* jsModuleRecord = jsCast<JSModuleRecord*>(&moduleRecord);
        const auto& jsSourceCode = jsModuleRecord->sourceCode();
        InspectorInstrumentation::willEvaluateScript(protectedFrame(), sourceURL.string(), jsSourceCode.firstLine().oneBasedInt(), jsSourceCode.startColumn().oneBasedInt());
    }
    auto returnValue = moduleRecord.evaluate(&lexicalGlobalObject, awaitedValue, resumeMode);
    InspectorInstrumentation::didEvaluateScript(protectedFrame());

    return returnValue;
}

JSC::JSValue ScriptController::evaluateModule(const URL& sourceURL, AbstractModuleRecord& moduleRecord, JSC::JSValue awaitedValue, JSC::JSValue resumeMode)
{
    return evaluateModule(sourceURL, moduleRecord, mainThreadNormalWorldSingleton(), awaitedValue, resumeMode);
}

Ref<DOMWrapperWorld> ScriptController::createWorld(const String& name, WorldType type)
{
    return DOMWrapperWorld::create(commonVM(), type == WorldType::User ? DOMWrapperWorld::Type::User : DOMWrapperWorld::Type::Internal, name);
}

void ScriptController::getAllWorlds(Vector<Ref<DOMWrapperWorld>>& worlds)
{
    downcast<JSVMClientData>(commonVM().clientData)->getAllWorlds(worlds);
}

void ScriptController::initScriptForWindowProxy(JSWindowProxy& windowProxy)
{
    Ref world = windowProxy.world();
    JSC::VM& vm = world->vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    jsCast<JSDOMWindow*>(windowProxy.window())->updateDocument();
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception());

    windowProxy.window()->setConsoleClient(m_frame->console());

    if (RefPtr document = m_frame->document())
        document->checkedContentSecurityPolicy()->didCreateWindowProxy(windowProxy);

    if (RefPtr page = m_frame->page()) {
        windowProxy.attachDebugger(page->debugger());
        windowProxy.window()->setProfileGroup(page->group().identifier());
    }

    protectedFrame()->loader().dispatchDidClearWindowObjectInWorld(world);
}

Ref<LocalFrame> ScriptController::protectedFrame() const
{
    return m_frame;
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
        auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        if (errorValue.isObject()) {
            auto* object = JSC::asObject(errorValue);
            if (JSValue failureKindValue = object->getDirect(vm, vm.propertyNames->builtinNames().moduleFetchFailureKindPrivateName())) {
                // This is host propagated error in the module loader pipeline.
                switch (static_cast<ModuleFetchFailureKind>(failureKindValue.asInt32())) {
                case ModuleFetchFailureKind::WasPropagatedError:
                    moduleScript->notifyLoadFailed(LoadableScript::Error {
                        LoadableScript::ErrorType::Fetch,
                        { },
                        { }
                    });
                    break;
                // For a fetch error that was not propagated from further in the
                // pipeline, we include the console error message but do not
                // include an error value as it should not be reported.
                case ModuleFetchFailureKind::WasFetchError:
                    moduleScript->notifyLoadFailed(LoadableScript::Error {
                        LoadableScript::ErrorType::Fetch,
                        LoadableScript::ConsoleMessage {
                            MessageSource::JS,
                            MessageLevel::Error,
                            retrieveErrorMessage(*globalObject, vm, errorValue, scope),
                        },
                        { }
                    });
                    break;
                case ModuleFetchFailureKind::WasResolveError:
                    moduleScript->notifyLoadFailed(LoadableScript::Error {
                        LoadableScript::ErrorType::Resolve,
                        LoadableScript::ConsoleMessage {
                            MessageSource::JS,
                            MessageLevel::Error,
                            retrieveErrorMessage(*globalObject, vm, errorValue, scope),
                        },
                        // The error value is included so that it can be reported to the
                        // appropriate global object.
                        { vm, errorValue }
                    });
                    break;
                case ModuleFetchFailureKind::WasCanceled:
                    moduleScript->notifyLoadWasCanceled();
                    break;
                }
                return JSValue::encode(jsUndefined());
            }
        }

        moduleScript->notifyLoadFailed(LoadableScript::Error {
            LoadableScript::ErrorType::Script,
            LoadableScript::ConsoleMessage {
                MessageSource::JS,
                MessageLevel::Error,
                retrieveErrorMessage(*globalObject, vm, errorValue, scope),
            },
            // The error value is included so that it can be reported to the
            // appropriate global object.
            { vm, errorValue }
        });
        return JSValue::encode(jsUndefined());
    });

    promise.then(&lexicalGlobalObject, &fulfillHandler, &rejectHandler);
}

WindowProxy& ScriptController::windowProxy()
{
    return m_frame->windowProxy();
}

JSWindowProxy& ScriptController::jsWindowProxy(DOMWrapperWorld& world)
{
    auto* jsWindowProxy = protect(protectedFrame()->windowProxy())->jsWindowProxy(world);
    ASSERT_WITH_MESSAGE(jsWindowProxy, "The JSWindowProxy can only be null if the frame has been destroyed");
    return *jsWindowProxy;
}

TextPosition ScriptController::eventHandlerPosition() const
{
    // FIXME: If we are not currently parsing, we should use our current location
    // in JavaScript, to cover cases like "element.setAttribute('click', ...)".

    // FIXME: This location maps to the end of the HTML tag, and not to the
    // exact column number belonging to the event handler attribute.
    if (RefPtr parser = protect(m_frame->document())->scriptableDocumentParser())
        return parser->textPosition();
    return TextPosition();
}

void ScriptController::setEvalEnabled(bool value, const String& errorMessage)
{
    auto* jsWindowProxy = protect(windowProxy())->existingJSWindowProxy(mainThreadNormalWorldSingleton());
    if (!jsWindowProxy)
        return;
    jsWindowProxy->window()->setEvalEnabled(value, errorMessage);
}

void ScriptController::setWebAssemblyEnabled(bool value, const String& errorMessage)
{
    auto* jsWindowProxy = protect(windowProxy())->existingJSWindowProxy(mainThreadNormalWorldSingleton());
    if (!jsWindowProxy)
        return;
    jsWindowProxy->window()->setWebAssemblyEnabled(value, errorMessage);
}

void ScriptController::setTrustedTypesEnforcement(JSC::TrustedTypesEnforcement enforcement)
{
    auto* proxy = protect(windowProxy())->existingJSWindowProxy(mainThreadNormalWorldSingleton());
    if (!proxy)
        return;
    proxy->window()->setTrustedTypesEnforcement(enforcement);
}

bool ScriptController::canAccessFromCurrentOrigin(LocalFrame* frame, Document& accessingDocument)
{
    auto* lexicalGlobalObject = JSExecState::currentState();

    // If the current lexicalGlobalObject is null we should use the accessing document for the security check.
    if (!lexicalGlobalObject) {
        RefPtr targetDocument = frame ? frame->document() : nullptr;
        return targetDocument && protect(accessingDocument.securityOrigin())->isSameOriginDomain(protect(targetDocument->securityOrigin()));
    }

    return BindingSecurity::shouldAllowAccessToFrame(lexicalGlobalObject, frame);
}

void ScriptController::updateDocument()
{
    for (auto& jsWindowProxy : protect(windowProxy())->jsWindowProxiesAsVector()) {
        JSLockHolder lock(jsWindowProxy->world().vm());
        jsCast<JSDOMWindow*>(jsWindowProxy->window())->updateDocument();
    }
}

Bindings::RootObject* ScriptController::cacheableBindingRootObject()
{
    if (!canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
        return nullptr;

    if (!m_cacheableBindingRootObject) {
        JSLockHolder lock(commonVM());
        m_cacheableBindingRootObject = Bindings::RootObject::create(nullptr, globalObject(pluginWorldSingleton()));
    }
    return m_cacheableBindingRootObject.get();
}

Bindings::RootObject* ScriptController::bindingRootObject()
{
    if (!canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
        return nullptr;

    if (!m_bindingRootObject) {
        JSLockHolder lock(commonVM());
        m_bindingRootObject = Bindings::RootObject::create(nullptr, globalObject(pluginWorldSingleton()));
    }
    return m_bindingRootObject.get();
}

RefPtr<JSC::Bindings::RootObject> ScriptController::protectedBindingRootObject()
{
    return bindingRootObject();
}

Ref<Bindings::RootObject> ScriptController::createRootObject(void* nativeHandle)
{
    auto it = m_rootObjects.find(nativeHandle);
    if (it != m_rootObjects.end())
        return it->value.copyRef();

    auto rootObject = Bindings::RootObject::create(nativeHandle, globalObject(pluginWorldSingleton()));

    m_rootObjects.set(nativeHandle, rootObject.copyRef());
    return rootObject;
}

void ScriptController::collectIsolatedContexts(Vector<std::pair<JSC::JSGlobalObject*, RefPtr<SecurityOrigin>>>& result)
{
    for (auto& jsWindowProxy : protect(windowProxy())->jsWindowProxiesAsVector()) {
        auto* lexicalGlobalObject = jsWindowProxy->window();
        RefPtr origin = downcast<LocalDOMWindow>(jsWindowProxy->protectedWrapped())->protectedDocument()->securityOrigin();
        result.append(std::make_pair(lexicalGlobalObject, WTF::move(origin)));
    }
}

#if !PLATFORM(COCOA)
RefPtr<JSC::Bindings::Instance> ScriptController::createScriptInstanceForWidget(Widget*)
{
    return nullptr;
}
#endif

JSObject* ScriptController::jsObjectForPluginElement(HTMLPlugInElement* plugin)
{
    // Can't create JSObjects when JavaScript is disabled
    if (!canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
        return nullptr;

    JSLockHolder lock(commonVM());

    // Create a JSObject bound to this element
    auto* globalObj = globalObject(pluginWorldSingleton());
    // FIXME: is normal okay? - used for NP plugins?
    JSValue jsElementValue = plugin ? toJS(globalObj, globalObj, *plugin) : jsNull();
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

    Ref { it->value }->invalidate();
    m_rootObjects.remove(it);
}

void ScriptController::clearScriptObjects()
{
    JSLockHolder lock(commonVM());

    for (auto& rootObject : m_rootObjects.values())
        rootObject->invalidate();

    m_rootObjects.clear();

    if (m_bindingRootObject) {
        Ref { *m_bindingRootObject }->invalidate();
        m_bindingRootObject = nullptr;
    }
}

JSC::JSValue ScriptController::executeScriptIgnoringException(const String& script, JSC::SourceTaintedOrigin taintedness, bool forceUserGesture)
{
    return executeScriptInWorldIgnoringException(mainThreadNormalWorldSingleton(), script, taintedness, forceUserGesture);
}

JSC::JSValue ScriptController::executeScriptInWorldIgnoringException(DOMWrapperWorld& world, const String& script, JSC::SourceTaintedOrigin taintedness, bool forceUserGesture)
{
    auto result = executeScriptInWorld(world, { script, taintedness, URL { }, false, std::nullopt, forceUserGesture, RemoveTransientActivation::Yes });
    return result ? result.value() : JSC::JSValue { };
}

ValueOrException ScriptController::executeScriptInWorld(DOMWrapperWorld& world, RunJavaScriptParameters&& parameters)
{
#if ENABLE(APP_BOUND_DOMAINS)
    if (m_frame->loader().client().shouldEnableInAppBrowserPrivacyProtections()) {
        if (RefPtr document = m_frame->document())
            document->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "Ignoring user script injection for non-app bound domain."_s);
        SCRIPTCONTROLLER_RELEASE_LOG_ERROR(Loading, "executeScriptInWorld: Ignoring user script injection for non app-bound domain");
        return makeUnexpected(ExceptionDetails { "Ignoring user script injection for non-app bound domain"_s });
    }
    m_frame->loader().client().notifyPageOfAppBoundBehavior();
#endif

    UserGestureIndicator gestureIndicator(parameters.forceUserGesture == ForceUserGesture::Yes ? std::optional<IsProcessingUserGesture>(IsProcessingUserGesture::Yes) : std::nullopt, m_frame->document(), UserGestureType::ActivationTriggering, UserGestureIndicator::ProcessInteractionStyle::Never);

    if (parameters.forceUserGesture == ForceUserGesture::Yes && UserGestureIndicator::currentUserGesture() && parameters.removeTransientActivation == RemoveTransientActivation::Yes) {
        UserGestureIndicator::currentUserGesture()->addDestructionObserver([](UserGestureToken& token) {
            token.forEachImpactedDocument([](Document& document) {
                if (RefPtr window = document.window())
                    window->consumeTransientActivation();
            });
        });
    }

    if (!canExecuteScripts(ReasonForCallingCanExecuteScripts::AboutToExecuteScript, &world) || isPaused())
        return makeUnexpected(ExceptionDetails { "Cannot execute JavaScript in this document"_s });

    auto sourceURL = parameters.sourceURL;
    if (!sourceURL.isValid()) {
        // FIXME: This is gross, but when setTimeout() and setInterval() are passed JS strings, the thrown errors should use the frame document URL (according to WPT).
        sourceURL = m_frame->document()->url();
    }

    switch (parameters.runAsAsyncFunction) {
    case RunAsAsyncFunction::No:
        return evaluateInWorld(ScriptSourceCode { parameters.source, parameters.taintedness, WTF::move(sourceURL), TextPosition(), JSC::SourceProviderSourceType::Program, CachedScriptFetcher::create(protect(m_frame->document())->charset()) }, world);
    case RunAsAsyncFunction::Yes:
        return callInWorld(WTF::move(parameters), world);
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
    functionStringBuilder.append("(async function("_s);
    for (auto argument = parameters.arguments->begin(); argument != parameters.arguments->end();) {
        functionStringBuilder.append(argument->key);

        auto scope = DECLARE_TOP_EXCEPTION_SCOPE(globalObject.vm());
        auto jsArgument = argument->value(globalObject);
        if (scope.exception()) [[unlikely]] {
            errorMessage = "Unable to deserialize argument to execute asynchronous JavaScript function"_s;
            break;
        }

        markedArguments.append(jsArgument);

        ++argument;
        if (argument != parameters.arguments->end())
            functionStringBuilder.append(',');
    }
    ASSERT(!markedArguments.hasOverflowed());

    if (!errorMessage.isEmpty())
        return makeUnexpected(ExceptionDetails { errorMessage });

    functionStringBuilder.append("){"_s, parameters.source, "})"_s);

    auto sourceCode = ScriptSourceCode { functionStringBuilder.toString(), parameters.taintedness, WTF::move(parameters.sourceURL), TextPosition(), JSC::SourceProviderSourceType::Program, CachedScriptFetcher::create(protect(m_frame->document())->charset()) };
    const auto& jsSourceCode = sourceCode.jsSourceCode();

    const URL& sourceURL = jsSourceCode.provider()->sourceOrigin().url();

    Ref protector { m_frame.get() };
    SetForScope sourceURLScope(m_sourceURL, &sourceURL);

    InspectorInstrumentation::willEvaluateScript(protectedFrame(), sourceURL.string(), sourceCode.startLine(), sourceCode.startColumn());

    NakedPtr<JSC::Exception> evaluationException;
    std::optional<ExceptionDetails> optionalDetails;
    JSValue returnValue;
    do {
        JSValue functionObject = JSExecState::profiledEvaluate(&globalObject, JSC::ProfilingReason::Other, jsSourceCode, &proxy, evaluationException);

        if (evaluationException)
            break;

        if (!functionObject || !functionObject.isCallable()) {
            optionalDetails = { { "Unable to create JavaScript async function to call"_s } };
            break;
        }

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=205562
        // Getting CallData shouldn't be required to call into JS.
        auto callData = JSC::getCallData(functionObject);
        if (callData.type == CallData::Type::None) {
            optionalDetails = { { "Unable to prepare JavaScript async function to be called"_s } };
            break;
        }

        returnValue = JSExecState::profiledCall(&globalObject, JSC::ProfilingReason::Other, functionObject, callData, &proxy, markedArguments, evaluationException);
    } while (false);

    InspectorInstrumentation::didEvaluateScript(protectedFrame());

    if (evaluationException && !optionalDetails) {
        ExceptionDetails details;
        reportException(&globalObject, evaluationException, sourceCode.cachedScript(), false, &details);
        optionalDetails = WTF::move(details);
    }

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
    return executeScriptInWorld(world, { script, JSC::SourceTaintedOrigin::Untainted, URL { }, false, std::nullopt, forceUserGesture, RemoveTransientActivation::No });
}

void ScriptController::executeAsynchronousUserAgentScriptInWorld(DOMWrapperWorld& world, RunJavaScriptParameters&& parameters, ResolveFunction&& resolveCompletionHandler)
{
    auto runAsAsyncFunction = parameters.runAsAsyncFunction;
    auto result = executeScriptInWorld(world, WTF::move(parameters));
    
    if (runAsAsyncFunction == RunAsAsyncFunction::No || !result || !result.value().isObject()) {
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

    auto callData = JSC::getCallData(thenFunction);
    if (callData.type == CallData::Type::None) {
        resolveCompletionHandler(result);
        return;
    }

    auto sharedResolveFunction = createSharedTask<void(ValueOrException)>([resolveCompletionHandler = WTF::move(resolveCompletionHandler)](ValueOrException result) mutable {
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
    auto finalizeGuard = createSharedTask<void()>([sharedResolveFunction = WTF::move(sharedResolveFunction), finalizeCount = WTF::move(finalizeCount)]() {
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
    ASSERT(!arguments.hasOverflowed());

    call(&globalObject, thenFunction, callData, result.value(), arguments);
}

bool ScriptController::canExecuteScripts(ReasonForCallingCanExecuteScripts reason, DOMWrapperWorld* world)
{
    if (reason == ReasonForCallingCanExecuteScripts::AboutToExecuteScript)
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());

    if (world && !world->isNormal())
        return true;

    if (m_frame->document() && m_frame->document()->isSandboxed(SandboxFlag::Scripts)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        if (reason == ReasonForCallingCanExecuteScripts::AboutToExecuteScript || reason == ReasonForCallingCanExecuteScripts::AboutToCreateEventListener)
            protect(m_frame->document())->addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Blocked script execution in '"_s, m_frame->document()->url().stringCenterEllipsizedToLength(), "' because the document's frame is sandboxed and the 'allow-scripts' permission is not set."_s));
        return false;
    }

    if (!m_frame->page())
        return false;

    return m_frame->loader().client().allowScript(m_frame->settings().isScriptEnabled());
}

void ScriptController::executeJavaScriptURL(const URL& url, const NavigationAction& action, bool& didReplaceDocument)
{
    ASSERT(url.protocolIsJavaScript());

    // We need to hold onto the Frame here because executing script can
    // destroy the frame.
    Ref frame = m_frame.get();
    RefPtr ownerDocument = m_frame->document();

    RefPtr requesterSecurityOrigin = action.requester() ? action.requester()->securityOrigin.ptr() : nullptr;
    if (requesterSecurityOrigin && !requesterSecurityOrigin->isSameOriginDomain(protect(ownerDocument->securityOrigin())))
        return;

    if (!frame->page())
        return;

    JSDOMGlobalObject* globalObject = jsWindowProxy(mainThreadNormalWorldSingleton()).window();

    RefPtr scriptExecutionContext = globalObject->scriptExecutionContext();
    if (!scriptExecutionContext)
        return;

    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto preNavigationCheckHolder = requireTrustedTypesForPreNavigationCheckPasses(*scriptExecutionContext, url.string());
    if (preNavigationCheckHolder.hasException()) {
        TRY_CLEAR_EXCEPTION(throwScope, void());
        return;
    }

    auto preNavigationCheckURLString = preNavigationCheckHolder.releaseReturnValue();
    if (preNavigationCheckURLString.isNull())
        return;

    if (!ownerDocument->checkedContentSecurityPolicy()->allowJavaScriptURLs(ownerDocument->url().string(), eventHandlerPosition().m_line, preNavigationCheckURLString, nullptr))
        return;

    const int javascriptSchemeLength = sizeof("javascript:") - 1;
    String decodedURL = PAL::decodeURLEscapeSequences(preNavigationCheckURLString);
    // FIXME: This probably needs to figure out if the origin is considered tainted.
    auto result = executeScriptIgnoringException(decodedURL.substring(javascriptSchemeLength), JSC::SourceTaintedOrigin::Untainted);
    RELEASE_ASSERT(&vm == &jsWindowProxy(mainThreadNormalWorldSingleton()).window()->vm());

    // If executing script caused this frame to be removed from the page, we
    // don't want to try to replace its document!
    if (!frame->page())
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
    if (action.shouldReplaceDocumentIfJavaScriptURL() == ReplaceDocumentIfJavaScriptURL) {
        RefPtr documentLoader = protect(m_frame->document())->loader();

        // We're still in a frame, so there should be a DocumentLoader.
        ASSERT(documentLoader);

        // If there is no current history item, create one since we're about to commit a document
        // from the JS URL.
        if (!m_frame->loader().history().currentItem())
            m_frame->loader().history().updateForRedirectWithLockedBackForwardList();

        // Since we're replacing the document, this JavaScript URL load acts as a "Replace" navigation.
        // Make sure the triggering action get set on the DocumentLoader since some logic in
        // FrameLoader::didBeginDocument() relies on it for example.
        if (documentLoader)
            documentLoader->setTriggeringAction(NavigationAction { action });

        // Signal to FrameLoader to disable navigations within this frame while replacing it with the result of executing javascript
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=200523
        // The only reason we do a nestable save/restore of this flag here is because we sometimes nest javascript: url loads as
        // some will load synchronously. We'd like to remove those synchronous loads and then change this.
        SetForScope willBeReplaced(m_willReplaceWithResultOfExecutingJavascriptURL, true);

        if (documentLoader) {
            documentLoader->writer().replaceDocumentWithResultOfExecutingJavascriptURL(scriptResult, ownerDocument.get());
            didReplaceDocument = true;
        }
    }
}

void ScriptController::reportExceptionFromScriptError(LoadableScript::Error error, bool isModule)
{
    auto& world = mainThreadNormalWorldSingleton();
    JSC::VM& vm = world.vm();
    JSLockHolder lock(vm);

    auto& proxy = jsWindowProxy(world);
    auto& lexicalGlobalObject = *proxy.window();

    reportException(&lexicalGlobalObject, error.errorValue.get(), nullptr, isModule);
}

class ImportMapLogReporter final : public JSC::ImportMap::Reporter {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    ImportMapLogReporter(JSDOMGlobalObject* globalObject)
        : m_globalObject(globalObject)
    {
    }

    void reportWarning(const String& message) const final
    {
        m_globalObject->protectedScriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, message);
    }

    void reportError(const String& message) const final
    {
        m_globalObject->protectedScriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, message);
    }

private:
    JSDOMGlobalObject* m_globalObject;
};

void ScriptController::registerImportMap(const ScriptSourceCode& sourceCode, const URL& baseURL)
{
    auto& world = mainThreadNormalWorldSingleton();
    JSC::VM& vm = world.vm();
    JSLockHolder lock(vm);
    JSDOMGlobalObject* globalObject = jsWindowProxy(world).window();
    ImportMapLogReporter reporter(globalObject);
    auto newImportMap = ImportMap::parseImportMapString(sourceCode.jsSourceCode(), baseURL, reporter);

    if (newImportMap)
        globalObject->importMap().mergeExistingAndNewImportMaps(WTF::move(newImportMap.value()), reporter);
}

bool ScriptController::registerSpeculationRules(Node& sourceNode, const ScriptSourceCode& sourceCode, const URL& baseURL)
{
    RefPtr document = m_frame->document();
    if (!document || !document->settings().speculationRulesPrefetchEnabled())
        return false;

    return document->speculationRules()->parseSpeculationRules(sourceNode, sourceCode.source(), baseURL, document->url());
}

} // namespace WebCore

#undef SCRIPTCONTROLLER_RELEASE_LOG_ERROR
