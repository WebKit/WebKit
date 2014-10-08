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

#include "CachedScript.h"
#include "DOMTimer.h"
#include "ErrorEvent.h"
#include "MessagePort.h"
#include "PublicURLManager.h"
#include "Settings.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <inspector/ScriptCallStack.h>
#include <wtf/MainThread.h>
#include <wtf/Ref.h>

// FIXME: This is a layering violation.
#include "JSDOMWindow.h"

#if PLATFORM(IOS)
#include "Document.h"
#endif

#if ENABLE(SQL_DATABASE)
#include "DatabaseContext.h"
#endif

using namespace Inspector;

namespace WebCore {

class ScriptExecutionContext::PendingException {
    WTF_MAKE_NONCOPYABLE(PendingException);
public:
    PendingException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, PassRefPtr<ScriptCallStack> callStack)
        : m_errorMessage(errorMessage)
        , m_lineNumber(lineNumber)
        , m_columnNumber(columnNumber)
        , m_sourceURL(sourceURL)
        , m_callStack(callStack)
    {
    }
    String m_errorMessage;
    int m_lineNumber;
    int m_columnNumber;
    String m_sourceURL;
    RefPtr<ScriptCallStack> m_callStack;
};

ScriptExecutionContext::ScriptExecutionContext()
    : m_circularSequentialID(0)
    , m_inDispatchErrorEvent(false)
    , m_activeDOMObjectsAreSuspended(false)
    , m_reasonForSuspendingActiveDOMObjects(static_cast<ActiveDOMObject::ReasonForSuspension>(-1))
    , m_activeDOMObjectsAreStopped(false)
    , m_activeDOMObjectAdditionForbidden(false)
    , m_timerNestingLevel(0)
#if !ASSERT_DISABLED
    , m_inScriptExecutionContextDestructor(false)
    , m_activeDOMObjectRemovalForbidden(false)
#endif
{
}

#if ASSERT_DISABLED

inline void ScriptExecutionContext::checkConsistency() const
{
}

#else

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

#endif

ScriptExecutionContext::~ScriptExecutionContext()
{
    checkConsistency();

#if !ASSERT_DISABLED
    m_inScriptExecutionContextDestructor = true;
#endif

    while (auto* destructionObserver = m_destructionObservers.takeAny())
        destructionObserver->contextDestroyed();

    for (auto* messagePort : m_messagePorts)
        messagePort->contextDestroyed();

#if !ASSERT_DISABLED
    m_inScriptExecutionContextDestructor = false;
#endif
}

void ScriptExecutionContext::processMessagePortMessagesSoon()
{
    postTask([] (ScriptExecutionContext& context) {
        context.dispatchMessagePortEvents();
    });
}

void ScriptExecutionContext::dispatchMessagePortEvents()
{
    checkConsistency();

    Ref<ScriptExecutionContext> protect(*this);

    // Make a frozen copy of the ports so we can iterate while new ones might be added or destroyed.
    Vector<MessagePort*> possibleMessagePorts;
    copyToVector(m_messagePorts, possibleMessagePorts);
    for (auto* messagePort : possibleMessagePorts) {
        // The port may be destroyed, and another one created at the same address,
        // but this is harmless. The worst that can happen as a result is that
        // dispatchMessages() will be called needlessly.
        if (m_messagePorts.contains(messagePort) && messagePort->started())
            messagePort->dispatchMessages();
    }
}

void ScriptExecutionContext::createdMessagePort(MessagePort& messagePort)
{
    ASSERT((isDocument() && isMainThread())
        || (isWorkerGlobalScope() && currentThread() == toWorkerGlobalScope(this)->thread().threadID()));

    m_messagePorts.add(&messagePort);
}

void ScriptExecutionContext::destroyedMessagePort(MessagePort& messagePort)
{
    ASSERT((isDocument() && isMainThread())
        || (isWorkerGlobalScope() && currentThread() == toWorkerGlobalScope(this)->thread().threadID()));

    m_messagePorts.remove(&messagePort);
}

void ScriptExecutionContext::didLoadResourceSynchronously(const ResourceRequest&)
{
}

bool ScriptExecutionContext::canSuspendActiveDOMObjects()
{
    checkConsistency();

    bool canSuspend = true;

    m_activeDOMObjectAdditionForbidden = true;
#if !ASSERT_DISABLED
    m_activeDOMObjectRemovalForbidden = true;
#endif

    // We assume that m_activeDOMObjects will not change during iteration: canSuspend
    // functions should not add new active DOM objects, nor execute arbitrary JavaScript.
    // An ASSERT or RELEASE_ASSERT will fire if this happens, but it's important to code
    // canSuspend functions so it will not happen!
    for (auto* activeDOMObject : m_activeDOMObjects) {
        if (!activeDOMObject->canSuspend()) {
            canSuspend = false;
            break;
        }
    }

    m_activeDOMObjectAdditionForbidden = false;
#if !ASSERT_DISABLED
    m_activeDOMObjectRemovalForbidden = false;
#endif

    return canSuspend;
}

void ScriptExecutionContext::suspendActiveDOMObjects(ActiveDOMObject::ReasonForSuspension why)
{
    checkConsistency();

#if PLATFORM(IOS)
    if (m_activeDOMObjectsAreSuspended) {
        ASSERT(m_reasonForSuspendingActiveDOMObjects == ActiveDOMObject::DocumentWillBePaused);
        return;
    }
#endif

    m_activeDOMObjectAdditionForbidden = true;
#if !ASSERT_DISABLED
    m_activeDOMObjectRemovalForbidden = true;
#endif

    // We assume that m_activeDOMObjects will not change during iteration: suspend
    // functions should not add new active DOM objects, nor execute arbitrary JavaScript.
    // An ASSERT or RELEASE_ASSERT will fire if this happens, but it's important to code
    // suspend functions so it will not happen!
    for (auto* activeDOMObject : m_activeDOMObjects)
        activeDOMObject->suspend(why);

    m_activeDOMObjectAdditionForbidden = false;
#if !ASSERT_DISABLED
    m_activeDOMObjectRemovalForbidden = false;
#endif

    m_activeDOMObjectsAreSuspended = true;
    m_reasonForSuspendingActiveDOMObjects = why;
}

void ScriptExecutionContext::resumeActiveDOMObjects(ActiveDOMObject::ReasonForSuspension why)
{
    checkConsistency();

    if (m_reasonForSuspendingActiveDOMObjects != why)
        return;
    m_activeDOMObjectsAreSuspended = false;

    m_activeDOMObjectAdditionForbidden = true;
#if !ASSERT_DISABLED
    m_activeDOMObjectRemovalForbidden = true;
#endif

    // We assume that m_activeDOMObjects will not change during iteration: resume
    // functions should not add new active DOM objects, nor execute arbitrary JavaScript.
    // An ASSERT or RELEASE_ASSERT will fire if this happens, but it's important to code
    // resume functions so it will not happen!
    for (auto* activeDOMObject : m_activeDOMObjects)
        activeDOMObject->resume();

    m_activeDOMObjectAdditionForbidden = false;
#if !ASSERT_DISABLED
    m_activeDOMObjectRemovalForbidden = false;
#endif
}

void ScriptExecutionContext::stopActiveDOMObjects()
{
    checkConsistency();

    if (m_activeDOMObjectsAreStopped)
        return;
    m_activeDOMObjectsAreStopped = true;

    // Make a frozen copy of the objects so we can iterate while new ones might be destroyed.
    Vector<ActiveDOMObject*> possibleActiveDOMObjects;
    copyToVector(m_activeDOMObjects, possibleActiveDOMObjects);

    m_activeDOMObjectAdditionForbidden = true;

    // We assume that new objects will not be added to m_activeDOMObjects during iteration:
    // stop functions should not add new active DOM objects, nor execute arbitrary JavaScript.
    // A RELEASE_ASSERT will fire if this happens, but it's important to code stop functions
    // so it will not happen!
    for (auto* activeDOMObject : possibleActiveDOMObjects) {
        // Check if this object was deleted already. If so, just skip it.
        // Calling contains on a possibly-already-deleted object is OK because we guarantee
        // no new object can be added, so even if a new object ends up allocated with the
        // same address, that will be *after* this function exits.
        if (!m_activeDOMObjects.contains(activeDOMObject))
            continue;
        activeDOMObject->stop();
    }

    m_activeDOMObjectAdditionForbidden = false;

    // FIXME: Make message ports be active DOM objects and let them implement stop instead
    // of having this separate mechanism just for them.
    for (auto* messagePort : m_messagePorts)
        messagePort->close();
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
    ASSERT(!m_activeDOMObjectRemovalForbidden);
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

bool ScriptExecutionContext::sanitizeScriptError(String& errorMessage, int& lineNumber, int& columnNumber, String& sourceURL, CachedScript* cachedScript)
{
    URL targetURL = completeURL(sourceURL);
    if (securityOrigin()->canRequest(targetURL) || (cachedScript && cachedScript->passesAccessControlCheck(securityOrigin())))
        return false;
    errorMessage = "Script error.";
    sourceURL = String();
    lineNumber = 0;
    columnNumber = 0;
    return true;
}

void ScriptExecutionContext::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, PassRefPtr<ScriptCallStack> callStack, CachedScript* cachedScript)
{
    if (m_inDispatchErrorEvent) {
        if (!m_pendingExceptions)
            m_pendingExceptions = std::make_unique<Vector<std::unique_ptr<PendingException>>>();
        m_pendingExceptions->append(std::make_unique<PendingException>(errorMessage, lineNumber, columnNumber, sourceURL, callStack));
        return;
    }

    // First report the original exception and only then all the nested ones.
    if (!dispatchErrorEvent(errorMessage, lineNumber, columnNumber, sourceURL, cachedScript))
        logExceptionToConsole(errorMessage, sourceURL, lineNumber, columnNumber, callStack);

    if (!m_pendingExceptions)
        return;

    std::unique_ptr<Vector<std::unique_ptr<PendingException>>> pendingExceptions = WTF::move(m_pendingExceptions);
    for (auto& exception : *pendingExceptions)
        logExceptionToConsole(exception->m_errorMessage, exception->m_sourceURL, exception->m_lineNumber, exception->m_columnNumber, exception->m_callStack);
}

void ScriptExecutionContext::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, JSC::ExecState* state, unsigned long requestIdentifier)
{
    addMessage(source, level, message, sourceURL, lineNumber, columnNumber, 0, state, requestIdentifier);
}

bool ScriptExecutionContext::dispatchErrorEvent(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, CachedScript* cachedScript)
{
    EventTarget* target = errorEventTarget();
    if (!target)
        return false;

#if PLATFORM(IOS)
    if (target == target->toDOMWindow() && isDocument()) {
        Settings* settings = toDocument(this)->settings();
        if (settings && !settings->shouldDispatchJavaScriptWindowOnErrorEvents())
            return false;
    }
#endif

    String message = errorMessage;
    int line = lineNumber;
    int column = columnNumber;
    String sourceName = sourceURL;
    sanitizeScriptError(message, line, column, sourceName, cachedScript);

    ASSERT(!m_inDispatchErrorEvent);
    m_inDispatchErrorEvent = true;
    RefPtr<ErrorEvent> errorEvent = ErrorEvent::create(message, sourceName, line, column);
    target->dispatchEvent(errorEvent);
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

void ScriptExecutionContext::adjustMinimumTimerInterval(double oldMinimumTimerInterval)
{
    if (minimumTimerInterval() != oldMinimumTimerInterval) {
        for (auto& timer : m_timeouts.values())
            timer->updateTimerIntervalIfNecessary();
    }
}

double ScriptExecutionContext::minimumTimerInterval() const
{
    // The default implementation returns the DOMTimer's default
    // minimum timer interval. FIXME: to make it work with dedicated
    // workers, we will have to override it in the appropriate
    // subclass, and provide a way to enumerate a Document's dedicated
    // workers so we can update them all.
    return Settings::defaultMinDOMTimerInterval();
}

void ScriptExecutionContext::didChangeTimerAlignmentInterval()
{
    for (auto& timer : m_timeouts.values())
        timer->didChangeAlignmentInterval();
}

double ScriptExecutionContext::timerAlignmentInterval() const
{
    return Settings::defaultDOMTimerAlignmentInterval();
}

JSC::VM& ScriptExecutionContext::vm()
{
     if (isDocument())
        return JSDOMWindow::commonVM();

    return toWorkerGlobalScope(*this).script()->vm();
}

#if ENABLE(SQL_DATABASE)
void ScriptExecutionContext::setDatabaseContext(DatabaseContext* databaseContext)
{
    ASSERT(!m_databaseContext);
    m_databaseContext = databaseContext;
}
#endif

bool ScriptExecutionContext::hasPendingActivity() const
{
    checkConsistency();

    for (auto* activeDOMObject : m_activeDOMObjects) {
        if (activeDOMObject->hasPendingActivity())
            return true;
    }

    for (auto* messagePort : m_messagePorts) {
        if (messagePort->hasPendingActivity())
            return true;
    }

    return false;
}

} // namespace WebCore
