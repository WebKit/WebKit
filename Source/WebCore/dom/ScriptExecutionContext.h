/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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

#pragma once

#include "ActiveDOMObject.h"
#include "DOMTimer.h"
#include "SecurityContext.h"
#include "ServiceWorkerTypes.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/HandleTypes.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class Exception;
class ExecState;
class JSPromise;
class VM;
template<typename> class Strong;
}

namespace Inspector {
class ConsoleMessage;
class ScriptCallStack;
}

namespace PAL {
class SessionID;
}

namespace WebCore {

class CachedScript;
class DatabaseContext;
class EventQueue;
class EventTarget;
class MessagePort;
class PublicURLManager;
class RejectedPromiseTracker;
class ResourceRequest;
class SecurityOrigin;
class SocketProvider;

#if ENABLE(SERVICE_WORKER)
class ServiceWorker;
class ServiceWorkerContainer;
#endif

namespace IDBClient {
class IDBConnectionProxy;
}

enum ScriptExecutionContextIdentifierType { };
using ScriptExecutionContextIdentifier = ObjectIdentifier<ScriptExecutionContextIdentifierType>;

class ScriptExecutionContext : public SecurityContext {
public:
    ScriptExecutionContext();
    virtual ~ScriptExecutionContext();

    virtual bool isDocument() const { return false; }
    virtual bool isWorkerGlobalScope() const { return false; }
    virtual bool isWorkletGlobalScope() const { return false; }

    virtual bool isContextThread() const { return true; }
    virtual bool isJSExecutionForbidden() const = 0;

    virtual const URL& url() const = 0;
    virtual URL completeURL(const String& url) const = 0;
    virtual PAL::SessionID sessionID() const = 0;

    virtual String userAgent(const URL&) const = 0;

    virtual void disableEval(const String& errorMessage) = 0;
    virtual void disableWebAssembly(const String& errorMessage) = 0;

#if ENABLE(INDEXED_DATABASE)
    virtual IDBClient::IDBConnectionProxy* idbConnectionProxy() = 0;
#endif
    virtual SocketProvider* socketProvider() = 0;

    virtual String resourceRequestIdentifier() const { return String(); };

    bool canIncludeErrorDetails(CachedScript*, const String& sourceURL);
    void reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, JSC::Exception*, RefPtr<Inspector::ScriptCallStack>&&, CachedScript* = nullptr);
    void reportUnhandledPromiseRejection(JSC::ExecState&, JSC::JSPromise&, RefPtr<Inspector::ScriptCallStack>&&);

    virtual void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) = 0;

    // The following addConsoleMessage functions are deprecated.
    // Callers should try to create the ConsoleMessage themselves.
    void addConsoleMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, JSC::ExecState* = nullptr, unsigned long requestIdentifier = 0);
    virtual void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier = 0) = 0;

    virtual SecurityOrigin& topOrigin() const = 0;
    virtual String origin() const = 0;

    virtual bool shouldBypassMainWorldContentSecurityPolicy() const { return false; }

    PublicURLManager& publicURLManager();

    // Active objects are not garbage collected even if inaccessible, e.g. because their activity may result in callbacks being invoked.
    WEBCORE_EXPORT bool canSuspendActiveDOMObjectsForDocumentSuspension(Vector<ActiveDOMObject*>* unsuspendableObjects = nullptr);

    // Active objects can be asked to suspend even if canSuspendActiveDOMObjectsForDocumentSuspension() returns 'false' -
    // step-by-step JS debugging is one example.
    virtual void suspendActiveDOMObjects(ReasonForSuspension);
    virtual void resumeActiveDOMObjects(ReasonForSuspension);
    virtual void stopActiveDOMObjects();

    bool activeDOMObjectsAreSuspended() const { return m_activeDOMObjectsAreSuspended; }
    bool activeDOMObjectsAreStopped() const { return m_activeDOMObjectsAreStopped; }

    // Called from the constructor and destructors of ActiveDOMObject.
    void didCreateActiveDOMObject(ActiveDOMObject&);
    void willDestroyActiveDOMObject(ActiveDOMObject&);

    // Called after the construction of an ActiveDOMObject to synchronize suspend state.
    void suspendActiveDOMObjectIfNeeded(ActiveDOMObject&);

    void didCreateDestructionObserver(ContextDestructionObserver&);
    void willDestroyDestructionObserver(ContextDestructionObserver&);

    // MessagePort is conceptually a kind of ActiveDOMObject, but it needs to be tracked separately for message dispatch.
    void processMessageWithMessagePortsSoon();
    void dispatchMessagePortEvents();
    void createdMessagePort(MessagePort&);
    void destroyedMessagePort(MessagePort&);

    virtual void didLoadResourceSynchronously();

    void ref() { refScriptExecutionContext(); }
    void deref() { derefScriptExecutionContext(); }

    class Task {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        enum CleanupTaskTag { CleanupTask };

        template<typename T, typename = typename std::enable_if<!std::is_base_of<Task, T>::value && std::is_convertible<T, WTF::Function<void (ScriptExecutionContext&)>>::value>::type>
        Task(T task)
            : m_task(WTFMove(task))
            , m_isCleanupTask(false)
        {
        }

        Task(WTF::Function<void ()>&& task)
            : m_task([task = WTFMove(task)](ScriptExecutionContext&) { task(); })
            , m_isCleanupTask(false)
        {
        }

        template<typename T, typename = typename std::enable_if<std::is_convertible<T, WTF::Function<void (ScriptExecutionContext&)>>::value>::type>
        Task(CleanupTaskTag, T task)
            : m_task(WTFMove(task))
            , m_isCleanupTask(true)
        {
        }

        void performTask(ScriptExecutionContext& context) { m_task(context); }
        bool isCleanupTask() const { return m_isCleanupTask; }

    protected:
        WTF::Function<void (ScriptExecutionContext&)> m_task;
        bool m_isCleanupTask;
    };

    virtual void postTask(Task&&) = 0; // Executes the task on context's thread asynchronously.

    template<typename... Arguments>
    void postCrossThreadTask(Arguments&&... arguments)
    {
        postTask([crossThreadTask = createCrossThreadTask(arguments...)](ScriptExecutionContext&) mutable {
            crossThreadTask.performTask();
        });
    }

    // Gets the next id in a circular sequence from 1 to 2^31-1.
    int circularSequentialID();

    bool addTimeout(int timeoutId, DOMTimer& timer) { return m_timeouts.add(timeoutId, &timer).isNewEntry; }
    void removeTimeout(int timeoutId) { m_timeouts.remove(timeoutId); }
    DOMTimer* findTimeout(int timeoutId) { return m_timeouts.get(timeoutId); }

    WEBCORE_EXPORT JSC::VM& vm();

    void adjustMinimumDOMTimerInterval(Seconds oldMinimumTimerInterval);
    virtual Seconds minimumDOMTimerInterval() const;

    void didChangeTimerAlignmentInterval();
    virtual Seconds domTimerAlignmentInterval(bool hasReachedMaxNestingLevel) const;

    virtual EventQueue& eventQueue() const = 0;
    virtual EventTarget* errorEventTarget() = 0;

    DatabaseContext* databaseContext() { return m_databaseContext.get(); }
    void setDatabaseContext(DatabaseContext*);

#if ENABLE(WEB_CRYPTO)
    virtual bool wrapCryptoKey(const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey) = 0;
    virtual bool unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key) = 0;
#endif

    int timerNestingLevel() const { return m_timerNestingLevel; }
    void setTimerNestingLevel(int timerNestingLevel) { m_timerNestingLevel = timerNestingLevel; }

    RejectedPromiseTracker& ensureRejectedPromiseTracker()
    {
        if (m_rejectedPromiseTracker)
            return *m_rejectedPromiseTracker.get();
        return ensureRejectedPromiseTrackerSlow();
    }

    WEBCORE_EXPORT JSC::ExecState* execState();

    WEBCORE_EXPORT String domainForCachePartition() const;
    void setDomainForCachePartition(String&& domain) { m_domainForCachePartition = WTFMove(domain); }

    bool allowsMediaDevices() const;
    bool hasServiceWorkerScheme() const;
#if ENABLE(SERVICE_WORKER)
    ServiceWorker* activeServiceWorker() const;
    void setActiveServiceWorker(RefPtr<ServiceWorker>&&);

    void registerServiceWorker(ServiceWorker&);
    void unregisterServiceWorker(ServiceWorker&);
    ServiceWorker* serviceWorker(ServiceWorkerIdentifier identifier) { return m_serviceWorkers.get(identifier); }

    ServiceWorkerContainer* serviceWorkerContainer();

    WEBCORE_EXPORT static bool postTaskTo(const DocumentOrWorkerIdentifier&, WTF::Function<void(ScriptExecutionContext&)>&&);
#endif
    WEBCORE_EXPORT static bool postTaskTo(ScriptExecutionContextIdentifier, Task&&);

    ScriptExecutionContextIdentifier contextIdentifier() const;

protected:
    class AddConsoleMessageTask : public Task {
    public:
        AddConsoleMessageTask(std::unique_ptr<Inspector::ConsoleMessage>&& consoleMessage)
            : Task([&consoleMessage](ScriptExecutionContext& context) {
                context.addConsoleMessage(WTFMove(consoleMessage));
            })
        {
        }

        AddConsoleMessageTask(MessageSource source, MessageLevel level, const String& message)
            : Task([source, level, message = message.isolatedCopy()](ScriptExecutionContext& context) {
                context.addConsoleMessage(source, level, message);
            })
        {
        }
    };

    ReasonForSuspension reasonForSuspendingActiveDOMObjects() const { return m_reasonForSuspendingActiveDOMObjects; }

    bool hasPendingActivity() const;
    void removeFromContextsMap();
    void removeRejectedPromiseTracker();

private:
    // The following addMessage function is deprecated.
    // Callers should try to create the ConsoleMessage themselves.
    virtual void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&&, JSC::ExecState* = nullptr, unsigned long requestIdentifier = 0) = 0;
    virtual void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&&) = 0;
    bool dispatchErrorEvent(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, JSC::Exception*, CachedScript*);

    virtual void refScriptExecutionContext() = 0;
    virtual void derefScriptExecutionContext() = 0;

    enum class ShouldContinue { No, Yes };
    void forEachActiveDOMObject(const Function<ShouldContinue(ActiveDOMObject&)>&) const;

    RejectedPromiseTracker& ensureRejectedPromiseTrackerSlow();

    void checkConsistency() const;

    HashSet<MessagePort*> m_messagePorts;
    HashSet<ContextDestructionObserver*> m_destructionObservers;
    HashSet<ActiveDOMObject*> m_activeDOMObjects;

    HashMap<int, RefPtr<DOMTimer>> m_timeouts;

    struct PendingException;
    std::unique_ptr<Vector<std::unique_ptr<PendingException>>> m_pendingExceptions;
    std::unique_ptr<RejectedPromiseTracker> m_rejectedPromiseTracker;

    ReasonForSuspension m_reasonForSuspendingActiveDOMObjects { static_cast<ReasonForSuspension>(-1) };

    std::unique_ptr<PublicURLManager> m_publicURLManager;

    RefPtr<DatabaseContext> m_databaseContext;

    int m_circularSequentialID { 0 };
    int m_timerNestingLevel { 0 };

    bool m_activeDOMObjectsAreSuspended { false };
    bool m_activeDOMObjectsAreStopped { false };
    bool m_inDispatchErrorEvent { false };
    mutable bool m_activeDOMObjectAdditionForbidden { false };
    bool m_willprocessMessageWithMessagePortsSoon { false };

#if !ASSERT_DISABLED
    bool m_inScriptExecutionContextDestructor { false };
#endif

#if ENABLE(SERVICE_WORKER)
    RefPtr<ServiceWorker> m_activeServiceWorker;
    HashMap<ServiceWorkerIdentifier, ServiceWorker*> m_serviceWorkers;
#endif

    String m_domainForCachePartition;
    mutable ScriptExecutionContextIdentifier m_contextIdentifier;
};

} // namespace WebCore
