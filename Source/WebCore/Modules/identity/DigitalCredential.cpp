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

#include "AbortSignal.h"
#include "Chrome.h"
#include "CredentialRequestCoordinator.h"
#include "CredentialRequestOptions.h"
#include "DigitalCredentialPresentationProtocol.h"
#include "DocumentPage.h"
#include "DocumentSecurityOrigin.h"
#include "ExceptionOr.h"
#include "FrameDestructionObserverInlines.h"
#include "IDLTypes.h"
#include "JSDOMConvertDictionary.h"
#include "JSDOMConvertJSON.h"
#include "JSDOMPromiseDeferred.h"
#include "JSOpenID4VPMultisignedRequest.h"
#include "JSOpenID4VPSignedRequest.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "MediationRequirement.h"
#include "PermissionsPolicy.h"
#include "Settings.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/JSONObject.h>
#include <Logging.h>
#include <wtf/JSONValues.h>
#include <wtf/UUID.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

Ref<DigitalCredential> DigitalCredential::create(JSC::Strong<JSC::JSObject>&& data, DigitalCredentialPresentationProtocol protocol)
{
    return adoptRef(*new DigitalCredential(WTF::move(data), protocol));
}

DigitalCredential::~DigitalCredential() = default;

DigitalCredential::DigitalCredential(JSC::Strong<JSC::JSObject>&& data, DigitalCredentialPresentationProtocol protocol)
    : BasicCredential(createVersion4UUIDString(), Type::DigitalCredential, Discovery::CredentialStore)
    , m_protocol(protocol)
    , m_data(WTF::move(data))
{
}

bool DigitalCredential::userAgentAllowsProtocol(const Document& document, const String& protocol)
{
    auto parsed = digitalCredentialPresentationProtocolFromString(protocol);
    if (!parsed)
        return false;

    using enum DigitalCredentialPresentationProtocol;
    switch (*parsed) {
    case OrgIsoMdoc:
        return true;
    case Openid4vpV1Unsigned:
    case Openid4vpV1Signed:
    case Openid4vpV1Multisigned:
        return document.settings().digitalCredentialsOpenID4VPEnabled();
    }
    return false;
}

static std::optional<DigitalCredentialPresentationProtocol> convertProtocolString(const Document& document, const String& protocolString)
{
    auto protocol = digitalCredentialPresentationProtocolFromString(protocolString);
    if (!protocol)
        return std::nullopt;

    using enum DigitalCredentialPresentationProtocol;
    switch (*protocol) {
    case OrgIsoMdoc:
        return protocol;
    case Openid4vpV1Signed:
    case Openid4vpV1Multisigned:
        return document.settings().digitalCredentialsOpenID4VPEnabled() ? protocol : std::nullopt;
    case Openid4vpV1Unsigned:
        // FIXME (webkit.org/b/320207): support once DCQL parsing lands.
        return std::nullopt;
    }
    return std::nullopt;
}

static ExceptionOr<std::optional<UnvalidatedDigitalCredentialRequest>> jsToCredentialRequest(const Document& document, const DigitalCredentialGetRequest& request)
{
    auto protocol = convertProtocolString(document, request.protocol);
    if (!protocol)
        return std::optional<UnvalidatedDigitalCredentialRequest> { std::nullopt }; // Skip requests with an unsupported protocol.

    auto scope = DECLARE_THROW_SCOPE(document.globalObject()->vm());
    auto* globalObject = document.globalObject();

    // Check that the object is JSON stringifiable.
    JSC::JSONStringify(globalObject, request.data.get(), 0);
    if (scope.exception()) [[unlikely]]
        return Exception { ExceptionCode::ExistingExceptionError };

    using enum DigitalCredentialPresentationProtocol;
    switch (*protocol) {
    case OrgIsoMdoc: {
        auto result = convertDictionary<MobileDocumentRequest>(*globalObject, request.data.get());
        if (result.hasException(scope)) [[unlikely]]
            return Exception { ExceptionCode::ExistingExceptionError };
        return std::make_optional<UnvalidatedDigitalCredentialRequest>(result.releaseReturnValue());
    }
    case Openid4vpV1Signed: {
        auto result = convertDictionary<OpenID4VPSignedRequest>(*globalObject, request.data.get());
        if (result.hasException(scope)) [[unlikely]]
            return Exception { ExceptionCode::ExistingExceptionError };
        return std::make_optional<UnvalidatedDigitalCredentialRequest>(result.releaseReturnValue());
    }
    case Openid4vpV1Multisigned: {
        auto result = convertDictionary<OpenID4VPMultisignedRequest>(*globalObject, request.data.get());
        if (result.hasException(scope)) [[unlikely]]
            return Exception { ExceptionCode::ExistingExceptionError };
        return std::make_optional<UnvalidatedDigitalCredentialRequest>(result.releaseReturnValue());
    }
    case Openid4vpV1Unsigned:
        // FIXME (webkit.org/b/320207): support once DCQL parsing lands.
        return std::optional<UnvalidatedDigitalCredentialRequest> { std::nullopt };
    }
}

ExceptionOr<Vector<UnvalidatedDigitalCredentialRequest>> DigitalCredential::convertObjectsToDigitalPresentationRequests(const Document& document, const Vector<DigitalCredentialGetRequest>& requests)
{
    Vector<UnvalidatedDigitalCredentialRequest> results;
    for (auto& request : requests) {
        auto result = jsToCredentialRequest(document, request);
        if (result.hasException())
            return result.releaseException();

        if (auto value = result.returnValue()) {
            results.append(*value);
            continue;
        }

        if (RefPtr context = document.scriptExecutionContext()) {
            String warning = makeString("Ignoring DigitalCredentialGetRequest with unsupported protocol: \""_s, request.protocol, "\""_s);
            context->addConsoleMessage(MessageSource::Other, MessageLevel::Warning, warning);
        }
    }

    if (results.isEmpty())
        return Exception { ExceptionCode::TypeError, "At least one supported DigitalCredentialGetRequest must present"_s };

    return results;
}

void DigitalCredential::discoverFromExternalSource(const Document& document, CredentialPromise&& promise, CredentialRequestOptions&& options)
{
    ASSERT(options.digital);

    if (document.securityOrigin().isOpaque()) {
        promise.reject(Exception { ExceptionCode::SecurityError, "The credential operation is not allowed in an opaque origin."_s });
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

    if (!document.isFullyActiveAndHasUserAttention()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document must be focused and visible."_s });
        return;
    }

    if (options.digital->requests.isEmpty()) {
        promise.reject(Exception { ExceptionCode::TypeError, "At least one request must present."_s });
        return;
    }

    options.digital->requests.removeAllMatching([&](auto& request) {
        if (userAgentAllowsProtocol(document, request.protocol))
            return false;
        if (RefPtr context = document.scriptExecutionContext()) {
            String warning = makeString("Ignoring DigitalCredentialGetRequest with unsupported protocol: \""_s, request.protocol, "\""_s);
            context->addConsoleMessage(MessageSource::Other, MessageLevel::Warning, warning);
        }
        return true;
    });

    if (options.digital->requests.isEmpty()) {
        promise.reject(Exception { ExceptionCode::TypeError, "At least one supported DigitalCredentialGetRequest must be present."_s });
        return;
    }

    if (!window->consumeTransientActivation()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "Calling get() needs to be triggered by an activation triggering user event."_s });
        return;
    }

    auto presentationRequestsOrException = convertObjectsToDigitalPresentationRequests(document, options.digital->requests);
    if (presentationRequestsOrException.hasException()) {
        promise.reject(presentationRequestsOrException.releaseException());
        return;
    }

    Ref coordinator = page->credentialRequestCoordinator();
    coordinator->prepareCredentialRequests(document, WTF::move(promise), presentationRequestsOrException.releaseReturnValue(), options.signal);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
