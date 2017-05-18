/*
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_RTC)
#include "MediaEndpointOwr.h"

#include "MediaEndpointSessionConfiguration.h"
#include "MediaPayload.h"
#include "NotImplemented.h"
#include "OpenWebRTCUtilities.h"
#include "PeerConnectionStates.h"
#include "RTCDataChannelHandler.h"
#include "RealtimeAudioSourceOwr.h"
#include "RealtimeVideoSourceOwr.h"
#include <owr/owr.h>
#include <owr/owr_audio_payload.h>
#include <owr/owr_crypto_utils.h>
#include <owr/owr_media_session.h>
#include <owr/owr_transport_agent.h>
#include <owr/owr_video_payload.h>
#include <wtf/text/CString.h>

namespace WebCore {

static void gotCandidate(OwrSession*, OwrCandidate*, MediaEndpointOwr*);
static void candidateGatheringDone(OwrSession*, MediaEndpointOwr*);
static void iceConnectionStateChange(OwrSession*, GParamSpec*, MediaEndpointOwr*);
static void gotIncomingSource(OwrMediaSession*, OwrMediaSource*, MediaEndpointOwr*);

static const Vector<String> candidateTypes = { "host", "srflx", "prflx", "relay" };
static const Vector<String> candidateTcpTypes = { "", "active", "passive", "so" };
static const Vector<String> codecTypes = { "NONE", "PCMU", "PCMA", "OPUS", "H264", "VP8" };

static const char* helperServerRegEx = "(turns|turn|stun):([\\w\\.\\-]+|\\[[\\w\\:]+\\])(:\\d+)?(\\?.+)?";

static const unsigned short helperServerDefaultPort = 3478;
static const unsigned short candidateDefaultPort = 9;

static std::unique_ptr<MediaEndpoint> createMediaEndpointOwr(MediaEndpointClient& client)
{
    return std::unique_ptr<MediaEndpoint>(new MediaEndpointOwr(client));
}

CreateMediaEndpoint MediaEndpoint::create = createMediaEndpointOwr;

MediaEndpointOwr::MediaEndpointOwr(MediaEndpointClient& client)
    : m_transportAgent(nullptr)
    , m_client(client)
    , m_numberOfReceivePreparedSessions(0)
    , m_numberOfSendPreparedSessions(0)
{
    initializeOpenWebRTC();

    GRegexCompileFlags compileFlags = G_REGEX_JAVASCRIPT_COMPAT;
    GRegexMatchFlags matchFlags = static_cast<GRegexMatchFlags>(0);
    m_helperServerRegEx = g_regex_new(helperServerRegEx, compileFlags, matchFlags, nullptr);
}

MediaEndpointOwr::~MediaEndpointOwr()
{
    stop();

    g_regex_unref(m_helperServerRegEx);
}

void MediaEndpointOwr::setConfiguration(MediaEndpointConfiguration&& configuration)
{
    m_configuration = WTFMove(configuration);
}

std::unique_ptr<RTCDataChannelHandler> MediaEndpointOwr::createDataChannelHandler(const String&, const RTCDataChannelInit&)
{
    // FIXME: Implement data channel.
    ASSERT_NOT_REACHED();
    return nullptr;
}

static void cryptoDataCallback(gchar* privateKey, gchar* certificate, gchar* fingerprint, gchar* fingerprintFunction, gpointer data)
{
    MediaEndpointOwr* mediaEndpoint = (MediaEndpointOwr*) data;
    mediaEndpoint->dispatchDtlsFingerprint(g_strdup(privateKey), g_strdup(certificate), String(fingerprint), String(fingerprintFunction));
}

void MediaEndpointOwr::generateDtlsInfo()
{
    owr_crypto_create_crypto_data(cryptoDataCallback, this);
}

MediaPayloadVector MediaEndpointOwr::getDefaultAudioPayloads()
{
    MediaPayloadVector payloads;

    // FIXME: This list should be based on what is available in the platform (bug: http://webkit.org/b/163723)
    MediaPayload payload1;
    payload1.type = 111;
    payload1.encodingName = "OPUS";
    payload1.clockRate = 48000;
    payload1.channels = 2;
    payloads.append(WTFMove(payload1));

    MediaPayload payload2;
    payload2.type = 8;
    payload2.encodingName = "PCMA";
    payload2.clockRate = 8000;
    payload2.channels = 1;
    payloads.append(WTFMove(payload2));

    MediaPayload payload3;
    payload3.type = 0;
    payload3.encodingName = "PCMU";
    payload3.clockRate = 8000;
    payload3.channels = 1;
    payloads.append(WTFMove(payload3));

    return payloads;
}

MediaPayloadVector MediaEndpointOwr::getDefaultVideoPayloads()
{
    MediaPayloadVector payloads;

    // FIXME: This list should be based on what is available in the platform (bug: http://webkit.org/b/163723)
    MediaPayload payload1;
    payload1.type = 103;
    payload1.encodingName = "H264";
    payload1.clockRate = 90000;
    payload1.ccmfir = true;
    payload1.nackpli = true;
    payload1.addParameter("packetizationMode", 1);
    payloads.append(WTFMove(payload1));

    MediaPayload payload2;
    payload2.type = 100;
    payload2.encodingName = "VP8";
    payload2.clockRate = 90000;
    payload2.ccmfir = true;
    payload2.nackpli = true;
    payload2.nack = true;
    payloads.append(WTFMove(payload2));

    MediaPayload payload3;
    payload3.type = 120;
    payload3.encodingName = "RTX";
    payload3.clockRate = 90000;
    payload3.addParameter("apt", 100);
    payload3.addParameter("rtxTime", 200);
    payloads.append(WTFMove(payload3));

    return payloads;
}

static bool payloadsContainType(const Vector<const MediaPayload*>& payloads, unsigned payloadType)
{
    for (auto payload : payloads) {
        ASSERT(payload);
        if (payload->type == payloadType)
            return true;
    }
    return false;
}

MediaPayloadVector MediaEndpointOwr::filterPayloads(const MediaPayloadVector& remotePayloads, const MediaPayloadVector& defaultPayloads)
{
    Vector<const MediaPayload*> filteredPayloads;

    for (auto& remotePayload : remotePayloads) {
        const MediaPayload* defaultPayload = nullptr;
        for (auto& p : defaultPayloads) {
            if (p.encodingName == remotePayload.encodingName.convertToASCIIUppercase()) {
                defaultPayload = &p;
                break;
            }
        }
        if (!defaultPayload)
            continue;

        if (defaultPayload->parameters.contains("packetizationMode") && remotePayload.parameters.contains("packetizationMode")
            && (defaultPayload->parameters.get("packetizationMode") != defaultPayload->parameters.get("packetizationMode")))
            continue;

        filteredPayloads.append(&remotePayload);
    }

    MediaPayloadVector filteredAptPayloads;

    for (auto filteredPayload : filteredPayloads) {
        if (filteredPayload->parameters.contains("apt") && (!payloadsContainType(filteredPayloads, filteredPayload->parameters.get("apt"))))
            continue;
        filteredAptPayloads.append(*filteredPayload);
    }

    return filteredAptPayloads;
}

MediaEndpoint::UpdateResult MediaEndpointOwr::updateReceiveConfiguration(MediaEndpointSessionConfiguration* configuration, bool isInitiator)
{
    Vector<TransceiverConfig> transceiverConfigs;
    for (unsigned i = m_transceivers.size(); i < configuration->mediaDescriptions().size(); ++i) {
        TransceiverConfig config;
        config.type = SessionTypeMedia;
        config.isDtlsClient = configuration->mediaDescriptions()[i].dtlsSetup == "active";
        config.mid = configuration->mediaDescriptions()[i].mid;
        transceiverConfigs.append(WTFMove(config));
    }

    ensureTransportAgentAndTransceivers(isInitiator, transceiverConfigs);

    // Prepare the new sessions.
    for (unsigned i = m_numberOfReceivePreparedSessions; i < m_transceivers.size(); ++i) {
        OwrSession* session = m_transceivers[i]->session();
        prepareMediaSession(OWR_MEDIA_SESSION(session), &configuration->mediaDescriptions()[i], isInitiator);
        owr_transport_agent_add_session(m_transportAgent, session);
    }

    owr_transport_agent_start(m_transportAgent);
    m_numberOfReceivePreparedSessions = m_transceivers.size();

    return UpdateResult::Success;
}

static const MediaPayload* findRtxPayload(const MediaPayloadVector& payloads, unsigned apt)
{
    for (auto& payload : payloads) {
        if (payload.encodingName.convertToASCIIUppercase() == "RTX" && payload.parameters.contains("apt")
            && (payload.parameters.get("apt") == apt))
            return &payload;
    }
    return nullptr;
}

MediaEndpoint::UpdateResult MediaEndpointOwr::updateSendConfiguration(MediaEndpointSessionConfiguration* configuration, const RealtimeMediaSourceMap& sendSourceMap, bool isInitiator)
{
    Vector<TransceiverConfig> transceiverConfigs;
    for (unsigned i = m_transceivers.size(); i < configuration->mediaDescriptions().size(); ++i) {
        TransceiverConfig config;
        config.type = SessionTypeMedia;
        config.isDtlsClient = configuration->mediaDescriptions()[i].dtlsSetup != "active";
        config.mid = configuration->mediaDescriptions()[i].mid;
        transceiverConfigs.append(WTFMove(config));
    }

    ensureTransportAgentAndTransceivers(isInitiator, transceiverConfigs);

    for (unsigned i = 0; i < m_transceivers.size(); ++i) {
        auto* session = m_transceivers[i]->session();
        auto& mdesc = configuration->mediaDescriptions()[i];

        if (mdesc.type == "audio" || mdesc.type == "video")
            g_object_set(session, "rtcp-mux", mdesc.rtcpMux, nullptr);

        if (mdesc.iceCandidates.size()) {
            for (auto& candidate : mdesc.iceCandidates)
                internalAddRemoteCandidate(session, candidate, mdesc.iceUfrag, mdesc.icePassword);
        }

        if (i < m_numberOfSendPreparedSessions)
            continue;

        if (!sendSourceMap.contains(mdesc.mid))
            continue;

        const MediaPayload* payload = nullptr;
        for (auto& p : mdesc.payloads) {
            if (p.encodingName.convertToASCIIUppercase() != "RTX") {
                payload = &p;
                break;
            }
        }

        if (!payload)
            return UpdateResult::Failed;

        auto* rtxPayload = findRtxPayload(mdesc.payloads, payload->type);
        auto* source = static_cast<RealtimeMediaSourceOwr*>(sendSourceMap.get(mdesc.mid));

        ASSERT(codecTypes.find(payload->encodingName.convertToASCIIUppercase()) != notFound);
        auto codecType = static_cast<OwrCodecType>(codecTypes.find(payload->encodingName.convertToASCIIUppercase()));

        OwrPayload* sendPayload;
        if (mdesc.type == "audio")
            sendPayload = owr_audio_payload_new(codecType, payload->type, payload->clockRate, payload->channels);
        else {
            sendPayload = owr_video_payload_new(codecType, payload->type, payload->clockRate, payload->ccmfir, payload->nackpli);
            g_object_set(sendPayload, "rtx-payload-type", rtxPayload ? rtxPayload->type : -1,
                "rtx-time", rtxPayload && rtxPayload->parameters.contains("rtxTime") ? rtxPayload->parameters.get("rtxTime") : 0, nullptr);
        }

        owr_media_session_set_send_payload(OWR_MEDIA_SESSION(session), sendPayload);
        owr_media_session_set_send_source(OWR_MEDIA_SESSION(session), source->mediaSource());

        // FIXME: Support for group-ssrc SDP line is missing.
        const Vector<unsigned> receiveSsrcs = mdesc.ssrcs;
        if (receiveSsrcs.size()) {
            g_object_set(session, "receive-ssrc", receiveSsrcs[0], nullptr);
            if (receiveSsrcs.size() == 2)
                g_object_set(session, "receive-rtx-ssrc", receiveSsrcs[1], nullptr);
        }

        m_numberOfSendPreparedSessions = i + 1;
    }

    return UpdateResult::Success;
}

void MediaEndpointOwr::addRemoteCandidate(const IceCandidate& candidate, const String& mid, const String& ufrag, const String& password)
{
    for (auto& transceiver : m_transceivers) {
        if (transceiver->mid() == mid) {
            internalAddRemoteCandidate(transceiver->session(), candidate, ufrag, password);
            break;
        }
    }
}

void MediaEndpointOwr::replaceMutedRemoteSourceMid(const String& oldMid, const String& newMid)
{
    RefPtr<RealtimeMediaSourceOwr> remoteSource = m_mutedRemoteSources.take(oldMid);
    m_mutedRemoteSources.set(newMid, remoteSource);
}

Ref<RealtimeMediaSource> MediaEndpointOwr::createMutedRemoteSource(const String& mid, RealtimeMediaSource::Type type)
{
    String name;
    String id("not used");
    RefPtr<RealtimeMediaSourceOwr> source;

    switch (type) {
    case RealtimeMediaSource::Type::Audio:
        name = "remote audio";
        source = adoptRef(new RealtimeAudioSourceOwr(nullptr, id, type, name));
        break;
    case RealtimeMediaSource::Type::Video:
        name = "remote video";
        source = adoptRef(new RealtimeVideoSourceOwr(nullptr, id, type, name));
        break;
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }

    m_mutedRemoteSources.set(mid, source);

    return *source;
}

void MediaEndpointOwr::replaceSendSource(RealtimeMediaSource& newSource, const String& mid)
{
    UNUSED_PARAM(newSource);
    UNUSED_PARAM(mid);

    // FIXME: We want to use owr_media_session_set_send_source here, but it doesn't work as intended.
    // Issue tracked by OpenWebRTC bug: https://github.com/EricssonResearch/openwebrtc/issues/533

    notImplemented();
}

void MediaEndpointOwr::stop()
{
    if (!m_transportAgent)
        return;

    for (auto& transceiver : m_transceivers)
        owr_media_session_set_send_source(OWR_MEDIA_SESSION(transceiver->session()), nullptr);

    g_object_unref(m_transportAgent);
    m_transportAgent = nullptr;
}

size_t MediaEndpointOwr::transceiverIndexForSession(OwrSession* session) const
{
    for (unsigned i = 0; i < m_transceivers.size(); ++i) {
        if (m_transceivers[i]->session() == session)
            return i;
    }

    ASSERT_NOT_REACHED();
    return notFound;
}

const String& MediaEndpointOwr::sessionMid(OwrSession* session) const
{
    size_t index = transceiverIndexForSession(session);
    return m_transceivers[index]->mid();
}

OwrTransceiver* MediaEndpointOwr::matchTransceiverByMid(const String& mid) const
{
    for (auto& transceiver : m_transceivers) {
        if (transceiver->mid() == mid)
            return transceiver.get();
    }
    return nullptr;
}

void MediaEndpointOwr::dispatchNewIceCandidate(const String& mid, IceCandidate&& iceCandidate)
{
    m_client.gotIceCandidate(mid, WTFMove(iceCandidate));
}

void MediaEndpointOwr::dispatchGatheringDone(const String& mid)
{
    m_client.doneGatheringCandidates(mid);
}

void MediaEndpointOwr::processIceTransportStateChange(OwrSession* session)
{
    OwrIceState owrIceState;
    g_object_get(session, "ice-connection-state", &owrIceState, nullptr);

    OwrTransceiver& transceiver = *m_transceivers[transceiverIndexForSession(session)];
    if (owrIceState < transceiver.owrIceState())
        return;

    transceiver.setOwrIceState(owrIceState);

    // We cannot go to Completed if there may be more remote candidates.
    if (owrIceState == OWR_ICE_STATE_READY && !transceiver.gotEndOfRemoteCandidates())
        return;

    RTCIceTransportState transportState;
    switch (owrIceState) {
    case OWR_ICE_STATE_CONNECTING:
        transportState = RTCIceTransportState::Checking;
        break;
    case OWR_ICE_STATE_CONNECTED:
        transportState = RTCIceTransportState::Connected;
        break;
    case OWR_ICE_STATE_READY:
        transportState = RTCIceTransportState::Completed;
        break;
    case OWR_ICE_STATE_FAILED:
        transportState = RTCIceTransportState::Failed;
        break;
    default:
        return;
    }

    m_client.iceTransportStateChanged(transceiver.mid(), transportState);
}

void MediaEndpointOwr::dispatchDtlsFingerprint(gchar* privateKey, gchar* certificate, const String& fingerprint, const String& fingerprintFunction)
{
    m_dtlsPrivateKey = String(privateKey);
    m_dtlsCertificate = String(certificate);

    g_free(privateKey);
    g_free(certificate);

    m_client.gotDtlsFingerprint(fingerprint, fingerprintFunction);
}

void MediaEndpointOwr::unmuteRemoteSource(const String& mid, OwrMediaSource* realSource)
{
    RefPtr<RealtimeMediaSourceOwr> remoteSource = m_mutedRemoteSources.take(mid);
    if (!remoteSource) {
        LOG_ERROR("Unable to find muted remote source.");
        return;
    }

    if (remoteSource->isProducingData())
        remoteSource->swapOutShallowSource(*realSource);
}

void MediaEndpointOwr::prepareSession(OwrSession* session, PeerMediaDescription* mediaDescription)
{
    g_object_set_data_full(G_OBJECT(session), "ice-ufrag", g_strdup(mediaDescription->iceUfrag.ascii().data()), g_free);
    g_object_set_data_full(G_OBJECT(session), "ice-password", g_strdup(mediaDescription->icePassword.ascii().data()), g_free);

    g_signal_connect(session, "on-new-candidate", G_CALLBACK(gotCandidate), this);
    g_signal_connect(session, "on-candidate-gathering-done", G_CALLBACK(candidateGatheringDone), this);
    g_signal_connect(session, "notify::ice-connection-state", G_CALLBACK(iceConnectionStateChange), this);
}

void MediaEndpointOwr::prepareMediaSession(OwrMediaSession* mediaSession, PeerMediaDescription* mediaDescription, bool isInitiator)
{
    prepareSession(OWR_SESSION(mediaSession), mediaDescription);

    bool useRtcpMux = !isInitiator && mediaDescription->rtcpMux;
    g_object_set(mediaSession, "rtcp-mux", useRtcpMux, nullptr);

    if (!mediaDescription->cname.isEmpty() && mediaDescription->ssrcs.size()) {
        g_object_set(mediaSession, "cname", mediaDescription->cname.ascii().data(),
            "send-ssrc", mediaDescription->ssrcs[0],
            nullptr);
    }

    g_signal_connect(mediaSession, "on-incoming-source", G_CALLBACK(gotIncomingSource), this);

    for (auto& payload : mediaDescription->payloads) {
        if (payload.encodingName.convertToASCIIUppercase() == "RTX")
            continue;

        auto* rtxPayload = findRtxPayload(mediaDescription->payloads, payload.type);

        ASSERT_WITH_MESSAGE(codecTypes.find(payload.encodingName.convertToASCIIUppercase()) != notFound, "Unable to find codec: %s", payload.encodingName.utf8().data());
        OwrCodecType codecType = static_cast<OwrCodecType>(codecTypes.find(payload.encodingName.convertToASCIIUppercase()));

        OwrPayload* receivePayload;
        if (mediaDescription->type == "audio")
            receivePayload = owr_audio_payload_new(codecType, payload.type, payload.clockRate, payload.channels);
        else {
            receivePayload = owr_video_payload_new(codecType, payload.type, payload.clockRate, payload.ccmfir, payload.nackpli);
            g_object_set(receivePayload, "rtx-payload-type", rtxPayload ? rtxPayload->type : -1,
                "rtx-time", rtxPayload && rtxPayload->parameters.contains("rtxTime") ? rtxPayload->parameters.get("rtxTime") : 0, nullptr);
        }

        owr_media_session_add_receive_payload(mediaSession, receivePayload);
    }
}

struct HelperServerUrl {
    String protocol;
    String host;
    unsigned short port;
    String query;
};

static void parseHelperServerUrl(GRegex& regex, const URL& url, HelperServerUrl& outUrl)
{
    GMatchInfo* matchInfo;

    if (g_regex_match(&regex, url.string().ascii().data(), static_cast<GRegexMatchFlags>(0), &matchInfo)) {
        gchar** matches =  g_match_info_fetch_all(matchInfo);
        gint matchCount = g_strv_length(matches);

        outUrl.protocol = matches[1];
        outUrl.host = matches[2][0] == '[' ? String(matches[2] + 1, strlen(matches[2]) - 2) // IPv6
            : matches[2];

        outUrl.port = 0;
        if (matchCount >= 4) {
            String portString = String(matches[3] + 1); // Skip port colon
            outUrl.port = portString.toUIntStrict();
        }

        if (matchCount == 5)
            outUrl.query = String(matches[4] + 1); // Skip question mark

        g_strfreev(matches);
    }

    g_match_info_free(matchInfo);
}

void MediaEndpointOwr::ensureTransportAgentAndTransceivers(bool isInitiator, const Vector<TransceiverConfig>& transceiverConfigs)
{
    ASSERT(m_dtlsPrivateKey);
    ASSERT(m_dtlsCertificate);

    if (!m_transportAgent) {
        // FIXME: Handle SDP BUNDLE line from the remote source instead of falling back to balanced.
        OwrBundlePolicyType bundlePolicy = OWR_BUNDLE_POLICY_TYPE_BALANCED;

        switch (m_configuration->bundlePolicy) {
        case PeerConnectionStates::BundlePolicy::Balanced:
            bundlePolicy = OWR_BUNDLE_POLICY_TYPE_BALANCED;
            break;
        case PeerConnectionStates::BundlePolicy::MaxCompat:
            bundlePolicy = OWR_BUNDLE_POLICY_TYPE_MAX_COMPAT;
            break;
        case PeerConnectionStates::BundlePolicy::MaxBundle:
            bundlePolicy = OWR_BUNDLE_POLICY_TYPE_MAX_BUNDLE;
            break;
        default:
            ASSERT_NOT_REACHED();
        };

        m_transportAgent = owr_transport_agent_new(false, bundlePolicy);

        ASSERT(m_configuration);
        for (auto& server : m_configuration->iceServers) {
            for (auto& webkitUrl : server.urls) {
                HelperServerUrl url;
                // WebKit's URL class can't handle ICE helper server urls properly
                parseHelperServerUrl(*m_helperServerRegEx, webkitUrl, url);

                unsigned short port = url.port ? url.port : helperServerDefaultPort;

                if (url.protocol == "stun") {
                    owr_transport_agent_add_helper_server(m_transportAgent, OWR_HELPER_SERVER_TYPE_STUN,
                        url.host.ascii().data(), port, nullptr, nullptr);

                } else if (url.protocol == "turn") {
                    OwrHelperServerType serverType = url.query == "transport=tcp" ? OWR_HELPER_SERVER_TYPE_TURN_TCP
                        : OWR_HELPER_SERVER_TYPE_TURN_UDP;

                    owr_transport_agent_add_helper_server(m_transportAgent, serverType,
                        url.host.ascii().data(), port,
                        server.username.ascii().data(), server.credential.ascii().data());
                } else if (url.protocol == "turns") {
                    owr_transport_agent_add_helper_server(m_transportAgent, OWR_HELPER_SERVER_TYPE_TURN_TLS,
                        url.host.ascii().data(), port,
                        server.username.ascii().data(), server.credential.ascii().data());
                } else
                    ASSERT_NOT_REACHED();
            }
        }
    }

    g_object_set(m_transportAgent, "ice-controlling-mode", isInitiator, nullptr);

    for (auto& config : transceiverConfigs) {
        OwrSession* session = OWR_SESSION(owr_media_session_new(config.isDtlsClient));
        g_object_set(session, "dtls-certificate", m_dtlsCertificate.utf8().data(),
            "dtls-key", m_dtlsPrivateKey.utf8().data(),
            nullptr);

        m_transceivers.append(OwrTransceiver::create(config.mid, session));
    }
}

void MediaEndpointOwr::internalAddRemoteCandidate(OwrSession* session, const IceCandidate& candidate, const String& ufrag, const String& password)
{
    gboolean rtcpMux;
    g_object_get(session, "rtcp-mux", &rtcpMux, nullptr);

    if (rtcpMux && candidate.componentId == OWR_COMPONENT_TYPE_RTCP)
        return;

    ASSERT(candidateTypes.find(candidate.type) != notFound);

    OwrCandidateType candidateType = static_cast<OwrCandidateType>(candidateTypes.find(candidate.type));
    OwrComponentType componentId = static_cast<OwrComponentType>(candidate.componentId);
    OwrTransportType transportType;

    if (candidate.transport.convertToASCIIUppercase() == "UDP")
        transportType = OWR_TRANSPORT_TYPE_UDP;
    else {
        ASSERT(candidateTcpTypes.find(candidate.tcpType) != notFound);
        transportType = static_cast<OwrTransportType>(candidateTcpTypes.find(candidate.tcpType));
    }

    OwrCandidate* owrCandidate = owr_candidate_new(candidateType, componentId);
    g_object_set(owrCandidate, "transport-type", transportType,
        "address", candidate.address.ascii().data(),
        "port", candidate.port,
        "base-address", candidate.relatedAddress.ascii().data(),
        "base-port", candidate.relatedPort,
        "priority", candidate.priority,
        "foundation", candidate.foundation.ascii().data(),
        "ufrag", ufrag.ascii().data(),
        "password", password.ascii().data(),
        nullptr);

    owr_session_add_remote_candidate(session, owrCandidate);
}

static void gotCandidate(OwrSession* session, OwrCandidate* candidate, MediaEndpointOwr* mediaEndpoint)
{
    OwrCandidateType candidateType;
    gchar* foundation;
    OwrComponentType componentId;
    OwrTransportType transportType;
    gint priority;
    gchar* address;
    guint port;
    gchar* relatedAddress;
    guint relatedPort;

    g_object_get(candidate, "type", &candidateType,
        "foundation", &foundation,
        "component-type", &componentId,
        "transport-type", &transportType,
        "priority", &priority,
        "address", &address,
        "port", &port,
        "base-address", &relatedAddress,
        "base-port", &relatedPort,
        nullptr);

    ASSERT(candidateType >= 0 && candidateType < candidateTypes.size());
    ASSERT(transportType >= 0 && transportType < candidateTcpTypes.size());

    IceCandidate iceCandidate;
    iceCandidate.type = candidateTypes[candidateType];
    iceCandidate.foundation = foundation;
    iceCandidate.componentId = componentId;
    iceCandidate.priority = priority;
    iceCandidate.address = address;
    iceCandidate.port = port ? port : candidateDefaultPort;

    if (transportType == OWR_TRANSPORT_TYPE_UDP)
        iceCandidate.transport = "UDP";
    else {
        iceCandidate.transport = "TCP";
        iceCandidate.tcpType = candidateTcpTypes[transportType];
    }

    if (candidateType != OWR_CANDIDATE_TYPE_HOST) {
        iceCandidate.relatedAddress = relatedAddress;
        iceCandidate.relatedPort = relatedPort ? relatedPort : candidateDefaultPort;
    }

    g_object_set(G_OBJECT(candidate), "ufrag", g_object_get_data(G_OBJECT(session), "ice-ufrag"),
        "password", g_object_get_data(G_OBJECT(session), "ice-password"),
        nullptr);

    mediaEndpoint->dispatchNewIceCandidate(mediaEndpoint->sessionMid(session), WTFMove(iceCandidate));

    g_free(foundation);
    g_free(address);
    g_free(relatedAddress);
}

static void candidateGatheringDone(OwrSession* session, MediaEndpointOwr* mediaEndpoint)
{
    mediaEndpoint->dispatchGatheringDone(mediaEndpoint->sessionMid(session));
}

static void iceConnectionStateChange(OwrSession* session, GParamSpec*, MediaEndpointOwr* mediaEndpoint)
{
    mediaEndpoint->processIceTransportStateChange(session);
}

static void gotIncomingSource(OwrMediaSession* mediaSession, OwrMediaSource* source, MediaEndpointOwr* mediaEndpoint)
{
    mediaEndpoint->unmuteRemoteSource(mediaEndpoint->sessionMid(OWR_SESSION(mediaSession)), source);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
