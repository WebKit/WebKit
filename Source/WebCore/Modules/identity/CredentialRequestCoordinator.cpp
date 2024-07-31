/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CredentialRequestCoordinator.h"

#if ENABLE(WEB_AUTHN)

#include "AbortSignal.h"
#include "CredentialRequestCoordinatorClient.h"
#include "CredentialRequestOptions.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/Logger.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

CredentialRequestCoordinator::CredentialRequestCoordinator(std::unique_ptr<CredentialRequestCoordinatorClient>&& client)
    : m_client(WTFMove(client))
{
}

void CredentialRequestCoordinator::discoverFromExternalSource(const Document& document, CredentialRequestOptions&& requestOptions, CredentialPromise&& promise)
{
    if (!m_client) {
        LOG_ERROR("No client found");
        promise.reject(Exception { ExceptionCode::UnknownError, "Unknown internal error."_s });
        return;
    }

    const auto& options = requestOptions.digital.value();

    if (requestOptions.signal) {
        requestOptions.signal->addAlgorithm([weakThis = WeakPtr { *this }](JSC::JSValue) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !protectedThis->m_client)
                return;

            ASSERT(!protectedThis->m_isCancelling);

            protectedThis->m_isCancelling = true;
            protectedThis->m_client->cancel([protectedThis = WTFMove(protectedThis)]() mutable {
                if (!protectedThis)
                    return;
                protectedThis->m_isCancelling = false;
                if (auto queuedRequest = WTFMove(protectedThis->m_queuedRequest))
                    queuedRequest();
            });
        });
    }

    auto callback = [promise = WTFMove(promise), abortSignal = WTFMove(requestOptions.signal)](ExceptionData&& exception) mutable {
        if (abortSignal && abortSignal->aborted()) {
            LOG_ERROR("Request aborted by AbortSignal");
            promise.reject(Exception { ExceptionCode::AbortError, "Aborted by AbortSignal."_s });
            return;
        }
        ASSERT(!exception.message.isNull());
        LOG_ERROR("Exception occurred: %s", exception.message.utf8().data());
        promise.reject(exception.toException());
    };

    RefPtr frame = document.frame();
    ASSERT(frame);

    if (m_isCancelling) {
        m_queuedRequest = [weakThis = WeakPtr { *this }, weakFrame = WeakPtr { *frame }, requestOptions = WTFMove(requestOptions), callback = WTFMove(callback)]() mutable {
            RefPtr protectedThis = weakThis.get();
            RefPtr protectedFrame = weakFrame.get();
            if (!protectedThis || !protectedFrame || !protectedThis->m_client) {
                LOG_ERROR("No this, or no frame, or no client found");
                return;
            }
            const auto options = requestOptions.digital.value();
            protectedThis->m_client->requestDigitalCredential(*protectedFrame, options, WTFMove(callback));
        };
        return;
    }

    m_client->requestDigitalCredential(*frame, options, WTFMove(callback));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
