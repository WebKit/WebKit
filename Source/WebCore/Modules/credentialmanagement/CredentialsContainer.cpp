/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "CredentialCreationOptions.h"
#include "CredentialRequestCoordinator.h"
#include "CredentialRequestOptions.h"
#include "DigitalCredential.h"
#include "DocumentPage.h"
#include "JSBasicCredential.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertNullable.h"
#include "JSDigitalCredential.h"
#include "JSValueInWrappedObjectInlines.h"
#include "LocalFrame.h"
#include "Navigator.h"
#include "Page.h"
#include <wtf/OptionSet.h>

namespace WebCore {

CredentialsContainer::CredentialsContainer(WeakPtr<Document, WeakPtrImplWithEventTargetData>&& document)
    : m_document(WTF::move(document))
{
}

// https://w3c.github.io/webappsec-credential-management/#credential-type-registry
static bool checkCredentialTypeCombinations(const CredentialRequestOptions& options, CredentialPromise& promise)
{
    enum class CredentialType : uint8_t {
        Password  = 1 << 0,
        Federated = 1 << 1,
        Identity  = 1 << 2,
        OTP       = 1 << 3,
        PublicKey = 1 << 4,
        Digital   = 1 << 5,
    };

    OptionSet<CredentialType> requestedTypes;
    if (options.password)
        requestedTypes.add(CredentialType::Password);
    if (options.federated)
        requestedTypes.add(CredentialType::Federated);
    if (options.identity)
        requestedTypes.add(CredentialType::Identity);
    if (options.otp)
        requestedTypes.add(CredentialType::OTP);
    if (options.publicKey)
        requestedTypes.add(CredentialType::PublicKey);
    if (options.digital)
        requestedTypes.add(CredentialType::Digital);

    if (requestedTypes.isEmpty()) {
        promise.reject(Exception { ExceptionCode::NotSupportedError, "No credential type was specified."_s });
        return false;
    }

    // NOTE: allowedWith() must be symmetric — if type A lists B, then B must
    // also list A. Asymmetry causes inconsistent rejection depending on
    // iteration order. See https://w3c.github.io/webappsec-credential-management/#credential-type-registry
    auto allowedWith = [](CredentialType type) -> OptionSet<CredentialType> {
        switch (type) {
        case CredentialType::Password:  return { CredentialType::Federated };
        case CredentialType::Federated: return { CredentialType::Password };
        case CredentialType::Identity:
        case CredentialType::OTP:
        case CredentialType::PublicKey:
        case CredentialType::Digital:   return { };
        }
        ASSERT_NOT_REACHED();
        return { };
    };

    for (auto type : requestedTypes) {
        auto others = requestedTypes - type;
        if (!others.isEmpty() && !allowedWith(type).containsAll(others)) {
            promise.reject(Exception { ExceptionCode::NotSupportedError, "The credential type combination is not supported."_s });
            return false;
        }
    }
    return true;
}

template<typename Options>
static bool performCommonChecks(const Document& document, const Options& options, CredentialPromise& promise)
{
    if (!document.isFullyActive()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "The document is not fully active."_s });
        return false;
    }

    if (!document.page()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "No browsing context"_s });
        return false;
    }

    if (options.signal && options.signal->aborted()) {
        promise.rejectType<IDLAny>(options.signal->reason().getValue());
        return false;
    }

    if constexpr (std::is_same_v<Options, CredentialRequestOptions>) {
#if ENABLE(WEB_AUTHN)
        if (!checkCredentialTypeCombinations(options, promise))
            return false;
#else
        if (!options.publicKey) {
            promise.reject(Exception { ExceptionCode::NotSupportedError, "Missing request type."_s });
            return false;
        }
#endif
    }

    ASSERT(document.isSecureContext());
    return true;
}

void CredentialsContainer::get(CredentialRequestOptions&& options, CredentialPromise&& promise)
{
    // https://w3c.github.io/webappsec-credential-management/#algorithm-request
    RefPtr document = this->document();
    if (!document) {
        promise.reject(Exception { ExceptionCode::NotSupportedError });
        return;
    }

    if (!performCommonChecks(*document, options, promise))
        return;

    if (options.digital) {
#if HAVE(DIGITAL_CREDENTIALS_UI)
        DigitalCredential::discoverFromExternalSource(*document, WTF::move(promise), WTF::move(options));
#else
        promise.resolve(nullptr);
#endif
        return;
    }

    if (!options.publicKey) {
        promise.resolve(nullptr);
        return;
    }

    document->page()->authenticatorCoordinator().discoverFromExternalSource(*document, WTF::move(options), WTF::move(promise));
}

void CredentialsContainer::store(const BasicCredential&, CredentialPromise&& promise)
{
    promise.reject(Exception { ExceptionCode::NotSupportedError, "Not implemented."_s });
}

void CredentialsContainer::isCreate(CredentialCreationOptions&& options, CredentialPromise&& promise)
{
    RefPtr document = this->document();
    if (!document) {
        promise.reject(Exception { ExceptionCode::NotSupportedError });
        return;
    }

    if (!performCommonChecks(*document, options, promise))
        return;

    // Extra.
    if (!document->hasFocus()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document is not focused."_s });
        return;
    }

    if (options.publicKey) {
        document->page()->authenticatorCoordinator().create(*document, WTF::move(options), WTF::move(options.signal), WTF::move(promise));
        return;
    }

    promise.reject(Exception { ExceptionCode::NotSupportedError, "No credential type was specified."_s });
}

void CredentialsContainer::preventSilentAccess(DOMPromiseDeferred<void>&& promise) const
{
    if (RefPtr document = this->document(); !document->isFullyActive()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "The document is not fully active."_s });
        return;
    }
    promise.resolve();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
