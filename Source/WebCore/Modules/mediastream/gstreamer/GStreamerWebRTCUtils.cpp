/*
 *  Copyright (C) 2019-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
#include "GStreamerWebRTCUtils.h"

#include "OpenSSLCryptoUniquePtr.h"
#include "RTCIceCandidate.h"
#include "RTCIceProtocol.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/WallTime.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY_STATIC(webkit_webrtc_utils_debug);
#define GST_CAT_DEFAULT webkit_webrtc_utils_debug

namespace WebCore {

static inline RTCIceComponent toRTCIceComponent(int component)
{
    return component == 1 ? RTCIceComponent::Rtp : RTCIceComponent::Rtcp;
}

static inline std::optional<RTCIceProtocol> toRTCIceProtocol(const String& protocol)
{
    if (protocol.isEmpty())
        return { };
    if (protocol == "udp"_s)
        return RTCIceProtocol::Udp;
    ASSERT(protocol == "tcp"_s);
    return RTCIceProtocol::Tcp;
}

static inline std::optional<RTCIceTcpCandidateType> toRTCIceTcpCandidateType(const String& type)
{
    if (type.isEmpty())
        return { };
    if (type == "active"_s)
        return RTCIceTcpCandidateType::Active;
    if (type == "passive"_s)
        return RTCIceTcpCandidateType::Passive;
    ASSERT(type == "so"_s);
    return RTCIceTcpCandidateType::So;
}

static inline std::optional<RTCIceCandidateType> toRTCIceCandidateType(const String& type)
{
    if (type.isEmpty())
        return { };
    if (type == "host"_s)
        return RTCIceCandidateType::Host;
    if (type == "srflx"_s)
        return RTCIceCandidateType::Srflx;
    if (type == "prflx"_s)
        return RTCIceCandidateType::Prflx;
    ASSERT(type == "relay"_s);
    return RTCIceCandidateType::Relay;
}

RefPtr<RTCError> toRTCError(GError* rtcError)
{
    auto detail = toRTCErrorDetailType(static_cast<GstWebRTCError>(rtcError->code));
    if (!detail)
        return nullptr;
    return RTCError::create(*detail, String::fromUTF8(rtcError->message));
}

static inline double toWebRTCBitRatePriority(RTCPriorityType priority)
{
    switch (priority) {
    case RTCPriorityType::VeryLow:
        return 0.5;
    case RTCPriorityType::Low:
        return 1;
    case RTCPriorityType::Medium:
        return 2;
    case RTCPriorityType::High:
        return 4;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

GUniquePtr<GstStructure> fromRTCEncodingParameters(const RTCRtpEncodingParameters& parameters)
{
    GUniquePtr<GstStructure> rtcParameters(gst_structure_new("encoding-parameters", "active", G_TYPE_BOOLEAN, parameters.active,
        "rid", G_TYPE_STRING, parameters.rid.utf8().data(), "bitrate-priority", G_TYPE_DOUBLE, toWebRTCBitRatePriority(parameters.priority), nullptr));

    if (parameters.ssrc)
        gst_structure_set(rtcParameters.get(), "ssrc", G_TYPE_ULONG, parameters.ssrc, nullptr);

    if (parameters.maxBitrate)
        gst_structure_set(rtcParameters.get(), "max-bitrate", G_TYPE_ULONG, parameters.maxBitrate, nullptr);

    if (parameters.maxFramerate)
        gst_structure_set(rtcParameters.get(), "max-framerate", G_TYPE_ULONG, parameters.maxFramerate, nullptr);

    if (parameters.scaleResolutionDownBy)
        gst_structure_set(rtcParameters.get(), "scale-resolution-down-by", G_TYPE_DOUBLE, parameters.scaleResolutionDownBy, nullptr);

    if (parameters.networkPriority)
        gst_structure_set(rtcParameters.get(), "network-priority", G_TYPE_INT, *parameters.networkPriority, nullptr);

    return rtcParameters;
}

static inline RTCPriorityType fromWebRTCBitRatePriority(double priority)
{
    if (priority < 0.7)
        return RTCPriorityType::VeryLow;
    if (priority < 1.5)
        return RTCPriorityType::Low;
    if (priority < 2.5)
        return RTCPriorityType::Medium;
    return RTCPriorityType::High;
}

static inline RTCRtpEncodingParameters toRTCEncodingParameters(const GstStructure* rtcParameters)
{
    RTCRtpEncodingParameters parameters;

    unsigned ssrc;
    if (gst_structure_get_uint(rtcParameters, "ssrc", &ssrc))
        parameters.ssrc = ssrc;

    gst_structure_get(rtcParameters, "active", G_TYPE_BOOLEAN, &(parameters.active), nullptr);

    uint64_t maxBitrate;
    if (gst_structure_get_uint64(rtcParameters, "max-bitrate", &maxBitrate))
        parameters.maxBitrate = maxBitrate;

    uint64_t maxFramerate;
    if (gst_structure_get_uint64(rtcParameters, "max-framerate", &maxFramerate))
        parameters.maxFramerate = maxFramerate;

    parameters.rid = String::fromLatin1(gst_structure_get_string(rtcParameters, "rid"));

    double scaleResolutionDownBy;
    if (gst_structure_get_double(rtcParameters, "scale-resolution-down-by", &scaleResolutionDownBy))
        parameters.scaleResolutionDownBy = scaleResolutionDownBy;

    double bitratePriority;
    if (gst_structure_get_double(rtcParameters, "bitrate-priority", &bitratePriority))
        parameters.priority = fromWebRTCBitRatePriority(bitratePriority);

    int networkPriority;
    if (gst_structure_get_int(rtcParameters, "network-priority", &networkPriority))
        parameters.networkPriority = static_cast<RTCPriorityType>(networkPriority);

    return parameters;
}

RTCRtpSendParameters toRTCRtpSendParameters(const GstStructure* rtcParameters)
{
    if (!rtcParameters)
        return { };

    RTCRtpSendParameters parameters;
    auto* encodings = gst_structure_get_value(rtcParameters, "encodings");
    unsigned size = gst_value_list_get_size(encodings);
    for (unsigned i = 0; i < size; i++) {
        const auto* value = gst_value_list_get_value(encodings, i);
        parameters.encodings.append(toRTCEncodingParameters(GST_STRUCTURE_CAST(value)));
    }

    // FIXME: Handle rtcParameters.degradation_preference.
    return parameters;
}


static void ensureDebugCategoryInitialized()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_utils_debug, "webkitwebrtcutils", 0, "WebKit WebRTC utilities");
    });
}

std::optional<RTCIceCandidate::Fields> parseIceCandidateSDP(const String& sdp)
{
    ensureDebugCategoryInitialized();
    GST_DEBUG("Parsing ICE Candidate: %s", sdp.utf8().data());
    if (!sdp.startsWith("candidate:"_s))
        return { };

    String foundation;
    unsigned componentId = 0;
    String transport;
    unsigned priority = 0;
    String address;
    uint32_t port = 0;
    String type;
    String tcptype;
    String relatedAddress;
    guint16 relatedPort = 0;
    String usernameFragment;
    auto tokens = sdp.convertToASCIILowercase().substring(10).split(' ');

    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        auto i = std::distance(tokens.begin(), it);
        auto token = *it;
        switch (i) {
        case 0:
            foundation = token;
            break;
        case 1:
            if (auto value = parseInteger<unsigned>(token))
                componentId = *value;
            else
                return { };
            break;
        case 2:
            transport = token;
            break;
        case 3:
            if (auto value = parseInteger<unsigned>(token))
                priority = *value;
            else
                return { };
            break;
        case 4:
            address = token;
            break;
        case 5:
            if (auto value = parseInteger<unsigned>(token))
                port = *value;
            else
                return { };
            break;
        default:
            if (it + 1 == tokens.end())
                return { };

            it++;
            if (token == "typ"_s)
                type = *it;
            else if (token == "raddr"_s)
                relatedAddress = *it;
            else if (token == "rport"_s)
                relatedPort = parseInteger<unsigned>(*it).value_or(0);
            else if (token == "tcptype"_s)
                tcptype = *it;
            else if (token == "ufrag"_s)
                usernameFragment = *it;
            break;
        }
    }

    if (type.isEmpty())
        return { };

    RTCIceCandidate::Fields fields;
    fields.foundation = foundation;
    fields.component = toRTCIceComponent(componentId);
    fields.priority = priority;
    fields.protocol = toRTCIceProtocol(transport);
    if (!address.isEmpty()) {
        fields.address = address;
        fields.port = port;
    }
    fields.type = toRTCIceCandidateType(type);
    fields.tcpType = toRTCIceTcpCandidateType(tcptype);
    if (!relatedAddress.isEmpty()) {
        fields.relatedAddress = relatedAddress;
        fields.relatedPort = relatedPort;
    }
    fields.usernameFragment = usernameFragment;
    return fields;
}

static String x509Serialize(X509* x509)
{
    auto bio = BIOPtr(BIO_new(BIO_s_mem()));
    if (!bio)
        return { };

    if (!PEM_write_bio_X509(bio.get(), x509))
        return { };

    Vector<char> buffer;
    buffer.reserveCapacity(4096);
    int length = BIO_read(bio.get(), buffer.data(), 4096);
    if (!length)
        return { };

    return String(buffer.data(), length);
}

static String privateKeySerialize(EVP_PKEY* privateKey)
{
    auto bio = BIOPtr(BIO_new(BIO_s_mem()));
    if (!bio)
        return { };

    if (!PEM_write_bio_PrivateKey(bio.get(), privateKey, nullptr, nullptr, 0, nullptr, nullptr))
        return { };

    Vector<char> buffer;
    buffer.reserveCapacity(4096);
    int length = BIO_read(bio.get(), buffer.data(), 4096);
    if (!length)
        return { };

    return String(buffer.data(), length);
}

std::optional<Ref<RTCCertificate>> generateCertificate(Ref<SecurityOrigin>&& origin, const PeerConnectionBackend::CertificateInformation& info)
{
    ensureDebugCategoryInitialized();
    EvpPKeyPtr privateKey;

    switch (info.type) {
    case PeerConnectionBackend::CertificateInformation::Type::ECDSAP256: {
        privateKey.reset(EVP_EC_gen("prime256v1"));
        if (!privateKey)
            return { };
        break;
    }
    case PeerConnectionBackend::CertificateInformation::Type::RSASSAPKCS1v15: {
        int publicExponent = info.rsaParameters ? info.rsaParameters->publicExponent : 65537;
        auto modulusLength = info.rsaParameters ? info.rsaParameters->modulusLength : 2048;

        auto ctx = EvpPKeyCtxPtr(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr));
        if (!ctx)
            return { };

        EVP_PKEY_keygen_init(ctx.get());

        auto paramsBuilder = OsslParamBldPtr(OSSL_PARAM_BLD_new());
        if (!paramsBuilder)
            return { };

        auto exponent = BIGNUMPtr(BN_new());
        if (!BN_set_word(exponent.get(), publicExponent))
            return { };

        auto modulus = BIGNUMPtr(BN_new());
        if (!BN_set_word(modulus.get(), modulusLength))
            return { };

        if (!OSSL_PARAM_BLD_push_BN(paramsBuilder.get(), "n", modulus.get()))
            return { };

        if (!OSSL_PARAM_BLD_push_BN(paramsBuilder.get(), "e", exponent.get()))
            return { };

        if (!OSSL_PARAM_BLD_push_BN(paramsBuilder.get(), "d", nullptr))
            return { };

        auto params = OsslParamPtr(OSSL_PARAM_BLD_to_param(paramsBuilder.get()));
        if (!params)
            return { };

        EVP_PKEY_CTX_set_params(ctx.get(), params.get());

        EVP_PKEY* pkey = nullptr;
        EVP_PKEY_generate(ctx.get(), &pkey);
        if (!pkey)
            return { };
        privateKey.reset(pkey);
        break;
    }
    }

    auto x509 = X509Ptr(X509_new());
    if (!x509) {
        GST_WARNING("Failed to create certificate");
        return { };
    }

    X509_set_version(x509.get(), 2);

    // Set a random 64 bit integer as serial number.
    auto serialNumber = BIGNUMPtr(BN_new());
    BN_rand(serialNumber.get(), 64, 0, 0);
    ASN1_INTEGER* asn1SerialNumber = X509_get_serialNumber(x509.get());
    BN_to_ASN1_INTEGER(serialNumber.get(), asn1SerialNumber);

    // Set a random 8 byte base64 string as issuer/subject.
    X509_NAME* name = X509_NAME_new();
    Vector<uint8_t> buffer(8);
    WTF::cryptographicallyRandomValues(buffer.data(), buffer.size());
    auto commonName = base64EncodeToString(buffer);
    X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_ASC, (const guchar*)commonName.ascii().data(), -1, -1, 0);
    X509_set_subject_name(x509.get(), name);
    X509_set_issuer_name(x509.get(), name);
    X509_NAME_free(name);

    // Fallback to 30 days, max out at one year.
    uint64_t expires = info.expires.value_or(2592000);
    expires = std::min<uint64_t>(expires, 31536000000);
    X509_gmtime_adj(X509_getm_notBefore(x509.get()), 0);
    X509_gmtime_adj(X509_getm_notAfter(x509.get()), expires);
    X509_set_pubkey(x509.get(), privateKey.get());

    if (!X509_sign(x509.get(), privateKey.get(), EVP_sha256())) {
        GST_WARNING("Failed to sign certificate");
        return { };
    }

    auto pem = x509Serialize(x509.get());
    GST_DEBUG("Generated certificate PEM: %s", pem.ascii().data());
    auto serializedPrivateKey = privateKeySerialize(privateKey.get());
    Vector<RTCCertificate::DtlsFingerprint> fingerprints;
    // FIXME: Fill fingerprints.
    auto expirationTime = WTF::WallTime::now().secondsSinceEpoch() + WTF::Seconds(expires);
    return RTCCertificate::create(WTFMove(origin), expirationTime.milliseconds(), WTFMove(fingerprints), WTFMove(pem), WTFMove(serializedPrivateKey));
}

bool sdpMediaHasAttributeKey(const GstSDPMedia* media, const char* key)
{
    unsigned len = gst_sdp_media_attributes_len(media);
    for (unsigned i = 0; i < len; i++) {
        const auto* attribute = gst_sdp_media_get_attribute(media, i);
        if (!g_strcmp0(attribute->key, key))
            return true;
    }

    return false;
}

GRefPtr<GstCaps> capsFromRtpCapabilities(const RTCRtpCapabilities& capabilities, Function<void(GstStructure*)> supplementCapsCallback)
{
    auto caps = adoptGRef(gst_caps_new_empty());
    for (unsigned index = 0; auto& codec : capabilities.codecs) {
        auto components = codec.mimeType.split('/');
        auto* codecStructure = gst_structure_new("application/x-rtp", "media", G_TYPE_STRING, components[0].ascii().data(),
            "encoding-name", G_TYPE_STRING, components[1].ascii().data(), "clock-rate", G_TYPE_INT, codec.clockRate, nullptr);

        if (!codec.sdpFmtpLine.isEmpty()) {
            for (auto& fmtp : codec.sdpFmtpLine.split(';')) {
                auto fieldAndValue = fmtp.split('=');
                gst_structure_set(codecStructure, fieldAndValue[0].ascii().data(), G_TYPE_STRING, fieldAndValue[1].ascii().data(), nullptr);
            }
        }

        if (codec.channels && *codec.channels > 1)
            gst_structure_set(codecStructure, "encoding-params", G_TYPE_STRING, makeString(*codec.channels).ascii().data(), nullptr);

        supplementCapsCallback(codecStructure);

        if (!index) {
            for (unsigned i = 0; auto& extension : capabilities.headerExtensions)
                gst_structure_set(codecStructure, makeString("extmap-", i++).ascii().data(), G_TYPE_STRING, extension.uri.ascii().data(), nullptr);
        }
        gst_caps_append_structure(caps.get(), codecStructure);
        index++;
    }

    return caps;
}

GstWebRTCRTPTransceiverDirection getDirectionFromSDPMedia(const GstSDPMedia* media)
{
    for (unsigned i = 0; i < gst_sdp_media_attributes_len(media); i++) {
        const auto* attribute = gst_sdp_media_get_attribute(media, i);

        if (!g_strcmp0(attribute->key, "sendonly"))
            return GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
        if (!g_strcmp0(attribute->key, "sendrecv"))
            return GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
        if (!g_strcmp0(attribute->key, "recvonly"))
            return GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
        if (!g_strcmp0(attribute->key, "inactive"))
            return GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE;
    }

    return GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE;
}

GRefPtr<GstCaps> capsFromSDPMedia(const GstSDPMedia* media)
{
    ensureDebugCategoryInitialized();
    unsigned numberOfFormats = gst_sdp_media_formats_len(media);
    auto caps = adoptGRef(gst_caps_new_empty());
    for (unsigned i = 0; i < numberOfFormats; i++) {
        const char* rtpMapValue = gst_sdp_media_get_attribute_val_n(media, "rtpmap", i);
        if (!rtpMapValue) {
            GST_DEBUG("Skipping media format without rtpmap");
            continue;
        }
        auto rtpMap = StringView::fromLatin1(rtpMapValue);
        auto components = rtpMap.split(' ');
        auto payloadType = parseInteger<int>(*components.begin());
        if (!payloadType || !*payloadType) {
            GST_WARNING("Invalid payload type in rtpmap %s", rtpMap.utf8().data());
            continue;
        }

        auto* formatCaps = gst_sdp_media_get_caps_from_media(media, *payloadType);
        if (!formatCaps) {
            GST_WARNING("No caps found for payload type %d", *payloadType);
            continue;
        }

        // Relay SDP attributes to the caps, this is specially useful so that elements in
        // webrtcbin will be able to enable RTP header extensions.
        gst_sdp_media_attributes_to_caps(media, formatCaps);
        for (unsigned j = 0; j < gst_caps_get_size(formatCaps); j++) {
            auto* structure = gst_caps_get_structure(formatCaps, j);
            gst_structure_set_name(structure, "application/x-rtp");
        }

        gst_caps_append(caps.get(), formatCaps);
    }
    return caps;
}

} // namespace WebCore

#endif
