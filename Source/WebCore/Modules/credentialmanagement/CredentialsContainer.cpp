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

#include "AbortSignal.h"
#include "CredentialCreationOptions.h"
#include "CredentialRequestOptions.h"
#include "Document.h"
#include "ExceptionOr.h"
#include "JSBasicCredential.h"
#include "PublicKeyCredential.h"
#include "SecurityOrigin.h"

namespace WebCore {

CredentialsContainer::CredentialsContainer(WeakPtr<Document>&& document)
    : m_document(WTFMove(document))
    , m_workQueue(WorkQueue::create("com.apple.WebKit.CredentialQueue"))
{
}

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

template<typename OperationType>
void CredentialsContainer::dispatchTask(OperationType&& operation, Ref<DeferredPromise>&& promise)
{
    auto* promiseIndex = promise.ptr();
    m_pendingPromises.add(promiseIndex, WTFMove(promise));
    auto weakThis = m_weakPtrFactory.createWeakPtr(*this);
    auto task = [promiseIndex, weakThis, isSameOriginWithItsAncestors = doesHaveSameOriginAsItsAncestors(), operation = WTFMove(operation)] () {
        auto result = operation(isSameOriginWithItsAncestors);
        callOnMainThread([promiseIndex, weakThis, result = WTFMove(result)] () mutable {
            if (weakThis) {
                if (auto promise = weakThis->m_pendingPromises.take(promiseIndex)) {
                    if (result.hasException())
                        promise.value()->reject(result.releaseException());
                    else
                        promise.value()->resolve<IDLNullable<IDLInterface<BasicCredential>>>(result.releaseReturnValue().get());
                }
            }
        });
    };
    m_workQueue->dispatch(WTFMove(task));
}

void CredentialsContainer::get(CredentialRequestOptions&& options, Ref<DeferredPromise>&& promise)
{
    // FIXME: Optional options are passed with no contents. It should be std::optional.
    if ((!options.signal && !options.publicKey) || !m_document) {
        promise->reject(Exception { NotSupportedError });
        return;
    }
    if (options.signal && options.signal->aborted()) {
        promise->reject(Exception { AbortError });
        return;
    }
    ASSERT(m_document->isSecureContext());

    // The followings is a shortcut to https://www.w3.org/TR/credential-management-1/#algorithm-request,
    // as we only support PublicKeyCredential which can only be requested from [[discoverFromExternalSource]].
    if (!options.publicKey) {
        promise->reject(Exception { NotSupportedError });
        return;
    }

    auto operation = [options = WTFMove(options)] (bool isSameOriginWithItsAncestors) {
        return PublicKeyCredential::discoverFromExternalSource(options, isSameOriginWithItsAncestors);
    };
    dispatchTask(WTFMove(operation), WTFMove(promise));
}

void CredentialsContainer::store(const BasicCredential&, Ref<DeferredPromise>&& promise)
{
    promise->reject(Exception { NotSupportedError });
}

void CredentialsContainer::isCreate(CredentialCreationOptions&& options, Ref<DeferredPromise>&& promise)
{
    // FIXME: Optional options are passed with no contents. It should be std::optional.
    if ((!options.signal && !options.publicKey) || !m_document) {
        promise->reject(Exception { NotSupportedError });
        return;
    }
    if (options.signal && options.signal->aborted()) {
        promise->reject(Exception { AbortError });
        return;
    }
    ASSERT(m_document->isSecureContext());

    // This is a shortcut to https://www.w3.org/TR/credential-management-1/#credentialrequestoptions-relevant-credential-interface-objects,
    // as we only support one kind of credentials.
    if (!options.publicKey) {
        promise->reject(Exception { NotSupportedError });
        return;
    }

    auto operation = [options = WTFMove(options)] (bool isSameOriginWithItsAncestors) {
        // Shortcut as well.
        return PublicKeyCredential::create(options, isSameOriginWithItsAncestors);
    };
    dispatchTask(WTFMove(operation), WTFMove(promise));
}

void CredentialsContainer::preventSilentAccess(Ref<DeferredPromise>&& promise)
{
    promise->reject(Exception { NotSupportedError });
}

} // namespace WebCore
