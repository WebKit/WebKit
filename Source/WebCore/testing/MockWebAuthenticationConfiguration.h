/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct MockWebAuthenticationConfiguration {
    enum class HidStage : bool {
        Info,
        Request
    };

    enum class HidSubStage : bool {
        Init,
        Msg
    };

    enum class HidError : uint8_t {
        Success,
        DataNotSent,
        EmptyReport,
        WrongChannelId,
        MaliciousPayload,
        UnsupportedOptions,
        WrongNonce
    };

    enum class NfcError : uint8_t {
        Success,
        NoTags,
        WrongTagType,
        NoConnections,
        MaliciousPayload
    };

    enum class UserVerification : uint8_t {
        No,
        Yes,
        Cancel,
        Presence
    };

    struct LocalConfiguration {
        UserVerification userVerification { UserVerification::No };
        bool acceptAttestation { false };
        String privateKeyBase64;
        String userCertificateBase64;
        String intermediateCACertificateBase64;
        String preferredCredentialIdBase64;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<LocalConfiguration> decode(Decoder&);
    };

    struct HidConfiguration {
        Vector<String> payloadBase64;
        HidStage stage { HidStage::Info };
        HidSubStage subStage { HidSubStage::Init };
        HidError error { HidError::Success };
        bool isU2f { false };
        bool keepAlive { false };
        bool fastDataArrival { false };
        bool continueAfterErrorData { false };
        bool canDowngrade { false };
        bool expectCancel { false };
        bool supportClientPin { false };

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<HidConfiguration> decode(Decoder&);
    };

    struct NfcConfiguration {
        NfcError error { NfcError::Success };
        Vector<String> payloadBase64;
        bool multipleTags { false };
        bool multiplePhysicalTags { false };

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<NfcConfiguration> decode(Decoder&);
    };

    bool silentFailure { false };
    std::optional<LocalConfiguration> local;
    std::optional<HidConfiguration> hid;
    std::optional<NfcConfiguration> nfc;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<MockWebAuthenticationConfiguration> decode(Decoder&);
};

template<class Encoder>
void MockWebAuthenticationConfiguration::LocalConfiguration::encode(Encoder& encoder) const
{
    encoder << userVerification << acceptAttestation << privateKeyBase64 << userCertificateBase64 << intermediateCACertificateBase64 << preferredCredentialIdBase64;
}

template<class Decoder>
std::optional<MockWebAuthenticationConfiguration::LocalConfiguration> MockWebAuthenticationConfiguration::LocalConfiguration::decode(Decoder& decoder)
{
    MockWebAuthenticationConfiguration::LocalConfiguration result;

    std::optional<UserVerification> userVerification;
    decoder >> userVerification;
    if (!userVerification)
        return std::nullopt;
    result.userVerification = *userVerification;

    std::optional<bool> acceptAttestation;
    decoder >> acceptAttestation;
    if (!acceptAttestation)
        return std::nullopt;
    result.acceptAttestation = *acceptAttestation;

    std::optional<String> privateKeyBase64;
    decoder >> privateKeyBase64;
    if (!privateKeyBase64)
        return std::nullopt;
    result.privateKeyBase64 = WTFMove(*privateKeyBase64);

    std::optional<String> userCertificateBase64;
    decoder >> userCertificateBase64;
    if (!userCertificateBase64)
        return std::nullopt;
    result.userCertificateBase64 = WTFMove(*userCertificateBase64);

    std::optional<String> intermediateCACertificateBase64;
    decoder >> intermediateCACertificateBase64;
    if (!intermediateCACertificateBase64)
        return std::nullopt;
    result.intermediateCACertificateBase64 = WTFMove(*intermediateCACertificateBase64);

    std::optional<String> preferredCredentialIdBase64;
    decoder >> preferredCredentialIdBase64;
    if (!preferredCredentialIdBase64)
        return std::nullopt;
    result.preferredCredentialIdBase64 = WTFMove(*preferredCredentialIdBase64);

    return result;
}

template<class Encoder>
void MockWebAuthenticationConfiguration::HidConfiguration::encode(Encoder& encoder) const
{
    encoder << payloadBase64 << stage << subStage << error << isU2f << keepAlive << fastDataArrival << continueAfterErrorData << canDowngrade << expectCancel << supportClientPin;
}

template<class Decoder>
std::optional<MockWebAuthenticationConfiguration::HidConfiguration> MockWebAuthenticationConfiguration::HidConfiguration::decode(Decoder& decoder)
{
    MockWebAuthenticationConfiguration::HidConfiguration result;
    if (!decoder.decode(result.payloadBase64))
        return std::nullopt;
    if (!decoder.decode(result.stage))
        return std::nullopt;
    if (!decoder.decode(result.subStage))
        return std::nullopt;
    if (!decoder.decode(result.error))
        return std::nullopt;
    if (!decoder.decode(result.isU2f))
        return std::nullopt;
    if (!decoder.decode(result.keepAlive))
        return std::nullopt;
    if (!decoder.decode(result.fastDataArrival))
        return std::nullopt;
    if (!decoder.decode(result.continueAfterErrorData))
        return std::nullopt;
    if (!decoder.decode(result.canDowngrade))
        return std::nullopt;
    if (!decoder.decode(result.expectCancel))
        return std::nullopt;
    if (!decoder.decode(result.supportClientPin))
        return std::nullopt;
    return result;
}

template<class Encoder>
void MockWebAuthenticationConfiguration::NfcConfiguration::encode(Encoder& encoder) const
{
    encoder << error << payloadBase64 << multipleTags << multiplePhysicalTags;
}

template<class Decoder>
std::optional<MockWebAuthenticationConfiguration::NfcConfiguration> MockWebAuthenticationConfiguration::NfcConfiguration::decode(Decoder& decoder)
{
    MockWebAuthenticationConfiguration::NfcConfiguration result;
    if (!decoder.decode(result.error))
        return std::nullopt;
    if (!decoder.decode(result.payloadBase64))
        return std::nullopt;
    if (!decoder.decode(result.multipleTags))
        return std::nullopt;
    if (!decoder.decode(result.multiplePhysicalTags))
        return std::nullopt;
    return result;
}

template<class Encoder>
void MockWebAuthenticationConfiguration::encode(Encoder& encoder) const
{
    encoder << silentFailure << local << hid << nfc;
}

template<class Decoder>
std::optional<MockWebAuthenticationConfiguration> MockWebAuthenticationConfiguration::decode(Decoder& decoder)
{
    MockWebAuthenticationConfiguration result;

    std::optional<bool> silentFailure;
    decoder >> silentFailure;
    if (!silentFailure)
        return std::nullopt;
    result.silentFailure = *silentFailure;

    std::optional<std::optional<LocalConfiguration>> local;
    decoder >> local;
    if (!local)
        return std::nullopt;
    result.local = WTFMove(*local);

    std::optional<std::optional<HidConfiguration>> hid;
    decoder >> hid;
    if (!hid)
        return std::nullopt;
    result.hid = WTFMove(*hid);

    std::optional<std::optional<NfcConfiguration>> nfc;
    decoder >> nfc;
    if (!nfc)
        return std::nullopt;
    result.nfc = WTFMove(*nfc);

    return result;
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::MockWebAuthenticationConfiguration::HidStage> {
    using values = EnumValues<
        WebCore::MockWebAuthenticationConfiguration::HidStage,
        WebCore::MockWebAuthenticationConfiguration::HidStage::Info,
        WebCore::MockWebAuthenticationConfiguration::HidStage::Request
    >;
};

template<> struct EnumTraits<WebCore::MockWebAuthenticationConfiguration::HidSubStage> {
    using values = EnumValues<
        WebCore::MockWebAuthenticationConfiguration::HidSubStage,
        WebCore::MockWebAuthenticationConfiguration::HidSubStage::Init,
        WebCore::MockWebAuthenticationConfiguration::HidSubStage::Msg
    >;
};

template<> struct EnumTraits<WebCore::MockWebAuthenticationConfiguration::HidError> {
    using values = EnumValues<
        WebCore::MockWebAuthenticationConfiguration::HidError,
        WebCore::MockWebAuthenticationConfiguration::HidError::Success,
        WebCore::MockWebAuthenticationConfiguration::HidError::DataNotSent,
        WebCore::MockWebAuthenticationConfiguration::HidError::EmptyReport,
        WebCore::MockWebAuthenticationConfiguration::HidError::WrongChannelId,
        WebCore::MockWebAuthenticationConfiguration::HidError::MaliciousPayload,
        WebCore::MockWebAuthenticationConfiguration::HidError::UnsupportedOptions,
        WebCore::MockWebAuthenticationConfiguration::HidError::WrongNonce
    >;
};

template<> struct EnumTraits<WebCore::MockWebAuthenticationConfiguration::NfcError> {
    using values = EnumValues<
        WebCore::MockWebAuthenticationConfiguration::NfcError,
        WebCore::MockWebAuthenticationConfiguration::NfcError::Success,
        WebCore::MockWebAuthenticationConfiguration::NfcError::NoTags,
        WebCore::MockWebAuthenticationConfiguration::NfcError::WrongTagType,
        WebCore::MockWebAuthenticationConfiguration::NfcError::NoConnections,
        WebCore::MockWebAuthenticationConfiguration::NfcError::MaliciousPayload
    >;
};

template<> struct EnumTraits<WebCore::MockWebAuthenticationConfiguration::UserVerification> {
    using values = EnumValues<
        WebCore::MockWebAuthenticationConfiguration::UserVerification,
        WebCore::MockWebAuthenticationConfiguration::UserVerification::No,
        WebCore::MockWebAuthenticationConfiguration::UserVerification::Yes,
        WebCore::MockWebAuthenticationConfiguration::UserVerification::Cancel,
        WebCore::MockWebAuthenticationConfiguration::UserVerification::Presence
    >;
};

} // namespace WTF

#endif // ENABLE(WEB_AUTHN)
