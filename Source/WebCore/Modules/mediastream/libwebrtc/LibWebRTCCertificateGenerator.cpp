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

#if USE(LIBWEBRTC)

#include "JSDOMPromiseDeferred.h"
#include "JSRTCCertificate.h"
#include "LibWebRTCMacros.h"
#include "RTCCertificate.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/rtc_base/rtc_certificate_generator.h>

ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

namespace LibWebRTCCertificateGenerator {

static inline String fromStdString(const std::string& value)
{
    return String::fromUTF8(value.data(), value.length());
}

class RTCCertificateGeneratorCallback : public ThreadSafeRefCounted<RTCCertificateGeneratorCallback, WTF::DestructionThread::Main>, public rtc::RTCCertificateGeneratorCallback {
public:
    RTCCertificateGeneratorCallback(Ref<SecurityOrigin>&& origin, DOMPromiseDeferred<IDLInterface<RTCCertificate>>&& promise)
        : m_origin(WTFMove(origin))
        , m_promise(WTFMove(promise))
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
        callOnMainThread([origin = m_origin.releaseNonNull(), promise = WTFMove(m_promise.value()), certificate]() mutable {
            Vector<RTCCertificate::DtlsFingerprint> fingerprints;
            auto stats = certificate->ssl_certificate().GetStats();
            auto* info = stats.get();
            while (info) {
                StringView fingerprint { reinterpret_cast<const unsigned char*>(info->fingerprint.data()), static_cast<unsigned>(info->fingerprint.length()) };
                fingerprints.append({ fromStdString(info->fingerprint_algorithm), fingerprint.convertToASCIILowercase() });
                info = info->issuer.get();
            };

            auto pem = certificate->ToPEM();
            promise.resolve(RTCCertificate::create(WTFMove(origin), certificate->Expires(), WTFMove(fingerprints), fromStdString(pem.certificate()), fromStdString(pem.private_key())));
        });
    }

    void OnFailure() final
    {
        callOnMainThread([promise = WTFMove(m_promise.value())]() mutable {
            promise.reject(Exception { TypeError, "Unable to create a certificate"_s});
        });
    }

    RefPtr<SecurityOrigin> m_origin;
    Optional<DOMPromiseDeferred<IDLInterface<RTCCertificate>>> m_promise;
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

void generateCertificate(Ref<SecurityOrigin>&& origin, LibWebRTCProvider& provider, const PeerConnectionBackend::CertificateInformation& info, DOMPromiseDeferred<IDLInterface<RTCCertificate>>&& promise)
{
    rtc::scoped_refptr<RTCCertificateGeneratorCallback> callback(new rtc::RefCountedObject<RTCCertificateGeneratorCallback>(WTFMove(origin), WTFMove(promise)));

    absl::optional<uint64_t> expiresMs;
    if (info.expires)
        expiresMs = static_cast<uint64_t>(*info.expires);

    provider.prepareCertificateGenerator([info, expiresMs, callback = WTFMove(callback)](auto& generator) mutable {
        generator.GenerateCertificateAsync(keyParamsFromCertificateType(info), expiresMs, WTFMove(callback));
    });
}

} // namespace LibWebRTCCertificateGenerator

} // namespace WebCore

#endif // USE(LIBWEBRTC)
