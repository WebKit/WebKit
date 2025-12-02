/*
 * Copyright (C) 2008-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SecurityContext.h>
#include <WebCore/ServiceWorkerIdentifier.h>
#include <WebCore/Timer.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class CrossThreadTask;
class NativePromiseRequest;
class URL;
} // namespace WTF

using WTF::CrossThreadTask;
using WTF::NativePromiseRequest;
using WTF::URL;

namespace JSC {
class CallFrame;
class Exception;
class JSGlobalObject;
class JSPromise;
class VM;
enum class MessageLevel : uint8_t;
enum class MessageSource : uint8_t;
enum class MessageType : uint8_t;
enum class ScriptExecutionStatus;
enum class TrustedTypesEnforcement;
}

using JSC::MessageSource;
using JSC::MessageLevel;

namespace Inspector {
class ConsoleMessage;
class ScriptCallStack;
}

namespace PAL {
class SessionID;
} // namespace PAL

namespace WebCore {

class ActiveDOMObject;
class EventLoop;
class CachedScript;
class CSSFontSelector;
class CSSValuePool;
class ContextDestructionObserver;
class DOMTimer;
class DatabaseContext;
class DeferredPromise;
class Document;
class EventQueue;
class EventLoopTaskGroup;
class EventTarget;
class FontLoadRequest;
class GraphicsClient;
class MessagePort;
class NotificationClient;
class PublicURLManager;
class RejectedPromiseTracker;
class RTCDataChannelRemoteHandlerConnection;
class ResourceRequest;
class ServiceWorker;
class ServiceWorkerContainer;
class SocketProvider;
class WeakPtrImplWithEventTargetData;
class WebCoreOpaqueRoot;
class WebTransport;
enum class AdvancedPrivacyProtections : uint16_t;
enum class CrossOriginMode : bool;
enum class LoadedFromOpaqueSource : bool;
enum class NoiseInjectionPolicy : uint8_t;
enum class ReasonForSuspension : uint8_t;
enum class ScriptTrackingPrivacyCategory : uint8_t;
enum class StorageBlockingPolicy : uint8_t;
enum class TaskSource : uint8_t;
struct CryptoKeyData;
struct SettingsValues;

#if ENABLE(NOTIFICATIONS)
class NotificationClient;
#endif

namespace IDBClient {
class IDBConnectionProxy;
}

enum class ScriptExecutionContextType : uint8_t {
    Document,
    WorkerOrWorkletGlobalScope,
    EmptyScriptExecutionContext
};

class ScriptExecutionContext : public SecurityContext, public TimerAlignment, public CanMakeThreadSafeCheckedPtr<ScriptExecutionContext> {
    WTF_MAKE_TZONE_ALLOCATED(ScriptExecutionContext);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ScriptExecutionContext);
public:
    using Type = ScriptExecutionContextType;

    explicit ScriptExecutionContext(Type, std::optional<ScriptExecutionContextIdentifier> = std::nullopt);
    virtual ~ScriptExecutionContext();

    bool isDocument() const { return m_type == Type::Document; }
    bool isWorkerOrWorkletGlobalScope() const { return m_type == Type::WorkerOrWorkletGlobalScope; }
    bool isEmptyScriptExecutionContext() const { return m_type == Type::EmptyScriptExecutionContext; }
    virtual bool isWorkerGlobalScope() const { return false; }
    virtual bool isServiceWorkerGlobalScope() const { return false; }
    virtual bool isWorkletGlobalScope() const { return false; }

    virtual bool isContextThread() const { return true; }
    virtual bool isJSExecutionForbidden() const = 0;

    virtual EventLoopTaskGroup& eventLoop() = 0;
    inline CheckedRef<EventLoopTaskGroup> checkedEventLoop();

    virtual const URL& url() const = 0;
    enum class ForceUTF8 : bool { No, Yes };
    virtual URL completeURL(const String& url, ForceUTF8 = ForceUTF8::No) const = 0;

    virtual const URL& cookieURL() const = 0;

    virtual String userAgent(const URL&) const = 0;

    virtual const SettingsValues& settingsValues() const = 0;

    virtual NotificationClient* notificationClient() { return nullptr; }
    virtual std::optional<PAL::SessionID> sessionID() const;

    virtual void disableEval(const String& errorMessage) = 0;
    virtual void disableWebAssembly(const String& errorMessage) = 0;
    virtual void setTrustedTypesEnforcement(JSC::TrustedTypesEnforcement) = 0;

    virtual IDBClient::IDBConnectionProxy* idbConnectionProxy() = 0;

    virtual SocketProvider* socketProvider() = 0;
    RefPtr<SocketProvider> protectedSocketProvider();

    virtual GraphicsClient* graphicsClient() { return nullptr; }

    virtual OptionSet<AdvancedPrivacyProtections> advancedPrivacyProtections() const = 0;
    virtual std::optional<uint64_t> noiseInjectionHashSalt() const = 0;
    virtual OptionSet<NoiseInjectionPolicy> noiseInjectionPolicies() const = 0;

    virtual RefPtr<RTCDataChannelRemoteHandlerConnection> createRTCDataChannelRemoteHandlerConnection();

    virtual String resourceRequestIdentifier() const { return String(); };

    bool canIncludeErrorDetails(CachedScript*, const String& sourceURL, bool = false);
    void reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, JSC::Exception*, RefPtr<Inspector::ScriptCallStack>&&, CachedScript* = nullptr, bool = false);
    void reportUnhandledPromiseRejection(JSC::JSGlobalObject&, JSC::JSPromise&, RefPtr<Inspector::ScriptCallStack>&&);

    virtual void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) = 0;

    // The following addConsoleMessage functions are deprecated.
    // Callers should try to create the ConsoleMessage themselves.
    void addConsoleMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, JSC::JSGlobalObject* = nullptr, unsigned long requestIdentifier = 0);
    virtual void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier = 0) = 0;

    virtual SecurityOrigin& topOrigin() const = 0;
    Ref<SecurityOrigin> protectedTopOrigin() const;

    virtual bool shouldBypassMainWorldContentSecurityPolicy() const { return false; }

    PublicURLManager& publicURLManager();
    Ref<PublicURLManager> protectedPublicURLManager();

    virtual void suspendActiveDOMObjects(ReasonForSuspension);
    virtual void resumeActiveDOMObjects(ReasonForSuspension);
    virtual void stopActiveDOMObjects();

    bool activeDOMObjectsAreSuspended() const { return m_activeDOMObjectsAreSuspended; }
    bool activeDOMObjectsAreStopped() const { return m_activeDOMObjectsAreStopped; }

    JSC::ScriptExecutionStatus jscScriptExecutionStatus() const;

    enum class CallStackPosition : bool { BottomMost, TopMost };
    URL currentSourceURL(CallStackPosition = CallStackPosition::BottomMost) const;

    // Called from the constructor and destructors of ActiveDOMObject.
    void didCreateActiveDOMObject(ActiveDOMObject&);
    void willDestroyActiveDOMObject(ActiveDOMObject&);

    // Called after the construction of an ActiveDOMObject to synchronize suspend state.
    void suspendActiveDOMObjectIfNeeded(ActiveDOMObject&);

    void didCreateDestructionObserver(ContextDestructionObserver&);
    void willDestroyDestructionObserver(ContextDestructionObserver&);

    // MessagePort is conceptually a kind of ActiveDOMObject, but it needs to be tracked separately for message dispatch.
    void processMessageWithMessagePortsSoon(CompletionHandler<void()>&&);
    void createdMessagePort(MessagePort&);
    void destroyedMessagePort(MessagePort&);

    virtual void didLoadResourceSynchronously(const URL&);

    virtual CSSFontSelector* cssFontSelector() { return nullptr; }
    virtual CSSValuePool& cssValuePool();
    virtual RefPtr<FontLoadRequest> fontLoadRequest(const String& url, bool isSVG, bool isInitiatingElementInUserAgentShadowTree, LoadedFromOpaqueSource);
    virtual void beginLoadingFontSoon(FontLoadRequest&) { }

    WEBCORE_EXPORT static void setCrossOriginMode(CrossOriginMode);
    static CrossOriginMode crossOriginMode();

    WEBCORE_EXPORT void ref();
    WEBCORE_EXPORT void deref();

    enum class IncludeConsoleLog : bool { No, Yes };
    WEBCORE_EXPORT bool requiresScriptTrackingPrivacyProtection(ScriptTrackingPrivacyCategory, IncludeConsoleLog = IncludeConsoleLog::Yes);

    class Task {
        WTF_MAKE_TZONE_ALLOCATED(Task);
    public:
        enum CleanupTaskTag { CleanupTask };

        template<typename T>
            requires (!std::derived_from<T, Task> && std::convertible_to<T, Function<void(ScriptExecutionContext&)>>)
        Task(T task)
            : m_task(WTFMove(task))
            , m_isCleanupTask(false)
        {
        }

        Task(Function<void()>&& task)
            : m_task([task = WTFMove(task)](ScriptExecutionContext&) { task(); })
            , m_isCleanupTask(false)
        {
        }

        template<typename T>
            requires std::convertible_to<T, Function<void(ScriptExecutionContext&)>>
        Task(CleanupTaskTag, T task)
            : m_task(WTFMove(task))
            , m_isCleanupTask(true)
        {
        }

        void performTask(ScriptExecutionContext& context) { m_task(context); }
        bool isCleanupTask() const { return m_isCleanupTask; }

    protected:
        Function<void(ScriptExecutionContext&)> m_task;
        bool m_isCleanupTask;
    };

    virtual void postTask(Task&&) = 0; // Executes the task on context's thread asynchronously.

    template<typename... Arguments>
    inline void postCrossThreadTask(Arguments&&...);

    void postTaskToResponsibleDocument(Function<void(Document&)>&&);

    // Gets the next id in a circular sequence from 1 to 2^31-1.
    int circularSequentialID();

    inline bool addTimeout(int timeoutId, DOMTimer&); // Defined in ScriptExecutionContextInlines.h
    inline RefPtr<DOMTimer> takeTimeout(int timeoutId); // Defined in ScriptExecutionContextInlines.h
    inline DOMTimer* findTimeout(int timeoutId); // Defined in ScriptExecutionContextInlines.h

    virtual JSC::VM& vm() = 0;
    virtual Ref<JSC::VM> protectedVM();
    virtual JSC::VM* vmIfExists() const = 0;

    void adjustMinimumDOMTimerInterval(Seconds oldMinimumTimerInterval);
    virtual Seconds minimumDOMTimerInterval() const;

    void didChangeTimerAlignmentInterval();
    virtual Seconds domTimerAlignmentInterval(bool hasReachedMaxNestingLevel) const;

    // TimerAlignment
    WEBCORE_EXPORT MonotonicTime alignedFireTime(bool hasReachedMaxNestingLevel, MonotonicTime fireTime) const final;

    virtual EventTarget* errorEventTarget() = 0;

    DatabaseContext* databaseContext() { return m_databaseContext.get(); }
    void setDatabaseContext(DatabaseContext*);

    // These two methods are used when CryptoKeys are serialized into IndexedDB. As a side effect, it is also
    // used for things that utilize the same structure clone algorithm, for example, message passing between
    // worker and document.
    virtual std::optional<Vector<uint8_t>> serializeAndWrapCryptoKey(CryptoKeyData&&) = 0;
    virtual std::optional<Vector<uint8_t>> unwrapCryptoKey(const Vector<uint8_t>& wrappedKey) = 0;

    int timerNestingLevel() const { return m_timerNestingLevel; }
    void setTimerNestingLevel(int timerNestingLevel) { m_timerNestingLevel = timerNestingLevel; }

    RejectedPromiseTracker* rejectedPromiseTracker()
    {
        return m_rejectedPromiseTracker.get();
    }

    RejectedPromiseTracker* ensureRejectedPromiseTracker()
    {
        if (m_rejectedPromiseTracker)
            return m_rejectedPromiseTracker.get();
        return ensureRejectedPromiseTrackerSlow();
    }

    WEBCORE_EXPORT JSC::JSGlobalObject* globalObject() const;

    WEBCORE_EXPORT String domainForCachePartition() const;
    void setDomainForCachePartition(String&& domain) { m_domainForCachePartition = WTFMove(domain); }

    bool allowsMediaDevices() const;
    ServiceWorker* activeServiceWorker() const { return m_activeServiceWorker.get(); }
    void setActiveServiceWorker(RefPtr<ServiceWorker>&&);

    void registerServiceWorker(ServiceWorker&);
    void unregisterServiceWorker(ServiceWorker&);
    inline ServiceWorker* serviceWorker(ServiceWorkerIdentifier); // Defined in ServiceWorker.h.

    ServiceWorkerContainer* serviceWorkerContainer();
    ServiceWorkerContainer* ensureServiceWorkerContainer();
    virtual void updateServiceWorkerClientData() { ASSERT_NOT_REACHED(); }
    WEBCORE_EXPORT static bool postTaskTo(ScriptExecutionContextIdentifier, Task&&);
    WEBCORE_EXPORT static bool postTaskForModeToWorkerOrWorklet(ScriptExecutionContextIdentifier, Task&&, const String&);
    WEBCORE_EXPORT static bool ensureOnContextThread(ScriptExecutionContextIdentifier, Task&&);
    WEBCORE_EXPORT static bool isContextThread(ScriptExecutionContextIdentifier);
    WEBCORE_EXPORT static bool ensureOnContextThreadForCrossThreadTask(ScriptExecutionContextIdentifier, CrossThreadTask&&);

    ScriptExecutionContextIdentifier identifier() const { return m_identifier; }

    bool hasLoggedAuthenticatedEncryptionWarning() const { return m_hasLoggedAuthenticatedEncryptionWarning; }
    void setHasLoggedAuthenticatedEncryptionWarning(bool value) { m_hasLoggedAuthenticatedEncryptionWarning = value; }

    void setStorageBlockingPolicy(StorageBlockingPolicy policy) { m_storageBlockingPolicy = policy; }
    enum class ResourceType : uint8_t {
        Cookies,
        Geolocation,
        IndexedDB,
        LocalStorage,
        Plugin,
        SessionStorage,
        StorageManager,
        WebSQL
    };
    enum class HasResourceAccess : uint8_t { No, Yes, DefaultForThirdParty };
    WEBCORE_EXPORT HasResourceAccess canAccessResource(ResourceType) const;

    enum NotificationCallbackIdentifierType { };
    using NotificationCallbackIdentifier = AtomicObjectIdentifier<NotificationCallbackIdentifierType>;

    WEBCORE_EXPORT NotificationCallbackIdentifier addNotificationCallback(CompletionHandler<void()>&&);
    WEBCORE_EXPORT CompletionHandler<void()> takeNotificationCallback(NotificationCallbackIdentifier);

    template<typename Promise, typename TaskType>
    void enqueueTaskWhenSettled(Ref<Promise>&&, TaskSource, TaskType&&);

    template<typename Promise, typename TaskType, typename Finalizer>
    void enqueueTaskWhenSettled(Ref<Promise>&&, TaskSource, TaskType&&, Finalizer&&);

    bool isAlwaysOnLoggingAllowed() const;

    void createdWebTransport(WebTransport&);

protected:
    class AddConsoleMessageTask : public Task {
    public:
        inline AddConsoleMessageTask(std::unique_ptr<Inspector::ConsoleMessage>&&);
        inline AddConsoleMessageTask(MessageSource, MessageLevel, const String&);
    };

    ReasonForSuspension reasonForSuspendingActiveDOMObjects() const { return m_reasonForSuspendingActiveDOMObjects; }

    bool hasPendingActivity() const;
    WEBCORE_EXPORT void addToContextsMap();
    void removeFromContextsMap();
    void removeRejectedPromiseTracker();
    void regenerateIdentifier();

private:

    std::unique_ptr<ContentSecurityPolicy> makeEmptyContentSecurityPolicy() final;

    // The following addMessage function is deprecated.
    // Callers should try to create the ConsoleMessage themselves.
    virtual void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&&, JSC::JSGlobalObject* = nullptr, unsigned long requestIdentifier = 0) = 0;
    virtual void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&&) = 0;

    bool dispatchErrorEvent(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, JSC::Exception*, CachedScript*, bool);

    void dispatchMessagePortEvents();

    enum class ShouldContinue : bool { No, Yes };
    void forEachActiveDOMObject(NOESCAPE const Function<ShouldContinue(ActiveDOMObject&)>&) const;

    RejectedPromiseTracker* ensureRejectedPromiseTrackerSlow();

    void checkConsistency() const;
    WEBCORE_EXPORT GuaranteedSerialFunctionDispatcher& nativePromiseDispatcher();

    WeakHashSet<MessagePort, WeakPtrImplWithEventTargetData> m_messagePorts;
    WeakHashSet<ContextDestructionObserver> m_destructionObservers;
    WeakHashSet<ActiveDOMObject> m_activeDOMObjects;

    HashMap<int, RefPtr<DOMTimer>> m_timeouts;

    struct PendingException;
    std::unique_ptr<Vector<std::unique_ptr<PendingException>>> m_pendingExceptions;
    std::unique_ptr<RejectedPromiseTracker> m_rejectedPromiseTracker;

    RefPtr<PublicURLManager> m_publicURLManager;

    RefPtr<DatabaseContext> m_databaseContext;

    int m_circularSequentialID { 0 };
    int m_timerNestingLevel { 0 };

    Vector<CompletionHandler<void()>> m_processMessageWithMessagePortsSoonHandlers;

#if ASSERT_ENABLED
    bool m_inScriptExecutionContextDestructor { false };
#endif

    RefPtr<ServiceWorker> m_activeServiceWorker;
    HashMap<ServiceWorkerIdentifier, WeakRef<ServiceWorker, WeakPtrImplWithEventTargetData>> m_serviceWorkers;

    String m_domainForCachePartition;
    mutable ScriptExecutionContextIdentifier m_identifier;

    HashMap<NotificationCallbackIdentifier, CompletionHandler<void()>> m_notificationCallbacks;

    StorageBlockingPolicy m_storageBlockingPolicy;
    ReasonForSuspension m_reasonForSuspendingActiveDOMObjects { static_cast<ReasonForSuspension>(-1) };

    Type m_type;
    bool m_activeDOMObjectsAreSuspended { false };
    bool m_activeDOMObjectsAreStopped { false };
    bool m_inDispatchErrorEvent { false };
    mutable bool m_activeDOMObjectAdditionForbidden { false };
    bool m_willprocessMessageWithMessagePortsSoon { false };
    bool m_hasLoggedAuthenticatedEncryptionWarning { false };

    RefPtr<GuaranteedSerialFunctionDispatcher> m_nativePromiseDispatcher;
    WeakHashSet<NativePromiseRequest> m_nativePromiseRequests;
    ThreadSafeWeakHashSet<WebTransport> m_activeWebTransports;
};

WebCoreOpaqueRoot root(ScriptExecutionContext*);

} // namespace WebCore
