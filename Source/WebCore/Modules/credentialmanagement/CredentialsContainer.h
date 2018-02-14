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

#pragma once

#if ENABLE(WEB_AUTHN)

#include "JSDOMPromiseDeferred.h"
#include "Timer.h"
#include <wtf/Function.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

class BasicCredential;
class Document;

struct CredentialCreationOptions;
struct CredentialRequestOptions;

class CredentialsContainer : public RefCounted<CredentialsContainer> {
public:
    static Ref<CredentialsContainer> create(WeakPtr<Document>&& document) { return adoptRef(*new CredentialsContainer(WTFMove(document))); }

    void get(CredentialRequestOptions&&, Ref<DeferredPromise>&&);

    void store(const BasicCredential&, Ref<DeferredPromise>&&);

    void isCreate(CredentialCreationOptions&&, Ref<DeferredPromise>&&);

    void preventSilentAccess(Ref<DeferredPromise>&&) const;

private:
    CredentialsContainer(WeakPtr<Document>&&);

    bool doesHaveSameOriginAsItsAncestors();
    template<typename OperationType>
    void dispatchTask(OperationType&&, Ref<DeferredPromise>&&, std::optional<unsigned long> timeOutInMs = std::nullopt);

    WeakPtr<Document> m_document;
    Ref<WorkQueue> m_workQueue;
    WeakPtrFactory<CredentialsContainer> m_weakPtrFactory;

    // Bundle promise and timer, such that the timer can
    // times out the corresponding promsie.
    struct PendingPromise : public RefCounted<PendingPromise> {
        static Ref<PendingPromise> create(Ref<DeferredPromise>&& promise, std::unique_ptr<Timer>&& timer)
        {
            return adoptRef(*new PendingPromise(WTFMove(promise), WTFMove(timer)));
        }
        static Ref<PendingPromise> create(Ref<DeferredPromise>&& promise)
        {
            return adoptRef(*new PendingPromise(WTFMove(promise)));
        }

    private:
        PendingPromise(Ref<DeferredPromise>&&, std::unique_ptr<Timer>&&);
        PendingPromise(Ref<DeferredPromise>&&);

    public:
        Ref<DeferredPromise> promise;
        std::unique_ptr<Timer> timer;
    };
    HashMap<DeferredPromise*, Ref<PendingPromise>> m_pendingPromises;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
