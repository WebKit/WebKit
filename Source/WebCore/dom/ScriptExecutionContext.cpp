/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "ScriptExecutionContext.h"

#include "CSSValuePool.h"
#include "CachedScript.h"
#include "CommonVM.h"
#include "CrossOriginOpenerPolicy.h"
#include "DOMTimer.h"
#include "DOMWindow.h"
#include "DatabaseContext.h"
#include "Document.h"
#include "ErrorEvent.h"
#include "FontLoadRequest.h"
#include "FrameDestructionObserverInlines.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDOMWindow.h"
#include "JSWorkerGlobalScope.h"
#include "JSWorkletGlobalScope.h"
#include "LegacySchemeRegistry.h"
#include "MessagePort.h"
#include "Navigator.h"
#include "Page.h"
#include "Performance.h"
#include "PublicURLManager.h"
#include "RTCDataChannelRemoteHandlerConnection.h"
#include "RejectedPromiseTracker.h"
#include "ResourceRequest.h"
#include "SWClientConnection.h"
#include "SWContextManager.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ServiceWorker.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerProvider.h"
#include "Settings.h"
#include "WebCoreJSClientData.h"
#include "WebCoreOpaqueRoot.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerNavigator.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerOrWorkletScriptController.h"
#include "WorkerOrWorkletThread.h"
#include "WorkerThread.h"
#include "WorkletGlobalScope.h"
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/DeferredWorkTimer.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSPromise.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/StackVisitor.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/VM.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>

namespace WebCore {
using namespace Inspector;

static std::atomic<CrossOriginMode> globalCrossOriginMode { CrossOriginMode::Shared };

static Lock allScriptExecutionContextsMapLock;
static HashMap<ScriptExecutionContextIdentifier, ScriptExecutionContext*>& allScriptExecutionContextsMap() WTF_REQUIRES_LOCK(allScriptExecutionContextsMapLock)
{
    static NeverDestroyed<HashMap<ScriptExecutionContextIdentifier, ScriptExecutionContext*>> contexts;
    ASSERT(allScriptExecutionContextsMapLock.isLocked());
    return contexts;
}

struct ScriptExecutionContext::PendingException {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PendingException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, RefPtr<ScriptCallStack>&& callStack)
        : m_errorMessage(errorMessage)
        , m_lineNumber(lineNumber)
        , m_columnNumber(columnNumber)
        , m_sourceURL(sourceURL)
        , m_callStack(WTFMove(callStack))
    {
    }
    String m_errorMessage;
    int m_lineNumber;
    int m_columnNumber;
    String m_sourceURL;
    RefPtr<ScriptCallStack> m_callStack;
};

ScriptExecutionContext::ScriptExecutionContext(ScriptExecutionContextIdentifier contextIdentifier)
    : m_identifier(contextIdentifier ? contextIdentifier : ScriptExecutionContextIdentifier::generate())
{
}

void ScriptExecutionContext::regenerateIdentifier()
{
    Locker locker { allScriptExecutionContextsMapLock };

    ASSERT(allScriptExecutionContextsMap().contains(m_identifier));
    allScriptExecutionContextsMap().remove(m_identifier);

    m_identifier = ScriptExecutionContextIdentifier::generate();

    ASSERT(!allScriptExecutionContextsMap().contains(m_identifier));
    allScriptExecutionContextsMap().add(m_identifier, this);
}

void ScriptExecutionContext::addToContextsMap()
{
    Locker locker { allScriptExecutionContextsMapLock };
    ASSERT(!allScriptExecutionContextsMap().contains(m_identifier));
    allScriptExecutionContextsMap().add(m_identifier, this);
}

void ScriptExecutionContext::removeFromContextsMap()
{
    Locker locker { allScriptExecutionContextsMapLock };
    ASSERT(allScriptExecutionContextsMap().contains(m_identifier));
    allScriptExecutionContextsMap().remove(m_identifier);
}

#if !ASSERT_ENABLED

inline void ScriptExecutionContext::checkConsistency() const
{
}

#else // ASSERT_ENABLED

void ScriptExecutionContext::checkConsistency() const
{
    for (auto* messagePort : m_messagePorts)
        ASSERT(messagePort->scriptExecutionContext() == this);

    for (auto* destructionObserver : m_destructionObservers)
        ASSERT(destructionObserver->scriptExecutionContext() == this);

    for (auto* activeDOMObject : m_activeDOMObjects) {
        ASSERT(activeDOMObject->scriptExecutionContext() == this);
        activeDOMObject->assertSuspendIfNeededWasCalled();
    }
}

#endif // ASSERT_ENABLED

ScriptExecutionContext::~ScriptExecutionContext()
{
    checkConsistency();

#if ASSERT_ENABLED
    {
        Locker locker { allScriptExecutionContextsMapLock };
        ASSERT_WITH_MESSAGE(!allScriptExecutionContextsMap().contains(m_identifier), "A ScriptExecutionContext subclass instance implementing postTask should have already removed itself from the map");
    }

    m_inScriptExecutionContextDestructor = true;
#endif // ASSERT_ENABLED

    auto callbacks = WTFMove(m_notificationCallbacks);
    for (auto& callback : callbacks.values())
        callback();

    auto postMessageCompletionHandlers = WTFMove(m_processMessageWithMessagePortsSoonHandlers);
    for (auto& completionHandler : postMessageCompletionHandlers)
        completionHandler();

#if ENABLE(SERVICE_WORKER)
    setActiveServiceWorker(nullptr);
#endif

    while (auto* destructionObserver = m_destructionObservers.takeAny())
        destructionObserver->contextDestroyed();

#if ASSERT_ENABLED
    m_inScriptExecutionContextDestructor = false;
#endif
}

void ScriptExecutionContext::processMessageWithMessagePortsSoon(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isContextThread());
    m_processMessageWithMessagePortsSoonHandlers.append(WTFMove(completionHandler));

    if (m_willprocessMessageWithMessagePortsSoon)
        return;

    m_willprocessMessageWithMessagePortsSoon = true;
    postTask([] (ScriptExecutionContext& context) {
        context.dispatchMessagePortEvents();
    });
}

void ScriptExecutionContext::dispatchMessagePortEvents()
{
    ASSERT(isContextThread());
    checkConsistency();

    Ref<ScriptExecutionContext> protectedThis(*this);
    ASSERT(m_willprocessMessageWithMessagePortsSoon);
    m_willprocessMessageWithMessagePortsSoon = false;

    auto completionHandlers = std::exchange(m_processMessageWithMessagePortsSoonHandlers, Vector<CompletionHandler<void()>> { });

    // Make a frozen copy of the ports so we can iterate while new ones might be added or destroyed.
    for (auto* messagePort : copyToVector(m_messagePorts)) {
        // The port may be destroyed, and another one created at the same address,
        // but this is harmless. The worst that can happen as a result is that
        // dispatchMessages() will be called needlessly.
        if (m_messagePorts.contains(messagePort) && messagePort->started())
            messagePort->dispatchMessages();
    }

    for (auto& completionHandler : completionHandlers)
        completionHandler();
}

void ScriptExecutionContext::createdMessagePort(MessagePort& messagePort)
{
    ASSERT(isContextThread());

    m_messagePorts.add(&messagePort);
}

void ScriptExecutionContext::destroyedMessagePort(MessagePort& messagePort)
{
    ASSERT(isContextThread());

    m_messagePorts.remove(&messagePort);
}

void ScriptExecutionContext::didLoadResourceSynchronously(const URL&)
{
}

CSSValuePool& ScriptExecutionContext::cssValuePool()
{
    return CSSValuePool::singleton();
}

std::unique_ptr<FontLoadRequest> ScriptExecutionContext::fontLoadRequest(const String&, bool, bool, LoadedFromOpaqueSource)
{
    return nullptr;
}

void ScriptExecutionContext::forEachActiveDOMObject(const Function<ShouldContinue(ActiveDOMObject&)>& apply) const
{
    // It is not allowed to run arbitrary script or construct new ActiveDOMObjects while we are iterating over ActiveDOMObjects.
    // An ASSERT_WITH_SECURITY_IMPLICATION or RELEASE_ASSERT will fire if this happens, but it's important to code
    // suspend() / resume() / stop() functions so it will not happen!
    ScriptDisallowedScope scriptDisallowedScope;
    SetForScope activeDOMObjectAdditionForbiddenScope(m_activeDOMObjectAdditionForbidden, true);

    // Make a frozen copy of the objects so we can iterate while new ones might be destroyed.
    auto possibleActiveDOMObjects = copyToVector(m_activeDOMObjects);

    for (auto* activeDOMObject : possibleActiveDOMObjects) {
        // Check if this object was deleted already. If so, just skip it.
        // Calling contains on a possibly-already-deleted object is OK because we guarantee
        // no new object can be added, so even if a new object ends up allocated with the
        // same address, that will be *after* this function exits.
        if (!m_activeDOMObjects.contains(activeDOMObject))
            continue;

        if (apply(*activeDOMObject) == ShouldContinue::No)
            break;
    }
}

JSC::ScriptExecutionStatus ScriptExecutionContext::jscScriptExecutionStatus() const
{
    if (activeDOMObjectsAreSuspended())
        return JSC::ScriptExecutionStatus::Suspended;
    if (activeDOMObjectsAreStopped())
        return JSC::ScriptExecutionStatus::Stopped;
    return JSC::ScriptExecutionStatus::Running;
}

URL ScriptExecutionContext::currentSourceURL() const
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return { };

    auto& vm = globalObject->vm();
    auto* topCallFrame = vm.topCallFrame;
    if (!topCallFrame)
        return { };

    URL sourceURL;
    StackVisitor::visit(topCallFrame, vm, [&sourceURL](auto& visitor) {
        if (visitor->isNativeFrame())
            return IterationStatus::Continue;

        auto urlString = visitor->sourceURL();
        if (urlString.isEmpty())
            return IterationStatus::Continue;

        sourceURL = URL { WTFMove(urlString) };
        if (sourceURL.isValid())
            return IterationStatus::Continue;

        return IterationStatus::Done;
    });
    return sourceURL;
}

void ScriptExecutionContext::suspendActiveDOMObjects(ReasonForSuspension why)
{
    checkConsistency();

    if (m_activeDOMObjectsAreSuspended) {
        // A page may subsequently suspend DOM objects, say as part of entering the back/forward cache, after the embedding
        // client requested the page be suspended. We ignore such requests so long as the embedding client requested
        // the suspension first. See <rdar://problem/13754896> for more details.
        ASSERT(m_reasonForSuspendingActiveDOMObjects == ReasonForSuspension::PageWillBeSuspended);
        return;
    }

    m_activeDOMObjectsAreSuspended = true;

    forEachActiveDOMObject([why](auto& activeDOMObject) {
        activeDOMObject.suspend(why);
        return ShouldContinue::Yes;
    });

    m_reasonForSuspendingActiveDOMObjects = why;
}

void ScriptExecutionContext::resumeActiveDOMObjects(ReasonForSuspension why)
{
    checkConsistency();

    if (m_reasonForSuspendingActiveDOMObjects != why)
        return;

    forEachActiveDOMObject([](auto& activeDOMObject) {
        activeDOMObject.resume();
        return ShouldContinue::Yes;
    });

    vm().deferredWorkTimer->didResumeScriptExecutionOwner();

    m_activeDOMObjectsAreSuspended = false;

    // In case there were pending messages at the time the script execution context entered the BackForwardCache,
    // make sure those get dispatched shortly after restoring from the BackForwardCache.
    processMessageWithMessagePortsSoon([] { });
}

void ScriptExecutionContext::stopActiveDOMObjects()
{
    checkConsistency();

    if (m_activeDOMObjectsAreStopped)
        return;
    m_activeDOMObjectsAreStopped = true;

    forEachActiveDOMObject([](auto& activeDOMObject) {
        activeDOMObject.stop();
        return ShouldContinue::Yes;
    });
    m_deferredPromises.clear();
}

void ScriptExecutionContext::suspendActiveDOMObjectIfNeeded(ActiveDOMObject& activeDOMObject)
{
    ASSERT(m_activeDOMObjects.contains(&activeDOMObject));
    if (m_activeDOMObjectsAreSuspended)
        activeDOMObject.suspend(m_reasonForSuspendingActiveDOMObjects);
    if (m_activeDOMObjectsAreStopped)
        activeDOMObject.stop();
}

void ScriptExecutionContext::didCreateActiveDOMObject(ActiveDOMObject& activeDOMObject)
{
    // The m_activeDOMObjectAdditionForbidden check is a RELEASE_ASSERT because of the
    // consequences of having an ActiveDOMObject that is not correctly reflected in the set.
    // If we do have one of those, it can possibly be a security vulnerability. So we'd
    // rather have a crash than continue running with the set possibly compromised.
    ASSERT(!m_inScriptExecutionContextDestructor);
    RELEASE_ASSERT(!m_activeDOMObjectAdditionForbidden);
    m_activeDOMObjects.add(&activeDOMObject);
}

void ScriptExecutionContext::willDestroyActiveDOMObject(ActiveDOMObject& activeDOMObject)
{
    m_activeDOMObjects.remove(&activeDOMObject);
}

void ScriptExecutionContext::didCreateDestructionObserver(ContextDestructionObserver& observer)
{
    ASSERT(!m_inScriptExecutionContextDestructor);
    m_destructionObservers.add(&observer);
}

void ScriptExecutionContext::willDestroyDestructionObserver(ContextDestructionObserver& observer)
{
    m_destructionObservers.remove(&observer);
}

RefPtr<RTCDataChannelRemoteHandlerConnection> ScriptExecutionContext::createRTCDataChannelRemoteHandlerConnection()
{
    return nullptr;
}

// FIXME: Should this function be in SecurityContext or SecurityOrigin instead?
bool ScriptExecutionContext::canIncludeErrorDetails(CachedScript* script, const String& sourceURL, bool fromModule)
{
    ASSERT(securityOrigin());
    // Errors from module scripts are never muted.
    if (fromModule)
        return true;
    URL completeSourceURL = completeURL(sourceURL);
    if (completeSourceURL.protocolIsData())
        return true;
    if (script) {
        ASSERT(script->origin());
        ASSERT(securityOrigin()->toString() == script->origin()->toString());
        return script->isCORSSameOrigin();
    }
    return securityOrigin()->canRequest(completeSourceURL);
}

void ScriptExecutionContext::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, JSC::Exception* exception, RefPtr<ScriptCallStack>&& callStack, CachedScript* cachedScript, bool fromModule)
{
    if (m_inDispatchErrorEvent) {
        if (!m_pendingExceptions)
            m_pendingExceptions = makeUnique<Vector<std::unique_ptr<PendingException>>>();
        m_pendingExceptions->append(makeUnique<PendingException>(errorMessage, lineNumber, columnNumber, sourceURL, WTFMove(callStack)));
        return;
    }

    // First report the original exception and only then all the nested ones.
    if (!dispatchErrorEvent(errorMessage, lineNumber, columnNumber, sourceURL, exception, cachedScript, fromModule))
        logExceptionToConsole(errorMessage, sourceURL, lineNumber, columnNumber, callStack.copyRef());

    if (!m_pendingExceptions)
        return;

    auto pendingExceptions = WTFMove(m_pendingExceptions);
    for (auto& exception : *pendingExceptions)
        logExceptionToConsole(exception->m_errorMessage, exception->m_sourceURL, exception->m_lineNumber, exception->m_columnNumber, WTFMove(exception->m_callStack));
}

void ScriptExecutionContext::reportUnhandledPromiseRejection(JSC::JSGlobalObject& state, JSC::JSPromise& promise, RefPtr<Inspector::ScriptCallStack>&& callStack)
{
    Page* page = nullptr;
    if (is<Document>(this))
        page = downcast<Document>(this)->page();
    // FIXME: allow Workers to mute unhandled promise rejection messages.

    if (page && !page->settings().unhandledPromiseRejectionToConsoleEnabled())
        return;

    JSC::VM& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSC::JSValue result = promise.result(vm);
    String resultMessage = retrieveErrorMessage(state, vm, result, scope);
    String errorMessage;

    auto tryMakeErrorString = [&] (unsigned length) -> String {
        bool addEllipsis = length != resultMessage.length();
        return tryMakeString("Unhandled Promise Rejection: ", StringView(resultMessage).left(length), addEllipsis ? "..." : "");
    };

    if (!!resultMessage && !scope.exception()) {
        constexpr unsigned maxLength = 200;
        constexpr unsigned shortLength = 10;
        errorMessage = tryMakeErrorString(std::min(resultMessage.length(), maxLength));
        if (!errorMessage && resultMessage.length() > shortLength)
            errorMessage = tryMakeErrorString(shortLength);
    }

    if (!errorMessage)
        errorMessage = "Unhandled Promise Rejection"_s;

    std::unique_ptr<Inspector::ConsoleMessage> message;
    if (callStack)
        message = makeUnique<Inspector::ConsoleMessage>(MessageSource::JS, MessageType::Log, MessageLevel::Error, errorMessage, callStack.releaseNonNull());
    else
        message = makeUnique<Inspector::ConsoleMessage>(MessageSource::JS, MessageType::Log, MessageLevel::Error, errorMessage);
    addConsoleMessage(WTFMove(message));
}

void ScriptExecutionContext::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, JSC::JSGlobalObject* state, unsigned long requestIdentifier)
{
    addMessage(source, level, message, sourceURL, lineNumber, columnNumber, nullptr, state, requestIdentifier);
}

bool ScriptExecutionContext::dispatchErrorEvent(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, JSC::Exception* exception, CachedScript* cachedScript, bool fromModule)
{
    auto* target = errorEventTarget();
    if (!target)
        return false;

    RefPtr<ErrorEvent> errorEvent;
    if (canIncludeErrorDetails(cachedScript, sourceURL, fromModule))
        errorEvent = ErrorEvent::create(errorMessage, sourceURL, lineNumber, columnNumber, { vm(), exception ? exception->value() : JSC::jsNull() });
    else
        errorEvent = ErrorEvent::create("Script error."_s, { }, 0, 0, { });

    ASSERT(!m_inDispatchErrorEvent);
    m_inDispatchErrorEvent = true;
    target->dispatchEvent(*errorEvent);
    m_inDispatchErrorEvent = false;
    return errorEvent->defaultPrevented();
}

int ScriptExecutionContext::circularSequentialID()
{
    ++m_circularSequentialID;
    if (m_circularSequentialID <= 0)
        m_circularSequentialID = 1;
    return m_circularSequentialID;
}

PublicURLManager& ScriptExecutionContext::publicURLManager()
{
    if (!m_publicURLManager)
        m_publicURLManager = PublicURLManager::create(this);
    return *m_publicURLManager;
}

void ScriptExecutionContext::adjustMinimumDOMTimerInterval(Seconds oldMinimumTimerInterval)
{
    if (minimumDOMTimerInterval() != oldMinimumTimerInterval) {
        for (auto& timer : m_timeouts.values())
            timer->updateTimerIntervalIfNecessary();
    }
}

Seconds ScriptExecutionContext::minimumDOMTimerInterval() const
{
    // The default implementation returns the DOMTimer's default
    // minimum timer interval. FIXME: to make it work with dedicated
    // workers, we will have to override it in the appropriate
    // subclass, and provide a way to enumerate a Document's dedicated
    // workers so we can update them all.
    return DOMTimer::defaultMinimumInterval();
}

void ScriptExecutionContext::didChangeTimerAlignmentInterval()
{
    for (auto& timer : m_timeouts.values())
        timer->didChangeAlignmentInterval();
}

Seconds ScriptExecutionContext::domTimerAlignmentInterval(bool) const
{
    return DOMTimer::defaultAlignmentInterval();
}

RejectedPromiseTracker* ScriptExecutionContext::ensureRejectedPromiseTrackerSlow()
{
    // ScriptExecutionContext::vm() in Worker is only available after WorkerGlobalScope initialization is done.
    // When initializing ScriptExecutionContext, vm() is not ready.

    ASSERT(!m_rejectedPromiseTracker);
    if (is<WorkerOrWorkletGlobalScope>(*this)) {
        auto* scriptController = downcast<WorkerOrWorkletGlobalScope>(*this).script();
        // Do not re-create the promise tracker if we are in a worker / worklet whose execution is terminating.
        if (!scriptController || scriptController->isTerminatingExecution())
            return nullptr;
    }
    m_rejectedPromiseTracker = makeUnique<RejectedPromiseTracker>(*this, vm());
    return m_rejectedPromiseTracker.get();
}

void ScriptExecutionContext::removeRejectedPromiseTracker()
{
    m_rejectedPromiseTracker = nullptr;
}

void ScriptExecutionContext::setDatabaseContext(DatabaseContext* databaseContext)
{
    m_databaseContext = databaseContext;
}

bool ScriptExecutionContext::hasPendingActivity() const
{
    checkConsistency();

    for (auto* activeDOMObject : m_activeDOMObjects) {
        if (activeDOMObject->hasPendingActivity())
            return true;
    }

    return false;
}

JSC::JSGlobalObject* ScriptExecutionContext::globalObject() const
{
    if (is<Document>(*this)) {
        auto frame = downcast<Document>(*this).frame();
        return frame ? frame->script().globalObject(mainThreadNormalWorld()) : nullptr;
    }

    if (is<WorkerOrWorkletGlobalScope>(*this)) {
        auto script = downcast<WorkerOrWorkletGlobalScope>(*this).script();
        return script ? script->globalScopeWrapper() : nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

String ScriptExecutionContext::domainForCachePartition() const
{
    if (!m_domainForCachePartition.isNull())
        return m_domainForCachePartition;

    if (m_storageBlockingPolicy != StorageBlockingPolicy::BlockThirdParty)
        return emptyString();

    return topOrigin().domainForCachePartition();
}

bool ScriptExecutionContext::allowsMediaDevices() const
{
#if ENABLE(MEDIA_STREAM)
    if (!is<Document>(*this))
        return false;
    auto page = downcast<Document>(*this).page();
    return page ? !page->settings().mediaCaptureRequiresSecureConnection() : false;
#else
    return false;
#endif
}

#if ENABLE(SERVICE_WORKER)

ServiceWorker* ScriptExecutionContext::activeServiceWorker() const
{
    return m_activeServiceWorker.get();
}

void ScriptExecutionContext::setActiveServiceWorker(RefPtr<ServiceWorker>&& serviceWorker)
{
    m_activeServiceWorker = WTFMove(serviceWorker);
}

void ScriptExecutionContext::registerServiceWorker(ServiceWorker& serviceWorker)
{
    auto addResult = m_serviceWorkers.add(serviceWorker.identifier(), &serviceWorker);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void ScriptExecutionContext::unregisterServiceWorker(ServiceWorker& serviceWorker)
{
    m_serviceWorkers.remove(serviceWorker.identifier());
}

ServiceWorkerContainer* ScriptExecutionContext::serviceWorkerContainer()
{
    NavigatorBase* navigator = nullptr;
    if (is<Document>(*this)) {
        if (auto* window = downcast<Document>(*this).domWindow())
            navigator = window->optionalNavigator();
    } else
        navigator = downcast<WorkerGlobalScope>(*this).optionalNavigator();

    return navigator ? &navigator->serviceWorker() : nullptr;
}

ServiceWorkerContainer* ScriptExecutionContext::ensureServiceWorkerContainer()
{
    NavigatorBase* navigator = nullptr;
    if (is<Document>(*this)) {
        if (auto* window = downcast<Document>(*this).domWindow())
            navigator = &window->navigator();
    } else
        navigator = &downcast<WorkerGlobalScope>(*this).navigator();
        
    return navigator ? &navigator->serviceWorker() : nullptr;
}

#endif

void ScriptExecutionContext::setCrossOriginMode(CrossOriginMode crossOriginMode)
{
    globalCrossOriginMode = crossOriginMode;
    if (crossOriginMode == CrossOriginMode::Isolated)
        Performance::allowHighPrecisionTime();
}

CrossOriginMode ScriptExecutionContext::crossOriginMode()
{
    return globalCrossOriginMode;
}

bool ScriptExecutionContext::postTaskTo(ScriptExecutionContextIdentifier identifier, Task&& task)
{
    Locker locker { allScriptExecutionContextsMapLock };
    auto* context = allScriptExecutionContextsMap().get(identifier);

    if (!context)
        return false;

    context->postTask(WTFMove(task));
    return true;
}

bool ScriptExecutionContext::postTaskForModeToWorkerOrWorklet(ScriptExecutionContextIdentifier identifier, Task&& task, const String& mode)
{
    Locker locker { allScriptExecutionContextsMapLock };
    auto* context = dynamicDowncast<WorkerOrWorkletGlobalScope>(allScriptExecutionContextsMap().get(identifier));

    if (!context)
        return false;

    context->postTaskForMode(WTFMove(task), mode);
    return true;
}

bool ScriptExecutionContext::ensureOnContextThread(ScriptExecutionContextIdentifier identifier, Task&& task)
{
    ScriptExecutionContext* context = nullptr;
    {
        Locker locker { allScriptExecutionContextsMapLock };
        context = allScriptExecutionContextsMap().get(identifier);

        if (!context)
            return false;

        if (!context->isContextThread()) {
            context->postTask(WTFMove(task));
            return true;
        }
    }

    task.performTask(*context);
    return true;
}

void ScriptExecutionContext::postTaskToResponsibleDocument(Function<void(Document&)>&& callback)
{
    if (is<Document>(this)) {
        callback(downcast<Document>(*this));
        return;
    }

    ASSERT(is<WorkerOrWorkletGlobalScope>(this));
    if (!is<WorkerOrWorkletGlobalScope>(this))
        return;

    auto* thread = downcast<WorkerOrWorkletGlobalScope>(this)->workerOrWorkletThread();
    if (thread) {
        thread->workerLoaderProxy().postTaskToLoader([callback = WTFMove(callback)](auto&& context) {
            callback(downcast<Document>(context));
        });
        return;
    }

    if (auto document = downcast<WorkletGlobalScope>(this)->responsibleDocument())
        callback(*document);
}

static bool isOriginEquivalentToLocal(const SecurityOrigin& origin)
{
    return origin.isLocal() && !origin.needsStorageAccessFromFileURLsQuirk() && !origin.hasUniversalAccess();
}

ScriptExecutionContext::HasResourceAccess ScriptExecutionContext::canAccessResource(ResourceType type) const
{
    auto* origin = securityOrigin();
    if (!origin || origin->isOpaque())
        return HasResourceAccess::No;

    switch (type) {
    case ResourceType::Cookies:
    case ResourceType::Geolocation:
        return HasResourceAccess::Yes;
    case ResourceType::ApplicationCache:
    case ResourceType::Plugin:
    case ResourceType::WebSQL:
    case ResourceType::IndexedDB:
    case ResourceType::LocalStorage:
    case ResourceType::StorageManager:
        if (isOriginEquivalentToLocal(*origin))
            return HasResourceAccess::No;
        FALLTHROUGH;
    case ResourceType::SessionStorage:
        if (m_storageBlockingPolicy == StorageBlockingPolicy::BlockAll)
            return HasResourceAccess::No;
        if ((m_storageBlockingPolicy == StorageBlockingPolicy::BlockThirdParty) && !topOrigin().isSameOriginAs(*origin) && !origin->hasUniversalAccess())
            return HasResourceAccess::DefaultForThirdParty;
        return HasResourceAccess::Yes;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

ScriptExecutionContext::NotificationCallbackIdentifier ScriptExecutionContext::addNotificationCallback(CompletionHandler<void()>&& callback)
{
    auto identifier = NotificationCallbackIdentifier::generateThreadSafe();
    m_notificationCallbacks.add(identifier, WTFMove(callback));
    return identifier;
}

CompletionHandler<void()> ScriptExecutionContext::takeNotificationCallback(NotificationCallbackIdentifier identifier)
{
    return m_notificationCallbacks.take(identifier);
}

void ScriptExecutionContext::addDeferredPromise(Ref<DeferredPromise>&& promise)
{
    m_deferredPromises.add(WTFMove(promise));
}

RefPtr<DeferredPromise> ScriptExecutionContext::takeDeferredPromise(DeferredPromise* promise)
{
    return m_deferredPromises.take(promise);
}

WebCoreOpaqueRoot root(ScriptExecutionContext* context)
{
    return WebCoreOpaqueRoot { context };
}

} // namespace WebCore
