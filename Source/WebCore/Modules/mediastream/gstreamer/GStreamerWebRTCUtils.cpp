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
#include <cstdint>
#include <limits>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WallTime.h>
#include <wtf/WeakRandomNumber.h>
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

ExceptionOr<GUniquePtr<GstStructure>> fromRTCEncodingParameters(const RTCRtpEncodingParameters& parameters, const String& kind)
{
    if (kind == "video"_s && parameters.scaleResolutionDownBy && *parameters.scaleResolutionDownBy < 1)
        return Exception { ExceptionCode::RangeError, "scaleResolutionDownBy should be >= 1"_s };

    if (parameters.rid.length() > 255)
        return Exception { ExceptionCode::TypeError, "rid is too long"_s };
    if (!parameters.rid.containsOnlyASCII() || !parameters.rid.containsOnly<isASCIIAlphanumeric>())
        return Exception { ExceptionCode::TypeError, "rid contains invalid characters"_s };

    GUniquePtr<GstStructure> rtcParameters(gst_structure_new("encoding-parameters", "active", G_TYPE_BOOLEAN, parameters.active,
        "rid", G_TYPE_STRING, parameters.rid.utf8().data(), "bitrate-priority", G_TYPE_DOUBLE, toWebRTCBitRatePriority(parameters.priority), nullptr));

    if (parameters.ssrc)
        gst_structure_set(rtcParameters.get(), "ssrc", G_TYPE_UINT, parameters.ssrc, nullptr);

    if (parameters.maxBitrate)
        gst_structure_set(rtcParameters.get(), "max-bitrate", G_TYPE_UINT, *parameters.maxBitrate, nullptr);

    if (parameters.maxFramerate)
        gst_structure_set(rtcParameters.get(), "max-framerate", G_TYPE_UINT, *parameters.maxFramerate, nullptr);

    if (parameters.scaleResolutionDownBy && kind == "video"_s)
        gst_structure_set(rtcParameters.get(), "scale-resolution-down-by", G_TYPE_DOUBLE, *parameters.scaleResolutionDownBy, nullptr);

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

    if (auto ssrc = gstStructureGet<unsigned>(rtcParameters, "ssrc"_s))
        parameters.ssrc = *ssrc;

    gst_structure_get(rtcParameters, "active", G_TYPE_BOOLEAN, &(parameters.active), nullptr);

    if (auto maxBitrate = gstStructureGet<unsigned>(rtcParameters, "max-bitrate"_s))
        parameters.maxBitrate = *maxBitrate;

    if (auto maxFramerate = gstStructureGet<unsigned>(rtcParameters, "max-framerate"_s))
        parameters.maxFramerate = *maxFramerate;

    if (auto rid = gstStructureGetString(rtcParameters, "rid"_s))
        parameters.rid = makeString(rid);

    if (auto scaleResolutionDownBy = gstStructureGet<double>(rtcParameters, "scale-resolution-down-by"_s))
        parameters.scaleResolutionDownBy = *scaleResolutionDownBy;

    if (auto bitratePriority = gstStructureGet<double>(rtcParameters, "bitrate-priority"_s))
        parameters.priority = fromWebRTCBitRatePriority(*bitratePriority);

    if (auto networkPriority = gstStructureGet<int>(rtcParameters, "network-priority"_s))
        parameters.networkPriority = static_cast<RTCPriorityType>(*networkPriority);

    return parameters;
}

static inline RTCRtpCodecParameters toRTCCodecParameters(const GstStructure* rtcParameters)
{
    RTCRtpCodecParameters parameters;

    if (auto pt = gstStructureGet<unsigned>(rtcParameters, "pt"_s))
        parameters.payloadType = *pt;

    if (auto mimeType = gstStructureGetString(rtcParameters, "mime-type"_s))
        parameters.mimeType = mimeType.toString();

    if (auto clockRate = gstStructureGet<unsigned>(rtcParameters, "clock-rate"_s))
        parameters.clockRate = *clockRate;

    if (auto channels = gstStructureGet<unsigned>(rtcParameters, "channels"_s))
        parameters.channels = *channels;

    if (auto fmtpLine = gstStructureGetString(rtcParameters, "fmtp-line"_s))
        parameters.sdpFmtpLine = fmtpLine.toString();

    return parameters;
}

RTCRtpSendParameters toRTCRtpSendParameters(const GstStructure* rtcParameters)
{
    if (!rtcParameters)
        return { };

    RTCRtpSendParameters parameters;
    if (auto transactionId = gstStructureGetString(rtcParameters, "transaction-id"_s))
        parameters.transactionId = makeString(transactionId);

    if (auto encodings = gst_structure_get_value(rtcParameters, "encodings")) {
        unsigned size = gst_value_list_get_size(encodings);
        parameters.encodings.reserveInitialCapacity(size);
        for (unsigned i = 0; i < size; i++) {
            const auto value = gst_value_list_get_value(encodings, i);
            RELEASE_ASSERT(GST_VALUE_HOLDS_STRUCTURE(value));
            parameters.encodings.append(toRTCEncodingParameters(gst_value_get_structure(value)));
        }
    }

    if (auto codecs = gst_structure_get_value(rtcParameters, "codecs")) {
        unsigned size = gst_value_list_get_size(codecs);
        parameters.codecs.reserveInitialCapacity(size);
        for (unsigned i = 0; i < size; i++) {
            const auto value = gst_value_list_get_value(codecs, i);
            RELEASE_ASSERT(GST_VALUE_HOLDS_STRUCTURE(value));
            parameters.codecs.append(toRTCCodecParameters(gst_value_get_structure(value)));
        }
    }

    // FIXME: The rtcp parameters should not be hardcoded.
    parameters.rtcp.cname = "unused"_s;
    parameters.rtcp.reducedSize = false;

    // FIXME: Handle rtcParameters.degradationPreference, headerExtensions.
    return parameters;
}

GUniquePtr<GstStructure> fromRTCCodecParameters(const RTCRtpCodecParameters& parameters)
{
    GUniquePtr<GstStructure> rtcParameters(gst_structure_new("codec-parameters", "pt", G_TYPE_UINT, parameters.payloadType,
        "mime-type", G_TYPE_STRING, parameters.mimeType.utf8().data(), "clock-rate", G_TYPE_UINT, parameters.clockRate,
        "channels", G_TYPE_UINT, parameters.channels, "fmtp-line", G_TYPE_STRING, parameters.sdpFmtpLine.utf8().data(), nullptr));
    return rtcParameters;
}

ExceptionOr<GUniquePtr<GstStructure>> fromRTCSendParameters(const RTCRtpSendParameters& parameters, const String& kind)
{
    GUniquePtr<GstStructure> gstParameters(gst_structure_new("send-parameters", "transaction-id", G_TYPE_STRING, parameters.transactionId.ascii().data(), nullptr));
    GValue encodingsValue = G_VALUE_INIT;
    g_value_init(&encodingsValue, GST_TYPE_LIST);
    for (auto& encoding : parameters.encodings) {
        auto encodingData = fromRTCEncodingParameters(encoding, kind);
        if (encodingData.hasException())
            return encodingData.releaseException();
        GValue value = G_VALUE_INIT;
        g_value_init(&value, GST_TYPE_STRUCTURE);
        gst_value_set_structure(&value, encodingData.returnValue().get());
        gst_value_list_append_value(&encodingsValue, &value);
        g_value_unset(&value);
    }
    gst_structure_take_value(gstParameters.get(), "encodings", &encodingsValue);

    GValue codecsValue = G_VALUE_INIT;
    g_value_init(&codecsValue, GST_TYPE_LIST);
    for (auto& codec : parameters.codecs) {
        auto codecData = fromRTCCodecParameters(codec);
        GValue value = G_VALUE_INIT;
        g_value_init(&value, GST_TYPE_STRUCTURE);
        gst_value_set_structure(&value, codecData.get());
        gst_value_list_append_value(&codecsValue, &value);
        g_value_unset(&value);
    }
    gst_structure_take_value(gstParameters.get(), "codecs", &codecsValue);

    // FIXME: Missing serialization for degradationPreference, headerExtensions, rtcp.

    return gstParameters;
}

static void ensureDebugCategoryInitialized()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_utils_debug, "webkitwebrtcutils", 0, "WebKit WebRTC utilities");
    });
}

enum class NextSDPField {
    None,
    Typ,
    Raddr,
    Rport,
    TcpType,
    Ufrag
};

std::optional<RTCIceCandidate::Fields> parseIceCandidateSDP(const String& sdp)
{
    ensureDebugCategoryInitialized();
    GST_TRACE("Parsing ICE Candidate: %s", sdp.utf8().data());
    if (!sdp.startsWith("candidate:"_s)) {
        GST_WARNING("Invalid SDP ICE candidate format, must start with candidate: prefix");
        return { };
    }

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
    String lowercasedSDP = sdp.convertToASCIILowercase();
    StringView view = StringView(lowercasedSDP).substring(10);
    unsigned i = 0;
    NextSDPField nextSdpField { NextSDPField::None };

    for (auto token : view.split(' ')) {
        auto tokenString = token.toStringWithoutCopying();
        switch (i) {
        case 0:
            foundation = tokenString;
            break;
        case 1:
            if (auto value = parseInteger<unsigned>(token))
                componentId = *value;
            else {
                GST_WARNING("Invalid SDP candidate component ID: %s", tokenString.utf8().data());
                return { };
            }
            break;
        case 2:
            transport = tokenString;
            break;
        case 3:
            if (auto value = parseInteger<unsigned>(token))
                priority = *value;
            else {
                GST_WARNING("Invalid SDP candidate priority: %s", tokenString.utf8().data());
                return { };
            }
            break;
        case 4:
            address = tokenString;
            break;
        case 5:
            if (auto value = parseInteger<unsigned>(token))
                port = *value;
            else {
                GST_WARNING("Invalid SDP candidate port: %s", tokenString.utf8().data());
                return { };
            }
            break;
        default:
            if (token == "typ"_s)
                nextSdpField = NextSDPField::Typ;
            else if (token == "raddr"_s)
                nextSdpField = NextSDPField::Raddr;
            else if (token == "rport"_s)
                nextSdpField = NextSDPField::Rport;
            else if (token == "tcptype"_s)
                nextSdpField = NextSDPField::TcpType;
            else if (token == "ufrag"_s)
                nextSdpField = NextSDPField::Ufrag;
            else {
                switch (nextSdpField) {
                case NextSDPField::None:
                    break;
                case NextSDPField::Typ:
                    type = tokenString;
                    break;
                case NextSDPField::Raddr:
                    relatedAddress = tokenString;
                    break;
                case NextSDPField::Rport:
                    relatedPort = parseInteger<unsigned>(token).value_or(0);
                    break;
                case NextSDPField::TcpType:
                    tcptype = tokenString;
                    break;
                case NextSDPField::Ufrag:
                    usernameFragment = tokenString;
                    break;
                }
            }
            break;
        }
        i++;
    }

    if (type.isEmpty()) {
        GST_WARNING("Unable to parse candidate type");
        return { };
    }

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

    return buffer.subspan(0, length);
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

    return buffer.subspan(0, length);
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
    WTF::cryptographicallyRandomValues(buffer.mutableSpan());
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(UniqueSSRCGenerator);

uint32_t UniqueSSRCGenerator::generateSSRC()
{
    Locker locker { m_lock };
    unsigned remainingAttempts = 255;
    while (remainingAttempts) {
        auto candidate = weakRandomNumber<uint32_t>();
        if (!m_knownIds.contains(candidate)) {
            m_knownIds.append(candidate);
            return candidate;
        }
        remainingAttempts--;
    }
    return std::numeric_limits<uint32_t>::max();
}

std::optional<int> payloadTypeForEncodingName(StringView encodingName)
{
    static HashMap<String, int> staticPayloadTypes = {
        { "PCMU"_s, 0 },
        { "PCMA"_s, 8 },
        { "G722"_s, 9 },
    };

    const auto key = encodingName.toStringWithoutCopying();
    if (staticPayloadTypes.contains(key))
        return staticPayloadTypes.get(key);
    return { };
}

GRefPtr<GstCaps> capsFromRtpCapabilities(const RTCRtpCapabilities& capabilities, Function<void(GstStructure*)> supplementCapsCallback)
{
    auto caps = adoptGRef(gst_caps_new_empty());
    for (unsigned index = 0; auto& codec : capabilities.codecs) {
        auto components = codec.mimeType.split('/');
        auto* codecStructure = gst_structure_new("application/x-rtp", "media", G_TYPE_STRING, components[0].ascii().data(),
            "encoding-name", G_TYPE_STRING, components[1].convertToASCIIUppercase().ascii().data() , "clock-rate", G_TYPE_INT, codec.clockRate, nullptr);

        if (!codec.sdpFmtpLine.isEmpty()) {
            for (auto& fmtp : codec.sdpFmtpLine.split(';')) {
                auto fieldAndValue = fmtp.split('=');
                gst_structure_set(codecStructure, fieldAndValue[0].ascii().data(), G_TYPE_STRING, fieldAndValue[1].ascii().data(), nullptr);
            }
        }

        if (codec.channels && *codec.channels > 1)
            gst_structure_set(codecStructure, "encoding-params", G_TYPE_STRING, makeString(*codec.channels).ascii().data(), nullptr);

        if (auto encodingName = gstStructureGetString(codecStructure, "encoding-name"_s)) {
            if (auto payloadType = payloadTypeForEncodingName(encodingName))
                gst_structure_set(codecStructure, "payload", G_TYPE_INT, *payloadType, nullptr);
        }

        supplementCapsCallback(codecStructure);

        if (!index) {
            for (unsigned i = 1; auto& extension : capabilities.headerExtensions)
                gst_structure_set(codecStructure, makeString("extmap-"_s, i++).ascii().data(), G_TYPE_STRING, extension.uri.ascii().data(), nullptr);
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
        if (!payloadType) {
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

            // Remove attributes unrelated with codec preferences, potentially leading to internal
            // webrtcbin confusions such as duplicated RTP direction attributes for instance.
            gst_structure_remove_fields(structure, "a-setup", "a-ice-ufrag", "a-ice-pwd", "a-sendrecv", "a-inactive",
                "a-sendonly", "a-recvonly", "a-end-of-candidates", nullptr);

            if (auto name = gstStructureGetString(structure, "encoding-name"_s)) {
                auto encodingName = name.convertToASCIIUppercase();
                gst_structure_set(structure, "encoding-name", G_TYPE_STRING, encodingName.ascii().data(), nullptr);
            }

            // Remove ssrc- attributes that end up being accumulated in fmtp SDP media parameters.
            gstStructureFilterAndMapInPlace(structure, [&](auto id, auto) -> bool {
                auto fieldId = gstIdToString(id);
                return !fieldId.startsWith("ssrc-"_s);
            });
            // Align with caps from RealtimeOutgoingAudioSourceGStreamer
            setSsrcAudioLevelVadOn(structure);
        }

        gst_caps_append(caps.get(), formatCaps);
    }
    return caps;
}

void setSsrcAudioLevelVadOn(GstStructure* structure)
{
    unsigned totalFields = gst_structure_n_fields(structure);
    for (unsigned i = 0; i < totalFields; i++) {
        String fieldName = WTF::span(gst_structure_nth_field_name(structure, i));
        if (!fieldName.startsWith("extmap-"_s))
            continue;

        const auto value = gst_structure_get_value(structure, fieldName.ascii().data());
        if (!G_VALUE_HOLDS_STRING(value))
            continue;

        const char* uri = g_value_get_string(value);
        if (!g_str_equal(uri, GST_RTP_HDREXT_BASE "ssrc-audio-level"))
            continue;

        GValue arrayValue G_VALUE_INIT;
        gst_value_array_init(&arrayValue, 3);

        GValue stringValue G_VALUE_INIT;
        g_value_init(&stringValue, G_TYPE_STRING);

        g_value_set_static_string(&stringValue, "");
        gst_value_array_append_value(&arrayValue, &stringValue);

        g_value_set_string(&stringValue, uri);
        gst_value_array_append_value(&arrayValue, &stringValue);

        g_value_set_static_string(&stringValue, "vad=on");
        gst_value_array_append_and_take_value(&arrayValue, &stringValue);

        gst_structure_remove_field(structure, fieldName.ascii().data());
        gst_structure_take_value(structure, fieldName.ascii().data(), &arrayValue);
    }
}

StatsTimestampConverter& StatsTimestampConverter::singleton()
{
    static NeverDestroyed<StatsTimestampConverter> sharedInstance;
    return sharedInstance;
}

Seconds StatsTimestampConverter::convertFromMonotonicTime(Seconds value) const
{
    auto monotonicOffset = value - m_initialMonotonicTime;
    auto newTimestamp = m_epoch.secondsSinceEpoch() + monotonicOffset;
    return Performance::reduceTimeResolution(newTimestamp.secondsSinceEpoch());
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif
