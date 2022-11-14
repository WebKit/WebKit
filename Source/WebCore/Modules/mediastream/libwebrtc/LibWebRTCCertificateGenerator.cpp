/*
 * Copyright (C) 2018 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibWebRTCCertificateGenerator.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "LibWebRTCProvider.h"
#include "RTCCertificate.h"

ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_COMMA_BEGIN

#include <webrtc/rtc_base/ref_counted_object.h>
#include <webrtc/rtc_base/rtc_certificate_generator.h>
#include <webrtc/rtc_base/ssl_certificate.h>

ALLOW_COMMA_END
ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

namespace LibWebRTCCertificateGenerator {

static inline String fromStdString(const std::string& value)
{
    return String::fromUTF8(value.data(), value.length());
}

class RTCCertificateGeneratorCallback : public ThreadSafeRefCounted<RTCCertificateGeneratorCallback, WTF::DestructionThread::Main>, public rtc::RTCCertificateGeneratorCallback {
public:
    RTCCertificateGeneratorCallback(Ref<SecurityOrigin>&& origin, Function<void(ExceptionOr<Ref<RTCCertificate>>&&)>&& resultCallback)
        : m_origin(WTFMove(origin))
        , m_resultCallback(WTFMove(resultCallback))
    {
    }

    void AddRef() const { ref(); }
    rtc::RefCountReleaseStatus Release() const
    {
        auto result = refCount() - 1;
        deref();
        return result ? rtc::RefCountReleaseStatus::kOtherRefsRemained : rtc::RefCountReleaseStatus::kDroppedLastRef;
    }

private:
    void OnSuccess(const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) final
    {
        callOnMainThread([origin = m_origin.releaseNonNull(), callback = WTFMove(m_resultCallback), certificate]() mutable {
            Vector<RTCCertificate::DtlsFingerprint> fingerprints;
            auto stats = certificate->GetSSLCertificate().GetStats();
            auto* info = stats.get();
            while (info) {
                StringView fingerprint { reinterpret_cast<const unsigned char*>(info->fingerprint.data()), static_cast<unsigned>(info->fingerprint.length()) };
                fingerprints.append({ fromStdString(info->fingerprint_algorithm), fingerprint.convertToASCIILowercase() });
                info = info->issuer.get();
            };

            auto pem = certificate->ToPEM();
            callback(RTCCertificate::create(WTFMove(origin), certificate->Expires(), WTFMove(fingerprints), fromStdString(pem.certificate()), fromStdString(pem.private_key())));
        });
    }

    void OnFailure() final
    {
        callOnMainThread([callback = WTFMove(m_resultCallback)]() mutable {
            callback(Exception { TypeError, "Unable to create a certificate"_s});
        });
    }

    RefPtr<SecurityOrigin> m_origin;
    Function<void(ExceptionOr<Ref<RTCCertificate>>&&)> m_resultCallback;
};

static inline rtc::KeyParams keyParamsFromCertificateType(const PeerConnectionBackend::CertificateInformation& info)
{
    switch (info.type) {
    case PeerConnectionBackend::CertificateInformation::Type::ECDSAP256:
        return rtc::KeyParams::ECDSA();
    case PeerConnectionBackend::CertificateInformation::Type::RSASSAPKCS1v15:
        if (info.rsaParameters)
            return rtc::KeyParams::RSA(info.rsaParameters->modulusLength, info.rsaParameters->publicExponent);
        return rtc::KeyParams::RSA(2048, 65537);
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void generateCertificate(Ref<SecurityOrigin>&& origin, LibWebRTCProvider& provider, const PeerConnectionBackend::CertificateInformation& info, Function<void(ExceptionOr<Ref<RTCCertificate>>&&)>&& resultCallback)
{
    rtc::scoped_refptr<RTCCertificateGeneratorCallback> callback(new rtc::RefCountedObject<RTCCertificateGeneratorCallback>(WTFMove(origin), WTFMove(resultCallback)));

    absl::optional<uint64_t> expiresMs;
    if (info.expires)
        expiresMs = static_cast<uint64_t>(*info.expires);

    provider.prepareCertificateGenerator([info, expiresMs, callback = WTFMove(callback)](auto& generator) mutable {
        generator.GenerateCertificateAsync(keyParamsFromCertificateType(info), expiresMs, WTFMove(callback));
    });
}

} // namespace LibWebRTCCertificateGenerator

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
