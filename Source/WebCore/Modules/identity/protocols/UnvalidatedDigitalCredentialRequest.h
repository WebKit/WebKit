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

#pragma once

#include <WebCore/MobileDocumentRequest.h>
#include <WebCore/OpenID4VPMultisignedRequest.h>
#include <WebCore/OpenID4VPSignedRequest.h>
#include <WebCore/OpenID4VPUnsignedRequest.h>
#include <optional>
#include <wtf/JSONValues.h>
#include <wtf/Variant.h>

namespace WebCore {

// Every alternative must be IPC-serializable; see WebCoreArgumentCodersAuth.serialization.in.
using UnvalidatedDigitalCredentialRequest = Variant<
    MobileDocumentRequest,
    OpenID4VPSignedRequest,
    OpenID4VPMultisignedRequest,
    OpenID4VPUnsignedRequest
>;

inline std::optional<std::pair<DigitalCredentialPresentationProtocol, String>> openID4VPRequestJSON(const UnvalidatedDigitalCredentialRequest& request)
{
    using enum DigitalCredentialPresentationProtocol;

    return WTF::switchOn(request,
        [](const OpenID4VPSignedRequest& signedRequest) -> std::optional<std::pair<DigitalCredentialPresentationProtocol, String>> {
            Ref object = JSON::Object::create();
            object->setString("request"_s, signedRequest.request);
            return std::make_pair(Openid4vpV1Signed, object->toJSONString());
        },
        [](const OpenID4VPMultisignedRequest& multisignedRequest) -> std::optional<std::pair<DigitalCredentialPresentationProtocol, String>> {
            Ref signatures = JSON::Array::create();
            for (auto& signature : multisignedRequest.signatures) {
                Ref signatureObject = JSON::Object::create();
                signatureObject->setString("protected"_s, signature.protectedHeader);
                signatureObject->setString("signature"_s, signature.signature);
                signatures->pushObject(WTF::move(signatureObject));
            }

            Ref object = JSON::Object::create();
            object->setString("payload"_s, multisignedRequest.payload);
            object->setArray("signatures"_s, WTF::move(signatures));
            return std::make_pair(Openid4vpV1Multisigned, object->toJSONString());
        },
        [](const OpenID4VPUnsignedRequest& unsignedRequest) -> std::optional<std::pair<DigitalCredentialPresentationProtocol, String>> {
            return std::make_pair(Openid4vpV1Unsigned, unsignedRequest.requestJSON);
        },
        [](const MobileDocumentRequest&) -> std::optional<std::pair<DigitalCredentialPresentationProtocol, String>> {
            return std::nullopt;
        });
}

} // namespace WebCore
