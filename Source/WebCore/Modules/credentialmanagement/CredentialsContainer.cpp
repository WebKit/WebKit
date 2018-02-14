/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CredentialsContainer.h"

#if ENABLE(WEB_AUTHN)

#include "AbortSignal.h"
#include "CredentialCreationOptions.h"
#include "CredentialRequestOptions.h"
#include "Document.h"
#include "ExceptionOr.h"
#include "JSBasicCredential.h"
#include "PublicKeyCredential.h"
#include "SecurityOrigin.h"
#include <wtf/MainThread.h>

namespace WebCore {

CredentialsContainer::PendingPromise::PendingPromise(Ref<DeferredPromise>&& promise, std::unique_ptr<Timer>&& timer)
    : promise(WTFMove(promise))
    , timer(WTFMove(timer))
{
}

CredentialsContainer::PendingPromise::PendingPromise(Ref<DeferredPromise>&& promise)
    : promise(WTFMove(promise))
{
}

CredentialsContainer::CredentialsContainer(WeakPtr<Document>&& document)
    : m_document(WTFMove(document))
    , m_workQueue(WorkQueue::create("com.apple.WebKit.CredentialQueue"))
{
}

// The following implements https://w3c.github.io/webappsec-credential-management/#same-origin-with-its-ancestors
// as of 14 November 2017.
bool CredentialsContainer::doesHaveSameOriginAsItsAncestors()
{
    if (!m_document)
        return false;

    auto& origin = m_document->securityOrigin();
    for (auto* document = m_document->parentDocument(); document; document = document->parentDocument()) {
        if (!originsMatch(document->securityOrigin(), origin))
            return false;
    }
    return true;
}

// FIXME(181946): Since the underlying authenticator model is not clear at this moment, the timer is moved to CredentialsContainer such that
// timer can stay with main thread and therefore can easily time out activities on the work queue.
// FIXME(181945): The usages of AbortSignal are also moved here for the very same reason. Also the AbortSignal is kind of bogus at this moment
// since it doesn't support observers (or other means) to trigger the actual abort action. Enhancement to AbortSignal is needed.
template<typename OperationType>
void CredentialsContainer::dispatchTask(OperationType&& operation, Ref<DeferredPromise>&& promise, std::optional<unsigned long> timeOutInMs)
{
    ASSERT(isMainThread());
    if (!m_document)
        return;

    auto* promiseIndex = promise.ptr();
    auto weakThis = m_weakPtrFactory.createWeakPtr(*this);
    // FIXME(181947): We should probably trim timeOutInMs to some max allowable number.
    if (timeOutInMs) {
        auto pendingPromise = PendingPromise::create(WTFMove(promise), std::make_unique<Timer>([promiseIndex, weakThis] () {
            ASSERT(isMainThread());
            if (weakThis) {
                // A lock should not be needed as all callbacks are executed in the main thread.
                if (auto promise = weakThis->m_pendingPromises.take(promiseIndex))
                    promise.value()->promise->reject(Exception { NotAllowedError });
            }
        }));
        pendingPromise->timer->startOneShot(Seconds(timeOutInMs.value() / 1000.0));
        m_pendingPromises.add(promiseIndex, WTFMove(pendingPromise));
    } else
        m_pendingPromises.add(promiseIndex, PendingPromise::create(WTFMove(promise)));

    auto task = [promiseIndex, weakThis, origin = m_document->securityOrigin().isolatedCopy(), isSameOriginWithItsAncestors = doesHaveSameOriginAsItsAncestors(), operation = WTFMove(operation)] () {
        auto result = operation(origin, isSameOriginWithItsAncestors);
        callOnMainThread([promiseIndex, weakThis, result = WTFMove(result)] () mutable {
            if (weakThis) {
                // A lock should not be needed as all callbacks are executed in the main thread.
                if (auto promise = weakThis->m_pendingPromises.take(promiseIndex)) {
                    if (result.hasException())
                        promise.value()->promise->reject(result.releaseException());
                    else
                        promise.value()->promise->resolve<IDLNullable<IDLInterface<BasicCredential>>>(result.returnValue().get());
                }
            }
        });
    };
    m_workQueue->dispatch(WTFMove(task));
}

void CredentialsContainer::get(CredentialRequestOptions&& options, Ref<DeferredPromise>&& promise)
{
    // The following implements https://www.w3.org/TR/credential-management-1/#algorithm-request as of 4 August 2017
    // with enhancement from 14 November 2017 Editor's Draft.
    // FIXME: Optional options are passed with no contents. It should be std::optional.
    if ((!options.signal && !options.publicKey) || !m_document) {
        promise->reject(Exception { NotSupportedError });
        return;
    }
    if (options.signal && options.signal->aborted()) {
        promise->reject(Exception { AbortError });
        return;
    }
    // Step 1-2.
    ASSERT(m_document->isSecureContext());

    // Step 3 is enhanced with doesHaveSameOriginAsItsAncestors.
    // Step 4-6. Shortcut as we only support PublicKeyCredential which can only
    // be requested from [[discoverFromExternalSource]].
    if (!options.publicKey) {
        promise->reject(Exception { NotSupportedError });
        return;
    }

    auto timeout = options.publicKey->timeout;
    auto operation = [options = WTFMove(options.publicKey.value())] (const SecurityOrigin& origin, bool isSameOriginWithItsAncestors) {
        return PublicKeyCredential::discoverFromExternalSource(origin, options, isSameOriginWithItsAncestors);
    };
    dispatchTask(WTFMove(operation), WTFMove(promise), timeout);
}

void CredentialsContainer::store(const BasicCredential&, Ref<DeferredPromise>&& promise)
{
    promise->reject(Exception { NotSupportedError });
}

void CredentialsContainer::isCreate(CredentialCreationOptions&& options, Ref<DeferredPromise>&& promise)
{
    // The following implements https://www.w3.org/TR/credential-management-1/#algorithm-create as of 4 August 2017
    // with enhancement from 14 November 2017 Editor's Draft.
    // FIXME: Optional options are passed with no contents. It should be std::optional.
    if ((!options.signal && !options.publicKey) || !m_document) {
        promise->reject(Exception { NotSupportedError });
        return;
    }
    if (options.signal && options.signal->aborted()) {
        promise->reject(Exception { AbortError });
        return;
    }
    // Step 1-2.
    ASSERT(m_document->isSecureContext());

    // Step 3-4. Shortcut as we only support one kind of credentials.
    if (!options.publicKey) {
        promise->reject(Exception { NotSupportedError });
        return;
    }

    auto timeout = options.publicKey->timeout;
    // Step 5-7.
    auto operation = [options = WTFMove(options.publicKey.value())] (const SecurityOrigin& origin, bool isSameOriginWithItsAncestors) {
        // Shortcut as well.
        return PublicKeyCredential::create(origin, options, isSameOriginWithItsAncestors);
    };
    dispatchTask(WTFMove(operation), WTFMove(promise), timeout);
}

void CredentialsContainer::preventSilentAccess(Ref<DeferredPromise>&& promise) const
{
    promise->reject(Exception { NotSupportedError });
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
