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

#include "WebCoreArgumentCoders.h"
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <wtf/Vector.h>
#include <wtf/glib/GSpanExtras.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace IPC {

void ArgumentCoder<GRefPtr<GByteArray>>::encode(Encoder& encoder, const GRefPtr<GByteArray>& array)
{
    if (!array) {
        encoder << false;
        return;
    }

    encoder << true;
    encoder << span(array);
}

std::optional<GRefPtr<GByteArray>> ArgumentCoder<GRefPtr<GByteArray>>::decode(Decoder& decoder)
{
    auto isEngaged = decoder.decode<bool>();
    if (!UNLIKELY(isEngaged))
        return std::nullopt;

    if (!(*isEngaged))
        return GRefPtr<GByteArray>();

    auto data = decoder.decode<std::span<const uint8_t>>();
    if (UNLIKELY(!data))
        return std::nullopt;

    GRefPtr<GByteArray> array = adoptGRef(g_byte_array_sized_new(data->size()));
    g_byte_array_append(array.get(), data->data(), data->size());

    return array;
}

void ArgumentCoder<GRefPtr<GVariant>>::encode(Encoder& encoder, const GRefPtr<GVariant>& variant)
{
    if (!variant) {
        encoder << CString();
        return;
    }

    encoder << CString(g_variant_get_type_string(variant.get()));
    encoder << span(variant);
}

std::optional<GRefPtr<GVariant>> ArgumentCoder<GRefPtr<GVariant>>::decode(Decoder& decoder)
{
    auto variantTypeString = decoder.decode<CString>();
    if (UNLIKELY(!variantTypeString))
        return std::nullopt;

    if (variantTypeString->isNull())
        return GRefPtr<GVariant>();

    if (!g_variant_type_string_is_valid(variantTypeString->data()))
        return std::nullopt;

    auto data = decoder.decode<std::span<const uint8_t>>();
    if (UNLIKELY(!data))
        return std::nullopt;

    GUniquePtr<GVariantType> variantType(g_variant_type_new(variantTypeString->data()));
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(data->data(), data->size()));
    return g_variant_new_from_bytes(variantType.get(), bytes.get(), FALSE);
}

void ArgumentCoder<GRefPtr<GTlsCertificate>>::encode(Encoder& encoder, const GRefPtr<GTlsCertificate>& certificate)
{
    if (!certificate) {
        encoder << Vector<std::span<const uint8_t>> { };
        return;
    }

    Vector<GRefPtr<GByteArray>> certificatesData;
    for (auto* nextCertificate = certificate.get(); nextCertificate; nextCertificate = g_tls_certificate_get_issuer(nextCertificate)) {
        GRefPtr<GByteArray> certificateData;
        g_object_get(nextCertificate, "certificate", &certificateData.outPtr(), nullptr);

        if (!certificateData) {
            certificatesData.clear();
            break;
        }
        certificatesData.insert(0, WTFMove(certificateData));
    }

    if (certificatesData.isEmpty())
        return;

    encoder << certificatesData;

#if GLIB_CHECK_VERSION(2, 69, 0)
    GRefPtr<GByteArray> privateKey;
    GUniqueOutPtr<char> privateKeyPKCS11Uri;
    g_object_get(certificate.get(), "private-key", &privateKey.outPtr(), "private-key-pkcs11-uri", &privateKeyPKCS11Uri.outPtr(), nullptr);
    encoder << privateKey;
    encoder << CString(privateKeyPKCS11Uri.get());
#endif
}

std::optional<GRefPtr<GTlsCertificate>> ArgumentCoder<GRefPtr<GTlsCertificate>>::decode(Decoder& decoder)
{
    auto certificatesData = decoder.decode<WTF::Vector<GRefPtr<GByteArray>>>();

    if (UNLIKELY(!certificatesData))
        return std::nullopt;

    if (!certificatesData->size())
        return GRefPtr<GTlsCertificate>();

#if GLIB_CHECK_VERSION(2, 69, 0)
    std::optional<GRefPtr<GByteArray>> privateKey;
    decoder >> privateKey;
    if (UNLIKELY(!privateKey))
        return std::nullopt;

    std::optional<CString> privateKeyPKCS11Uri;
    decoder >> privateKeyPKCS11Uri;
    if (UNLIKELY(!privateKeyPKCS11Uri))
        return std::nullopt;
#endif

    GType certificateType = g_tls_backend_get_certificate_type(g_tls_backend_get_default());
    GRefPtr<GTlsCertificate> certificate;
    GTlsCertificate* issuer = nullptr;
    for (uint32_t i = 0; auto& certificateData : *certificatesData) {
        certificate = adoptGRef(G_TLS_CERTIFICATE(g_initable_new(
            certificateType, nullptr, nullptr,
            "certificate", certificateData.get(),
            "issuer", issuer,
#if GLIB_CHECK_VERSION(2, 69, 0)
            "private-key", i == certificatesData->size() - 1 ? privateKey->get() : nullptr,
            "private-key-pkcs11-uri", i == certificatesData->size() - 1 ? privateKeyPKCS11Uri->data() : nullptr,
#endif
            nullptr)));
        issuer = certificate.get();
    }

    return certificate;
}

void ArgumentCoder<GTlsCertificateFlags>::encode(Encoder& encoder, GTlsCertificateFlags flags)
{
    encoder << static_cast<uint32_t>(flags);
}

std::optional<GTlsCertificateFlags> ArgumentCoder<GTlsCertificateFlags>::decode(Decoder& decoder)
{
    auto flags = decoder.decode<uint32_t>();
    if (UNLIKELY(!flags))
        return std::nullopt;
    return static_cast<GTlsCertificateFlags>(*flags);
}

void ArgumentCoder<GRefPtr<GUnixFDList>>::encode(Encoder& encoder, const GRefPtr<GUnixFDList>& fdList)
{
    if (!fdList) {
        encoder << false;
        return;
    }

    Vector<UnixFileDescriptor> attachments;
    unsigned length = std::max(0, g_unix_fd_list_get_length(fdList.get()));
    if (length) {
        attachments = Vector<UnixFileDescriptor>(length, [&](size_t i) {
            return UnixFileDescriptor { g_unix_fd_list_get(fdList.get(), i, nullptr), UnixFileDescriptor::Adopt };
        });
    }
    encoder << true << WTFMove(attachments);
}

std::optional<GRefPtr<GUnixFDList>> ArgumentCoder<GRefPtr<GUnixFDList>>::decode(Decoder& decoder)
{
    auto hasObject = decoder.decode<bool>();
    if (UNLIKELY(!hasObject))
        return std::nullopt;
    if (!*hasObject)
        return GRefPtr<GUnixFDList> { };

    auto attachments = decoder.decode<Vector<UnixFileDescriptor>>();
    if (UNLIKELY(!attachments))
        return std::nullopt;

    GRefPtr<GUnixFDList> fdList = adoptGRef(g_unix_fd_list_new());
    for (auto& attachment : *attachments) {
        int ret = g_unix_fd_list_append(fdList.get(), attachment.value(), nullptr);
        if (UNLIKELY(ret == -1))
            return std::nullopt;
    }
    return fdList;
}

} // namespace IPC
