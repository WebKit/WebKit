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

#include <optional>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

constexpr size_t maxOpenID4VPRequestDataLength = 1 * MB;

enum class DigitalCredentialPresentationProtocol : uint8_t {
    OrgIsoMdoc,
    Openid4vpV1Unsigned,
    Openid4vpV1Signed,
    Openid4vpV1Multisigned,
};

inline std::optional<DigitalCredentialPresentationProtocol> digitalCredentialPresentationProtocolFromString(const String& protocol)
{
    if (protocol == "org-iso-mdoc"_s)
        return DigitalCredentialPresentationProtocol::OrgIsoMdoc;
    if (protocol == "openid4vp-v1-unsigned"_s)
        return DigitalCredentialPresentationProtocol::Openid4vpV1Unsigned;
    if (protocol == "openid4vp-v1-signed"_s)
        return DigitalCredentialPresentationProtocol::Openid4vpV1Signed;
    if (protocol == "openid4vp-v1-multisigned"_s)
        return DigitalCredentialPresentationProtocol::Openid4vpV1Multisigned;
    return std::nullopt;
}

inline ASCIILiteral digitalCredentialPresentationProtocolToString(DigitalCredentialPresentationProtocol protocol)
{
    using enum DigitalCredentialPresentationProtocol;
    switch (protocol) {
    case OrgIsoMdoc:
        return "org-iso-mdoc"_s;
    case Openid4vpV1Unsigned:
        return "openid4vp-v1-unsigned"_s;
    case Openid4vpV1Signed:
        return "openid4vp-v1-signed"_s;
    case Openid4vpV1Multisigned:
        return "openid4vp-v1-multisigned"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

inline bool isOpenID4VPPresentationProtocol(DigitalCredentialPresentationProtocol protocol)
{
    using enum DigitalCredentialPresentationProtocol;
    switch (protocol) {
    case OrgIsoMdoc:
        return false;
    case Openid4vpV1Unsigned:
    case Openid4vpV1Signed:
    case Openid4vpV1Multisigned:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore
