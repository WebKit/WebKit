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
#include <wtf/Scope.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/WTFGType.h>
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
{
    ensureGStreamerInitialized();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_endpoint_debug, "webkitwebrtcendpoint", 0, "WebKit WebRTC end-point");
    });
}

bool GStreamerMediaEndpoint::initializePipeline()
{
    static uint32_t nPipeline = 0;
    auto pipelineName = makeString("webkit-webrt-pipeline-", nPipeline);
    m_pipeline = gst_pipeline_new(pipelineName.ascii().data());

    connectSimpleBusMessageCallback(m_pipeline.get(), [this](GstMessage* message) {
        handleMessage(message);
    });

    auto binName = makeString("webkit-webrtcbin-", nPipeline++);
    m_webrtcBin = makeGStreamerElement("webrtcbin", binName.ascii().data());
    if (!m_webrtcBin)
        return false;

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

        callOnMainThreadAndWait([protectedThis = Ref(*endPoint), pad] {
            if (protectedThis->isStopped())
                return;

            protectedThis->addRemoteStream(pad);
        });
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

    g_signal_connect_swapped(m_webrtcBin.get(), "on-data-channel", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint, GstWebRTCDataChannel* channel) {
        endPoint->onDataChannel(channel);
    }), this);

    gst_bin_add(GST_BIN_CAST(m_pipeline.get()), m_webrtcBin.get());
    return true;
}

GStreamerMediaEndpoint::~GStreamerMediaEndpoint()
{
    if (m_pipeline)
        teardownPipeline();
}

void GStreamerMediaEndpoint::teardownPipeline()
{
    ASSERT(m_pipeline);
#if !RELEASE_LOG_DISABLED
    stopLoggingStats();
#endif
    m_statsCollector->setElement(nullptr);

    g_signal_handlers_disconnect_by_data(m_webrtcBin.get(), this);
    disconnectSimpleBusMessageCallback(m_pipeline.get());
    gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);

    m_sources.clear();
    m_remoteMLineInfos.clear();
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
        if (!g_strcmp0(gst_structure_get_name(data), "GstBinForwarded")) {
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
    if (m_pipeline)
        teardownPipeline();

    if (!initializePipeline())
        return false;

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
                validURL.setUser(server.username);
                validURL.setPassword(server.credential);
                g_object_set(m_webrtcBin.get(), "stun-server", validURL.string().utf8().data(), nullptr);
                stunSet = true;
            }
        }
    }

    // WIP: https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/merge_requests/302
    GST_FIXME("%zu custom certificates not propagated to webrtcbin", configuration.certificates.size());

    gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
    GST_DEBUG_OBJECT(m_pipeline.get(), "End-point ready");
    return true;
}

void GStreamerMediaEndpoint::restartIce()
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "restarting ICE");
    // WIP in https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/merge_requests/1877
    initiate(true, gst_structure_new("webrtcbin-offer-options", "ice-restart", G_TYPE_BOOLEAN, TRUE, nullptr));
}

static std::optional<std::pair<RTCSdpType, String>> fetchDescription(GstElement* webrtcBin, const char* name)
{
    if (!webrtcBin)
        return { };

    GUniqueOutPtr<GstWebRTCSessionDescription> description;
    g_object_get(webrtcBin, makeString(name, "-description").utf8().data(), &description.outPtr(), nullptr);
    if (!description)
        return { };

    GUniquePtr<char> sdpString(gst_sdp_message_as_text(description->sdp));
    GST_TRACE_OBJECT(webrtcBin, "%s-description SDP: %s", name, sdpString.get());
    return { { fromSessionDescriptionType(*description.get()), String::fromLatin1(sdpString.get()) } };
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

enum class GatherSignalingState { No, Yes };
static std::optional<PeerConnectionBackend::DescriptionStates> descriptionsFromWebRTCBin(GstElement* webrtcBin, GatherSignalingState gatherSignalingState = GatherSignalingState::No)
{
    std::optional<RTCSdpType> currentLocalDescriptionSdpType, pendingLocalDescriptionSdpType, currentRemoteDescriptionSdpType, pendingRemoteDescriptionSdpType;
    String currentLocalDescriptionSdp, pendingLocalDescriptionSdp, currentRemoteDescriptionSdp, pendingRemoteDescriptionSdp;
    if (auto currentLocalDescription = fetchDescription(webrtcBin, "current-local")) {
        auto [sdpType, description] = *currentLocalDescription;
        currentLocalDescriptionSdpType = sdpType;
        currentLocalDescriptionSdp = WTFMove(description);
    }
    if (auto pendingLocalDescription = fetchDescription(webrtcBin, "pending-local")) {
        auto [sdpType, description] = *pendingLocalDescription;
        pendingLocalDescriptionSdpType = sdpType;
        pendingLocalDescriptionSdp = WTFMove(description);
    }
    if (auto currentRemoteDescription = fetchDescription(webrtcBin, "current-remote")) {
        auto [sdpType, description] = *currentRemoteDescription;
        currentRemoteDescriptionSdpType = sdpType;
        currentRemoteDescriptionSdp = WTFMove(description);
    }
    if (auto pendingRemoteDescription = fetchDescription(webrtcBin, "pending-remote")) {
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

void GStreamerMediaEndpoint::doSetLocalDescription(const RTCSessionDescription* description)
{
    setDescription(description, true, [protectedThis = Ref(*this), this] {
        if (protectedThis->isStopped())
            return;
        auto descriptions = descriptionsFromWebRTCBin(m_webrtcBin.get(), GatherSignalingState::Yes);
        GRefPtr<GstWebRTCSCTPTransport> transport;
        g_object_get(m_webrtcBin.get(), "sctp-transport", &transport.outPtr(), nullptr);
        m_peerConnectionBackend.setLocalDescriptionSucceeded(WTFMove(descriptions), transport ? makeUnique<GStreamerSctpTransportBackend>(WTFMove(transport)) : nullptr);
    }, [protectedThis = Ref(*this), this](const GError* error) {
        if (protectedThis->isStopped())
            return;
        if (error && error->code == GST_WEBRTC_ERROR_INVALID_STATE)
            m_peerConnectionBackend.setLocalDescriptionFailed(Exception { InvalidStateError, "Failed to set local answer sdp: no pending remote description."_s });
        else
            m_peerConnectionBackend.setLocalDescriptionFailed(Exception { OperationError, "Unable to apply session local description"_s });
    });
}

void GStreamerMediaEndpoint::doSetRemoteDescription(const RTCSessionDescription& description)
{
    setDescription(&description, false, [protectedThis = Ref(*this), this] {
        if (protectedThis->isStopped())
            return;
        GST_DEBUG_OBJECT(m_pipeline.get(), "Acking remote description");
        auto descriptions = descriptionsFromWebRTCBin(m_webrtcBin.get(), GatherSignalingState::Yes);
        GRefPtr<GstWebRTCSCTPTransport> transport;
        g_object_get(m_webrtcBin.get(), "sctp-transport", &transport.outPtr(), nullptr);
        m_peerConnectionBackend.setRemoteDescriptionSucceeded(WTFMove(descriptions), transport ? makeUnique<GStreamerSctpTransportBackend>(WTFMove(transport)) : nullptr);
    }, [protectedThis = Ref(*this), this](const GError* error) {
        if (protectedThis->isStopped())
            return;
        if (error && error->code == GST_WEBRTC_ERROR_INVALID_STATE)
            m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { InvalidStateError, "Failed to set remote answer sdp"_s });
        else
            m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { OperationError, "Unable to apply session remote description"_s });
    });
#if !RELEASE_LOG_DISABLED
    startLoggingStats();
#endif
}

struct SetDescriptionCallData {
    Function<void()> successCallback;
    Function<void(const GError*)> failureCallback;
};

WEBKIT_DEFINE_ASYNC_DATA_STRUCT(SetDescriptionCallData)

void GStreamerMediaEndpoint::setDescription(const RTCSessionDescription* description, bool isLocal, Function<void()>&& successCallback, Function<void(const GError*)>&& failureCallback)
{
    GstSDPMessage* message;
    auto sdpType = RTCSdpType::Offer;

    if (description) {
        if (gst_sdp_message_new_from_text(reinterpret_cast<const char*>(description->sdp().characters8()), &message) != GST_SDP_OK) {
            failureCallback(nullptr);
            return;
        }
        sdpType = description->type();
    } else if (gst_sdp_message_new(&message) != GST_SDP_OK) {
        failureCallback(nullptr);
        return;
    }

    if (!isLocal)
        storeRemoteMLineInfo(message);

    auto type = toSessionDescriptionType(sdpType);
    GST_DEBUG_OBJECT(m_pipeline.get(), "Creating %s session for SDP %s", isLocal ? "local" : "remote", gst_webrtc_sdp_type_to_string(type));

    GUniquePtr<GstWebRTCSessionDescription> sessionDescription(gst_webrtc_session_description_new(type, message));
    auto signalName = makeString("set-", isLocal ? "local" : "remote", "-description");
    auto* data = createSetDescriptionCallData();
    data->successCallback = WTFMove(successCallback);
    data->failureCallback = WTFMove(failureCallback);
    g_signal_emit_by_name(m_webrtcBin.get(), signalName.ascii().data(), sessionDescription.get(), gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) {
        auto* data = static_cast<SetDescriptionCallData*>(userData);
        auto promise = adoptGRef(rawPromise);
        auto result = gst_promise_wait(promise.get());
        const auto* reply = gst_promise_get_reply(promise.get());
        if (result != GST_PROMISE_RESULT_REPLIED || (reply && gst_structure_has_field(reply, "error"))) {
            std::optional<GUniquePtr<GError>> errorHolder;
            if (reply) {
                GUniqueOutPtr<GError> error;
                gst_structure_get(reply, "error", G_TYPE_ERROR, &error.outPtr(), nullptr);
                GST_ERROR("Unable to set description, error: %s", error->message);
                errorHolder = GUniquePtr<GError>(error.release());
            }
            callOnMainThread([error = WTFMove(errorHolder), failureCallback = WTFMove(data->failureCallback)] {
                failureCallback(error ? error->get() : nullptr);
            });
            return;
        }
        callOnMainThread([successCallback = WTFMove(data->successCallback)] {
            successCallback();
        });
    }, data, reinterpret_cast<GDestroyNotify>(destroySetDescriptionCallData)));
}

void GStreamerMediaEndpoint::storeRemoteMLineInfo(GstSDPMessage* message)
{
    m_remoteMLineInfos.clear();
    unsigned totalMedias = gst_sdp_message_medias_len(message);
    m_remoteMLineInfos.reserveCapacity(totalMedias);
    GST_DEBUG_OBJECT(m_pipeline.get(), "Storing %u remote pending mlines", totalMedias);
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

        m_mediaForMid.set(String::fromLatin1(mid), g_str_equal(mediaType, "audio"_s) ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video);

        // https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/merge_requests/1907
        if (sdpMediaHasAttributeKey(media, "ice-lite")) {
            GRefPtr<GObject> ice;
            g_object_get(m_webrtcBin.get(), "ice-agent", &ice.outPtr(), nullptr);
            g_object_set(ice.get(), "ice-lite", TRUE, nullptr);
        }

        auto caps = adoptGRef(gst_caps_new_empty());
        Vector<int> payloadTypes;
        unsigned totalFormats = gst_sdp_media_formats_len(media);
        GST_DEBUG_OBJECT(m_pipeline.get(), "Media %s has %u formats", mediaType, totalFormats);
        for (unsigned formatIndex = 0; formatIndex < totalFormats; formatIndex++) {
            auto format = makeString(gst_sdp_media_get_format(media, formatIndex));
            auto payloadType = parseInteger<int>(format);
            if (!payloadType || !*payloadType) {
                GST_WARNING_OBJECT(m_pipeline.get(), "Invalid payload type: %s", format.utf8().data());
                continue;
            }
            auto formatCaps = adoptGRef(gst_sdp_media_get_caps_from_media(media, *payloadType));
            if (!formatCaps) {
                GST_WARNING_OBJECT(m_pipeline.get(), "No caps found for payload type %d", *payloadType);
                continue;
            }

            // Relay SDP attributes to the caps, this is specially useful so that elements in
            // webrtcbin will be able to enable RTP header extensions.
            gst_sdp_media_attributes_to_caps(media, formatCaps.get());

            gst_caps_append(caps.get(), formatCaps.leakRef());
            m_ptCounter = std::max(m_ptCounter, *payloadType + 1);
            payloadTypes.append(*payloadType);
        }

        unsigned totalCaps = gst_caps_get_size(caps.get());
        if (totalCaps) {
            for (unsigned capsIndex = 0; capsIndex < totalCaps; capsIndex++) {
                GstStructure* structure = gst_caps_get_structure(caps.get(), capsIndex);
                gst_structure_set_name(structure, "application/x-rtp");
            }
            GST_DEBUG_OBJECT(m_pipeline.get(), "Caching payload caps: %" GST_PTR_FORMAT, caps.get());
            m_remoteMLineInfos.uncheckedAppend({ WTFMove(caps), false, WTFMove(payloadTypes) });
        }
    }
}

void GStreamerMediaEndpoint::configureAndLinkSource(RealtimeOutgoingMediaSourceGStreamer& source)
{
    auto caps = source.allowedCaps();
    std::optional<std::pair<PendingMLineInfo, GRefPtr<GstCaps>>> found;
    for (auto& mLineInfo : m_remoteMLineInfos) {
        if (mLineInfo.isUsed)
            continue;

        unsigned totalCaps = gst_caps_get_size(mLineInfo.caps.get());
        for (unsigned capsIndex = 0; capsIndex < totalCaps; capsIndex++) {
            auto* structure = gst_caps_get_structure(mLineInfo.caps.get(), capsIndex);
            auto caps2 = adoptGRef(gst_caps_new_full(gst_structure_copy(structure), nullptr));
            auto intersected = adoptGRef(gst_caps_intersect(caps.get(), caps2.get()));
            if (!gst_caps_is_empty(intersected.get())) {
                found = std::make_pair(mLineInfo, WTFMove(intersected));
                break;
            }
        }
        if (found.has_value())
            break;
    }

    bool payloadTypeWasSet = false;
    if (found) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Unused and compatible caps found for %" GST_PTR_FORMAT "... %" GST_PTR_FORMAT, caps.get(), found->second.get());
        payloadTypeWasSet = source.setPayloadType(found->second);
        found->first.isUsed = true;
    } else
        payloadTypeWasSet = source.setPayloadType(caps);

    if (!payloadTypeWasSet)
        return;

    if (!source.pad()) {
        source.setSinkPad(requestPad(m_requestPadCounter, source.allowedCaps()));
        m_requestPadCounter++;
    }

    auto& sinkPad = source.pad();
    ASSERT(!gst_pad_is_linked(sinkPad.get()));
    if (gst_pad_is_linked(sinkPad.get())) {
        WTFLogAlways("RealtimeMediaSource already linked, this should not happen.");
        return;
    }

    auto sourceBin = source.bin();
    gst_bin_add(GST_BIN_CAST(m_pipeline.get()), sourceBin.get());
    source.link();

#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> padId(gst_pad_get_name(sinkPad.get()));
    auto dotFileName = makeString(GST_OBJECT_NAME(m_pipeline.get()), ".outgoing-", padId.get());
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
#endif
}

GRefPtr<GstPad> GStreamerMediaEndpoint::requestPad(unsigned mlineIndex, const GRefPtr<GstCaps>& allowedCaps)
{
    auto padId = makeString("sink_", mlineIndex);
    auto caps = adoptGRef(gst_caps_copy(allowedCaps.get()));

    gst_caps_map_in_place(caps.get(), [](GstCapsFeatures*, GstStructure* structure, gpointer userData) -> gboolean {
        auto* endPoint = reinterpret_cast<GStreamerMediaEndpoint*>(userData);
        gst_structure_set(structure, "payload", G_TYPE_INT, endPoint->m_ptCounter++, nullptr);
        return TRUE;
    }, this);

    // FIXME: Restricting caps and codec-preferences breaks caps negotiation of outgoing sources in
    // some case (jitsi).
    GST_DEBUG_OBJECT(m_pipeline.get(), "Requesting pad %s restricted to %" GST_PTR_FORMAT, padId.utf8().data(), caps.get());
    auto* padTemplate = gst_element_get_pad_template(m_webrtcBin.get(), "sink_%u");
    auto sinkPad = adoptGRef(gst_element_request_pad(m_webrtcBin.get(), padTemplate, padId.utf8().data(), caps.get()));
    GRefPtr<GstWebRTCRTPTransceiver> transceiver;
    g_object_get(sinkPad.get(), "transceiver", &transceiver.outPtr(), nullptr);
    g_object_set(transceiver.get(), "codec-preferences", caps.get(), nullptr);
    return sinkPad;
}

bool GStreamerMediaEndpoint::addTrack(GStreamerRtpSenderBackend& sender, MediaStreamTrack& track, const FixedVector<String>&)
{
    GStreamerRtpSenderBackend::Source source;
    GRefPtr<GstWebRTCRTPSender> rtcSender;

    if (track.privateTrack().isAudio()) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Adding outgoing audio source");
        auto audioSource = RealtimeOutgoingAudioSourceGStreamer::create(track.privateTrack());
        configureAndLinkSource(audioSource);

        rtcSender = audioSource->sender();
        source = WTFMove(audioSource);
    } else {
        ASSERT(track.privateTrack().isVideo());
        GST_DEBUG_OBJECT(m_pipeline.get(), "Adding outgoing video source");
        auto videoSource = RealtimeOutgoingVideoSourceGStreamer::create(track.privateTrack());
        configureAndLinkSource(videoSource);

        rtcSender = videoSource->sender();
        source = WTFMove(videoSource);
    }

    sender.setSource(WTFMove(source));

    if (sender.rtcSender()) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Already has a sender.");
        return true;
    }

    sender.setRTCSender(WTFMove(rtcSender));

    GST_DEBUG_OBJECT(m_pipeline.get(), "Sender configured");
    return true;
}

void GStreamerMediaEndpoint::removeTrack(GStreamerRtpSenderBackend& sender)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Removing track");
    if (auto bin = sender.stopSource())
        gst_bin_remove(GST_BIN_CAST(m_pipeline.get()), bin.get());
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
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(GStreamerMediaEndpointHolder)

void GStreamerMediaEndpoint::initiate(bool isInitiator, GstStructure* rawOptions)
{
    m_isInitiator = isInitiator;
    const char* type = isInitiator ? "offer" : "answer";
    GST_DEBUG_OBJECT(m_pipeline.get(), "Creating %s", type);
    auto signalName = makeString("create-", type);
    GUniquePtr<GstStructure> options(rawOptions);
    auto* holder = createGStreamerMediaEndpointHolder();
    holder->endPoint = this;
    g_signal_emit_by_name(m_webrtcBin.get(), signalName.ascii().data(), options.get(), gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) {
        auto* holder = static_cast<GStreamerMediaEndpointHolder*>(userData);
        auto promise = adoptGRef(rawPromise);
        auto result = gst_promise_wait(promise.get());
        if (result != GST_PROMISE_RESULT_REPLIED) {
            holder->endPoint->createSessionDescriptionFailed({ });
            return;
        }

        const auto* reply = gst_promise_get_reply(promise.get());
        ASSERT(reply);
        if (gst_structure_has_field(reply, "error")) {
            GUniqueOutPtr<GError> promiseError;
            gst_structure_get(reply, "error", G_TYPE_ERROR, &promiseError.outPtr(), nullptr);
            holder->endPoint->createSessionDescriptionFailed(GUniquePtr<GError>(promiseError.release()));
            return;
        }

        const char* type = holder->endPoint->m_isInitiator ? "offer" : "answer";
        GUniqueOutPtr<GstWebRTCSessionDescription> sessionDescription;
        gst_structure_get(reply, type, GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &sessionDescription.outPtr(), nullptr);

#ifndef GST_DISABLE_GST_DEBUG
        GUniquePtr<char> sdp(gst_sdp_message_as_text(sessionDescription->sdp));
        GST_DEBUG_OBJECT(holder->endPoint->pipeline(), "Created %s: %s", type, sdp.get());
#endif
        holder->endPoint->createSessionDescriptionSucceeded(GUniquePtr<GstWebRTCSessionDescription>(sessionDescription.release()));
    }, holder, reinterpret_cast<GDestroyNotify>(destroyGStreamerMediaEndpointHolder)));
}

void GStreamerMediaEndpoint::getStats(GstPad* pad, Ref<DeferredPromise>&& promise)
{
    m_statsCollector->getStats([promise = WTFMove(promise), protectedThis = Ref(*this)](auto&& report) mutable {
        ASSERT(isMainThread());
        if (protectedThis->isStopped() || !report)
            return;

        promise->resolve<IDLInterface<RTCStatsReport>>(report.releaseNonNull());
    }, pad);
}

MediaStream& GStreamerMediaEndpoint::mediaStreamFromRTCStream(String label)
{
    auto mediaStream = m_remoteStreamsById.ensure(label, [label, this]() mutable {
        auto& document = downcast<Document>(*m_peerConnectionBackend.connection().scriptExecutionContext());
        return MediaStream::create(document, MediaStreamPrivate::create(document.logger(), { }, WTFMove(label)));
    });
    return *mediaStream.iterator->value;
}

void GStreamerMediaEndpoint::addRemoteStream(GstPad* pad)
{
    m_pendingIncomingStreams++;

    auto caps = adoptGRef(gst_pad_get_current_caps(pad));
    if (!caps)
        caps = adoptGRef(gst_pad_query_caps(pad, nullptr));

    const char* mediaType = capsMediaType(caps.get());
    GST_DEBUG_OBJECT(m_pipeline.get(), "Adding remote %s stream to %" GST_PTR_FORMAT " %u pending incoming streams, caps: %" GST_PTR_FORMAT, mediaType, pad, m_pendingIncomingStreams, caps.get());

    GRefPtr<GstWebRTCRTPTransceiver> rtcTransceiver;
    g_object_get(pad, "transceiver", &rtcTransceiver.outPtr(), nullptr);

    auto* transceiver = m_peerConnectionBackend.existingTransceiver([&](auto& transceiverBackend) {
        return rtcTransceiver.get() == transceiverBackend.rtcTransceiver();
    });
    if (!transceiver) {
        auto type = doCapsHaveType(caps.get(), "audio") ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video;
        transceiver = &m_peerConnectionBackend.newRemoteTransceiver(makeUnique<GStreamerRtpTransceiverBackend>(WTFMove(rtcTransceiver)), type);
    }

    GUniqueOutPtr<GstWebRTCSessionDescription> description;
    g_object_get(m_webrtcBin.get(), "remote-description", &description.outPtr(), nullptr);

    unsigned mLineIndex;
    g_object_get(rtcTransceiver.get(), "mlineindex", &mLineIndex, nullptr);
    const auto* media = gst_sdp_message_get_media(description->sdp, mLineIndex);
    GUniquePtr<char> sdp(gst_sdp_media_as_text(media));
    auto sdpString = makeString(sdp.get());

    GUniquePtr<gchar> name(gst_pad_get_name(pad));
    auto label = String::fromLatin1(name.get());
    auto key = makeString("msid:");
    auto lines = sdpString.split('\n');
    for (auto& line : lines) {
        auto i = line.find(key);
        if (i != notFound) {
            auto tmp = line.substring(i + key.ascii().length());
            label = tmp.left(tmp.find(' '));
            break;
        }
    }

    GST_DEBUG_OBJECT(m_pipeline.get(), "msid: %s", label.ascii().data());

    GstElement* bin = nullptr;
    auto& track = transceiver->receiver().track();
    auto& source = track.privateTrack().source();
    if (source.isIncomingAudioSource())
        bin = static_cast<RealtimeIncomingAudioSourceGStreamer&>(source).bin();
    else if (source.isIncomingVideoSource())
        bin = static_cast<RealtimeIncomingVideoSourceGStreamer&>(source).bin();
    ASSERT(bin);

    gst_bin_add(GST_BIN_CAST(m_pipeline.get()), bin);
    auto sinkPad = adoptGRef(gst_element_get_static_pad(bin, "sink"));
    gst_pad_link(pad, sinkPad.get());
    gst_element_sync_state_with_parent(bin);

    track.setEnabled(true);
    source.setMuted(false);

    auto& mediaStream = mediaStreamFromRTCStream(label);
    mediaStream.addTrackFromPlatform(track);

    Vector<RefPtr<MediaStream>> pendingStreams;
    pendingStreams.reserveInitialCapacity(1);
    pendingStreams.uncheckedAppend(&mediaStream);

    m_peerConnectionBackend.addPendingTrackEvent({ Ref(transceiver->receiver()), Ref(transceiver->receiver().track()), WTFMove(pendingStreams), Ref(*transceiver) });

    if (m_pendingIncomingStreams == gst_sdp_message_medias_len(description->sdp)) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Incoming streams gathered, now firing track events");
        m_pendingIncomingStreams = 0;
        m_peerConnectionBackend.dispatchPendingTrackEvents();
    }

#ifndef GST_DISABLE_GST_DEBUG
    auto dotFileName = makeString(GST_OBJECT_NAME(m_pipeline.get()), ".incoming-", mediaType, '-', GST_OBJECT_NAME(pad));
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
#endif
}

void GStreamerMediaEndpoint::removeRemoteStream(GstPad*)
{
    GST_FIXME_OBJECT(m_pipeline.get(), "removeRemoteStream");
    notImplemented();
}

std::optional<GStreamerMediaEndpoint::Backends> GStreamerMediaEndpoint::createTransceiverBackends(const String& kind, const RTCRtpTransceiverInit& init, GStreamerRtpSenderBackend::Source&& source)
{
    if (!m_webrtcBin)
        return std::nullopt;

    GST_DEBUG_OBJECT(m_pipeline.get(), "%zu streams in init data", init.streams.size());

    // FIXME: Should we build the caps from the init.sendEncodings? Problem is there is no
    // encodingName or clockRate in RTCRtpEncodingParameters.
    Vector<std::pair<const char*, int>> encodings;
    const char* media = kind.utf8().data();
    if (kind == "video"_s) {
        encodings.reserveInitialCapacity(3);
        encodings.uncheckedAppend({ "VP8", 90000 });
        encodings.uncheckedAppend({ "VP9", 90000 });
        encodings.uncheckedAppend({ "H264", 90000 });
    } else {
        encodings.reserveInitialCapacity(4);
        encodings.uncheckedAppend({ "OPUS", 48000 });
        encodings.uncheckedAppend({ "G722", 8000 });
        encodings.uncheckedAppend({ "PCMA", 8000 });
        encodings.uncheckedAppend({ "PCMU", 8000 });
    }

    auto caps = adoptGRef(gst_caps_new_empty());
    for (auto& [encodingName, clockRate] : encodings) {
        gst_caps_append_structure(caps.get(), gst_structure_new("application/x-rtp", "media", G_TYPE_STRING, media, "encoding-name", G_TYPE_STRING, encodingName,
            "payload", G_TYPE_INT, m_ptCounter++, "clock-rate", G_TYPE_INT, clockRate, nullptr));
    }

    auto direction = fromRTCRtpTransceiverDirection(init.direction);
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

    GValue encodingsValue = G_VALUE_INIT;
    g_value_init(&encodingsValue, GST_TYPE_LIST);
    if (kind == "audio"_s) {
        if (!init.sendEncodings.isEmpty()) {
            auto encodingData = fromRTCEncodingParameters(init.sendEncodings[0]);
            GValue value = G_VALUE_INIT;
            g_value_init(&value, GST_TYPE_STRUCTURE);
            gst_value_set_structure(&value, encodingData.get());
            gst_value_list_append_value(&encodingsValue, &value);
            g_value_unset(&value);
        }
    } else {
        for (auto& encoding : init.sendEncodings) {
            auto encodingData = fromRTCEncodingParameters(encoding);
            GValue value = G_VALUE_INIT;
            g_value_init(&value, GST_TYPE_STRUCTURE);
            gst_value_set_structure(&value, encodingData.get());
            gst_value_list_append_value(&encodingsValue, &value);
            g_value_unset(&value);
        }
    }
    gst_structure_take_value(initData.get(), "encodings", &encodingsValue);

    GRefPtr<GstWebRTCRTPTransceiver> rtcTransceiver;
    g_signal_emit_by_name(m_webrtcBin.get(), "add-transceiver", direction, caps.get(), &rtcTransceiver.outPtr());
    if (!rtcTransceiver)
        return std::nullopt;

    auto transceiver = makeUnique<GStreamerRtpTransceiverBackend>(WTFMove(rtcTransceiver));
    return GStreamerMediaEndpoint::Backends { transceiver->createSenderBackend(m_peerConnectionBackend, WTFMove(source), WTFMove(initData)), transceiver->createReceiverBackend(), WTFMove(transceiver) };
}

std::optional<GStreamerMediaEndpoint::Backends> GStreamerMediaEndpoint::addTransceiver(const String& trackKind, const RTCRtpTransceiverInit& init)
{
    return createTransceiverBackends(trackKind, init, nullptr);
}

GStreamerRtpSenderBackend::Source GStreamerMediaEndpoint::createSourceForTrack(MediaStreamTrack& track)
{
    if (track.privateTrack().isAudio())
        return RealtimeOutgoingAudioSourceGStreamer::create(track.privateTrack());

    ASSERT(track.privateTrack().isVideo());
    return RealtimeOutgoingVideoSourceGStreamer::create(track.privateTrack());
}

std::optional<GStreamerMediaEndpoint::Backends> GStreamerMediaEndpoint::addTransceiver(MediaStreamTrack& track, const RTCRtpTransceiverInit& init)
{
    return createTransceiverBackends(track.kind(), init, createSourceForTrack(track));
}

void GStreamerMediaEndpoint::setSenderSourceFromTrack(GStreamerRtpSenderBackend&, MediaStreamTrack&)
{
    GST_FIXME_OBJECT(m_pipeline.get(), "setSenderSourceFromTrack");
    notImplemented();
}

std::unique_ptr<GStreamerRtpTransceiverBackend> GStreamerMediaEndpoint::transceiverBackendFromSender(GStreamerRtpSenderBackend& backend)
{
    GRefPtr<GArray> transceivers;
    g_signal_emit_by_name(m_webrtcBin.get(), "get-transceivers", &transceivers.outPtr());

    GST_DEBUG_OBJECT(m_pipeline.get(), "Looking for sender %p in %u existing transceivers", backend.rtcSender(), transceivers->len);
    for (unsigned transceiverIndex = 0; transceiverIndex < transceivers->len; transceiverIndex++) {
        GstWebRTCRTPTransceiver* current = g_array_index(transceivers.get(), GstWebRTCRTPTransceiver*, transceiverIndex);
        GRefPtr<GstWebRTCRTPSender> sender;
        g_object_get(current, "sender", &sender.outPtr(), nullptr);

        if (!sender)
            continue;
        if (sender.get() == backend.rtcSender())
            return WTF::makeUnique<GStreamerRtpTransceiverBackend>(current);
    }

    return nullptr;
}

void GStreamerMediaEndpoint::addIceCandidate(GStreamerIceCandidate& candidate, PeerConnectionBackend::AddIceCandidateCallback&& callback)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Adding ICE candidate %s", candidate.candidate.utf8().data());

    if (!candidate.candidate.startsWith("candidate:"_s)) {
        callOnMainThread([task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(callback))]() mutable {
            task->run(Exception { OperationError, "Expect line: candidate:<candidate-str>"_s });
        });
        return;
    }

    auto parsedCandidate = parseIceCandidateSDP(candidate.candidate);
    if (!parsedCandidate) {
        callOnMainThread([task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(callback))]() mutable {
            task->run(Exception { OperationError, "Error processing ICE candidate"_s });
        });
        return;
    }

    // FIXME: invalid sdpMLineIndex exception not relayed from webrtcbin.
    // FIXME: Ideally this should pass the result/error to a GstPromise object.
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

    return WTF::makeUnique<GStreamerDataChannelHandler>(WTFMove(channel));
}

void GStreamerMediaEndpoint::onDataChannel(GstWebRTCDataChannel* dataChannel)
{
    GRefPtr<GstWebRTCDataChannel> channel = dataChannel;
    callOnMainThread([protectedThis = Ref(*this), this, dataChannel = WTFMove(channel)]() mutable {
        if (isStopped())
            return;
        GST_DEBUG_OBJECT(m_pipeline.get(), "Incoming data channel");
        auto& connection = m_peerConnectionBackend.connection();
        connection.dispatchEvent(GStreamerDataChannelHandler::createDataChannelEvent(*connection.document(), WTFMove(dataChannel)));
    });
}

void GStreamerMediaEndpoint::close()
{
    // https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/issues/1181
    GST_DEBUG_OBJECT(m_pipeline.get(), "Closing");
    if (m_pipeline)
        gst_element_set_state(m_pipeline.get(), GST_STATE_READY);

#if !RELEASE_LOG_DISABLED
    stopLoggingStats();
#endif
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
    gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED);
}

void GStreamerMediaEndpoint::resume()
{
    if (isStopped())
        return;

    GST_DEBUG_OBJECT(m_pipeline.get(), "Resuming");
    gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
}

void GStreamerMediaEndpoint::onNegotiationNeeded()
{
    m_isNegotiationNeeded = true;

    callOnMainThread([protectedThis = Ref(*this), this] {
        if (isStopped())
            return;
        GST_DEBUG_OBJECT(m_pipeline.get(), "Negotiation needed!");
        m_peerConnectionBackend.markAsNeedingNegotiation(0);
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
    callOnMainThread([protectedThis = Ref(*this), this, state] {
        if (isStopped())
            return;
        auto& connection = m_peerConnectionBackend.connection();
        if (state == GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE)
            m_peerConnectionBackend.doneGatheringCandidates();
        else if (state == GST_WEBRTC_ICE_GATHERING_STATE_GATHERING)
            connection.updateIceGatheringState(RTCIceGatheringState::Gathering);
        else if (state == GST_WEBRTC_ICE_GATHERING_STATE_NEW)
            connection.updateIceGatheringState(RTCIceGatheringState::New);
    });
}

void GStreamerMediaEndpoint::onIceCandidate(guint sdpMLineIndex, gchararray candidate)
{
    if (isStopped())
        return;

    // FIXME: Get mid from candidate?
    GUniqueOutPtr<GstWebRTCSessionDescription> description;
    g_object_get(m_webrtcBin.get(), "local-description", &description.outPtr(), nullptr);
    if (!description)
        return;

    const auto* media = gst_sdp_message_get_media(description->sdp, sdpMLineIndex);
    callOnMainThread([protectedThis = Ref(*this), this, mid = makeString(gst_sdp_media_get_attribute_val(media, "mid")), sdp = makeString(candidate), sdpMLineIndex]() mutable {
        if (isStopped())
            return;
        auto descriptions = descriptionsFromWebRTCBin(m_webrtcBin.get());
        m_peerConnectionBackend.newICECandidate(WTFMove(sdp), WTFMove(mid), sdpMLineIndex, { }, WTFMove(descriptions));
    });
}

void GStreamerMediaEndpoint::createSessionDescriptionSucceeded(GUniquePtr<GstWebRTCSessionDescription>&& description)
{
    callOnMainThread([protectedThis = Ref(*this), this, description = WTFMove(description)] {
        if (isStopped())
            return;

        GUniquePtr<char> sdp(gst_sdp_message_as_text(description->sdp));
        if (m_isInitiator)
            m_peerConnectionBackend.createOfferSucceeded(String::fromLatin1(sdp.get()));
        else
            m_peerConnectionBackend.createAnswerSucceeded(String::fromLatin1(sdp.get()));
    });
}

void GStreamerMediaEndpoint::createSessionDescriptionFailed(GUniquePtr<GError>&& error)
{
    callOnMainThread([protectedThis = Ref(*this), this, error = WTFMove(error)] {
        if (isStopped())
            return;

        auto exc = Exception { OperationError, error ? String::fromUTF8(error->message) : "Unknown Error"_s };
        if (m_isInitiator)
            m_peerConnectionBackend.createOfferFailed(WTFMove(exc));
        else
            m_peerConnectionBackend.createAnswerFailed(WTFMove(exc));
    });
}

void GStreamerMediaEndpoint::collectTransceivers()
{
    GRefPtr<GArray> transceivers;
    g_signal_emit_by_name(m_webrtcBin.get(), "get-transceivers", &transceivers.outPtr());
    for (unsigned i = 0; i < transceivers->len; i++) {
        GstWebRTCRTPTransceiver* current = g_array_index(transceivers.get(), GstWebRTCRTPTransceiver*, i);

        auto* existingTransceiver = m_peerConnectionBackend.existingTransceiver([&](auto& transceiverBackend) {
            return current == transceiverBackend.rtcTransceiver();
        });
        if (existingTransceiver)
            continue;

        GRefPtr<GstWebRTCRTPReceiver> receiver;
        GUniqueOutPtr<char> mid;
        g_object_get(current, "receiver", &receiver.outPtr(), "mid", &mid.outPtr(), nullptr);

        if (!receiver || !mid)
            continue;

        m_peerConnectionBackend.newRemoteTransceiver(WTF::makeUnique<GStreamerRtpTransceiverBackend>(WTFMove(current)), m_mediaForMid.get(String::fromLatin1(mid.get())));
    }
}

#if !RELEASE_LOG_DISABLED
void GStreamerMediaEndpoint::gatherStatsForLogging()
{
    auto* holder = createGStreamerMediaEndpointHolder();
    holder->endPoint = this;
    g_signal_emit_by_name(m_webrtcBin.get(), "get-stats", nullptr, gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) {
        auto promise = adoptGRef(rawPromise);
        auto result = gst_promise_wait(promise.get());
        if (result != GST_PROMISE_RESULT_REPLIED)
            return;

        const auto* reply = gst_promise_get_reply(promise.get());
        ASSERT(reply);
        if (gst_structure_has_field(reply, "error"))
            return;

        auto* holder = static_cast<GStreamerMediaEndpointHolder*>(userData);
        holder->endPoint->onStatsDelivered(reply);
    }, holder, reinterpret_cast<GDestroyNotify>(destroyGStreamerMediaEndpointHolder)));
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

void GStreamerMediaEndpoint::processStats(const GValue* value)
{
    if (!GST_VALUE_HOLDS_STRUCTURE(value))
        return;

    const GstStructure* structure = gst_value_get_structure(value);
    GstWebRTCStatsType statsType;
    if (!gst_structure_get(structure, "type", GST_TYPE_WEBRTC_STATS_TYPE, &statsType, nullptr))
        return;

    // Just check a single timestamp, inbound RTP for instance.
    if (!m_statsFirstDeliveredTimestamp && statsType == GST_WEBRTC_STATS_INBOUND_RTP) {
        double timestamp;
        if (gst_structure_get_double(structure, "timestamp", &timestamp)) {
            auto ts = Seconds::fromMilliseconds(timestamp);
            m_statsFirstDeliveredTimestamp = ts;

            if (!isStopped() && m_statsLogTimer.repeatInterval() != statsLogInterval(ts)) {
                m_statsLogTimer.stop();
                m_statsLogTimer.startRepeating(statsLogInterval(ts));
            }
        }
    }

    if (logger().willLog(logChannel(), WTFLogLevel::Debug)) {
        // Stats are very verbose, let's only display them in inspector console in verbose mode.
        logger().debug(LogWebRTC,
            Logger::LogSiteIdentifier("GStreamerMediaEndpoint", "onStatsDelivered", logIdentifier()),
            RTCStatsLogger { structure });
    } else {
        logger().logAlways(LogWebRTCStats,
            Logger::LogSiteIdentifier("GStreamerMediaEndpoint", "onStatsDelivered", logIdentifier()),
            RTCStatsLogger { structure });
    }
}

void GStreamerMediaEndpoint::onStatsDelivered(const GstStructure* stats)
{
    GUniquePtr<GstStructure> statsCopy(gst_structure_copy(stats));
    callOnMainThread([protectedThis = Ref(*this), this, stats = WTFMove(statsCopy)] {
        gst_structure_foreach(stats.get(), static_cast<GstStructureForeachFunc>([](GQuark, const GValue* value, gpointer userData) -> gboolean {
            auto* endPoint = reinterpret_cast<GStreamerMediaEndpoint*>(userData);
            endPoint->processStats(value);
            return TRUE;
        }), this);
    });
}
#endif

#if !RELEASE_LOG_DISABLED
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

#endif // USE(GSTREAMER_WEBRTC)
