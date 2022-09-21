/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "ArgumentCodersGLib.h"

#include "DataReference.h"
#include <gio/gio.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace IPC {

void ArgumentCoder<GRefPtr<GVariant>>::encode(Encoder& encoder, GRefPtr<GVariant> variant)
{
    if (!variant) {
        encoder << CString();
        return;
    }

    encoder << CString(g_variant_get_type_string(variant.get()));
    encoder << DataReference(static_cast<const uint8_t*>(g_variant_get_data(variant.get())), g_variant_get_size(variant.get()));
}

std::optional<GRefPtr<GVariant>> ArgumentCoder<GRefPtr<GVariant>>::decode(Decoder& decoder)
{
    CString variantTypeString;
    if (!decoder.decode(variantTypeString))
        return std::nullopt;

    if (variantTypeString.isNull())
        return GRefPtr<GVariant>();

    if (!g_variant_type_string_is_valid(variantTypeString.data()))
        return std::nullopt;

    DataReference data;
    if (!decoder.decode(data))
        return std::nullopt;

    GUniquePtr<GVariantType> variantType(g_variant_type_new(variantTypeString.data()));
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(data.data(), data.size()));
    return std::optional<GRefPtr<GVariant> >(g_variant_new_from_bytes(variantType.get(), bytes.get(), FALSE));
}

template<typename Encoder>
void ArgumentCoder<GRefPtr<GTlsCertificate>>::encode(Encoder& encoder, GRefPtr<GTlsCertificate> certificate)
{
    if (!certificate) {
        encoder << 0;
        return;
    }

    Vector<GRefPtr<GByteArray>> certificatesDataList;
    for (auto* nextCertificate = certificate.get(); nextCertificate; nextCertificate = g_tls_certificate_get_issuer(nextCertificate)) {
        GRefPtr<GByteArray> certificateData;
        g_object_get(nextCertificate, "certificate", &certificateData.outPtr(), nullptr);

        if (!certificateData) {
            certificatesDataList.clear();
            break;
        }

        certificatesDataList.append(WTFMove(certificateData));
    }

    encoder << static_cast<uint32_t>(certificatesDataList.size());
    if (certificatesDataList.isEmpty())
        return;

#if GLIB_CHECK_VERSION(2, 69, 0)
    GRefPtr<GByteArray> privateKey;
    GUniqueOutPtr<char> privateKeyPKCS11Uri;
    g_object_get(certificate.get(), "private-key", &privateKey.outPtr(), "private-key-pkcs11-uri", &privateKeyPKCS11Uri.outPtr(), nullptr);
    encoder << IPC::DataReference(privateKey ? privateKey->data : nullptr, privateKey ? privateKey->len : 0);
    encoder << CString(privateKeyPKCS11Uri.get());
#endif

    // Encode starting from the root certificate.
    while (!certificatesDataList.isEmpty()) {
        auto certificateData = certificatesDataList.takeLast();
        encoder << IPC::DataReference(certificateData->data, certificateData->len);
    }
}
template void ArgumentCoder<GRefPtr<GTlsCertificate>>::encode<Encoder>(Encoder&, GRefPtr<GTlsCertificate>);

template<typename Decoder>
std::optional<GRefPtr<GTlsCertificate>> ArgumentCoder<GRefPtr<GTlsCertificate>>::decode(Decoder& decoder)
{
    std::optional<uint32_t> chainLength;
    decoder >> chainLength;
    if (!chainLength)
        return std::nullopt;

    if (!*chainLength)
        return GRefPtr<GTlsCertificate>();

#if GLIB_CHECK_VERSION(2, 69, 0)
    std::optional<IPC::DataReference> privateKeyData;
    decoder >> privateKeyData;
    if (!privateKeyData)
        return std::nullopt;
    GRefPtr<GByteArray> privateKey;
    if (privateKeyData->size()) {
        privateKey = adoptGRef(g_byte_array_sized_new(privateKeyData->size()));
        g_byte_array_append(privateKey.get(), privateKeyData->data(), privateKeyData->size());
    }

    std::optional<CString> privateKeyPKCS11Uri;
    decoder >> privateKeyPKCS11Uri;
    if (!privateKeyPKCS11Uri)
        return std::nullopt;
#endif

    GType certificateType = g_tls_backend_get_certificate_type(g_tls_backend_get_default());
    GRefPtr<GTlsCertificate> certificate;
    GTlsCertificate* issuer = nullptr;
    for (uint32_t i = 0; i < *chainLength; i++) {
        std::optional<IPC::DataReference> certificateDataReference;
        decoder >> certificateDataReference;
        if (!certificateDataReference)
            return std::nullopt;

        GRefPtr<GByteArray> certificateData = adoptGRef(g_byte_array_sized_new(certificateDataReference->size()));
        g_byte_array_append(certificateData.get(), certificateDataReference->data(), certificateDataReference->size());

        certificate = adoptGRef(G_TLS_CERTIFICATE(g_initable_new(
            certificateType, nullptr, nullptr,
            "certificate", certificateData.get(),
            "issuer", issuer,
#if GLIB_CHECK_VERSION(2, 69, 0)
            "private-key", i == *chainLength - 1 ? privateKey.get() : nullptr,
            "private-key-pkcs11-uri", i == *chainLength - 1 ? privateKeyPKCS11Uri->data() : nullptr,
#endif
            nullptr)));
        issuer = certificate.get();
    }

    return certificate;
}
template std::optional<GRefPtr<GTlsCertificate>> ArgumentCoder<GRefPtr<GTlsCertificate>>::decode<Decoder>(Decoder&);

} // namespace IPC
