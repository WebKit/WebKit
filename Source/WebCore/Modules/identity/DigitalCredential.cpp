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
#include "DigitalCredentialPresentationProtocol.h"
#include "DocumentPage.h"
#include "ExceptionOr.h"
#include "FrameDestructionObserverInlines.h"
#include "IDLTypes.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "MediationRequirement.h"
#include "PermissionsPolicy.h"
#include "VisibilityState.h"
#include <JavaScriptCore/ConsoleTypes.h>
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

static std::optional<DigitalCredentialPresentationProtocol> convertProtocolString(const String& protocolString)
{
    if (protocolString == "org-iso-mdoc"_s)
        return DigitalCredentialPresentationProtocol::OrgIsoMdoc;
    return std::nullopt;
}

static ExceptionOr<std::optional<UnvalidatedDigitalCredentialRequest>> jsToCredentialRequest(const Document& document, const DigitalCredentialGetRequest& request)
{
    auto scope = DECLARE_THROW_SCOPE(document.globalObject()->vm());
    auto* globalObject = document.globalObject();

    // Check that the object is JSON stringifiable.
    JSC::JSONStringify(globalObject, request.data.get(), 0);
    if (scope.exception()) [[unlikely]]
        return Exception { ExceptionCode::ExistingExceptionError };

    auto protocol = convertProtocolString(request.protocol);
    if (!protocol)
        return std::optional<UnvalidatedDigitalCredentialRequest> { std::nullopt }; // Return empty optional for unknown protocols

    switch (*protocol) {
    case DigitalCredentialPresentationProtocol::OrgIsoMdoc: {
        auto result = convertDictionary<MobileDocumentRequest>(*globalObject, request.data.get());
        if (result.hasException(scope)) [[unlikely]]
            return Exception { ExceptionCode::ExistingExceptionError };
        return std::make_optional<UnvalidatedDigitalCredentialRequest>(result.releaseReturnValue());
    }
    default:
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::TypeError, "Unsupported protocol."_s };
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

    Ref coordinator = page->credentialRequestCoordinator();
    coordinator->prepareCredentialRequest(document, WTF::move(promise), presentationRequestsOrException.releaseReturnValue(), options.signal);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
