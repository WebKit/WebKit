/*
 *  Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
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
#include "GStreamerMediaEndpoint.h"

#if USE(GSTREAMER_WEBRTC)

#include "Document.h"
#include "GStreamerCommon.h"
#include "GStreamerDataChannelHandler.h"
#include "GStreamerIncomingTrackProcessor.h"
#include "GStreamerRegistryScanner.h"
#include "GStreamerRtpReceiverBackend.h"
#include "GStreamerRtpTransceiverBackend.h"
#include "GStreamerSctpTransportBackend.h"
#include "GStreamerWebRTCUtils.h"
#include "JSDOMPromiseDeferred.h"
#include "JSRTCStatsReport.h"
#include "Logging.h"
#include "MediaEndpointConfiguration.h"
#include "NotImplemented.h"
#include "RTCDataChannel.h"
#include "RTCDataChannelEvent.h"
#include "RTCIceCandidate.h"
#include "RTCOfferOptions.h"
#include "RTCPeerConnection.h"
#include "RTCRtpSender.h"
#include "RTCSctpTransportBackend.h"
#include "RTCSessionDescription.h"
#include "RTCStatsReport.h"
#include "RTCTrackEvent.h"
#include "RealtimeIncomingAudioSourceGStreamer.h"
#include "RealtimeIncomingVideoSourceGStreamer.h"
#include "RealtimeOutgoingAudioSourceGStreamer.h"
#include "RealtimeOutgoingVideoSourceGStreamer.h"

#include <gst/sdp/sdp.h>
#include <wtf/MainThread.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>
#include <wtf/UniqueRef.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY(webkit_webrtc_endpoint_debug);
#define GST_CAT_DEFAULT webkit_webrtc_endpoint_debug

namespace WebCore {

GStreamerMediaEndpoint::GStreamerMediaEndpoint(GStreamerPeerConnectionBackend& peerConnection)
    : m_peerConnectionBackend(peerConnection)
    , m_statsCollector(GStreamerStatsCollector::create())
#if !RELEASE_LOG_DISABLED
    , m_statsLogTimer(*this, &GStreamerMediaEndpoint::gatherStatsForLogging)
    , m_logger(peerConnection.logger())
    , m_logIdentifier(peerConnection.logIdentifier())
#endif
    , m_ssrcGenerator(UniqueSSRCGenerator::create())
{
    ensureGStreamerInitialized();
    registerWebKitGStreamerElements();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_endpoint_debug, "webkitwebrtcendpoint", 0, "WebKit WebRTC end-point");
    });

    initializePipeline();
}

GStreamerMediaEndpoint::~GStreamerMediaEndpoint()
{
    if (!m_pipeline)
        return;
    teardownPipeline();
}

bool GStreamerMediaEndpoint::initializePipeline()
{
    static uint32_t nPipeline = 0;
    auto pipelineName = makeString("webkit-webrtc-pipeline-"_s, nPipeline);
    m_pipeline = gst_pipeline_new(pipelineName.ascii().data());
    registerActivePipeline(m_pipeline);

    auto clock = adoptGRef(gst_system_clock_obtain());
    gst_pipeline_use_clock(GST_PIPELINE(m_pipeline.get()), clock.get());
    gst_element_set_base_time(m_pipeline.get(), 0);
    gst_element_set_start_time(m_pipeline.get(), GST_CLOCK_TIME_NONE);

    connectSimpleBusMessageCallback(m_pipeline.get(), [this](GstMessage* message) {
        handleMessage(message);
    });

    auto binName = makeString("webkit-webrtcbin-"_s, nPipeline++);
    m_webrtcBin = makeGStreamerElement("webrtcbin", binName.ascii().data());
    if (!m_webrtcBin)
        return false;

    // Lower default latency from 200ms to 40ms.
    g_object_set(m_webrtcBin.get(), "latency", 40, nullptr);

    auto rtpBin = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_webrtcBin.get()), "rtpbin"));
    if (!rtpBin) {
        GST_ERROR_OBJECT(m_webrtcBin.get(), "rtpbin not found. Please check that your GStreamer installation has the rtp and rtpmanager plugins.");
        return false;
    }

    if (gstObjectHasProperty(rtpBin.get(), "add-reference-timestamp-meta"))
        g_object_set(rtpBin.get(), "add-reference-timestamp-meta", TRUE, nullptr);

    g_signal_connect(rtpBin.get(), "new-jitterbuffer", G_CALLBACK(+[](GstElement*, GstElement* element, unsigned, unsigned ssrc, GStreamerMediaEndpoint* endPoint) {

        // Workaround for https://gitlab.freedesktop.org/gstreamer/gst-plugins-good/-/issues/914
        g_object_set(element, "rtx-next-seqnum", FALSE, nullptr);

        GST_DEBUG_OBJECT(endPoint->pipeline(), "Creating incoming track processor for SSRC %u", ssrc);
        endPoint->m_trackProcessors.ensure(ssrc, [] {
            return GStreamerIncomingTrackProcessor::create();
        });
    }), this);

    m_statsCollector->setElement(m_webrtcBin.get());
    g_signal_connect_swapped(m_webrtcBin.get(), "notify::ice-connection-state", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint) {
        endPoint->onIceConnectionChange();
    }), this);
    g_signal_connect_swapped(m_webrtcBin.get(), "notify::ice-gathering-state", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint) {
        endPoint->onIceGatheringChange();
    }), this);
    g_signal_connect_swapped(m_webrtcBin.get(), "on-negotiation-needed", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint) {
        endPoint->onNegotiationNeeded();
    }), this);
    g_signal_connect_swapped(m_webrtcBin.get(), "on-ice-candidate", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint, guint sdpMLineIndex, gchararray candidate) {
        endPoint->onIceCandidate(sdpMLineIndex, candidate);
    }), this);
    g_signal_connect_swapped(m_webrtcBin.get(), "pad-added", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint, GstPad* pad) {
        // Ignore outgoing pad notifications.
        if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
            return;

        if (endPoint->isStopped())
            return;

        endPoint->connectPad(pad);
    }), this);
    g_signal_connect_swapped(m_webrtcBin.get(), "pad-removed", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint, GstPad* pad) {
        // Ignore outgoing pad notifications.
        if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
            return;

        callOnMainThreadAndWait([protectedThis = Ref(*endPoint), pad] {
            if (protectedThis->isStopped())
                return;

            protectedThis->removeRemoteStream(pad);
        });
    }), this);

    if (webkitGstCheckVersion(1, 22, 0)) {
        g_signal_connect_swapped(m_webrtcBin.get(), "prepare-data-channel", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint, GstWebRTCDataChannel* channel, gboolean isLocal) {
            endPoint->prepareDataChannel(channel, isLocal);
        }), this);

        g_signal_connect_swapped(m_webrtcBin.get(), "request-aux-sender", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint, GstWebRTCDTLSTransport*) -> GstElement* {
            // `sender` ownership is transferred to the signal caller.
            if (auto sender = endPoint->requestAuxiliarySender())
                return sender;
            return nullptr;
        }), this);
    }

    g_signal_connect_swapped(m_webrtcBin.get(), "on-data-channel", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint, GstWebRTCDataChannel* channel) {
        endPoint->onDataChannel(channel);
    }), this);

#ifndef GST_DISABLE_GST_DEBUG
    g_signal_connect_swapped(m_webrtcBin.get(), "on-new-transceiver", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint, GstWebRTCRTPTransceiver* transceiver) {
        GST_DEBUG_OBJECT(endPoint->m_webrtcBin.get(), "New transceiver: %" GST_PTR_FORMAT, transceiver);
    }), this);

    g_signal_connect(m_webrtcBin.get(), "notify::connection-state", G_CALLBACK(+[](GstElement* webrtcBin, GParamSpec*, GStreamerMediaEndpoint* endPoint) {
        GstWebRTCPeerConnectionState state;
        g_object_get(webrtcBin, "connection-state", &state, nullptr);
        GUniquePtr<char> desc(g_enum_to_string(GST_TYPE_WEBRTC_PEER_CONNECTION_STATE, state));
        auto dotFilename = makeString(span(GST_ELEMENT_NAME(endPoint->pipeline())), '-', span(desc.get()));
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(endPoint->pipeline()), GST_DEBUG_GRAPH_SHOW_ALL, dotFilename.ascii().data());
    }), this);
#endif

    gst_bin_add(GST_BIN_CAST(m_pipeline.get()), m_webrtcBin.get());
    return true;
}

void GStreamerMediaEndpoint::teardownPipeline()
{
    ASSERT(m_pipeline);
    GST_DEBUG_OBJECT(m_pipeline.get(), "Tearing down.");
    unregisterPipeline(m_pipeline);
#if !RELEASE_LOG_DISABLED
    stopLoggingStats();
#endif
    m_statsCollector->setElement(nullptr);

    if (m_webrtcBin)
        g_signal_handlers_disconnect_by_data(m_webrtcBin.get(), this);
    disconnectSimpleBusMessageCallback(m_pipeline.get());

    m_peerConnectionBackend.tearDown();
    gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);

    m_trackProcessors.clear();
    m_incomingDataChannels.clear();
    m_remoteStreamsById.clear();
    m_webrtcBin = nullptr;
    m_pipeline = nullptr;
}

bool GStreamerMediaEndpoint::handleMessage(GstMessage* message)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "eos");
        break;
    case GST_MESSAGE_ELEMENT: {
        const auto* data = gst_message_get_structure(message);
        if (gstStructureGetName(data) == "GstBinForwarded"_s) {
            GRefPtr<GstMessage> subMessage;
            gst_structure_get(data, "message", GST_TYPE_MESSAGE, &subMessage.outPtr(), nullptr);
            if (GST_MESSAGE_TYPE(subMessage.get()) == GST_MESSAGE_EOS)
                disposeElementChain(GST_ELEMENT_CAST(GST_MESSAGE_SRC(subMessage.get())));
        }
        break;
    }
    default:
        break;
    }
    return true;
}

void GStreamerMediaEndpoint::disposeElementChain(GstElement* element)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Got element EOS message from %" GST_PTR_FORMAT, element);

    auto pad = adoptGRef(gst_element_get_static_pad(element, "sink"));
    auto peer = adoptGRef(gst_pad_get_peer(pad.get()));

    gst_element_set_locked_state(m_pipeline.get(), TRUE);
    gst_pad_unlink(peer.get(), pad.get());
    gst_bin_remove(GST_BIN_CAST(m_pipeline.get()), element);
    gst_element_release_request_pad(m_webrtcBin.get(), peer.get());
    gst_element_set_state(element, GST_STATE_NULL);
    gst_element_set_locked_state(m_pipeline.get(), FALSE);
}

bool GStreamerMediaEndpoint::setConfiguration(MediaEndpointConfiguration& configuration)
{
    auto bundlePolicy = bundlePolicyFromConfiguration(configuration);
    auto iceTransportPolicy = iceTransportPolicyFromConfiguration(configuration);
    g_object_set(m_webrtcBin.get(), "bundle-policy", bundlePolicy, "ice-transport-policy", iceTransportPolicy, nullptr);

    for (auto& server : configuration.iceServers) {
        bool stunSet = false;
        for (auto& url : server.urls) {
            if (url.protocol().startsWith("turn"_s)) {
                auto valid = makeStringByReplacingAll(url.string().isolatedCopy(), "turn:"_s, "turn://"_s);
                valid = makeStringByReplacingAll(valid, "turns:"_s, "turns://"_s);
                URL validURL(URL(), valid);
                // FIXME: libnice currently doesn't seem to handle IPv6 addresses very well.
                if (validURL.host().startsWith('['))
                    continue;
                validURL.setUser(server.username);
                validURL.setPassword(server.credential);
                gboolean result = FALSE;
                g_signal_emit_by_name(m_webrtcBin.get(), "add-turn-server", validURL.string().utf8().data(), &result);
                if (!result)
                    GST_WARNING("Unable to use TURN server: %s", validURL.string().utf8().data());
            }
            if (!stunSet && url.protocol().startsWith("stun"_s)) {
                auto valid = makeStringByReplacingAll(url.string().isolatedCopy(), "stun:"_s, "stun://"_s);
                URL validURL(URL(), valid);
                // FIXME: libnice currently doesn't seem to handle IPv6 addresses very well.
                if (validURL.host().startsWith('['))
                    continue;
                g_object_set(m_webrtcBin.get(), "stun-server", validURL.string().utf8().data(), nullptr);
                stunSet = true;
            }
        }
    }

    // WIP: https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/merge_requests/302
    GST_FIXME("%zu custom certificates not propagated to webrtcbin", configuration.certificates.size());

    gst_element_set_state(m_pipeline.get(), GST_STATE_READY);
    GST_DEBUG_OBJECT(m_pipeline.get(), "End-point ready");
    return true;
}

void GStreamerMediaEndpoint::restartIce()
{
    if (isStopped())
        return;

    GST_DEBUG_OBJECT(m_pipeline.get(), "restarting ICE");
    // WIP: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/4611
    // We should re-initiate negotiation with the ice-restart offer option set to true.
}

static std::optional<std::pair<RTCSdpType, String>> fetchDescription(GstElement* webrtcBin, ASCIILiteral name)
{
    if (!webrtcBin)
        return { };

    GUniqueOutPtr<GstWebRTCSessionDescription> description;
    g_object_get(webrtcBin, makeString(name, "-description"_s).utf8().data(), &description.outPtr(), nullptr);
    if (!description)
        return { };

    unsigned totalAttributesNumber = gst_sdp_message_attributes_len(description->sdp);
    for (unsigned i = 0; i < totalAttributesNumber; i++) {
        const auto attribute = gst_sdp_message_get_attribute(description->sdp, i);
        if (!g_strcmp0(attribute->key, "end-of-candidates")) {
            gst_sdp_message_remove_attribute(description->sdp, i);
            i--;
        }
    }

    GUniquePtr<char> sdpString(gst_sdp_message_as_text(description->sdp));
    return { { fromSessionDescriptionType(*description.get()), String::fromUTF8(sdpString.get()) } };
}

static GstWebRTCSignalingState fetchSignalingState(GstElement* webrtcBin)
{
    GstWebRTCSignalingState state;
    g_object_get(webrtcBin, "signaling-state", &state, nullptr);
#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> desc(g_enum_to_string(GST_TYPE_WEBRTC_SIGNALING_STATE, state));
    GST_DEBUG_OBJECT(webrtcBin, "Signaling state set to %s", desc.get());
#endif
    return state;
}

enum class GatherSignalingState : bool { No, Yes };
static std::optional<PeerConnectionBackend::DescriptionStates> descriptionsFromWebRTCBin(GstElement* webrtcBin, GatherSignalingState gatherSignalingState = GatherSignalingState::No)
{
    std::optional<RTCSdpType> currentLocalDescriptionSdpType, pendingLocalDescriptionSdpType, currentRemoteDescriptionSdpType, pendingRemoteDescriptionSdpType;
    String currentLocalDescriptionSdp, pendingLocalDescriptionSdp, currentRemoteDescriptionSdp, pendingRemoteDescriptionSdp;
    if (auto currentLocalDescription = fetchDescription(webrtcBin, "current-local"_s)) {
        auto [sdpType, description] = *currentLocalDescription;
        currentLocalDescriptionSdpType = sdpType;
        currentLocalDescriptionSdp = WTFMove(description);
    }
    if (auto pendingLocalDescription = fetchDescription(webrtcBin, "pending-local"_s)) {
        auto [sdpType, description] = *pendingLocalDescription;
        pendingLocalDescriptionSdpType = sdpType;
        pendingLocalDescriptionSdp = WTFMove(description);
    }
    if (auto currentRemoteDescription = fetchDescription(webrtcBin, "current-remote"_s)) {
        auto [sdpType, description] = *currentRemoteDescription;
        currentRemoteDescriptionSdpType = sdpType;
        currentRemoteDescriptionSdp = WTFMove(description);
    }
    if (auto pendingRemoteDescription = fetchDescription(webrtcBin, "pending-remote"_s)) {
        auto [sdpType, description] = *pendingRemoteDescription;
        pendingRemoteDescriptionSdpType = sdpType;
        pendingRemoteDescriptionSdp = WTFMove(description);
    }

    std::optional<RTCSignalingState> signalingState;
    if (gatherSignalingState == GatherSignalingState::Yes)
        signalingState = toSignalingState(fetchSignalingState(webrtcBin));

    return PeerConnectionBackend::DescriptionStates {
        signalingState,
        currentLocalDescriptionSdpType, currentLocalDescriptionSdp,
        pendingLocalDescriptionSdpType, pendingLocalDescriptionSdp,
        currentRemoteDescriptionSdpType, currentRemoteDescriptionSdp,
        pendingRemoteDescriptionSdpType, pendingRemoteDescriptionSdp
    };
}

struct GStreamerMediaEndpointTransceiverState {
    String mid;
    Vector<String> receiverStreamIds;
    std::optional<RTCRtpTransceiverDirection> firedDirection;

    GStreamerMediaEndpointTransceiverState isolatedCopy() &&;
};

inline GStreamerMediaEndpointTransceiverState GStreamerMediaEndpointTransceiverState::isolatedCopy() &&
{
    return {
        WTFMove(mid).isolatedCopy(),
        crossThreadCopy(WTFMove(receiverStreamIds)),
        firedDirection
    };
}

Vector<String> getMediaStreamIdsFromSDPMedia(const GstSDPMedia& media)
{
    HashSet<String> mediaStreamIdsSet;
    for (guint i = 0; i < gst_sdp_media_attributes_len(&media); ++i) {
        const auto attribute = gst_sdp_media_get_attribute(&media, i);
        if (!g_strcmp0(attribute->key, "msid")) {
            auto components = String::fromUTF8(attribute->value).split(' ');
            if (components.size() < 2)
                continue;
            mediaStreamIdsSet.add(components[0]);
        }
        // MSID may also come in ssrc attributes, specially if they're in an SDP answer. They look like:
        // a=ssrc:3612593434 msid:e1019f4a-0983-4863-b923-b75903cced2c webrtctransceiver1
        if (!g_strcmp0(attribute->key, "ssrc")) {
            auto outerComponents = String::fromUTF8(attribute->value).split(' ');
            for (auto& outer : outerComponents) {
                auto innerComponents = outer.split(':');
                if (innerComponents.size() < 2)
                    continue;
                if (innerComponents[0] == "msid"_s)
                    mediaStreamIdsSet.add(innerComponents[1]);
            }
        }
    }
    Vector<String> mediaStreamIds;
    mediaStreamIds.reserveInitialCapacity(mediaStreamIdsSet.size());
    for (const auto& msid : mediaStreamIdsSet)
        mediaStreamIds.append(msid);
    return mediaStreamIds;
}

inline bool isRecvDirection(GstWebRTCRTPTransceiverDirection direction)
{
    return direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV || direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
}

static std::optional<GStreamerMediaEndpointTransceiverState> toGStreamerMediaEndpointTransceiverState(GstElement* webrtcBin, GstWebRTCRTPTransceiver* transceiver)
{
    GRefPtr<GstWebRTCRTPReceiver> receiver;
    GUniqueOutPtr<char> mid;
    GstWebRTCRTPTransceiverDirection direction;
    guint mLineIndex;
    // GstWebRTCRTPTransceiver doesn't have a fired-direction property, so use direction. Until
    // GStreamer 1.26 direction and current-direction always had the same value. This was fixed by:
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/commit/cafb999fb0cdf32803fcc3f85f2652212f05c2d0
    g_object_get(transceiver, "receiver", &receiver.outPtr(), "direction", &direction, "mlineindex", &mLineIndex, "mid", &mid.outPtr(), nullptr);
#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> desc(g_enum_to_string(GST_TYPE_WEBRTC_RTP_TRANSCEIVER_DIRECTION, direction));
    GST_TRACE_OBJECT(webrtcBin, "Receiver = %" GST_PTR_FORMAT ", direction = %s, mlineindex = %u, mid = %s", receiver.get(), desc.get(), mLineIndex, GST_STR_NULL(mid.get()));
#endif

    if (UNLIKELY(!mid))
        return { };

    GUniqueOutPtr<GstWebRTCSessionDescription> localDescription, remoteDescription;
    g_object_get(webrtcBin, "local-description", &localDescription.outPtr(), "remote-description", &remoteDescription.outPtr(), nullptr);

#ifndef GST_DISABLE_GST_DEBUG
    if (localDescription) {
        GUniquePtr<char> sdp(gst_sdp_message_as_text(localDescription->sdp));
        GST_TRACE_OBJECT(webrtcBin, "Local-description:\n%s", sdp.get());
    }
    if (remoteDescription) {
        GUniquePtr<char> sdp(gst_sdp_message_as_text(remoteDescription->sdp));
        GST_TRACE_OBJECT(webrtcBin, "Remote-description:\n%s", sdp.get());
    }
#endif

    Vector<String> streamIds;
    if (remoteDescription && remoteDescription->sdp && mLineIndex < gst_sdp_message_medias_len(remoteDescription->sdp)) {
        const GstSDPMedia* media = gst_sdp_message_get_media(remoteDescription->sdp, mLineIndex);
        if (isRecvDirection(direction))
            streamIds = getMediaStreamIdsFromSDPMedia(*media);
    }

    return { { String::fromUTF8(mid.get()), WTFMove(streamIds), { toRTCRtpTransceiverDirection(direction) } } };
}

static Vector<GStreamerMediaEndpointTransceiverState> transceiverStatesFromWebRTCBin(GstElement* webrtcBin)
{
    Vector<GStreamerMediaEndpointTransceiverState> states;
    GRefPtr<GArray> transceivers;
    g_signal_emit_by_name(webrtcBin, "get-transceivers", &transceivers.outPtr());
    GST_TRACE_OBJECT(webrtcBin, "Filling transceiver states for %u transceivers", transceivers ? transceivers->len : 0);
    if (!transceivers || !transceivers->len)
        return states;

    states.reserveInitialCapacity(transceivers->len);
    for (unsigned i = 0; i < transceivers->len; i++) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
        GstWebRTCRTPTransceiver* transceiver = g_array_index(transceivers.get(), GstWebRTCRTPTransceiver*, i);
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        auto state = toGStreamerMediaEndpointTransceiverState(webrtcBin, transceiver);
        if (!state)
            continue;
        states.append(WTFMove(*state));
    }

    states.shrinkToFit();
    return states;
}

void GStreamerMediaEndpoint::doSetLocalDescription(const RTCSessionDescription* description)
{
    RefPtr initialDescription = description;
    if (!initialDescription) {
        // Generate offer or answer. Workaround for https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/3218.
        auto promise = gst_promise_new();
        switch (fetchSignalingState(m_webrtcBin.get())) {
        case GST_WEBRTC_SIGNALING_STATE_STABLE:
        case GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_OFFER:
        case GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_PRANSWER: {
            g_signal_emit_by_name(m_webrtcBin.get(), "create-offer", nullptr, promise);
            auto result = gst_promise_wait(promise);
            const auto reply = gst_promise_get_reply(promise);
            if (result != GST_PROMISE_RESULT_REPLIED || (reply && gst_structure_has_field(reply, "error"))) {
                if (reply) {
                    GUniqueOutPtr<GError> error;
                    gst_structure_get(reply, "error", G_TYPE_ERROR, &error.outPtr(), nullptr);
                    auto errorMessage = makeString("Unable to set local description, error: "_s, span(error->message));
                    GST_ERROR_OBJECT(m_webrtcBin.get(), "%s", errorMessage.utf8().data());
                    m_peerConnectionBackend.setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, WTFMove(errorMessage) });
                    return;
                }
                m_peerConnectionBackend.setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, "Unable to set local description"_s });
                return;
            }

            GUniqueOutPtr<GstWebRTCSessionDescription> sessionDescription;
            gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &sessionDescription.outPtr(), nullptr);
            GUniquePtr<char> sdp(gst_sdp_message_as_text(sessionDescription->sdp));
            initialDescription = RTCSessionDescription::create(RTCSdpType::Offer, span(sdp.get()));
            break;
        }
        case GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_PRANSWER:
        case GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_OFFER: {
            g_signal_emit_by_name(m_webrtcBin.get(), "create-answer", nullptr, promise);
            auto result = gst_promise_wait(promise);
            const auto reply = gst_promise_get_reply(promise);
            if (result != GST_PROMISE_RESULT_REPLIED || (reply && gst_structure_has_field(reply, "error"))) {
                if (reply) {
                    GUniqueOutPtr<GError> error;
                    gst_structure_get(reply, "error", G_TYPE_ERROR, &error.outPtr(), nullptr);
                    auto errorMessage = makeString("Unable to set local description, error: "_s, span(error->message));
                    GST_ERROR_OBJECT(m_webrtcBin.get(), "%s", errorMessage.utf8().data());
                    m_peerConnectionBackend.setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, WTFMove(errorMessage) });
                    return;
                }
                m_peerConnectionBackend.setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, "Unable to set local description"_s });
                return;
            }

            GUniqueOutPtr<GstWebRTCSessionDescription> sessionDescription;
            gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &sessionDescription.outPtr(), nullptr);
            GUniquePtr<char> sdp(gst_sdp_message_as_text(sessionDescription->sdp));
            initialDescription = RTCSessionDescription::create(RTCSdpType::Answer, span(sdp.get()));
            break;
        }
        case GST_WEBRTC_SIGNALING_STATE_CLOSED:
            m_peerConnectionBackend.setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, "The PeerConnection is closed."_s });
            return;
        };
    }

    auto initialSDP = description ? description->sdp().isolatedCopy() : emptyString();
    auto remoteDescription = m_peerConnectionBackend.connection().remoteDescription();
    String remoteDescriptionSdp = remoteDescription ? remoteDescription->sdp() : emptyString();
    std::optional<RTCSdpType> remoteDescriptionSdpType = remoteDescription ? std::make_optional(remoteDescription->type()) : std::nullopt;

    if (!initialDescription->sdp().isEmpty()) {
        GUniqueOutPtr<GstSDPMessage> sdpMessage;
        if (gst_sdp_message_new_from_text(initialDescription->sdp().utf8().data(), &sdpMessage.outPtr()) != GST_SDP_OK) {
            m_peerConnectionBackend.setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, "Invalid SDP"_s });
            return;
        }

        // Make sure each outgoing media source is configured using the proposed codec and linked to webrtcbin.
        unsigned totalMedias = gst_sdp_message_medias_len(sdpMessage.get());
        for (unsigned i = 0; i < totalMedias; i++) {
            const auto media = gst_sdp_message_get_media(sdpMessage.get(), i);
            auto mediaType = StringView::fromLatin1(gst_sdp_media_get_media(media));
            RealtimeMediaSource::Type sourceType;
            if (mediaType == "audio"_s)
                sourceType = RealtimeMediaSource::Type::Audio;
            else if (mediaType == "video"_s)
                sourceType = RealtimeMediaSource::Type::Video;
            else
                continue;

            auto msid = String::fromUTF8(gst_sdp_media_get_attribute_val(media, "msid"));
            if (msid.isEmpty())
                continue;

            GST_DEBUG_OBJECT(m_pipeline.get(), "Looking-up outgoing source with msid %s", msid.utf8().data());
            m_unlinkedOutgoingSources.removeFirstMatching([&](auto& source) -> bool {
                auto track = source->track();
                if (UNLIKELY(!track))
                    return false;
                if (track->type() != sourceType)
                    return false;

                auto sourceMsid = makeString(source->mediaStreamID(), ' ', track->id());
                if (sourceMsid != msid)
                    return false;

                auto allowedCaps = capsFromSDPMedia(media);
                source->configure(WTFMove(allowedCaps));
                if (!source->pad()) {
                    auto rtpCaps = source->rtpCaps();
                    auto sinkPad = requestPad(rtpCaps, source->mediaStreamID());
                    source->setSinkPad(WTFMove(sinkPad));
                }

                auto& sinkPad = source->pad();
                if (UNLIKELY(gst_pad_is_linked(sinkPad.get()))) {
                    ASSERT_WITH_MESSAGE(gst_pad_is_linked(sinkPad.get()), "RealtimeMediaSource already linked.");
                    return true;
                }

                source->link();
                callOnMainThreadAndWait([&] {
                    source->start();
                });
                return true;
            });
        }
    }

    if (!m_unlinkedOutgoingSources.isEmpty())
        GST_WARNING_OBJECT(m_pipeline.get(), "Unlinked outgoing sources lingering");
    gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);

    setDescription(initialDescription.get(), DescriptionType::Local, [protectedThis = Ref(*this), this, initialSDP = WTFMove(initialSDP), remoteDescriptionSdp = WTFMove(remoteDescriptionSdp), remoteDescriptionSdpType = WTFMove(remoteDescriptionSdpType)](const GstSDPMessage& message) {
        if (protectedThis->isStopped())
            return;

        auto descriptions = descriptionsFromWebRTCBin(m_webrtcBin.get(), GatherSignalingState::Yes);

        // The initial description we pass to webrtcbin might actually be invalid or empty SDP, so
        // what we would get in return is an empty SDP message, without media line. When this
        // happens, restore previous state on RTCPeerConnection.
        if (!initialSDP.isEmpty() && descriptions && !gst_sdp_message_medias_len(&message)) {
            if (!descriptions->pendingLocalDescriptionSdp.isEmpty())
                descriptions->pendingLocalDescriptionSdp = initialSDP;
            else if (!descriptions->currentLocalDescriptionSdp.isEmpty())
                descriptions->currentLocalDescriptionSdp = initialSDP;

            if (!remoteDescriptionSdp.isEmpty()) {
                descriptions->pendingRemoteDescriptionSdp = remoteDescriptionSdp;
                descriptions->pendingRemoteDescriptionSdpType = remoteDescriptionSdpType;
            }
        }

#ifndef GST_DISABLE_GST_DEBUG
        auto dotFileName = makeString(span(GST_OBJECT_NAME(m_pipeline.get())), ".setLocalDescription"_s);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
#endif

        auto rtcTransceiverStates = transceiverStatesFromWebRTCBin(m_webrtcBin.get());
        auto transceiverStates = WTF::map(rtcTransceiverStates, [this](auto& state) -> PeerConnectionBackend::TransceiverState {
            auto streams = WTF::map(state.receiverStreamIds, [this](auto& id) -> Ref<MediaStream> {
                return mediaStreamFromRTCStream(id);
            });
            return { WTFMove(state.mid), WTFMove(streams), state.firedDirection };
        });

        GRefPtr<GstWebRTCSCTPTransport> transport;
        g_object_get(m_webrtcBin.get(), "sctp-transport", &transport.outPtr(), nullptr);

        std::optional<double> maxMessageSize;
        if (transport) {
            uint64_t maxMessageSizeValue;
            g_object_get(transport.get(), "max-message-size", &maxMessageSizeValue, nullptr);
            maxMessageSize = static_cast<double>(maxMessageSizeValue);
        }

        m_peerConnectionBackend.setLocalDescriptionSucceeded(WTFMove(descriptions), WTFMove(transceiverStates), transport ? makeUnique<GStreamerSctpTransportBackend>(WTFMove(transport)) : nullptr, maxMessageSize);
    }, [protectedThis = Ref(*this), this](const GError* error) {
        if (protectedThis->isStopped())
            return;
        if (error) {
            if (error->code == GST_WEBRTC_ERROR_INVALID_STATE) {
                m_peerConnectionBackend.setLocalDescriptionFailed(Exception { ExceptionCode::InvalidStateError, "Failed to set local answer sdp: no pending remote description."_s });
                return;
            }
            m_peerConnectionBackend.setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, String::fromUTF8(error->message) });
        } else
            m_peerConnectionBackend.setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, "Unable to apply session local description"_s });
    });
}

void GStreamerMediaEndpoint::setTransceiverCodecPreferences(const GstSDPMedia& media, guint transceiverIdx)
{
    auto direction = getDirectionFromSDPMedia(&media);
    if (direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE)
        return;

    GRefPtr<GstWebRTCRTPTransceiver> rtcTransceiver;
    g_signal_emit_by_name(m_webrtcBin.get(), "get-transceiver", transceiverIdx, &rtcTransceiver.outPtr());
    if (!rtcTransceiver)
        return;

    auto caps = capsFromSDPMedia(&media);
    GST_TRACE_OBJECT(m_webrtcBin.get(), "Setting codec-preferences to %" GST_PTR_FORMAT, caps.get());
    g_object_set(rtcTransceiver.get(), "codec-preferences", caps.get(), nullptr);
}

void GStreamerMediaEndpoint::doSetRemoteDescription(const RTCSessionDescription& description)
{
    auto initialSDP = description.sdp().isolatedCopy();
    auto localDescription = m_peerConnectionBackend.connection().localDescription();
    String localDescriptionSdp = localDescription ? localDescription->sdp() : emptyString();
    std::optional<RTCSdpType> localDescriptionSdpType = localDescription ? std::make_optional(localDescription->type()) : std::nullopt;

    setDescription(&description, DescriptionType::Remote, [protectedThis = Ref(*this), this, initialSDP = WTFMove(initialSDP), localDescriptionSdp = WTFMove(localDescriptionSdp), localDescriptionSdpType = WTFMove(localDescriptionSdpType)](const GstSDPMessage& message) {
        if (protectedThis->isStopped())
            return;

        processSDPMessage(&message, [this](unsigned, StringView mid, const auto* media) {
            const char* mediaType = gst_sdp_media_get_media(media);
            m_mediaForMid.set(mid.toString(), g_str_equal(mediaType, "audio") ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video);

            // https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/merge_requests/1907
            if (sdpMediaHasAttributeKey(media, "ice-lite")) {
                GRefPtr<GstWebRTCICE> ice;
                g_object_get(m_webrtcBin.get(), "ice-agent", &ice.outPtr(), nullptr);
                g_object_set(ice.get(), "ice-lite", TRUE, nullptr);
            }
        });

        GST_DEBUG_OBJECT(m_pipeline.get(), "Acking remote description");
        auto descriptions = descriptionsFromWebRTCBin(m_webrtcBin.get(), GatherSignalingState::Yes);

        // The initial description we pass to webrtcbin might actually be invalid or empty SDP, so
        // what we would get in return is an empty SDP message, without media line. When this
        // happens, restore previous state on RTCPeerConnection.
        if (descriptions && !gst_sdp_message_medias_len(&message)) {
            if (!descriptions->pendingRemoteDescriptionSdp.isEmpty())
                descriptions->pendingRemoteDescriptionSdp = initialSDP;
            else if (!descriptions->currentRemoteDescriptionSdp.isEmpty())
                descriptions->currentRemoteDescriptionSdp = initialSDP;

            if (!localDescriptionSdp.isEmpty()) {
                descriptions->pendingLocalDescriptionSdp = localDescriptionSdp;
                descriptions->pendingLocalDescriptionSdpType = localDescriptionSdpType;
            }
        }

#ifndef GST_DISABLE_GST_DEBUG
        auto dotFileName = makeString(span(GST_OBJECT_NAME(m_pipeline.get())), ".setRemoteDescription"_s);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
#endif

        auto rtcTransceiverStates = transceiverStatesFromWebRTCBin(m_webrtcBin.get());
        auto transceiverStates = WTF::map(rtcTransceiverStates, [this](auto& state) -> PeerConnectionBackend::TransceiverState {
            auto streams = WTF::map(state.receiverStreamIds, [this](auto& id) -> Ref<MediaStream> {
                return mediaStreamFromRTCStream(id);
            });
            return { WTFMove(state.mid), WTFMove(streams), state.firedDirection };
        });

        GRefPtr<GstWebRTCSCTPTransport> transport;
        g_object_get(m_webrtcBin.get(), "sctp-transport", &transport.outPtr(), nullptr);

        std::optional<double> maxMessageSize;
        if (transport) {
            uint64_t maxMessageSizeValue;
            g_object_get(transport.get(), "max-message-size", &maxMessageSizeValue, nullptr);
            maxMessageSize = static_cast<double>(maxMessageSizeValue);
        }

        m_peerConnectionBackend.setRemoteDescriptionSucceeded(WTFMove(descriptions), WTFMove(transceiverStates), transport ? makeUnique<GStreamerSctpTransportBackend>(WTFMove(transport)) : nullptr, maxMessageSize);
    }, [protectedThis = Ref(*this), this](const GError* error) {
        if (protectedThis->isStopped())
            return;
        if (error && error->code == GST_WEBRTC_ERROR_INVALID_STATE)
            m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { ExceptionCode::InvalidStateError, "Failed to set remote answer sdp"_s });
        else
            m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { ExceptionCode::OperationError, "Unable to apply session remote description"_s });
    });
#if !RELEASE_LOG_DISABLED
    startLoggingStats();
#endif
}

struct SetDescriptionCallData {
    Function<void(const GstSDPMessage&)> successCallback;
    Function<void(const GError*)> failureCallback;
    GUniqueOutPtr<GstSDPMessage> message;
    ASCIILiteral typeString;
    GRefPtr<GstElement> webrtcBin;
};

WEBKIT_DEFINE_ASYNC_DATA_STRUCT(SetDescriptionCallData)

void GStreamerMediaEndpoint::setDescription(const RTCSessionDescription* description, DescriptionType descriptionType, Function<void(const GstSDPMessage&)>&& successCallback, Function<void(const GError*)>&& failureCallback)
{
    GST_DEBUG_OBJECT(m_webrtcBin.get(), "Setting %s description", descriptionType == DescriptionType::Local ? "local" : "remote");

    GUniqueOutPtr<GstSDPMessage> message;
    auto sdpType = RTCSdpType::Offer;

    if (description) {
        if (description->sdp().isEmpty()) {
            failureCallback(nullptr);
            return;
        }
        auto sdp = makeStringByReplacingAll(description->sdp(), "opus"_s, "OPUS"_s);
        if (gst_sdp_message_new_from_text(sdp.utf8().data(), &message.outPtr()) != GST_SDP_OK) {
            failureCallback(nullptr);
            return;
        }
        sdpType = description->type();
        if (descriptionType == DescriptionType::Local && sdpType == RTCSdpType::Answer && !gst_sdp_message_get_version(message.get())) {
            GError error;
            GUniquePtr<char> errorMessage(g_strdup("Expect line: v="));
            error.message = errorMessage.get();
            failureCallback(&error);
            return;
        }
    } else if (gst_sdp_message_new(&message.outPtr()) != GST_SDP_OK) {
        failureCallback(nullptr);
        return;
    }

    auto type = toSessionDescriptionType(sdpType);
    auto typeString = descriptionType == DescriptionType::Local ? "local"_s : "remote"_s;
    GST_DEBUG_OBJECT(m_pipeline.get(), "Creating %s session for SDP %s", typeString.characters(), gst_webrtc_sdp_type_to_string(type));
    auto signalName = makeString("set-"_s, typeString, "-description"_s);

    auto* data = createSetDescriptionCallData();
    data->successCallback = WTFMove(successCallback);
    data->failureCallback = WTFMove(failureCallback);
    data->typeString = WTFMove(typeString);
    data->webrtcBin = m_webrtcBin;
    gst_sdp_message_copy(message.get(), &data->message.outPtr());

#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> sdp(gst_sdp_message_as_text(data->message.get()));
    GST_DEBUG_OBJECT(m_pipeline.get(), "SDP: %s", sdp.get());
#endif

    GUniquePtr<GstWebRTCSessionDescription> sessionDescription(gst_webrtc_session_description_new(type, message.release()));
    g_signal_emit_by_name(m_webrtcBin.get(), signalName.ascii().data(), sessionDescription.get(), gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) {
        auto* data = static_cast<SetDescriptionCallData*>(userData);
        auto promise = adoptGRef(rawPromise);
        auto result = gst_promise_wait(promise.get());
        const auto* reply = gst_promise_get_reply(promise.get());
        GST_DEBUG_OBJECT(data->webrtcBin.get(), "%s description reply: %u %" GST_PTR_FORMAT, data->typeString.characters(), result, reply);
        if (result != GST_PROMISE_RESULT_REPLIED || (reply && gst_structure_has_field(reply, "error"))) {
            std::optional<GUniquePtr<GError>> errorHolder;
            if (reply) {
                GUniqueOutPtr<GError> error;
                gst_structure_get(reply, "error", G_TYPE_ERROR, &error.outPtr(), nullptr);
                GST_ERROR_OBJECT(data->webrtcBin.get(), "Unable to set description, error: %s", error->message);
                errorHolder = GUniquePtr<GError>(error.release());
            }
            callOnMainThread([error = WTFMove(errorHolder), failureCallback = WTFMove(data->failureCallback)] {
                failureCallback(error ? error->get() : nullptr);
            });
            return;
        }

        if (!data->successCallback)
            return;
        callOnMainThread([successCallback = WTFMove(data->successCallback), message = GUniquePtr<GstSDPMessage>(data->message.release())]() mutable {
            successCallback(*message.get());
        });
    }, data, reinterpret_cast<GDestroyNotify>(destroySetDescriptionCallData)));
}

void GStreamerMediaEndpoint::processSDPMessage(const GstSDPMessage* message, Function<void(unsigned, StringView, const GstSDPMedia*)> mediaCallback)
{
    unsigned totalMedias = gst_sdp_message_medias_len(message);
    for (unsigned mediaIndex = 0; mediaIndex < totalMedias; mediaIndex++) {
        const auto* media = gst_sdp_message_get_media(message, mediaIndex);
        const char* mediaType = gst_sdp_media_get_media(media);
        if (!g_str_equal(mediaType, "audio") && !g_str_equal(mediaType, "video"))
            continue;

#ifndef GST_DISABLE_GST_DEBUG
        GUniquePtr<char> mediaRepresentation(gst_sdp_media_as_text(media));
        GST_LOG_OBJECT(m_pipeline.get(), "Processing media:\n%s", mediaRepresentation.get());
#endif
        const char* mid = gst_sdp_media_get_attribute_val(media, "mid");
        if (!mid)
            continue;

        bool isInactive = false;
        unsigned totalAttributes = gst_sdp_media_attributes_len(media);
        for (unsigned attributeIndex = 0; attributeIndex < totalAttributes; attributeIndex++) {
            const auto* attribute = gst_sdp_media_get_attribute(media, attributeIndex);
            if (!g_strcmp0(attribute->key, "inactive")) {
                isInactive = true;
                break;
            }
        }
        if (isInactive) {
            GST_DEBUG_OBJECT(m_pipeline.get(), "Skipping inactive media");
            continue;
        }

        mediaCallback(mediaIndex, StringView::fromLatin1(mid), media);
    }
}

void GStreamerMediaEndpoint::configureSource(RealtimeOutgoingMediaSourceGStreamer& source, GUniquePtr<GstStructure>&& parameters)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Configuring outgoing source %" GST_PTR_FORMAT, source.bin().get());
    source.setInitialParameters(WTFMove(parameters));

    auto sourceBin = source.bin();
    gst_bin_add(GST_BIN_CAST(m_pipeline.get()), sourceBin.get());

#ifndef GST_DISABLE_GST_DEBUG
    auto dotFileName = makeString(span(GST_OBJECT_NAME(m_pipeline.get())), ".outgoing"_s);
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
#endif
}

GRefPtr<GstPad> GStreamerMediaEndpoint::requestPad(const GRefPtr<GstCaps>& allowedCaps, const String& mediaStreamID)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Requesting sink pad for %" GST_PTR_FORMAT " and mediaStreamID %s", allowedCaps.get(), mediaStreamID.ascii().data());
    auto caps = adoptGRef(gst_caps_copy(allowedCaps.get()));
    int availablePayloadType = pickAvailablePayloadType();
    unsigned i = 0;
    while (i < gst_caps_get_size(caps.get())) {
        auto* structure = gst_caps_get_structure(caps.get(), i);
        if (gst_structure_has_field(structure, "payload")) {
            i++;
            continue;
        }
        std::optional<int> payloadType;
        if (auto encodingName = gstStructureGetString(structure, "encoding-name"_s))
            payloadType = payloadTypeForEncodingName(encodingName);

        if (!payloadType) {
            if (availablePayloadType < 128)
                payloadType = availablePayloadType++;
        }
        if (!payloadType) {
            GST_WARNING_OBJECT(m_pipeline.get(), "Payload type will not fit in SDP offer. Removing codec from preferences: %" GST_PTR_FORMAT, structure);
            gst_caps_remove_structure(caps.get(), i);
            continue;
        }
        gst_structure_set(structure, "payload", G_TYPE_INT, *payloadType, nullptr);
        i++;
    }

    // Update codec preferences on the first matching un-associated transceiver, otherwise a new one
    // would be created, leading to extra m-line in SDP. This is a requirement since:
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/commit/09d870a39c28428dc1c8ed77006bf8ad6d3f005e
    // which is included in our SDKs.
    auto kind = webrtcKindFromCaps(caps);
    GRefPtr<GArray> transceivers;
    g_signal_emit_by_name(m_webrtcBin.get(), "get-transceivers", &transceivers.outPtr());
    if (transceivers && transceivers->len) {
        for (unsigned i = 0; i < transceivers->len; i++) {
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
            GstWebRTCRTPTransceiver* transceiver = g_array_index(transceivers.get(), GstWebRTCRTPTransceiver*, i);
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
            GstWebRTCKind transceiverKind;
            g_object_get(transceiver, "kind", &transceiverKind, nullptr);
            if (transceiverKind != kind)
                continue;

            bool isTransceiverAssociated = false;
            for (auto pad : GstIteratorAdaptor<GstPad>(GUniquePtr<GstIterator>(gst_element_iterate_sink_pads(m_webrtcBin.get())))) {
                GRefPtr<GstWebRTCRTPTransceiver> padTransceiver;
                g_object_get(pad, "transceiver", &padTransceiver.outPtr(), nullptr);
                if (padTransceiver.get() == transceiver) {
                    isTransceiverAssociated = true;
                    break;
                }
            }
            if (isTransceiverAssociated)
                continue;

            g_object_set(transceiver, "codec-preferences", caps.get(), nullptr);
            GST_DEBUG_OBJECT(m_pipeline.get(), "Expecting transceiver %" GST_PTR_FORMAT " to associate to new webrtc sink pad", transceiver);
            break;
        }
    }

    auto padTemplate = gst_element_get_pad_template(m_webrtcBin.get(), "sink_%u");
    auto sinkPad = adoptGRef(gst_element_request_pad(m_webrtcBin.get(), padTemplate, nullptr, caps.get()));

    if (!mediaStreamID.isEmpty()) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Setting msid to %s on sink pad %" GST_PTR_FORMAT, mediaStreamID.ascii().data(), sinkPad.get());
        if (gstObjectHasProperty(sinkPad.get(), "msid"))
            g_object_set(sinkPad.get(), "msid", mediaStreamID.ascii().data(), nullptr);
    }

    GRefPtr<GstWebRTCRTPTransceiver> transceiver;
    g_object_get(sinkPad.get(), "transceiver", &transceiver.outPtr(), nullptr);
    g_object_set(transceiver.get(), "codec-preferences", caps.get(), nullptr);
    return sinkPad;
}

std::optional<bool> GStreamerMediaEndpoint::isIceGatheringComplete(const String& currentLocalDescription)
{
    GUniqueOutPtr<GstSDPMessage> message;
    if (gst_sdp_message_new_from_text(currentLocalDescription.utf8().data(), &message.outPtr()) != GST_SDP_OK)
        return { };

    unsigned numberOfMedias = gst_sdp_message_medias_len(message.get());
    for (unsigned i = 0; i < numberOfMedias; i++) {
        const auto* media = gst_sdp_message_get_media(message.get(), i);
        const char* value = gst_sdp_media_get_attribute_val_n(media, "end-of-candidates", 0);
        if (!value)
            return false;
    }

    return true;
}

ExceptionOr<std::unique_ptr<GStreamerRtpSenderBackend>> GStreamerMediaEndpoint::addTrack(MediaStreamTrack& track, const FixedVector<String>& mediaStreamIds)
{
    GStreamerRtpSenderBackend::Source source;
    auto mediaStreamId = mediaStreamIds.isEmpty() ? emptyString() : mediaStreamIds[0];

    String kind;
    RTCRtpTransceiverInit init;
    init.direction = RTCRtpTransceiverDirection::Sendrecv;

    GST_DEBUG_OBJECT(m_pipeline.get(), "Adding source for track %s", track.id().utf8().data());
    if (track.privateTrack().isAudio()) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Adding outgoing audio source");
        auto audioSource = RealtimeOutgoingAudioSourceGStreamer::create(m_ssrcGenerator, mediaStreamId, track);
        source = WTFMove(audioSource);
        kind = "audio"_s;
    } else {
        ASSERT(track.privateTrack().isVideo());
        GST_DEBUG_OBJECT(m_pipeline.get(), "Adding outgoing video source");
        auto videoSource = RealtimeOutgoingVideoSourceGStreamer::create(m_ssrcGenerator, mediaStreamId, track);
        source = WTFMove(videoSource);
        kind = "video"_s;
    }

    auto backendsResult = createTransceiverBackends(kind, init, WTFMove(source), GStreamerPeerConnectionBackend::IgnoreNegotiationNeededFlag::No);
    if (backendsResult.hasException())
        return backendsResult.releaseException();

    auto backends = backendsResult.releaseReturnValue();
    auto senderBackend = WTFMove(backends.senderBackend);
    GST_DEBUG_OBJECT(m_pipeline.get(), "Sender configured");
    onNegotiationNeeded();
    return senderBackend;
}

void GStreamerMediaEndpoint::removeTrack(GStreamerRtpSenderBackend& sender)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Removing track");
    sender.stopSource();
    sender.clearSource();
    onNegotiationNeeded();
}

void GStreamerMediaEndpoint::doCreateOffer(const RTCOfferOptions& options)
{
    // https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/merge_requests/1877
    // FIXME: Plumb options.voiceActivityDetection.
    initiate(true, gst_structure_new("webrtcbin-offer-options", "ice-restart", G_TYPE_BOOLEAN, options.iceRestart, nullptr));
}

void GStreamerMediaEndpoint::doCreateAnswer()
{
    initiate(false, nullptr);
}

struct GStreamerMediaEndpointHolder {
    RefPtr<GStreamerMediaEndpoint> endPoint;
    RTCSdpType sdpType;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(GStreamerMediaEndpointHolder)

void GStreamerMediaEndpoint::initiate(bool isInitiator, GstStructure* rawOptions)
{
    auto type = isInitiator ? "offer"_s : "answer"_s;
    GST_DEBUG_OBJECT(m_pipeline.get(), "Creating %s", type.characters());
    auto signalName = makeString("create-"_s, type);
    GUniquePtr<GstStructure> options(rawOptions);
    auto* holder = createGStreamerMediaEndpointHolder();
    holder->endPoint = this;
    holder->sdpType = isInitiator ? RTCSdpType::Offer : RTCSdpType::Answer;

    g_signal_emit_by_name(m_webrtcBin.get(), signalName.ascii().data(), options.get(), gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) {
        auto* holder = static_cast<GStreamerMediaEndpointHolder*>(userData);
        auto promise = adoptGRef(rawPromise);
        auto result = gst_promise_wait(promise.get());
        if (result != GST_PROMISE_RESULT_REPLIED) {
            holder->endPoint->createSessionDescriptionFailed(holder->sdpType, { });
            return;
        }

        const auto* reply = gst_promise_get_reply(promise.get());
        ASSERT(reply);
        if (gst_structure_has_field(reply, "error")) {
            GUniqueOutPtr<GError> promiseError;
            gst_structure_get(reply, "error", G_TYPE_ERROR, &promiseError.outPtr(), nullptr);
            holder->endPoint->createSessionDescriptionFailed(holder->sdpType, GUniquePtr<GError>(promiseError.release()));
            return;
        }

        GUniqueOutPtr<GstWebRTCSessionDescription> sessionDescription;
        const char* sdpTypeString = holder->sdpType == RTCSdpType::Offer ? "offer" : "answer";
        gst_structure_get(reply, sdpTypeString, GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &sessionDescription.outPtr(), nullptr);

#ifndef GST_DISABLE_GST_DEBUG
        GUniquePtr<char> sdp(gst_sdp_message_as_text(sessionDescription->sdp));
        GST_DEBUG_OBJECT(holder->endPoint->pipeline(), "Created %s: %s", sdpTypeString, sdp.get());
#endif
        holder->endPoint->createSessionDescriptionSucceeded(GUniquePtr<GstWebRTCSessionDescription>(sessionDescription.release()));
    }, holder, reinterpret_cast<GDestroyNotify>(destroyGStreamerMediaEndpointHolder)));
}

void GStreamerMediaEndpoint::getStats(const GRefPtr<GstPad>& pad, Ref<DeferredPromise>&& promise)
{
    m_statsCollector->getStats([promise = WTFMove(promise), protectedThis = Ref(*this)](auto&& report) mutable {
        ASSERT(isMainThread());
        if (protectedThis->isStopped() || !report) {
            promise->resolve();
            return;
        }

        promise->resolve<IDLInterface<RTCStatsReport>>(report.releaseNonNull());
    }, pad, [weakThis = ThreadSafeWeakPtr { *this }, this](const auto& pad, const auto* stats) -> GUniquePtr<GstStructure> {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return nullptr;
        return preprocessStats(pad, stats);
    });
}

void GStreamerMediaEndpoint::getStats(RTCRtpReceiver& receiver, Ref<DeferredPromise>&& promise)
{
    GstElement* bin = nullptr;
    auto& source = receiver.track().privateTrack().source();
    if (source.isIncomingAudioSource())
        bin = static_cast<RealtimeIncomingAudioSourceGStreamer&>(source).bin();
    else if (source.isIncomingVideoSource())
        bin = static_cast<RealtimeIncomingVideoSourceGStreamer&>(source).bin();
    else
        RELEASE_ASSERT_NOT_REACHED();

    if (!bin) {
        promise->resolve();
        return;
    }

    auto sinkPad = adoptGRef(gst_element_get_static_pad(bin, "sink"));
    if (!sinkPad) {
        // The incoming source bin is not linked yet, so look for a matching upstream track processor.
        GRefPtr<GstPad> pad;
        const auto& trackId = receiver.track().id();
        for (auto& processor : m_trackProcessors.values()) {
            if (processor->trackId() != trackId)
                continue;
            pad = processor->pad();
            break;
        }
        getStats(pad, WTFMove(promise));
        return;
    }
    auto srcPad = adoptGRef(gst_pad_get_peer(sinkPad.get()));
    getStats(srcPad, WTFMove(promise));
}

MediaStream& GStreamerMediaEndpoint::mediaStreamFromRTCStream(String mediaStreamId)
{
    auto mediaStream = m_remoteStreamsById.ensure(mediaStreamId, [&] {
        auto& document = downcast<Document>(*m_peerConnectionBackend.connection().scriptExecutionContext());
        return MediaStream::create(document, MediaStreamPrivate::create(document.logger(), { }, WTFMove(mediaStreamId)));
    });
    return *mediaStream.iterator->value;
}

String GStreamerMediaEndpoint::trackIdFromSDPMedia(const GstSDPMedia& media)
{
    const char* msidAttribute = gst_sdp_media_get_attribute_val(&media, "msid");
    if (!msidAttribute)
        return emptyString();

    GST_LOG_OBJECT(m_pipeline.get(), "SDP media msid attribute value: %s", msidAttribute);
    auto components = String::fromUTF8(msidAttribute).split(' ');
    if (components.size() < 2)
        return emptyString();

    return components[1];
}

void GStreamerMediaEndpoint::connectIncomingTrack(WebRTCTrackData& data)
{
    ASSERT(isMainThread());

    GUniqueOutPtr<GstWebRTCSessionDescription> description;
    g_object_get(m_webrtcBin.get(), "remote-description", &description.outPtr(), nullptr);

    GRefPtr<GstWebRTCRTPTransceiver> rtcTransceiver(data.transceiver);
    auto trackId = data.trackId;
    auto transceiver = m_peerConnectionBackend.existingTransceiver([&](auto& backend) -> bool {
        return backend.rtcTransceiver() == rtcTransceiver.get();
    });
    if (!transceiver) {
        unsigned mLineIndex;
        g_object_get(rtcTransceiver.get(), "mlineindex", &mLineIndex, nullptr);
        const auto media = gst_sdp_message_get_media(description->sdp, mLineIndex);
        if (UNLIKELY(!media)) {
            GST_WARNING_OBJECT(m_pipeline.get(), "SDP media for transceiver %u not found, skipping incoming track setup", mLineIndex);
            return;
        }
        transceiver = &m_peerConnectionBackend.newRemoteTransceiver(makeUnique<GStreamerRtpTransceiverBackend>(WTFMove(rtcTransceiver)), data.type, trackId.isolatedCopy());
    }

    auto mediaStreamBin = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_pipeline.get()), data.mediaStreamBinName.ascii().data()));
    auto& track = transceiver->receiver().track();
    auto& source = track.privateTrack().source();
    if (source.isIncomingAudioSource()) {
        auto& audioSource = static_cast<RealtimeIncomingAudioSourceGStreamer&>(source);
        if (!audioSource.setBin(mediaStreamBin))
            return;
    } else if (source.isIncomingVideoSource()) {
        auto& videoSource = static_cast<RealtimeIncomingVideoSourceGStreamer&>(source);
        if (!videoSource.setBin(mediaStreamBin))
            return;
    }

    m_pendingIncomingTracks.append(&track.privateTrack());

    unsigned totalExpectedMediaTracks = 0;
    for (unsigned i = 0; i < gst_sdp_message_medias_len(description->sdp); i++) {
        const auto media = gst_sdp_message_get_media(description->sdp, i);
        const char* mediaType = gst_sdp_media_get_media(media);
        if (g_str_equal(mediaType, "audio") || g_str_equal(mediaType, "video"))
            totalExpectedMediaTracks++;
    }

    GST_DEBUG_OBJECT(m_pipeline.get(), "Expecting %u media tracks", totalExpectedMediaTracks);
    if (m_pendingIncomingTracks.size() < totalExpectedMediaTracks) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Only %zu track(s) received so far", m_pendingIncomingTracks.size());
        return;
    }

    GST_DEBUG_OBJECT(m_pipeline.get(), "Incoming stream %s ready, notifying observers", data.mediaStreamId.ascii().data());
    for (auto& track : m_pendingIncomingTracks) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Incoming stream has track %s", track->id().utf8().data());
        track->dataFlowStarted();
        track->source().setMuted(false);
    }

    m_pendingIncomingTracks.clear();
    gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
}

void GStreamerMediaEndpoint::connectPad(GstPad* pad)
{
    auto caps = adoptGRef(gst_pad_get_current_caps(pad));
    if (!caps)
        caps = adoptGRef(gst_pad_query_caps(pad, nullptr));

    auto structure = gst_caps_get_structure(caps.get(), 0);
    auto ssrc = gstStructureGet<unsigned>(structure, "ssrc"_s);
    if (!ssrc) {
        GST_ERROR_OBJECT(m_pipeline.get(), "Missing SSRC for webrtcin src pad %" GST_PTR_FORMAT, pad);
        return;
    }

    auto trackProcessor = m_trackProcessors.get(*ssrc);
    trackProcessor->configure(ThreadSafeWeakPtr { *this }, GRefPtr<GstPad>(pad));

    auto bin = trackProcessor->bin();
    gst_bin_add(GST_BIN_CAST(m_pipeline.get()), bin);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(bin, "sink"));
    gst_pad_link(pad, sinkPad.get());
    gst_element_set_state(bin, GST_STATE_PAUSED);

#ifndef GST_DISABLE_GST_DEBUG
    auto dotFileName = makeString(span(GST_OBJECT_NAME(m_pipeline.get())), ".pending-"_s, span(GST_OBJECT_NAME(pad)));
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
#endif
}

void GStreamerMediaEndpoint::removeRemoteStream(GstPad*)
{
    GST_FIXME_OBJECT(m_pipeline.get(), "removeRemoteStream");
    notImplemented();
}

struct PayloadTypeHolder {
    int payloadType { 95 };
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(PayloadTypeHolder);

int GStreamerMediaEndpoint::pickAvailablePayloadType()
{
    auto* holder = createPayloadTypeHolder();
    GRefPtr<GArray> transceivers;
    g_signal_emit_by_name(m_webrtcBin.get(), "get-transceivers", &transceivers.outPtr());
    GST_DEBUG_OBJECT(m_pipeline.get(), "Looking for unused payload type in %u transceivers", transceivers->len);
    for (unsigned i = 0; i < transceivers->len; i++) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
        GstWebRTCRTPTransceiver* current = g_array_index(transceivers.get(), GstWebRTCRTPTransceiver*, i);
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

        GRefPtr<GstCaps> codecPreferences;
        g_object_get(current, "codec-preferences", &codecPreferences.outPtr(), nullptr);
        if (!codecPreferences)
            continue;

        gst_caps_foreach(codecPreferences.get(), reinterpret_cast<GstCapsForeachFunc>(+[](GstCapsFeatures*, GstStructure* structure, gpointer data) -> gboolean {
            auto payloadType = gstStructureGet<int>(structure, "payload"_s);
            if (!payloadType)
                return TRUE;

            auto* holder = reinterpret_cast<PayloadTypeHolder*>(data);
            holder->payloadType = std::max(holder->payloadType, *payloadType);
            return TRUE;
        }), holder);
    }

    int payloadType = holder->payloadType;
    destroyPayloadTypeHolder(holder);
    payloadType++;
    GST_DEBUG_OBJECT(m_pipeline.get(), "Next available payload type is %d", payloadType);
    return payloadType;
}

ExceptionOr<GStreamerMediaEndpoint::Backends> GStreamerMediaEndpoint::createTransceiverBackends(const String& kind, const RTCRtpTransceiverInit& init, GStreamerRtpSenderBackend::Source&& source, PeerConnectionBackend::IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    if (!m_webrtcBin)
        return Exception { ExceptionCode::InvalidStateError, "End-point has not been configured yet"_s };

    // The current add-transceiver implementation in webrtcbin is synchronous and doesn't trigger
    // negotiation-needed signals but we keep the m_shouldIgnoreNegotiationNeededSignal in case this
    // changes in future versions of webrtcbin.
    bool shouldIgnoreNegotiationNeededSignal = ignoreNegotiationNeededFlag == PeerConnectionBackend::IgnoreNegotiationNeededFlag::Yes ? true : false;
    SetForScope scopedIgnoreNegotiationNeededSignal(m_shouldIgnoreNegotiationNeededSignal, shouldIgnoreNegotiationNeededSignal, false);

    GST_DEBUG_OBJECT(m_pipeline.get(), "%zu streams in init data", init.streams.size());

    auto& registryScanner = GStreamerRegistryScanner::singleton();
    auto direction = fromRTCRtpTransceiverDirection(init.direction);
    Vector<RTCRtpCodecCapability> codecs;

    auto rtpExtensions = kind == "video"_s ? registryScanner.videoRtpExtensions() : registryScanner.audioRtpExtensions();

    if (direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV) {
        auto mergeCodecs = [&codecs](auto& additionalCodecs) {
            for (auto& codec : additionalCodecs) {
                auto result = codecs.findIf([mimeType = codec.mimeType](auto& item) {
                    return item.mimeType == mimeType;
                });
                if (result == notFound)
                    codecs.append(codec);
            }
        };
        if (kind == "video"_s) {
            codecs = registryScanner.videoRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding).codecs;
            auto decodingCapabilities = registryScanner.videoRtpCapabilities(GStreamerRegistryScanner::Configuration::Decoding).codecs;
            mergeCodecs(decodingCapabilities);
        } else {
            codecs = registryScanner.audioRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding).codecs;
            auto decodingCapabilities = registryScanner.audioRtpCapabilities(GStreamerRegistryScanner::Configuration::Decoding).codecs;
            mergeCodecs(decodingCapabilities);
        }
    } else if (direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY) {
        if (kind == "video"_s)
            codecs = registryScanner.videoRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding).codecs;
        else
            codecs = registryScanner.audioRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding).codecs;
    } else if (direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY) {
        if (kind == "video"_s)
            codecs = registryScanner.videoRtpCapabilities(GStreamerRegistryScanner::Configuration::Decoding).codecs;
        else
            codecs = registryScanner.audioRtpCapabilities(GStreamerRegistryScanner::Configuration::Decoding).codecs;
    }

    String mediaStreamId;
    String trackId;
    switchOn(source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        mediaStreamId = source->mediaStreamID();
        if (auto track = source->track())
            trackId = track->id();
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        mediaStreamId = source->mediaStreamID();
        if (auto track = source->track())
            trackId = track->id();
    }, [](std::nullptr_t&) { });

    int payloadType = pickAvailablePayloadType();
    auto msid = !mediaStreamId.isEmpty() && !trackId.isEmpty() ? makeString(mediaStreamId, ' ', trackId) : emptyString();
    auto caps = capsFromRtpCapabilities({ .codecs = codecs, .headerExtensions = rtpExtensions }, [&payloadType, &msid](GstStructure* structure) {
        if (!gst_structure_has_field(structure, "payload"))
            gst_structure_set(structure, "payload", G_TYPE_INT, payloadType++, nullptr);
        if (msid.isEmpty())
            return;
        gst_structure_set(structure, "a-msid", G_TYPE_STRING, msid.utf8().data(), nullptr);
    });

#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> desc(g_enum_to_string(GST_TYPE_WEBRTC_RTP_TRANSCEIVER_DIRECTION, direction));
    GST_DEBUG_OBJECT(m_pipeline.get(), "Adding %s transceiver for payload %" GST_PTR_FORMAT, desc.get(), caps.get());
#endif

    // FIXME: None of this (excepted direction) is passed to webrtcbin yet.
    GUniquePtr<GstStructure> initData(gst_structure_new("transceiver-init-data", "direction", GST_TYPE_WEBRTC_RTP_TRANSCEIVER_DIRECTION, direction, nullptr));

    GValue streamIdsValue = G_VALUE_INIT;
    g_value_init(&streamIdsValue, GST_TYPE_LIST);
    for (auto& stream : init.streams) {
        GValue value = G_VALUE_INIT;
        g_value_init(&value, G_TYPE_STRING);
        g_value_set_string(&value, stream->id().utf8().data());
        gst_value_list_append_value(&streamIdsValue, &value);
        g_value_unset(&value);
    }
    gst_structure_take_value(initData.get(), "stream-ids", &streamIdsValue);

    GValue codecsValue = G_VALUE_INIT;
    g_value_init(&codecsValue, GST_TYPE_LIST);
    unsigned index = 0;
    for (auto& codec : codecs) {
        GUniquePtr<GstStructure> codecData(gst_structure_new("codec-parameters",
            "mime-type", G_TYPE_STRING, codec.mimeType.utf8().data(), "clock-rate", G_TYPE_UINT, codec.clockRate,
            "fmtp-line", G_TYPE_STRING, codec.sdpFmtpLine.utf8().data(), nullptr));
        if (codec.channels)
            gst_structure_set(codecData.get(), "channels", G_TYPE_UINT, codec.channels.value(), nullptr);

        auto codecStructure = gst_caps_get_structure(caps.get(), index);
        if (auto pt = gstStructureGet<int>(codecStructure, "payload"_s))
            gst_structure_set(codecData.get(), "pt", G_TYPE_UINT, static_cast<unsigned>(*pt), nullptr);

        GValue value = G_VALUE_INIT;
        g_value_init(&value, GST_TYPE_STRUCTURE);
        gst_value_set_structure(&value, codecData.get());
        gst_value_list_append_and_take_value(&codecsValue, &value);
        index++;
    }
    gst_structure_take_value(initData.get(), "codecs", &codecsValue);

    GValue encodingsValue = G_VALUE_INIT;
    g_value_init(&encodingsValue, GST_TYPE_LIST);
    auto scopeExit = makeScopeExit([&] {
        g_value_unset(&encodingsValue);
    });
    if (kind == "audio"_s) {
        if (!init.sendEncodings.isEmpty()) {
            auto encodingData = fromRTCEncodingParameters(init.sendEncodings[0], kind);
            if (encodingData.hasException())
                return encodingData.releaseException();
            GValue value = G_VALUE_INIT;
            g_value_init(&value, GST_TYPE_STRUCTURE);
            gst_value_set_structure(&value, encodingData.returnValue().get());
            gst_value_list_append_and_take_value(&encodingsValue, &value);
        } else {
            GUniquePtr<GstStructure> encodingData(gst_structure_new("encoding-parameters", "encoding-name", G_TYPE_STRING, "OPUS", "payload", G_TYPE_INT, 96, "active", G_TYPE_BOOLEAN, TRUE, nullptr));
            GValue value = G_VALUE_INIT;
            g_value_init(&value, GST_TYPE_STRUCTURE);
            gst_value_set_structure(&value, encodingData.get());
            gst_value_list_append_and_take_value(&encodingsValue, &value);
        }
    } else if (!init.sendEncodings.isEmpty()) {
        auto sendEncodings = init.sendEncodings;
        if (init.sendEncodings.size() > 10) {
            GST_WARNING_OBJECT(m_pipeline.get(), "Too many (%zu) sendEncodings requested for video transceiver. Limiting to 10.", init.sendEncodings.size());
            sendEncodings = sendEncodings.subvector(0, 10);
        }
        Vector<String> allRids;
        Vector<double> scaleValues;
        scaleValues.reserveInitialCapacity(sendEncodings.size());
        if (sendEncodings.size() == 1 && sendEncodings[0].scaleResolutionDownBy)
            scaleValues.append(sendEncodings[0].scaleResolutionDownBy.value());
        else if (allOf(sendEncodings, [](auto& encoding) { return encoding.scaleResolutionDownBy.value_or(1) == 1; })) {
            for (unsigned i = sendEncodings.size() - 1; i >= 1; i--)
                scaleValues.append(i * 2);
            scaleValues.append(1);
        }
        for (unsigned i = 0; i < sendEncodings.size(); i++) {
            auto& encoding = sendEncodings[i];
            if (allRids.contains(encoding.rid))
                return Exception { ExceptionCode::TypeError, makeString("Duplicate rid:"_s, encoding.rid) };
            allRids.append(encoding.rid);

            auto encodingCopy = encoding;
            if (i < scaleValues.size())
                encodingCopy.scaleResolutionDownBy = scaleValues[i];
            auto encodingData = fromRTCEncodingParameters(encodingCopy, kind);
            if (encodingData.hasException())
                return encodingData.releaseException();

            GValue value = G_VALUE_INIT;
            g_value_init(&value, GST_TYPE_STRUCTURE);
            gst_value_set_structure(&value, encodingData.returnValue().get());
            gst_value_list_append_and_take_value(&encodingsValue, &value);
        }
        if (allRids.isEmpty() && sendEncodings.size() > 1)
            return Exception { ExceptionCode::TypeError, "Missing rid"_s };
        if (allRids.size() > 1 && anyOf(allRids, [](auto& rid) { return rid.isNull() || rid.isEmpty(); }))
            return Exception { ExceptionCode::TypeError, "Empty rid"_s };
        if (allRids.size() == 1 && allRids[0] == emptyString())
            return Exception { ExceptionCode::TypeError, "Empty rid"_s };
    } else {
        String fallbackCodec = emptyString();
        for (auto& codec : codecs) {
            if (codec.mimeType == "video/H264"_s || codec.mimeType == "video/VP8"_s) {
                fallbackCodec = codec.mimeType.substring(6);
                break;
            }
        }

        GST_DEBUG_OBJECT(m_pipeline.get(), "Fallback codec: %s", fallbackCodec.ascii().data());
        if (!fallbackCodec.isEmpty()) {
            GUniquePtr<GstStructure> encodingData(gst_structure_new("encoding-parameters", "encoding-name", G_TYPE_STRING, fallbackCodec.ascii().data(), "payload", G_TYPE_INT, 97, "active", G_TYPE_BOOLEAN, TRUE, nullptr));
            GValue value = G_VALUE_INIT;
            g_value_init(&value, GST_TYPE_STRUCTURE);
            gst_value_set_structure(&value, encodingData.get());
            gst_value_list_append_and_take_value(&encodingsValue, &value);
        } else
            WTFLogAlways("Missing video encoder / RTP payloader. Please install an H.264 encoder and/or a VP8 encoder");
    }

    gst_structure_set_value(initData.get(), "encodings", &encodingsValue);

    auto transactionId = createVersion4UUIDString();
    gst_structure_set(initData.get(), "transaction-id", G_TYPE_STRING, transactionId.ascii().data(), nullptr);

    GRefPtr<GstWebRTCRTPTransceiver> rtcTransceiver;
    g_signal_emit_by_name(m_webrtcBin.get(), "add-transceiver", direction, caps.get(), &rtcTransceiver.outPtr());
    if (!rtcTransceiver)
        return Exception { ExceptionCode::InvalidAccessError, "Unable to add transceiver"_s };

    GUniquePtr<GstStructure> parameters(gst_structure_copy(initData.get()));
    switchOn(source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        configureSource(source, WTFMove(parameters));
        m_unlinkedOutgoingSources.append(source.ptr());
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        configureSource(source, WTFMove(parameters));
        m_unlinkedOutgoingSources.append(source.ptr());
    }, [](std::nullptr_t&) {
    });

    auto transceiver = makeUnique<GStreamerRtpTransceiverBackend>(WTFMove(rtcTransceiver));

    return GStreamerMediaEndpoint::Backends { transceiver->createSenderBackend(m_peerConnectionBackend, WTFMove(source), WTFMove(initData)), transceiver->createReceiverBackend(), WTFMove(transceiver) };
}

ExceptionOr<GStreamerMediaEndpoint::Backends> GStreamerMediaEndpoint::addTransceiver(const String& trackKind, const RTCRtpTransceiverInit& init, PeerConnectionBackend::IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Creating transceiver for %s track kind", trackKind.ascii().data());
    GStreamerRtpSenderBackend::Source source = nullptr;
    if (init.direction == RTCRtpTransceiverDirection::Sendonly || init.direction == RTCRtpTransceiverDirection::Sendrecv) {
        // A muted source is associated to the transceiver, this is a webrtcbin limitation, if a
        // transceiver is created without track and Offer/Answer is attempted, no corresponding m=
        // section is added to the SDP.
        if (trackKind == "audio"_s)
            source = RealtimeOutgoingAudioSourceGStreamer::createMuted(m_ssrcGenerator);
        else
            source = RealtimeOutgoingVideoSourceGStreamer::createMuted(m_ssrcGenerator);
    }
    return createTransceiverBackends(trackKind, init, WTFMove(source), ignoreNegotiationNeededFlag);
}

GStreamerRtpSenderBackend::Source GStreamerMediaEndpoint::createSourceForTrack(MediaStreamTrack& track)
{
    if (track.privateTrack().isAudio())
        return RealtimeOutgoingAudioSourceGStreamer::create(m_ssrcGenerator, track.mediaStreamId(), track);
    return RealtimeOutgoingVideoSourceGStreamer::create(m_ssrcGenerator, track.mediaStreamId(), track);
}

ExceptionOr<GStreamerMediaEndpoint::Backends> GStreamerMediaEndpoint::addTransceiver(MediaStreamTrack& track, const RTCRtpTransceiverInit& init, PeerConnectionBackend::IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Creating transceiver associated with %s track %s", track.kind().string().ascii().data(), track.id().ascii().data());
    return createTransceiverBackends(track.kind(), init, createSourceForTrack(track), ignoreNegotiationNeededFlag);
}

std::unique_ptr<GStreamerRtpTransceiverBackend> GStreamerMediaEndpoint::transceiverBackendFromSender(GStreamerRtpSenderBackend& backend)
{
    GRefPtr<GArray> transceivers;
    g_signal_emit_by_name(m_webrtcBin.get(), "get-transceivers", &transceivers.outPtr());

    GST_DEBUG_OBJECT(m_pipeline.get(), "Looking for sender %p in %u existing transceivers", backend.rtcSender(), transceivers->len);
    for (unsigned transceiverIndex = 0; transceiverIndex < transceivers->len; transceiverIndex++) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
        GstWebRTCRTPTransceiver* current = g_array_index(transceivers.get(), GstWebRTCRTPTransceiver*, transceiverIndex);
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        GRefPtr<GstWebRTCRTPSender> sender;
        g_object_get(current, "sender", &sender.outPtr(), nullptr);

        if (!sender)
            continue;
        if (sender.get() == backend.rtcSender())
            return WTF::makeUnique<GStreamerRtpTransceiverBackend>(current);
    }

    return nullptr;
}

struct AddIceCandidateCallData {
    GRefPtr<GstElement> webrtcBin;
    PeerConnectionBackend::AddIceCandidateCallback callback;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(AddIceCandidateCallData)

void GStreamerMediaEndpoint::addIceCandidate(GStreamerIceCandidate& candidate, PeerConnectionBackend::AddIceCandidateCallback&& callback)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Adding ICE candidate %s", candidate.candidate.utf8().data());

    if (!candidate.candidate.startsWith("candidate:"_s)) {
        callOnMainThread([task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(callback))]() mutable {
            task->run(Exception { ExceptionCode::OperationError, "Expect line: candidate:<candidate-str>"_s });
        });
        return;
    }

    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/3960
    if (webkitGstCheckVersion(1, 24, 0)) {
        auto* data = createAddIceCandidateCallData();
        data->webrtcBin = m_webrtcBin;
        data->callback = WTFMove(callback);
        g_signal_emit_by_name(m_webrtcBin.get(), "add-ice-candidate-full", candidate.sdpMLineIndex, candidate.candidate.utf8().data(), gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) {
            auto* data = reinterpret_cast<AddIceCandidateCallData*>(userData);
            auto promise = adoptGRef(rawPromise);
            auto result = gst_promise_wait(promise.get());
            const auto* reply = gst_promise_get_reply(promise.get());
            if (result != GST_PROMISE_RESULT_REPLIED || (reply && gst_structure_has_field(reply, "error"))) {
                if (reply) {
                    GUniqueOutPtr<GError> error;
                    gst_structure_get(reply, "error", G_TYPE_ERROR, &error.outPtr(), nullptr);
                    GST_ERROR("Unable to add ICE candidate, error: %s", error->message);
                }
                callOnMainThread([task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(data->callback))]() mutable {
                    task->run(Exception { ExceptionCode::OperationError, "Error processing ICE candidate"_s });
                });
                return;
            }

            callOnMainThread([task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(data->callback)), descriptions = descriptionsFromWebRTCBin(data->webrtcBin.get())]() mutable {
                task->run(WTFMove(descriptions));
            });
        }, data, reinterpret_cast<GDestroyNotify>(destroyAddIceCandidateCallData)));
        return;
    }

    // Candidate parsing is still needed for old GStreamer versions.
    auto parsedCandidate = parseIceCandidateSDP(candidate.candidate);
    if (!parsedCandidate) {
        callOnMainThread([task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(callback))]() mutable {
            task->run(Exception { ExceptionCode::OperationError, "Error processing ICE candidate"_s });
        });
        return;
    }

    // This is racy but nothing we can do about it when we are on older GStreamer runtimes.
    g_signal_emit_by_name(m_webrtcBin.get(), "add-ice-candidate", candidate.sdpMLineIndex, candidate.candidate.utf8().data());
    callOnMainThread([task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(callback)), descriptions = descriptionsFromWebRTCBin(m_webrtcBin.get())]() mutable {
        task->run(WTFMove(descriptions));
    });
}

std::unique_ptr<RTCDataChannelHandler> GStreamerMediaEndpoint::createDataChannel(const String& label, const RTCDataChannelInit& options)
{
    if (!m_webrtcBin)
        return nullptr;

    auto init = GStreamerDataChannelHandler::fromRTCDataChannelInit(options);
    GST_DEBUG_OBJECT(m_pipeline.get(), "Creating data channel for init options %" GST_PTR_FORMAT, init.get());
    GRefPtr<GstWebRTCDataChannel> channel;
    g_signal_emit_by_name(m_webrtcBin.get(), "create-data-channel", label.utf8().data(), init.get(), &channel.outPtr());
    if (!channel)
        return nullptr;

    onNegotiationNeeded();
    return WTF::makeUnique<GStreamerDataChannelHandler>(WTFMove(channel));
}

void GStreamerMediaEndpoint::prepareDataChannel(GstWebRTCDataChannel* dataChannel, gboolean isLocal)
{
    if (isLocal || isStopped())
        return;

    GRefPtr<GstWebRTCDataChannel> channel = dataChannel;
    GST_DEBUG_OBJECT(m_pipeline.get(), "Setting up data channel %p", channel.get());
    auto channelHandler = makeUniqueRef<GStreamerDataChannelHandler>(WTFMove(channel));
    auto identifier = ObjectIdentifier<GstWebRTCDataChannel>(reinterpret_cast<uintptr_t>(channelHandler->channel()));
    m_incomingDataChannels.add(identifier, WTFMove(channelHandler));
}

UniqueRef<GStreamerDataChannelHandler> GStreamerMediaEndpoint::findOrCreateIncomingChannelHandler(GRefPtr<GstWebRTCDataChannel>&& dataChannel)
{
    if (!webkitGstCheckVersion(1, 22, 0))
        return makeUniqueRef<GStreamerDataChannelHandler>(WTFMove(dataChannel));

    auto identifier = ObjectIdentifier<GstWebRTCDataChannel>(reinterpret_cast<uintptr_t>(dataChannel.get()));
    auto channelHandler = m_incomingDataChannels.take(identifier);
    RELEASE_ASSERT(channelHandler);
    return makeUniqueRefFromNonNullUniquePtr(WTFMove(channelHandler));
}

void GStreamerMediaEndpoint::onDataChannel(GstWebRTCDataChannel* dataChannel)
{
    GRefPtr<GstWebRTCDataChannel> channel = dataChannel;
    callOnMainThread([protectedThis = Ref(*this), this, dataChannel = WTFMove(channel)]() mutable {
        if (isStopped())
            return;

        GST_DEBUG_OBJECT(m_pipeline.get(), "Incoming data channel %p", dataChannel.get());
        auto channelHandler = findOrCreateIncomingChannelHandler(WTFMove(dataChannel));
        auto label = channelHandler->label();
        auto dataChannelInit = channelHandler->dataChannelInit();
        m_peerConnectionBackend.newDataChannel(WTFMove(channelHandler), WTFMove(label), WTFMove(dataChannelInit));
    });
}

GstElement* GStreamerMediaEndpoint::requestAuxiliarySender()
{
    // Don't use makeGStreamerElement() here because it would be called mutiple times and emit an
    // error every single time if the element is not found.
    auto* estimator = gst_element_factory_make("rtpgccbwe", nullptr);
    if (!estimator) {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [] {
            WTFLogAlways("gst-plugins-rs is not installed, RTP bandwidth estimation now disabled");
        });
        return nullptr;
    }

    g_signal_connect(estimator, "notify::estimated-bitrate", G_CALLBACK(+[](GstElement* estimator, GParamSpec*, GStreamerMediaEndpoint* endPoint) {
        uint32_t estimatedBitrate;
        g_object_get(estimator, "estimated-bitrate", &estimatedBitrate, nullptr);
        gst_element_send_event(endPoint->pipeline(), gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, gst_structure_new("encoder-bitrate-change-request", "bitrate", G_TYPE_UINT, static_cast<uint32_t>(estimatedBitrate / 1000), nullptr)));
    }), this);

    return estimator;
}

void GStreamerMediaEndpoint::close()
{
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/2760
    GST_DEBUG_OBJECT(m_pipeline.get(), "Closing");
    if (m_pipeline && GST_STATE(m_pipeline.get()) > GST_STATE_READY)
        gst_element_set_state(m_pipeline.get(), GST_STATE_READY);

#if !RELEASE_LOG_DISABLED
    stopLoggingStats();
#endif

    if (m_pipeline)
        teardownPipeline();
}

void GStreamerMediaEndpoint::stop()
{
#if !RELEASE_LOG_DISABLED
    stopLoggingStats();
#endif

    if (isStopped())
        return;

    GST_DEBUG_OBJECT(m_pipeline.get(), "Stopping");
    teardownPipeline();
}

void GStreamerMediaEndpoint::suspend()
{
    if (isStopped())
        return;

    GST_DEBUG_OBJECT(m_pipeline.get(), "Suspending");
    notImplemented();
}

void GStreamerMediaEndpoint::resume()
{
    if (isStopped())
        return;

    GST_DEBUG_OBJECT(m_pipeline.get(), "Resuming");
    notImplemented();
}

void GStreamerMediaEndpoint::onNegotiationNeeded()
{
    if (m_shouldIgnoreNegotiationNeededSignal) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Ignoring negotiation-needed signal");
        return;
    }

    if (GST_STATE(m_webrtcBin.get()) < GST_STATE_READY) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Deferring negotiation-needed until webrtc is ready");
        return;
    }

    if (m_peerConnectionBackend.isReconfiguring()) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "replaceTrack in progress, ignoring negotiation-needed signal");
        return;
    }

    GST_DEBUG_OBJECT(m_pipeline.get(), "Scheduling negotiation-needed");
    ++m_negotiationNeededEventId;
    callOnMainThread([protectedThis = Ref(*this), this] {
        if (isStopped())
            return;

        GST_DEBUG_OBJECT(m_pipeline.get(), "Negotiation needed!");
        m_peerConnectionBackend.markAsNeedingNegotiation(m_negotiationNeededEventId);
    });
}

void GStreamerMediaEndpoint::onIceConnectionChange()
{
    GstWebRTCICEConnectionState state;
    g_object_get(m_webrtcBin.get(), "ice-connection-state", &state, nullptr);
    callOnMainThread([protectedThis = Ref(*this), this, connectionState = toRTCIceConnectionState(state)] {
        if (isStopped())
            return;
        auto& connection = m_peerConnectionBackend.connection();
        if (connection.iceConnectionState() != connectionState)
            connection.updateIceConnectionState(connectionState);
    });
}

void GStreamerMediaEndpoint::onIceGatheringChange()
{
    GstWebRTCICEGatheringState state;
    g_object_get(m_webrtcBin.get(), "ice-gathering-state", &state, nullptr);
#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> desc(g_enum_to_string(GST_TYPE_WEBRTC_ICE_GATHERING_STATE, state));
    GST_DEBUG_OBJECT(m_pipeline.get(), "ICE gathering state changed to %s", desc.get());
#endif
    callOnMainThread([protectedThis = Ref(*this), this, state] {
        if (isStopped())
            return;
        m_peerConnectionBackend.iceGatheringStateChanged(toRTCIceGatheringState(state));
    });
}

void GStreamerMediaEndpoint::onIceCandidate(guint sdpMLineIndex, gchararray candidate)
{
    if (isStopped())
        return;

    String candidateString = span(candidate);

    // webrtcbin notifies an empty ICE candidate when gathering is complete.
    if (candidateString.isEmpty())
        return;

    callOnMainThread([protectedThis = Ref(*this), this, sdp = WTFMove(candidateString).isolatedCopy(), sdpMLineIndex]() mutable {
        if (isStopped())
            return;

        String mid;
        GUniqueOutPtr<GstWebRTCSessionDescription> description;
        g_object_get(m_webrtcBin.get(), "local-description", &description.outPtr(), nullptr);
        if (description && sdpMLineIndex < gst_sdp_message_medias_len(description->sdp)) {
            const auto media = gst_sdp_message_get_media(description->sdp, sdpMLineIndex);
            mid = span(gst_sdp_media_get_attribute_val(media, "mid"));
        }

        auto descriptions = descriptionsFromWebRTCBin(m_webrtcBin.get());
        GST_DEBUG_OBJECT(m_pipeline.get(), "Notifying ICE candidate: %s", sdp.ascii().data());
        m_peerConnectionBackend.newICECandidate(WTFMove(sdp), WTFMove(mid), sdpMLineIndex, { }, WTFMove(descriptions));
    });
}

void GStreamerMediaEndpoint::createSessionDescriptionSucceeded(GUniquePtr<GstWebRTCSessionDescription>&& description)
{
    callOnMainThread([protectedThis = Ref(*this), this, description = WTFMove(description)] {
        if (isStopped())
            return;

        GUniquePtr<char> sdp(gst_sdp_message_as_text(description->sdp));
        auto sdpString = String::fromUTF8(sdp.get());
        if (description->type == GST_WEBRTC_SDP_TYPE_OFFER) {
            m_peerConnectionBackend.createOfferSucceeded(WTFMove(sdpString));
            return;
        }

        if (description->type == GST_WEBRTC_SDP_TYPE_ANSWER) {
            m_peerConnectionBackend.createAnswerSucceeded(WTFMove(sdpString));
            return;
        }

        GST_WARNING_OBJECT(m_pipeline.get(), "Unsupported SDP type: %s", gst_webrtc_sdp_type_to_string(description->type));
    });
}

void GStreamerMediaEndpoint::createSessionDescriptionFailed(RTCSdpType sdpType, GUniquePtr<GError>&& error)
{
    callOnMainThread([protectedThis = Ref(*this), this, sdpType, error = WTFMove(error)] {
        if (isStopped())
            return;

        auto exc = Exception { ExceptionCode::OperationError, error ? String::fromUTF8(error->message) : "Unknown Error"_s };
        if (sdpType == RTCSdpType::Offer) {
            m_peerConnectionBackend.createOfferFailed(WTFMove(exc));
            return;
        }
        m_peerConnectionBackend.createAnswerFailed(WTFMove(exc));
    });
}

void GStreamerMediaEndpoint::collectTransceivers()
{
    GArray* transceivers;
    GST_DEBUG_OBJECT(m_pipeline.get(), "Collecting transceivers");
    g_signal_emit_by_name(m_webrtcBin.get(), "get-transceivers", &transceivers);

    auto scopeExit = makeScopeExit([&] {
        // Don't free segments because they're moved to local GRefPtrs.
        g_array_free(transceivers, FALSE);
    });

    GUniqueOutPtr<GstWebRTCSessionDescription> description;
    g_object_get(m_webrtcBin.get(), "remote-description", &description.outPtr(), nullptr);
    if (!description)
        return;

    for (unsigned i = 0; i < transceivers->len; i++) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
        auto current = adoptGRef(g_array_index(transceivers, GstWebRTCRTPTransceiver*, i));
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        auto* existingTransceiver = m_peerConnectionBackend.existingTransceiver([&](auto& transceiverBackend) {
            return current == transceiverBackend.rtcTransceiver();
        });
        if (existingTransceiver)
            continue;

        GUniqueOutPtr<char> mid;
        unsigned mLineIndex;
        g_object_get(current.get(), "mid", &mid.outPtr(), "mlineindex", &mLineIndex, nullptr);
        if (!mid)
            continue;

        const auto* media = gst_sdp_message_get_media(description->sdp, mLineIndex);
        if (UNLIKELY(!media)) {
            GST_WARNING_OBJECT(m_pipeline.get(), "SDP media for transceiver %u not found, skipping registration", mLineIndex);
            continue;
        }

        m_peerConnectionBackend.newRemoteTransceiver(WTF::makeUnique<GStreamerRtpTransceiverBackend>(WTFMove(current)), m_mediaForMid.get(String::fromUTF8(mid.get())), trackIdFromSDPMedia(*media));
    }
}

GUniquePtr<GstStructure> GStreamerMediaEndpoint::preprocessStats(const GRefPtr<GstPad>& pad, const GstStructure* stats)
{
    ASSERT(isMainThread());
    GUniquePtr<GstStructure> additionalStats(gst_structure_new_empty("stats"));
    auto mergeStructureInAdditionalStats = [&](const GstStructure* stats) {
        gstStructureForeach(stats, [&](auto id, const GValue* value) -> bool {
            gstStructureIdSetValue(additionalStats.get(), id, value);
            return TRUE;
        });
    };
    if (!pad) {
        for (auto& sender : m_peerConnectionBackend.connection().getSenders()) {
            auto& backend = m_peerConnectionBackend.backendFromRTPSender(sender);
            GUniquePtr<GstStructure> stats;
            if (auto* videoSource = backend.videoSource())
                stats = videoSource->stats();
            else if (auto audioSource = backend.audioSource())
                stats = audioSource->stats();

            if (!stats)
                continue;

            mergeStructureInAdditionalStats(stats.get());
        }
        for (auto& receiver : m_peerConnectionBackend.connection().getReceivers()) {
            auto& track = receiver.get().track();
            if (!is<RealtimeIncomingVideoSourceGStreamer>(track.source()))
                continue;

            auto& source = static_cast<RealtimeIncomingVideoSourceGStreamer&>(track.source());
            const auto* stats = source.stats();
            if (!stats)
                continue;

            mergeStructureInAdditionalStats(stats);
        }
    }

    for (auto& processor : m_trackProcessors.values()) {
        if (pad && pad != processor->pad())
            continue;

        const auto stats = processor->stats();
        if (!stats)
            continue;

        mergeStructureInAdditionalStats(stats);
    }

    GUniquePtr<GstStructure> result(gst_structure_copy(stats));
    gstStructureMapInPlace(result.get(), [&](auto, auto value) -> bool {
        if (!GST_VALUE_HOLDS_STRUCTURE(value))
            return TRUE;

        GUniquePtr<GstStructure> structure(gst_structure_copy(gst_value_get_structure(value)));
        GstWebRTCStatsType statsType;
        if (!gst_structure_get(structure.get(), "type", GST_TYPE_WEBRTC_STATS_TYPE, &statsType, nullptr))
            return TRUE;

        switch (statsType) {
        case GST_WEBRTC_STATS_INBOUND_RTP: {
            if (auto framesDecoded = gstStructureGet<uint64_t>(additionalStats.get(), "frames-decoded"_s))
                gst_structure_set(structure.get(), "frames-decoded", G_TYPE_UINT64, *framesDecoded, nullptr);
            if (auto framesDropped = gstStructureGet<uint64_t>(additionalStats.get(), "frames-dropped"_s))
                gst_structure_set(structure.get(), "frames-dropped", G_TYPE_UINT64, *framesDropped, nullptr);
            if (auto frameWidth = gstStructureGet<unsigned>(additionalStats.get(), "frame-width"_s))
                gst_structure_set(structure.get(), "frame-width", G_TYPE_UINT, *frameWidth, nullptr);
            if (auto frameHeight = gstStructureGet<unsigned>(additionalStats.get(), "frame-height"_s))
                gst_structure_set(structure.get(), "frame-height", G_TYPE_UINT, *frameHeight, nullptr);
            auto trackIdentifier = gstStructureGetString(additionalStats.get(), "track-identifier"_s);
            if (!trackIdentifier.isEmpty())
                gst_structure_set(structure.get(), "track-identifier", G_TYPE_STRING, trackIdentifier.toStringWithoutCopying().utf8().data(), nullptr);
            break;
        }
        case GST_WEBRTC_STATS_OUTBOUND_RTP: {
            // FIXME: This likely not correct, in simulcast case webrtcbin generates a single
            // outbound stat instead of one per simulcast layer.
            auto ssrc = gstStructureGet<unsigned>(structure.get(), "ssrc"_s);
            if (!ssrc) {
                GST_WARNING_OBJECT(pipeline(), "Missing SSRC in outbound stats %" GST_PTR_FORMAT, structure.get());
                break;
            }

            auto ssrcString = makeString(*ssrc);
            GUniqueOutPtr<GstStructure> ssrcStats;
            gst_structure_get(additionalStats.get(), ssrcString.ascii().data(), GST_TYPE_STRUCTURE, &ssrcStats.outPtr(), nullptr);
            if (!ssrcStats) {
                GST_WARNING_OBJECT(pipeline(), "Missing SSRC %s in additional outbound stats %" GST_PTR_FORMAT, ssrcString.ascii().data(), additionalStats.get());
                break;
            }

            if (auto framesSent = gstStructureGet<uint64_t>(ssrcStats.get(), "frames-sent"_s))
                gst_structure_set(structure.get(), "frames-sent", G_TYPE_UINT64, *framesSent, nullptr);
            if (auto framesEncoded = gstStructureGet<uint64_t>(ssrcStats.get(), "frames-encoded"_s))
                gst_structure_set(structure.get(), "frames-encoded", G_TYPE_UINT64, *framesEncoded, nullptr);
            if (auto targetBitrate = gstStructureGet<double>(ssrcStats.get(), "bitrate"_s))
                gst_structure_set(structure.get(), "target-bitrate", G_TYPE_DOUBLE, *targetBitrate, nullptr);

            if (auto frameWidth = gstStructureGet<unsigned>(ssrcStats.get(), "frame-width"_s))
                gst_structure_set(structure.get(), "frame-width", G_TYPE_UINT, *frameWidth, nullptr);
            if (auto frameHeight = gstStructureGet<unsigned>(ssrcStats.get(), "frame-height"_s))
                gst_structure_set(structure.get(), "frame-height", G_TYPE_UINT, *frameHeight, nullptr);
            if (auto framesPerSecond = gstStructureGet<double>(ssrcStats.get(), "frames-per-second"_s))
                gst_structure_set(structure.get(), "frames-per-second", G_TYPE_DOUBLE, *framesPerSecond, nullptr);

            if (auto midValue = gstStructureGetString(ssrcStats.get(), "mid"_s))
                gst_structure_set(structure.get(), "mid", G_TYPE_STRING, midValue.toString().ascii().data(), nullptr);
            if (auto ridValue = gstStructureGetString(ssrcStats.get(), "rid"_s))
                gst_structure_set(structure.get(), "rid", G_TYPE_STRING, ridValue.toString().ascii().data(), nullptr);
            break;
        }
        default:
            break;
        };

        auto timestamp = gstStructureGet<double>(structure.get(), "timestamp"_s);
        if (LIKELY(timestamp)) {
            auto newTimestamp = StatsTimestampConverter::singleton().convertFromMonotonicTime(Seconds::fromMilliseconds(*timestamp));
            gst_structure_set(structure.get(), "timestamp", G_TYPE_DOUBLE, newTimestamp.microseconds(), nullptr);
        }

        gst_value_set_structure(value, structure.get());
        return TRUE;
    });

    return result;
}

#if !RELEASE_LOG_DISABLED
void GStreamerMediaEndpoint::gatherStatsForLogging()
{
    g_signal_emit_by_name(m_webrtcBin.get(), "get-stats", nullptr, gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) {
        auto promise = adoptGRef(rawPromise);
        auto result = gst_promise_wait(promise.get());
        if (result != GST_PROMISE_RESULT_REPLIED)
            return;

        const auto* reply = gst_promise_get_reply(promise.get());
        ASSERT(reply);
        if (gst_structure_has_field(reply, "error"))
            return;

        auto weakSelf = static_cast<ThreadSafeWeakPtr<GStreamerMediaEndpoint>*>(userData);
        callOnMainThreadAndWait([weakSelf, reply] {
            auto self = weakSelf->get();
            if (!self)
                return;
            auto stats = self->preprocessStats(nullptr, reply);
            self->onStatsDelivered(stats.get());
        });
    }, new ThreadSafeWeakPtr<GStreamerMediaEndpoint> { *this }, reinterpret_cast<GDestroyNotify>(+[](gpointer data) {
        delete static_cast<ThreadSafeWeakPtr<GStreamerMediaEndpoint>*>(data);
    })));
}

class RTCStatsLogger {
public:
    explicit RTCStatsLogger(const GstStructure* stats)
        : m_stats(stats)
    { }

    String toJSONString() const { return gstStructureToJSONString(m_stats); }

private:
    const GstStructure* m_stats;
};

void GStreamerMediaEndpoint::processStatsItem(const GValue* value)
{
    if (!GST_VALUE_HOLDS_STRUCTURE(value))
        return;

    const GstStructure* structure = gst_value_get_structure(value);
    GstWebRTCStatsType statsType;
    if (!gst_structure_get(structure, "type", GST_TYPE_WEBRTC_STATS_TYPE, &statsType, nullptr))
        return;

    // Just check a single timestamp, inbound RTP for instance.
    if (!m_statsFirstDeliveredTimestamp && statsType == GST_WEBRTC_STATS_INBOUND_RTP) {
        if (auto timestamp = gstStructureGet<double>(structure, "timestamp"_s)) {
            auto ts = Seconds::fromMilliseconds(*timestamp);
            m_statsFirstDeliveredTimestamp = ts;

            if (!isStopped() && m_statsLogTimer.repeatInterval() != statsLogInterval(ts)) {
                m_statsLogTimer.stop();
                m_statsLogTimer.startRepeating(statsLogInterval(ts));
            }
        }
    }

    RTCStatsLogger statsLogger { structure };

    if (m_peerConnectionBackend.isJSONLogStreamingEnabled()) {
        auto event = m_peerConnectionBackend.generateJSONLogEvent(gstStructureToJSONString(structure), false);
        m_peerConnectionBackend.emitJSONLogEvent(WTFMove(event));
    }

    if (m_isGatheringRTCLogs) {
        auto event = m_peerConnectionBackend.generateJSONLogEvent(gstStructureToJSONString(structure), true);
        m_peerConnectionBackend.provideStatLogs(WTFMove(event));
    }

    if (logger().willLog(logChannel(), WTFLogLevel::Debug)) {
        // Stats are very verbose, let's only display them in inspector console in verbose mode.
        logger().debug(LogWebRTC, Logger::LogSiteIdentifier("GStreamerMediaEndpoint"_s, "OnStatsDelivered"_s, logIdentifier()), statsLogger);
    } else
        logger().logAlways(LogWebRTCStats, Logger::LogSiteIdentifier("GStreamerMediaEndpoint"_s, "OnStatsDelivered"_s, logIdentifier()), statsLogger);
}

void GStreamerMediaEndpoint::onStatsDelivered(const GstStructure* stats)
{
    gstStructureForeach(stats, [&](auto, const auto value) -> bool {
        processStatsItem(value);
        return true;
    });
}

void GStreamerMediaEndpoint::startLoggingStats()
{
    if (m_statsLogTimer.isActive())
        m_statsLogTimer.stop();
    m_statsLogTimer.startRepeating(statsLogInterval(Seconds::nan()));
}

void GStreamerMediaEndpoint::stopLoggingStats()
{
    m_statsLogTimer.stop();
}

WTFLogChannel& GStreamerMediaEndpoint::logChannel() const
{
    return LogWebRTC;
}

Seconds GStreamerMediaEndpoint::statsLogInterval(Seconds reportTimestamp) const
{
    if (m_isGatheringRTCLogs)
        return 1_s;

    if (logger().willLog(logChannel(), WTFLogLevel::Info))
        return 2_s;

    if (reportTimestamp - m_statsFirstDeliveredTimestamp > 15_s)
        return 10_s;

    return 4_s;
}
#endif

void GStreamerMediaEndpoint::gatherDecoderImplementationName(Function<void(String&&)>&& callback)
{
    // TODO: collect stats and lookup InboundRtp "decoder_implementation" field.
    callback({ });
}

std::optional<bool> GStreamerMediaEndpoint::canTrickleIceCandidates() const
{
    if (!m_webrtcBin)
        return std::nullopt;

    GUniqueOutPtr<GstWebRTCSessionDescription> description;
    g_object_get(m_webrtcBin.get(), "pending-remote-description", &description.outPtr(), nullptr);
    if (!description)
        return std::nullopt;

    for (unsigned i = 0; i < gst_sdp_message_attributes_len(description->sdp); i++) {
        const auto attribute = gst_sdp_message_get_attribute(description->sdp, i);
        if (g_strcmp0(attribute->key, "ice-options"))
            continue;

        auto values = makeString(span(attribute->value)).split(' ');
        if (values.contains("trickle"_s))
            return true;
    }
    return false;
}

void GStreamerMediaEndpoint::startRTCLogs()
{
    m_isGatheringRTCLogs = true;
#if !RELEASE_LOG_DISABLED
    startLoggingStats();
#endif
}

void GStreamerMediaEndpoint::stopRTCLogs()
{
    m_isGatheringRTCLogs = false;
}

} // namespace WebCore

#if !RELEASE_LOG_DISABLED
namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::RTCStatsLogger> {
    static String toString(const WebCore::RTCStatsLogger& logger)
    {
        return String(logger.toJSONString());
    }
};

}; // namespace WTF
#endif // !RELEASE_LOG_DISABLED

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC)
