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

    struct LocalConfiguration {
        bool acceptAuthentication { false };
        bool acceptAttestation { false };
        String privateKeyBase64;
        String userCertificateBase64;
        String intermediateCACertificateBase64;
        String preferredUserhandleBase64;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<LocalConfiguration> decode(Decoder&);
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
        template<class Decoder> static Optional<HidConfiguration> decode(Decoder&);
    };

    struct NfcConfiguration {
        NfcError error { NfcError::Success };
        Vector<String> payloadBase64;
        bool multipleTags { false };
        bool multiplePhysicalTags { false };

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<NfcConfiguration> decode(Decoder&);
    };

    bool silentFailure { false };
    Optional<LocalConfiguration> local;
    Optional<HidConfiguration> hid;
    Optional<NfcConfiguration> nfc;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<MockWebAuthenticationConfiguration> decode(Decoder&);
};

template<class Encoder>
void MockWebAuthenticationConfiguration::LocalConfiguration::encode(Encoder& encoder) const
{
    encoder << acceptAuthentication << acceptAttestation << privateKeyBase64 << userCertificateBase64 << intermediateCACertificateBase64 << preferredUserhandleBase64;
}

template<class Decoder>
Optional<MockWebAuthenticationConfiguration::LocalConfiguration> MockWebAuthenticationConfiguration::LocalConfiguration::decode(Decoder& decoder)
{
    MockWebAuthenticationConfiguration::LocalConfiguration result;

    Optional<bool> acceptAuthentication;
    decoder >> acceptAuthentication;
    if (!acceptAuthentication)
        return WTF::nullopt;
    result.acceptAuthentication = *acceptAuthentication;

    Optional<bool> acceptAttestation;
    decoder >> acceptAttestation;
    if (!acceptAttestation)
        return WTF::nullopt;
    result.acceptAttestation = *acceptAttestation;

    Optional<String> privateKeyBase64;
    decoder >> privateKeyBase64;
    if (!privateKeyBase64)
        return WTF::nullopt;
    result.privateKeyBase64 = WTFMove(*privateKeyBase64);

    Optional<String> userCertificateBase64;
    decoder >> userCertificateBase64;
    if (!userCertificateBase64)
        return WTF::nullopt;
    result.userCertificateBase64 = WTFMove(*userCertificateBase64);

    Optional<String> intermediateCACertificateBase64;
    decoder >> intermediateCACertificateBase64;
    if (!intermediateCACertificateBase64)
        return WTF::nullopt;
    result.intermediateCACertificateBase64 = WTFMove(*intermediateCACertificateBase64);

    Optional<String> preferredUserhandleBase64;
    decoder >> preferredUserhandleBase64;
    if (!preferredUserhandleBase64)
        return WTF::nullopt;
    result.preferredUserhandleBase64 = WTFMove(*preferredUserhandleBase64);

    return result;
}

template<class Encoder>
void MockWebAuthenticationConfiguration::HidConfiguration::encode(Encoder& encoder) const
{
    encoder << payloadBase64 << stage << subStage << error << isU2f << keepAlive << fastDataArrival << continueAfterErrorData << canDowngrade << expectCancel << supportClientPin;
}

template<class Decoder>
Optional<MockWebAuthenticationConfiguration::HidConfiguration> MockWebAuthenticationConfiguration::HidConfiguration::decode(Decoder& decoder)
{
    MockWebAuthenticationConfiguration::HidConfiguration result;
    if (!decoder.decode(result.payloadBase64))
        return WTF::nullopt;
    if (!decoder.decodeEnum(result.stage))
        return WTF::nullopt;
    if (!decoder.decodeEnum(result.subStage))
        return WTF::nullopt;
    if (!decoder.decodeEnum(result.error))
        return WTF::nullopt;
    if (!decoder.decode(result.isU2f))
        return WTF::nullopt;
    if (!decoder.decode(result.keepAlive))
        return WTF::nullopt;
    if (!decoder.decode(result.fastDataArrival))
        return WTF::nullopt;
    if (!decoder.decode(result.continueAfterErrorData))
        return WTF::nullopt;
    if (!decoder.decode(result.canDowngrade))
        return WTF::nullopt;
    if (!decoder.decode(result.expectCancel))
        return WTF::nullopt;
    if (!decoder.decode(result.supportClientPin))
        return WTF::nullopt;
    return result;
}

template<class Encoder>
void MockWebAuthenticationConfiguration::NfcConfiguration::encode(Encoder& encoder) const
{
    encoder << error << payloadBase64 << multipleTags << multiplePhysicalTags;
}

template<class Decoder>
Optional<MockWebAuthenticationConfiguration::NfcConfiguration> MockWebAuthenticationConfiguration::NfcConfiguration::decode(Decoder& decoder)
{
    MockWebAuthenticationConfiguration::NfcConfiguration result;
    if (!decoder.decodeEnum(result.error))
        return WTF::nullopt;
    if (!decoder.decode(result.payloadBase64))
        return WTF::nullopt;
    if (!decoder.decode(result.multipleTags))
        return WTF::nullopt;
    if (!decoder.decode(result.multiplePhysicalTags))
        return WTF::nullopt;
    return result;
}

template<class Encoder>
void MockWebAuthenticationConfiguration::encode(Encoder& encoder) const
{
    encoder << silentFailure << local << hid << nfc;
}

template<class Decoder>
Optional<MockWebAuthenticationConfiguration> MockWebAuthenticationConfiguration::decode(Decoder& decoder)
{
    MockWebAuthenticationConfiguration result;

    Optional<bool> silentFailure;
    decoder >> silentFailure;
    if (!silentFailure)
        return WTF::nullopt;
    result.silentFailure = *silentFailure;

    Optional<Optional<LocalConfiguration>> local;
    decoder >> local;
    if (!local)
        return WTF::nullopt;
    result.local = WTFMove(*local);

    Optional<Optional<HidConfiguration>> hid;
    decoder >> hid;
    if (!hid)
        return WTF::nullopt;
    result.hid = WTFMove(*hid);

    Optional<Optional<NfcConfiguration>> nfc;
    decoder >> nfc;
    if (!nfc)
        return WTF::nullopt;
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

} // namespace WTF

#endif // ENABLE(WEB_AUTHN)
