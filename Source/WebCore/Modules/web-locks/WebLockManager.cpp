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
#include "JSWebLockManagerSnapshot.h"
#include "NavigatorBase.h"
#include "SecurityOrigin.h"
#include "WebLockGrantedCallback.h"
#include "WebLockManagerSnapshot.h"
#include "WebLockRegistry.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>

namespace WebCore {

static std::optional<ClientOrigin> clientOriginFromContext(ScriptExecutionContext* context)
{
    if (!context)
        return std::nullopt;
    auto* origin = context->securityOrigin();
    if (!origin || origin->isUnique())
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

Ref<WebLockManager> WebLockManager::create(NavigatorBase& navigator)
{
    auto manager = adoptRef(*new WebLockManager(navigator));
    manager->suspendIfNeeded();
    return manager;
}

WebLockManager::WebLockManager(NavigatorBase& navigator)
    : ActiveDOMObject(navigator.scriptExecutionContext())
    , m_clientOrigin(clientOriginFromContext(navigator.scriptExecutionContext()))
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

    if (!m_clientOrigin) {
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
        options.signal->addAlgorithm([weakThis = WeakPtr { *this }, lockIdentifier]() mutable {
            if (weakThis)
                weakThis->signalToAbortTheRequest(lockIdentifier);
        });
    }

    m_pendingRequests.add(lockIdentifier, LockRequest { lockIdentifier, name, options.mode, WTFMove(grantedCallback), WTFMove(options.signal) });

    requestLockOnMainThread(lockIdentifier, name, options, [weakThis = WeakPtr { *this }, lockIdentifier](bool success) mutable {
        if (weakThis)
            weakThis->didCompleteLockRequest(lockIdentifier, success);
    }, [weakThis = WeakPtr { *this }, lockIdentifier](Exception&& exception) mutable {
        if (weakThis)
            weakThis->settleReleasePromise(lockIdentifier, WTFMove(exception));
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
                releaseLockOnMainThread(request.lockIdentifier, request.name);
                return;
            }

            auto lock = WebLock::create(request.lockIdentifier, request.name, request.mode);
            auto result = request.grantedCallback->handleEvent(lock.ptr());
            RefPtr<DOMPromise> waitingPromise = result.type() == CallbackResultType::Success ? result.releaseReturnValue() : nullptr;
            if (!waitingPromise) {
                releaseLockOnMainThread(request.lockIdentifier, request.name);
                settleReleasePromise(request.lockIdentifier, Exception { ExistingExceptionError });
                return;
            }

            DOMPromise::whenPromiseIsSettled(waitingPromise->globalObject(), waitingPromise->promise(), [this, weakThis = WTFMove(weakThis), lockIdentifier = request.lockIdentifier, name = request.name, waitingPromise] {
                if (!weakThis)
                    return;
                releaseLockOnMainThread(lockIdentifier, name);
                settleReleasePromise(lockIdentifier, static_cast<JSC::JSValue>(waitingPromise->promise()));
            });
        } else {
            auto result = request.grantedCallback->handleEvent(nullptr);
            RefPtr<DOMPromise> waitingPromise = result.type() == CallbackResultType::Success ? result.releaseReturnValue() : nullptr;
            if (!waitingPromise) {
                settleReleasePromise(request.lockIdentifier, Exception { ExistingExceptionError });
                return;
            }
            settleReleasePromise(request.lockIdentifier, static_cast<JSC::JSValue>(waitingPromise->promise()));
        }
    });
}

void WebLockManager::requestLockOnMainThread(WebLockIdentifier lockIdentifier, const String& name, const Options& options, Function<void(bool)>&& grantedHandler, Function<void(Exception&&)>&& releaseHandler)
{
    ensureOnMainThread([contextIdentifier = scriptExecutionContext()->identifier(), clientOrigin = crossThreadCopy(*m_clientOrigin), name = crossThreadCopy(name), mode = options.mode, steal = options.steal, ifAvailable = options.ifAvailable, lockIdentifier, grantedHandler = WTFMove(grantedHandler), releaseHandler = WTFMove(releaseHandler)]() mutable {
        WebLockRegistry::registryForOrigin(clientOrigin)->requestLock(lockIdentifier, contextIdentifier, name, mode, steal, ifAvailable, [contextIdentifier, grantedHandler = WTFMove(grantedHandler)] (bool success) mutable {
            ScriptExecutionContext::ensureOnContextThread(contextIdentifier, [grantedHandler = WTFMove(grantedHandler), success](auto&) mutable {
                grantedHandler(success);
            });
        }, [contextIdentifier, releaseHandler = WTFMove(releaseHandler)](Exception&& exception) mutable {
            ScriptExecutionContext::ensureOnContextThread(contextIdentifier, [releaseHandler = WTFMove(releaseHandler), exception = crossThreadCopy(exception)](auto&) mutable {
                releaseHandler(WTFMove(exception));
            });
        });
    });
}

void WebLockManager::releaseLockOnMainThread(WebLockIdentifier lockIdentifier, const String& name)
{
    ensureOnMainThread([clientOrigin = crossThreadCopy(*m_clientOrigin), lockIdentifier, name = crossThreadCopy(name)] {
        WebLockRegistry::registryForOrigin(clientOrigin)->releaseLock(lockIdentifier, name);
    });
}

void WebLockManager::abortLockRequestOnMainThread(WebLockIdentifier lockIdentifier, const String& name, CompletionHandler<void(bool)>&& completionHandler)
{
    ensureOnMainThread([contextIdentifier = scriptExecutionContext()->identifier(), clientOrigin = crossThreadCopy(*m_clientOrigin), lockIdentifier, name = crossThreadCopy(name), completionHandler = WTFMove(completionHandler)]() mutable {
        WebLockRegistry::registryForOrigin(clientOrigin)->abortLockRequest(lockIdentifier, name, [contextIdentifier, completionHandler = WTFMove(completionHandler)](bool wasAborted) mutable {
            ScriptExecutionContext::ensureOnContextThread(contextIdentifier, [completionHandler = WTFMove(completionHandler), wasAborted](auto&) mutable {
                completionHandler(wasAborted);
            });
        });
    });
}

void WebLockManager::queryOnMainThread(CompletionHandler<void(Snapshot&&)>&& completionHandler)
{
    ensureOnMainThread([contextIdentifier = scriptExecutionContext()->identifier(), clientOrigin = crossThreadCopy(*m_clientOrigin), completionHandler = WTFMove(completionHandler)]() mutable {
        WebLockRegistry::registryForOrigin(clientOrigin)->snapshot([contextIdentifier, completionHandler = WTFMove(completionHandler)](Snapshot&& snapshot) mutable {
            ScriptExecutionContext::ensureOnContextThread(contextIdentifier, [completionHandler = WTFMove(completionHandler), snapshot = crossThreadCopy(snapshot)](auto&) mutable {
                completionHandler(WTFMove(snapshot));
            });
        });
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

    if (!m_clientOrigin) {
        promise->reject(SecurityError, "Context's origin is opaque"_s);
        return;
    }

    queryOnMainThread([weakThis = WeakPtr { *this }, promise = WTFMove(promise)](Snapshot&& snapshot) mutable {
        if (!weakThis)
            return;

        weakThis->queueTaskKeepingObjectAlive(*weakThis, TaskSource::DOMManipulation, [promise = WTFMove(promise), snapshot = WTFMove(snapshot)]() mutable {
            promise->resolve<IDLDictionary<Snapshot>>(WTFMove(snapshot));
        });
    });
}

// https://wicg.github.io/web-locks/#signal-to-abort-the-request
void WebLockManager::signalToAbortTheRequest(WebLockIdentifier lockIdentifier)
{
    if (!scriptExecutionContext())
        return;

    auto requestsIterator = m_pendingRequests.find(lockIdentifier);
    if (requestsIterator == m_pendingRequests.end())
        return;
    auto& request = requestsIterator->value;

    abortLockRequestOnMainThread(request.lockIdentifier, request.name, [weakThis = WeakPtr { *this }, lockIdentifier](bool wasAborted) {
        if (wasAborted && weakThis)
            weakThis->m_pendingRequests.remove(lockIdentifier);
    });
    settleReleasePromise(lockIdentifier, Exception { AbortError, "Lock request was aborted via AbortSignal" });
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

    ensureOnMainThread([clientOrigin = crossThreadCopy(*m_clientOrigin), contextIdentifier = scriptExecutionContext()->identifier()] {
        WebLockRegistry::registryForOrigin(clientOrigin)->clientIsGoingAway(contextIdentifier);
    });
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
