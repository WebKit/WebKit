/*
 * Copyright (C) 2021, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebLockManager.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "ExceptionOr.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebLockManagerSnapshot.h"
#include "NavigatorBase.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "WebLock.h"
#include "WebLockGrantedCallback.h"
#include "WebLockManagerSnapshot.h"
#include "WebLockRegistry.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>

namespace WebCore {

static std::optional<ClientOrigin> clientOriginFromContext(ScriptExecutionContext* context)
{
    if (!context)
        return std::nullopt;
    auto* origin = context->securityOrigin();
    if (!origin || origin->isOpaque())
        return std::nullopt;
    return { { context->topOrigin().data(), origin->data() } };
}

struct WebLockManager::LockRequest {
    WebLockIdentifier lockIdentifier;
    String name;
    WebLockMode mode { WebLockMode::Exclusive };
    RefPtr<WebLockGrantedCallback> grantedCallback;
    RefPtr<AbortSignal> signal;

    bool isValid() const { return !!lockIdentifier; }
};

class WebLockManager::MainThreadBridge : public ThreadSafeRefCounted<MainThreadBridge, WTF::DestructionThread::Main> {
public:
    static RefPtr<MainThreadBridge> create(ScriptExecutionContext* context)
    {
        auto clientOrigin = clientOriginFromContext(context);
        if (!clientOrigin)
            return nullptr;

        auto sessionID = context->sessionID();
        if (!sessionID)
            return nullptr;

        return adoptRef(*new MainThreadBridge(*context, *sessionID, WTFMove(*clientOrigin)));
    }

    void requestLock(WebLockIdentifier, const String& name, const Options&, Function<void(bool)>&&, Function<void()>&& lockStolenHandler);
    void releaseLock(WebLockIdentifier, const String& name);
    void abortLockRequest(WebLockIdentifier, const String& name, CompletionHandler<void(bool)>&&);
    void query(CompletionHandler<void(Snapshot&&)>&&);
    void clientIsGoingAway();

private:
    MainThreadBridge(ScriptExecutionContext&, PAL::SessionID, ClientOrigin&&);

    const ScriptExecutionContextIdentifier m_clientID;
    const PAL::SessionID m_sessionID;
    const ClientOrigin m_clientOrigin; // Main thread only.
};

WebLockManager::MainThreadBridge::MainThreadBridge(ScriptExecutionContext& context, PAL::SessionID sessionID, ClientOrigin&& clientOrigin)
    : m_clientID(context.identifier())
    , m_sessionID(sessionID)
    , m_clientOrigin(WTFMove(clientOrigin).isolatedCopy())
{
}

void WebLockManager::MainThreadBridge::requestLock(WebLockIdentifier lockIdentifier, const String& name, const Options& options, Function<void(bool)>&& grantedHandler, Function<void()>&& lockStolenHandler)
{
    callOnMainThread([this, protectedThis = Ref { *this }, name = crossThreadCopy(name), mode = options.mode, steal = options.steal, ifAvailable = options.ifAvailable, lockIdentifier, grantedHandler = WTFMove(grantedHandler), lockStolenHandler = WTFMove(lockStolenHandler)]() mutable {
        WebLockRegistry::shared().requestLock(m_sessionID, m_clientOrigin, lockIdentifier, m_clientID, name, mode, steal, ifAvailable, [clientID = m_clientID, grantedHandler = WTFMove(grantedHandler)] (bool success) mutable {
            ScriptExecutionContext::ensureOnContextThread(clientID, [grantedHandler = WTFMove(grantedHandler), success](auto&) mutable {
                grantedHandler(success);
            });
        }, [clientID = m_clientID, lockStolenHandler = WTFMove(lockStolenHandler)]() mutable {
            ScriptExecutionContext::ensureOnContextThread(clientID, [lockStolenHandler = WTFMove(lockStolenHandler)](auto&) mutable {
                lockStolenHandler();
            });
        });
    });
}

void WebLockManager::MainThreadBridge::releaseLock(WebLockIdentifier lockIdentifier, const String& name)
{
    callOnMainThread([this, protectedThis = Ref { *this }, lockIdentifier, name = crossThreadCopy(name)] {
        WebLockRegistry::shared().releaseLock(m_sessionID, m_clientOrigin, lockIdentifier, m_clientID, name);
    });
}

void WebLockManager::MainThreadBridge::abortLockRequest(WebLockIdentifier lockIdentifier, const String& name, CompletionHandler<void(bool)>&& completionHandler)
{
    callOnMainThread([this, protectedThis = Ref { *this }, lockIdentifier, name = crossThreadCopy(name), completionHandler = WTFMove(completionHandler)]() mutable {
        WebLockRegistry::shared().abortLockRequest(m_sessionID, m_clientOrigin, lockIdentifier, m_clientID, name, [clientID = m_clientID, completionHandler = WTFMove(completionHandler)](bool wasAborted) mutable {
            ScriptExecutionContext::ensureOnContextThread(clientID, [completionHandler = WTFMove(completionHandler), wasAborted](auto&) mutable {
                completionHandler(wasAborted);
            });
        });
    });
}

void WebLockManager::MainThreadBridge::query(CompletionHandler<void(Snapshot&&)>&& completionHandler)
{
    callOnMainThread([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        WebLockRegistry::shared().snapshot(m_sessionID, m_clientOrigin, [clientID = m_clientID, completionHandler = WTFMove(completionHandler)](Snapshot&& snapshot) mutable {
            ScriptExecutionContext::ensureOnContextThread(clientID, [completionHandler = WTFMove(completionHandler), snapshot = crossThreadCopy(snapshot)](auto&) mutable {
                completionHandler(WTFMove(snapshot));
            });
        });
    });
}

void WebLockManager::MainThreadBridge::clientIsGoingAway()
{
    callOnMainThread([this, protectedThis = Ref { *this }] {
        WebLockRegistry::shared().clientIsGoingAway(m_sessionID, m_clientOrigin, m_clientID);
    });
}

Ref<WebLockManager> WebLockManager::create(NavigatorBase& navigator)
{
    auto manager = adoptRef(*new WebLockManager(navigator));
    manager->suspendIfNeeded();
    return manager;
}

WebLockManager::WebLockManager(NavigatorBase& navigator)
    : ActiveDOMObject(navigator.scriptExecutionContext())
    , m_mainThreadBridge(MainThreadBridge::create(navigator.scriptExecutionContext()))
{
}

WebLockManager::~WebLockManager()
{
    clientIsGoingAway();
}

void WebLockManager::request(const String& name, Ref<WebLockGrantedCallback>&& grantedCallback, Ref<DeferredPromise>&& promise)
{
    request(name, { }, WTFMove(grantedCallback), WTFMove(promise));
}

void WebLockManager::request(const String& name, Options&& options, Ref<WebLockGrantedCallback>&& grantedCallback, Ref<DeferredPromise>&& releasePromise)
{
    UNUSED_PARAM(name);
    if (!scriptExecutionContext()) {
        releasePromise->reject(InvalidStateError, "Context is invalid"_s);
        return;
    }
    auto& context = *scriptExecutionContext();
    if ((is<Document>(context) && !downcast<Document>(context).isFullyActive())) {
        releasePromise->reject(InvalidStateError, "Responsible document is not fully active"_s);
        return;
    }

    if (!m_mainThreadBridge) {
        releasePromise->reject(SecurityError, "Context's origin is opaque"_s);
        return;
    }

    if (name.startsWith('-')) {
        releasePromise->reject(NotSupportedError, "Lock name cannot start with '-'"_s);
        return;
    }

    if (options.steal && options.ifAvailable) {
        releasePromise->reject(NotSupportedError, "WebLockOptions's steal and ifAvailable cannot both be true"_s);
        return;
    }

    if (options.steal && options.mode != WebLockMode::Exclusive) {
        releasePromise->reject(NotSupportedError, "WebLockOptions's steal is true but mode is not 'exclusive'"_s);
        return;
    }

    if (options.signal && (options.steal || options.ifAvailable)) {
        releasePromise->reject(NotSupportedError, "WebLockOptions's steal and ifAvailable need to be false when a signal is provided"_s);
        return;
    }

    if (options.signal && options.signal->aborted()) {
        releasePromise->reject(AbortError, "WebLockOptions's signal is aborted"_s);
        return;
    }

    WebLockIdentifier lockIdentifier = WebLockIdentifier::generateThreadSafe();
    m_releasePromises.add(lockIdentifier, WTFMove(releasePromise));

    if (options.signal) {
        options.signal->addAlgorithm([weakThis = WeakPtr { *this }, lockIdentifier](JSC::JSValue reason) mutable {
            if (weakThis)
                weakThis->signalToAbortTheRequest(lockIdentifier, reason);
        });
    }

    m_pendingRequests.add(lockIdentifier, LockRequest { lockIdentifier, name, options.mode, WTFMove(grantedCallback), WTFMove(options.signal) });

    m_mainThreadBridge->requestLock(lockIdentifier, name, options, [weakThis = WeakPtr { *this }, lockIdentifier](bool success) mutable {
        if (weakThis)
            weakThis->didCompleteLockRequest(lockIdentifier, success);
    }, [weakThis = WeakPtr { *this }, lockIdentifier]() mutable {
        if (weakThis)
            weakThis->settleReleasePromise(lockIdentifier, Exception { AbortError, "Lock was stolen by another request"_s });
    });
}

void WebLockManager::didCompleteLockRequest(WebLockIdentifier lockIdentifier, bool success)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, weakThis = WeakPtr { *this }, lockIdentifier, success]() mutable {
        auto request = m_pendingRequests.take(lockIdentifier);
        if (!request.isValid())
            return;

        if (success) {
            if (request.signal && request.signal->aborted()) {
                m_mainThreadBridge->releaseLock(request.lockIdentifier, request.name);
                return;
            }

            auto lock = WebLock::create(request.lockIdentifier, request.name, request.mode);
            auto result = request.grantedCallback->handleEvent(lock.ptr());
            RefPtr<DOMPromise> waitingPromise = result.type() == CallbackResultType::Success ? result.releaseReturnValue() : nullptr;
            if (!waitingPromise || waitingPromise->isSuspended()) {
                m_mainThreadBridge->releaseLock(request.lockIdentifier, request.name);
                settleReleasePromise(request.lockIdentifier, Exception { ExistingExceptionError });
                return;
            }

            DOMPromise::whenPromiseIsSettled(waitingPromise->globalObject(), waitingPromise->promise(), [this, weakThis = WTFMove(weakThis), lockIdentifier = request.lockIdentifier, name = request.name, waitingPromise] {
                if (!weakThis)
                    return;
                m_mainThreadBridge->releaseLock(lockIdentifier, name);
                settleReleasePromise(lockIdentifier, static_cast<JSC::JSValue>(waitingPromise->promise()));
            });
        } else {
            auto result = request.grantedCallback->handleEvent(nullptr);
            RefPtr<DOMPromise> waitingPromise = result.type() == CallbackResultType::Success ? result.releaseReturnValue() : nullptr;
            if (!waitingPromise || waitingPromise->isSuspended()) {
                settleReleasePromise(request.lockIdentifier, Exception { ExistingExceptionError });
                return;
            }
            settleReleasePromise(request.lockIdentifier, static_cast<JSC::JSValue>(waitingPromise->promise()));
        }
    });
}

void WebLockManager::query(Ref<DeferredPromise>&& promise)
{
    if (!scriptExecutionContext()) {
        promise->reject(InvalidStateError, "Context is invalid"_s);
        return;
    }
    auto& context = *scriptExecutionContext();
    if ((is<Document>(context) && !downcast<Document>(context).isFullyActive())) {
        promise->reject(InvalidStateError, "Responsible document is not fully active"_s);
        return;
    }

    if (!m_mainThreadBridge) {
        promise->reject(SecurityError, "Context's origin is opaque"_s);
        return;
    }

    m_mainThreadBridge->query([weakThis = WeakPtr { *this }, promise = WTFMove(promise)](Snapshot&& snapshot) mutable {
        if (!weakThis)
            return;

        weakThis->queueTaskKeepingObjectAlive(*weakThis, TaskSource::DOMManipulation, [promise = WTFMove(promise), snapshot = WTFMove(snapshot)]() mutable {
            promise->resolve<IDLDictionary<Snapshot>>(WTFMove(snapshot));
        });
    });
}

// https://w3c.github.io/web-locks/#signal-to-abort-the-request
void WebLockManager::signalToAbortTheRequest(WebLockIdentifier lockIdentifier, JSC::JSValue reason)
{
    if (!scriptExecutionContext() || !m_mainThreadBridge)
        return;

    auto requestsIterator = m_pendingRequests.find(lockIdentifier);
    if (requestsIterator == m_pendingRequests.end())
        return;
    auto& request = requestsIterator->value;

    m_mainThreadBridge->abortLockRequest(request.lockIdentifier, request.name, [weakThis = WeakPtr { *this }, lockIdentifier](bool wasAborted) {
        if (wasAborted && weakThis)
            weakThis->m_pendingRequests.remove(lockIdentifier);
    });
    if (auto releasePromise = m_releasePromises.take(lockIdentifier))
        releasePromise->reject<IDLAny>(reason);
}

void WebLockManager::settleReleasePromise(WebLockIdentifier lockIdentifier, ExceptionOr<JSC::JSValue>&& result)
{
    auto releasePromise = m_releasePromises.take(lockIdentifier);
    if (!releasePromise)
        return;

    if (result.hasException())
        releasePromise->reject(result.releaseException());
    else
        releasePromise->resolveWithJSValue(result.releaseReturnValue());
}

void WebLockManager::stop()
{
    clientIsGoingAway();
}

void WebLockManager::clientIsGoingAway()
{
    if (m_pendingRequests.isEmpty() && m_releasePromises.isEmpty())
        return;

    m_pendingRequests.clear();
    m_releasePromises.clear();

    if (m_mainThreadBridge)
        m_mainThreadBridge->clientIsGoingAway();
}

bool WebLockManager::virtualHasPendingActivity() const
{
    return !m_pendingRequests.isEmpty() || !m_releasePromises.isEmpty();
}

const char* WebLockManager::activeDOMObjectName() const
{
    return "WebLockManager";
}

} // namespace WebCore
