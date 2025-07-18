/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "DigitalCredential.h"

#if ENABLE(WEB_AUTHN)

#include "Chrome.h"
#include "CredentialRequestCoordinator.h"
#include "CredentialRequestOptions.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "ExceptionOr.h"
#include "IDLTypes.h"
#include "IdentityCredentialProtocol.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "MediationRequirement.h"
#include "Page.h"
#include "PermissionsPolicy.h"
#include "VisibilityState.h"
#include <Logging.h>
#include <wtf/JSONValues.h>
#include <wtf/UUID.h>
#include <wtf/text/Base64.h>

namespace WebCore {

Ref<DigitalCredential> DigitalCredential::create(JSC::Strong<JSC::JSObject>&& data, IdentityCredentialProtocol protocol)
{
    return adoptRef(*new DigitalCredential(WTFMove(data), protocol));
}

DigitalCredential::~DigitalCredential() = default;

DigitalCredential::DigitalCredential(JSC::Strong<JSC::JSObject>&& data, IdentityCredentialProtocol protocol)
    : BasicCredential(createVersion4UUIDString(), Type::DigitalCredential, Discovery::CredentialStore)
    , m_protocol(protocol)
    , m_data(WTFMove(data))
{
}

static ExceptionOr<UnvalidatedDigitalCredentialRequest> jsToCredentialRequest(const Document& document, const DigitalCredentialRequest& request)
{
    auto scope = DECLARE_THROW_SCOPE(document.globalObject()->vm());
    auto* globalObject = document.globalObject();

    // Check that the object is JSON stringifiable.
    JSC::JSONStringify(globalObject, request.data.get(), 0);
    if (scope.exception()) [[unlikely]]
        return Exception { ExceptionCode::ExistingExceptionError };

    switch (request.protocol) {
    case IdentityCredentialProtocol::OrgIsoMdoc: {
        auto result = convertDictionary<MobileDocumentRequest>(*globalObject, request.data.get());
        if (result.hasException(scope)) [[unlikely]]
            return Exception { ExceptionCode::ExistingExceptionError };
        return DigitalCredentialRequestTypes { WTF::InPlaceType<MobileDocumentRequest>, result.releaseReturnValue() };
    }
    case IdentityCredentialProtocol::Openid4vp: {
        auto result = convertDictionary<OpenID4VPRequest>(*globalObject, request.data.get());
        if (result.hasException(scope)) [[unlikely]]
            return Exception { ExceptionCode::ExistingExceptionError };
        return DigitalCredentialRequestTypes { WTF::InPlaceType<OpenID4VPRequest>, result.releaseReturnValue() };
    }
    default:
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::TypeError, "Unsupported protocol."_s };
    }
}

ExceptionOr<Vector<UnvalidatedDigitalCredentialRequest>> DigitalCredential::convertObjectsToDigitalPresentationRequests(const Document& document, const Vector<DigitalCredentialRequest>& requests)
{
    Vector<UnvalidatedDigitalCredentialRequest> results;
    for (auto& request : requests) {
        auto resultOrException = jsToCredentialRequest(document, request);
        if (resultOrException.hasException())
            return resultOrException.releaseException();
        results.append(resultOrException.releaseReturnValue());
    }

    if (results.isEmpty())
        return Exception { ExceptionCode::TypeError, "At least one request must present."_s };

    return results;
}


void DigitalCredential::discoverFromExternalSource(const Document& document, CredentialPromise&& promise, CredentialRequestOptions&& options)
{
    ASSERT(options.digital);

    if (options.mediation != MediationRequirement::Required) {
        promise.reject(Exception { ExceptionCode::TypeError, "User mediation is required for DigitalCredential."_s });
        return;
    }

    if (!PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::DigitalCredentialsGetRule, document, PermissionsPolicy::ShouldReportViolation::No)) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "Third-party iframes are not allowed to call .get() unless explicitly allowed via Permissions Policy (digital-credentials-get)"_s });
        return;
    }

    RefPtr frame = document.frame();
    RefPtr window = document.window();
    if (!frame || !window) {
        LOG(DigitalCredentials, "Preconditions for DigitalCredential.get() are not met");
        promise.reject(ExceptionCode::InvalidStateError, "Preconditions for calling .get() are not met."_s);
        return;
    }

    RefPtr page = frame->page();
    if (!page) {
        LOG(DigitalCredentials, "Preconditions for DigitalCredential.get() are not met");
        promise.reject(ExceptionCode::InvalidStateError, "Preconditions for calling .get() are not met."_s);
        return;
    }

    if (!document.hasFocus()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document is not focused."_s });
        return;
    }

    if (document.visibilityState() != VisibilityState::Visible) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document is not visible."_s });
        return;
    }

    if (options.digital->requests.isEmpty()) {
        promise.reject(Exception { ExceptionCode::TypeError, "At least one request must present."_s });
        return;
    }

    auto presentationRequestsOrException = convertObjectsToDigitalPresentationRequests(document, options.digital->requests);
    if (presentationRequestsOrException.hasException()) {
        promise.reject(presentationRequestsOrException.releaseException());
        return;
    }

    if (!window->consumeTransientActivation()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "Calling get() needs to be triggered by an activation triggering user event."_s });
        return;
    }

#if HAVE(DIGITAL_CREDENTIALS_UI)
    Ref coordinator = page->credentialRequestCoordinator();
    coordinator->presentPicker(document, WTFMove(promise), presentationRequestsOrException.releaseReturnValue(), options.signal);
#else
    promise.reject(Exception { ExceptionCode::NotSupportedError, "Digital credentials are not supported."_s });
#endif
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
