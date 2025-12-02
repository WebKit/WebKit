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

#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "ExceptionOr.h"
#include "GStreamerCommon.h"
#include "GStreamerDataChannelHandler.h"
#include "GStreamerIceAgent.h"
#include "GStreamerIncomingTrackProcessor.h"
#include "GStreamerRegistryScanner.h"
#include "GStreamerRtpReceiverBackend.h"
#include "GStreamerRtpTransceiverBackend.h"
#include "GStreamerSctpTransportBackend.h"
#include "GStreamerWebRTCUtils.h"
#include "JSDOMPromiseDeferred.h"
#include "JSRTCStatsReport.h"
#include "LogInitialization.h"
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
#include <algorithm>
#include <gst/sdp/sdp.h>
#include <wtf/MainThread.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>
#include <wtf/UniqueRef.h>
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY(webkit_webrtc_endpoint_debug);
#define GST_CAT_DEFAULT webkit_webrtc_endpoint_debug

namespace WebCore {

GStreamerMediaEndpoint::GStreamerMediaEndpoint(GStreamerPeerConnectionBackend& peerConnection)
    : m_peerConnectionBackend(WeakPtr { &peerConnection })
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

RefPtr<GStreamerPeerConnectionBackend> GStreamerMediaEndpoint::peerConnectionBackend() const
{
    return m_peerConnectionBackend.get();
}

GStreamerMediaEndpoint::NetSimOptions GStreamerMediaEndpoint::netSimOptionsFromEnvironment(ASCIILiteral optionsEnvVarName)
{
    NetSimOptions options;
    auto tokens = StringView::fromLatin1(g_getenv(optionsEnvVarName));
    for (auto it : tokens.split(',')) {
        auto option = it.toString();
        auto keyValue = option.split('=');
        if (keyValue.size() < 2) [[unlikely]]
            continue;
        options.add(keyValue[0], keyValue[1]);
    }
    return options;
}

void GStreamerMediaEndpoint::maybeInsertNetSimForElement(GstBin* bin, GstElement* element)
{
    bool isSource = GST_OBJECT_FLAG_IS_SET(element, GST_ELEMENT_FLAG_SOURCE);
    const auto& options = isSource ? m_srcNetSimOptions : m_sinkNetSimOptions;
    if (options.isEmpty())
        return;

    // Unlink the element, add a netsim element in bin and link it to the element to simulate varying network conditions.
    const char* padName = isSource ? "src" : "sink";
    auto pad = adoptGRef(gst_element_get_static_pad(element, padName));
    auto peer = adoptGRef(gst_pad_get_peer(pad.get()));
    if (!peer) [[unlikely]]
        return;

    gst_pad_unlink(pad.get(), peer.get());

    auto netsim = makeGStreamerElement("netsim"_s);
    gst_bin_add(GST_BIN_CAST(bin), netsim);

    GST_DEBUG_OBJECT(m_pipeline.get(), "Configuring %" GST_PTR_FORMAT " for transport element %" GST_PTR_FORMAT, netsim, element);
    for (const auto& [key, value] : options)
        gst_util_set_object_arg(G_OBJECT(netsim), key.ascii().data(), value.ascii().data());

    pad = adoptGRef(gst_element_get_static_pad(netsim, padName));
    if (isSource) {
        gst_element_link(element, netsim);
        gst_pad_link(pad.get(), peer.get());
    } else {
        gst_pad_link(peer.get(), pad.get());
        gst_element_link(netsim, element);
    }

    gst_element_sync_state_with_parent(netsim);
}

bool GStreamerMediaEndpoint::initializePipeline()
{
    auto webrtcBinFactory = adoptGRef(gst_element_factory_find("webrtcbin"));
    if (!webrtcBinFactory) {
        gst_printerrln("GStreamer element webrtcbin not found. Please install gst-plugins-bad");
        return false;
    }

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
    }, AsynchronousPipelineDumping::Yes);

    auto binName = makeString("webkit-webrtcbin-"_s, nPipeline);
    nPipeline++;

#if USE(LIBRICE) && GST_CHECK_VERSION(1, 20, 0)
    auto disableNetworkSandbox = CStringView::unsafeFromUTF8(g_getenv("WEBKIT_GST_DISABLE_WEBRTC_NETWORK_SANDBOX"));
    if (gst_check_version(1, 22, 0) && disableNetworkSandbox != "1"_s) {
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return false;

        auto agent = webkitGstWebRTCCreateIceAgent(makeString(binName, ":ice"_s), peerConnectionBackend->context());
        if (!agent) {
            gst_printerrln("Unable to create WebRTC ICE agent");
            return false;
        }
        auto name = binName.ascii();
        m_webrtcBin = gst_element_factory_create_full(webrtcBinFactory.get(), "name", name.data(), "ice-agent", agent, nullptr);
    }
#endif // USE(LIBRICE) && GST_CHECK_VERSION(1, 20, 0)
    if (!m_webrtcBin)
        m_webrtcBin = gst_element_factory_create(webrtcBinFactory.get(), binName.ascii().data());

    // Lower default latency from 200ms to 40ms.
    g_object_set(m_webrtcBin.get(), "latency", 40, nullptr);

    m_srcNetSimOptions = netSimOptionsFromEnvironment("WEBKIT_WEBRTC_NETSIM_SRC_OPTIONS"_s);
    m_sinkNetSimOptions = netSimOptionsFromEnvironment("WEBKIT_WEBRTC_NETSIM_SINK_OPTIONS"_s);
    if (!m_srcNetSimOptions.isEmpty() || !m_sinkNetSimOptions.isEmpty()) {
        if (auto factory = adoptGRef(gst_element_factory_find("netsim"))) {
            g_signal_connect_swapped(m_webrtcBin.get(), "deep-element-added", G_CALLBACK(+[](GStreamerMediaEndpoint* self, GstBin* bin, GstElement* element) {
                GUniquePtr<char> elementName(gst_element_get_name(element));
                auto view = StringView::fromLatin1(elementName.get());
                if (view.startsWith("nice"_s) || view.startsWith("ice-sink"_s) || view.startsWith("ice-src"_s))
                    self->maybeInsertNetSimForElement(bin, element);
            }), this);
        } else
            gst_printerrln("WEBKIT_WEBRTC_NETSIM_{SRC,SINK}_OPTIONS was/were set but the GStreamer netsim element is missing.");
    }

    auto rtpBin = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_webrtcBin.get()), "rtpbin"));
    if (!rtpBin) {
        GST_ERROR_OBJECT(m_webrtcBin.get(), "rtpbin not found. Please check that your GStreamer installation has the rtp and rtpmanager plugins.");
        return false;
    }

    g_signal_connect_swapped(rtpBin.get(), "element-added", G_CALLBACK(+[](GStreamerMediaEndpoint* self, GstElement* element) {
        GUniquePtr<char> elementName(gst_element_get_name(element));
        auto view = StringView::fromLatin1(elementName.get());
        if (!view.startsWith("rtpptdemux"_s))
            return;

        auto pad = adoptGRef(gst_element_get_static_pad(element, "sink"));
        gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER), [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
            auto self = reinterpret_cast<GStreamerMediaEndpoint*>(userData);
            auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
            GstMappedRtpBuffer rtpBuffer(buffer, GST_MAP_READ);
            if (!rtpBuffer) [[unlikely]]
                return GST_PAD_PROBE_OK;

            uint32_t ssrc = gst_rtp_buffer_get_ssrc(rtpBuffer.mappedData());
            self->m_inputBuffers.add(ssrc, GRefPtr<GstBuffer>(buffer));
            return GST_PAD_PROBE_REMOVE;
        }, self, nullptr);

        g_signal_connect(element, "new-payload-type", G_CALLBACK(+[](GstElement* ptDemux, unsigned, GstPad* pad, GStreamerMediaEndpoint* self) {
            self->updatePtDemuxSrcPadCaps(ptDemux, pad);
        }), self);
    }), this);

    if (gstObjectHasProperty(rtpBin.get(), "add-reference-timestamp-meta"_s)) {
        auto disableCaptureTimeTracking = StringView::fromLatin1(g_getenv("WEBKIT_GST_DISABLE_WEBRTC_CAPTURE_TIME_TRACKING"));
        if (disableCaptureTimeTracking.isEmpty() || disableCaptureTimeTracking == "0"_s)
            g_object_set(rtpBin.get(), "add-reference-timestamp-meta", TRUE, nullptr);
    }

    // Prevent drift between RTP timestamps and NTP time as reported by RTCP packets.
    gst_util_set_object_arg(G_OBJECT(rtpBin.get()), "ntp-time-source", "clock-time");

    // Use the time at which the audio/video frames were captured, without latency induced by encoders.
    g_object_set(rtpBin.get(), "rtcp-sync-send-time", TRUE, nullptr);

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

        GST_DEBUG_OBJECT(endPoint->pipeline(), "Pad added: %" GST_PTR_FORMAT, pad);
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

    if (gst_check_version(1, 22, 0)) {
        g_signal_connect_swapped(m_webrtcBin.get(), "prepare-data-channel", G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint, GstWebRTCDataChannel* channel, gboolean isLocal) {
            endPoint->prepareDataChannel(channel, isLocal);
        }), this);

        ASCIILiteral requestAuxSenderSignalName = "request-aux-sender"_s;
        if (gst_check_version(1, 25, 0))
            requestAuxSenderSignalName = "request-post-rtp-aux-sender"_s;
        g_signal_connect_swapped(m_webrtcBin.get(), requestAuxSenderSignalName.characters(), G_CALLBACK(+[](GStreamerMediaEndpoint* endPoint, GstWebRTCDTLSTransport* transport) -> GstElement* {
            // `sender` ownership is transferred to the signal caller.
            return endPoint->requestAuxiliarySender(GRefPtr(transport));
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
        auto dotFilename = makeString(unsafeSpan(GST_ELEMENT_NAME(endPoint->pipeline())), '-', unsafeSpan(desc.get()));
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

    if (auto peerConnectionBackend = this->peerConnectionBackend())
        peerConnectionBackend->tearDown();
    gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);

    m_trackProcessors.clear();
    m_incomingDataChannels.clear();
    m_remoteStreamsById.clear();
    m_webrtcBin = nullptr;
    m_pipeline = nullptr;
    m_peerConnectionBackend = nullptr;
}

bool GStreamerMediaEndpoint::handleMessage(GstMessage* message)
{
    GST_TRACE_OBJECT(m_pipeline.get(), "Received message %s from %s", GST_MESSAGE_TYPE_NAME(message), GST_MESSAGE_SRC_NAME(message));
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

    gstElementLockAndSetState(element, GST_STATE_NULL);

    gst_pad_unlink(peer.get(), pad.get());
    gst_bin_remove(GST_BIN_CAST(m_pipeline.get()), element);
    gst_element_release_request_pad(m_webrtcBin.get(), peer.get());
}

bool GStreamerMediaEndpoint::setConfiguration(MediaEndpointConfiguration& configuration)
{
    auto peerConnectionBackend = this->peerConnectionBackend();
    if (!peerConnectionBackend)
        return false;

    auto& document = downcast<Document>(*peerConnectionBackend->connection().scriptExecutionContext());
    GST_DEBUG_OBJECT(m_pipeline.get(), "Configuring webrtcbin for PeerConnection created by %s", document.url().string().utf8().data());
    GstWebRTCBundlePolicy bundlePolicy;
    if (document.url().isMatchingDomain("www.xbox.com"_s)) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Applying XBox-play quirk, forcing max-bundle policy");
        bundlePolicy = GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE;
    } else
        bundlePolicy = bundlePolicyFromConfiguration(configuration);
    g_object_set(m_webrtcBin.get(), "bundle-policy", bundlePolicy, nullptr);

    auto iceTransportPolicy = iceTransportPolicyFromConfiguration(configuration);
    g_object_set(m_webrtcBin.get(), "ice-transport-policy", iceTransportPolicy, nullptr);

    bool stunSet = false;
    for (auto& server : configuration.iceServers) {
        for (auto& url : server.urls) {
            if (url.protocol().startsWith("turn"_s)) {
                auto valid = makeStringByReplacingAll(url.string().isolatedCopy(), "turn:"_s, "turn://"_s);
                valid = makeStringByReplacingAll(valid, "turns:"_s, "turns://"_s);
                URL validURL(URL(), valid);
                validURL.setUser(server.username);
                validURL.setPassword(server.credential);
                gboolean result = FALSE;
                g_signal_emit_by_name(m_webrtcBin.get(), "add-turn-server", validURL.string().utf8().data(), &result);
                if (!result)
                    GST_WARNING("Unable to use TURN server: %s", validURL.string().utf8().data());
            }
            if (!stunSet && url.protocol().startsWith("stun"_s)) {
                auto stunURL = makeStringByReplacingAll(url.string().isolatedCopy(), "stun:"_s, "stun://"_s);
                g_object_set(m_webrtcBin.get(), "stun-server", stunURL.utf8().data(), nullptr);
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

static std::optional<GStreamerMediaEndpointTransceiverState> toGStreamerMediaEndpointTransceiverState(GstElement* webrtcBin, GstWebRTCRTPTransceiver* transceiver, const GstWebRTCSessionDescription* remoteDescription)
{
    GRefPtr<GstWebRTCRTPReceiver> receiver;
    GUniqueOutPtr<char> mid;
    GstWebRTCRTPTransceiverDirection direction, currentDirection, firedDirection;
    guint mLineIndex;
    // GstWebRTCRTPTransceiver doesn't have a fired-direction property, so use direction. Until
    // GStreamer 1.26 direction and current-direction always had the same value. This was fixed by:
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/commit/cafb999fb0cdf32803fcc3f85f2652212f05c2d0
    g_object_get(transceiver, "receiver", &receiver.outPtr(), "direction", &direction, "current-direction", &currentDirection, "mlineindex", &mLineIndex, "mid", &mid.outPtr(), nullptr);
#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> desc(g_enum_to_string(GST_TYPE_WEBRTC_RTP_TRANSCEIVER_DIRECTION, direction));
    GUniquePtr<char> currentDesc(g_enum_to_string(GST_TYPE_WEBRTC_RTP_TRANSCEIVER_DIRECTION, currentDirection));
    GST_TRACE_OBJECT(webrtcBin, "Receiver = %" GST_PTR_FORMAT ", direction = %s, current-direction = %s, mlineindex = %u, mid = %s", receiver.get(), desc.get(), currentDesc.get(), mLineIndex, GST_STR_NULL(mid.get()));
#endif
    firedDirection = currentDirection != GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE ? currentDirection : direction;

    if (!mid) [[unlikely]]
        return { };

    Vector<String> streamIds;
    if (remoteDescription && remoteDescription->sdp && mLineIndex < gst_sdp_message_medias_len(remoteDescription->sdp)) {
        const GstSDPMedia* media = gst_sdp_message_get_media(remoteDescription->sdp, mLineIndex);
        if (isRecvDirection(direction))
            streamIds = getMediaStreamIdsFromSDPMedia(*media);
    }

#ifndef GST_DISABLE_GST_DEBUG
    if (streamIds.isEmpty())
        GST_TRACE_OBJECT(webrtcBin, "Receiver has no track");
    else {
        StringBuilder idsMessageBuilder;
        idsMessageBuilder.append(interleave(streamIds, ','));
        auto ids = idsMessageBuilder.toString();
        GST_TRACE_OBJECT(webrtcBin, "Receiver includes %zu tracks with IDs: %s", streamIds.size(), ids.utf8().data());
    }
#endif

    std::optional<RTCRtpTransceiverDirection> firedDirectionResult;
    if (!streamIds.isEmpty())
        firedDirectionResult = toRTCRtpTransceiverDirection(firedDirection);

    return { { String::fromUTF8(mid.get()), WTFMove(streamIds), WTFMove(firedDirectionResult) } };
}

static Vector<GStreamerMediaEndpointTransceiverState> transceiverStatesFromWebRTCBin(const GRefPtr<GstElement>& webrtcBin)
{
    Vector<GStreamerMediaEndpointTransceiverState> states;

    GUniqueOutPtr<GstWebRTCSessionDescription> remoteDescription;
    g_object_get(webrtcBin.get(), "remote-description", &remoteDescription.outPtr(), nullptr);

#ifndef GST_DISABLE_GST_DEBUG
    GUniqueOutPtr<GstWebRTCSessionDescription> localDescription;
    g_object_get(webrtcBin.get(), "local-description", &localDescription.outPtr(), nullptr);
    if (localDescription) {
        GUniquePtr<char> sdp(gst_sdp_message_as_text(localDescription->sdp));
        GST_TRACE_OBJECT(webrtcBin.get(), "Local-description:\n%s", sdp.get());
    }
    if (remoteDescription) {
        GUniquePtr<char> sdp(gst_sdp_message_as_text(remoteDescription->sdp));
        GST_TRACE_OBJECT(webrtcBin.get(), "Remote-description:\n%s", sdp.get());
    }
#endif

    forEachTransceiver(webrtcBin, [&](auto&& transceiver) -> bool {
        auto state = toGStreamerMediaEndpointTransceiverState(webrtcBin.get(), transceiver.get(), remoteDescription.get());
        if (!state) {
            GST_DEBUG_OBJECT(webrtcBin.get(), "Unable to compute state for transceiver %" GST_PTR_FORMAT, transceiver.get());
            return false;
        }
        states.append(WTFMove(*state));
        return false;
    });
    GST_TRACE_OBJECT(webrtcBin.get(), "Filled %zu transceiver states", states.size());
    return states;
}

void GStreamerMediaEndpoint::linkOutgoingSources(GstSDPMessage* sdpMessage)
{
    unsigned totalMedias = gst_sdp_message_medias_len(sdpMessage);
#ifndef GST_DISABLE_GST_DEBUG
    GST_DEBUG_OBJECT(m_pipeline.get(), "Linking outgoing sources for %u m-lines", totalMedias);
    GUniquePtr<char> sdp(gst_sdp_message_as_text(sdpMessage));
    GST_TRACE_OBJECT(m_pipeline.get(), "in SDP:\n%s", sdp.get());
#endif
    for (unsigned i = 0; i < totalMedias; i++) {
        const auto media = gst_sdp_message_get_media(sdpMessage, i);
        auto mediaType = StringView::fromLatin1(gst_sdp_media_get_media(media));
        RealtimeMediaSource::Type sourceType;
        if (mediaType == "audio"_s)
            sourceType = RealtimeMediaSource::Type::Audio;
        else if (mediaType == "video"_s)
            sourceType = RealtimeMediaSource::Type::Video;
        else {
            GST_DEBUG_OBJECT(m_pipeline.get(), "Skipping non audio/video source");
            continue;
        }

        auto msid = String::fromUTF8(gst_sdp_media_get_attribute_val(media, "msid"));

        GST_DEBUG_OBJECT(m_pipeline.get(), "Looking-up outgoing source with msid %s in %zu unlinked sources", msid.utf8().data(), m_unlinkedOutgoingSources.size());
        m_unlinkedOutgoingSources.removeFirstMatching([&](auto& source) -> bool {
            if (source->type() != sourceType)
                return false;

            if (const auto& track = source->track()) {
                auto sourceMsid = makeString(source->mediaStreamID(), ' ', track->id());
                if (!msid.isEmpty() && sourceMsid != msid)
                    return false;
            }

            auto allowedCaps = capsFromSDPMedia(media);
            source->configure(WTFMove(allowedCaps));
            if (!source->pad()) {
                auto rtpCaps = source->rtpCaps();
                auto sinkPad = requestPad(rtpCaps, source->mediaStreamID());
                source->setSinkPad(WTFMove(sinkPad));
            }

            auto& sinkPad = source->pad();
            if (gst_pad_is_linked(sinkPad.get())) [[unlikely]] {
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
            GST_DEBUG_OBJECT(m_pipeline.get(), "Empty local description, generating an offer");
            g_signal_emit_by_name(m_webrtcBin.get(), "create-offer", nullptr, promise);
            auto result = gst_promise_wait(promise);
            const auto reply = gst_promise_get_reply(promise);
            if (result != GST_PROMISE_RESULT_REPLIED || (reply && gst_structure_has_field(reply, "error"))) {
                if (reply) {
                    GUniqueOutPtr<GError> error;
                    gst_structure_get(reply, "error", G_TYPE_ERROR, &error.outPtr(), nullptr);
                    auto errorMessage = makeString("Unable to set local description, error: "_s, unsafeSpan(error->message));
                    GST_ERROR_OBJECT(m_webrtcBin.get(), "%s", errorMessage.utf8().data());
                    if (auto peerConnectionBackend = this->peerConnectionBackend())
                        peerConnectionBackend->setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, WTFMove(errorMessage) });
                    return;
                }
                if (auto peerConnectionBackend = this->peerConnectionBackend())
                    peerConnectionBackend->setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, "Unable to set local description"_s });
                return;
            }

            GUniqueOutPtr<GstWebRTCSessionDescription> sessionDescription;
            gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &sessionDescription.outPtr(), nullptr);
            initialDescription = RTCSessionDescription::create(RTCSdpType::Offer, sdpAsString(sessionDescription->sdp));
            break;
        }
        case GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_PRANSWER:
        case GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_OFFER: {
            GST_DEBUG_OBJECT(m_pipeline.get(), "Empty local description, generating an answer");
            auto pendingRemoteDescription = fetchDescription(m_webrtcBin.get(), "pending-remote"_s);
            g_signal_emit_by_name(m_webrtcBin.get(), "create-answer", nullptr, promise);
            auto result = gst_promise_wait(promise);
            const auto reply = gst_promise_get_reply(promise);
            if (result != GST_PROMISE_RESULT_REPLIED || (reply && gst_structure_has_field(reply, "error"))) {
                if (reply) {
                    GUniqueOutPtr<GError> error;
                    gst_structure_get(reply, "error", G_TYPE_ERROR, &error.outPtr(), nullptr);
                    auto errorMessage = makeString("Unable to set local description, error: "_s, unsafeSpan(error->message));
                    GST_ERROR_OBJECT(m_webrtcBin.get(), "%s", errorMessage.utf8().data());
                    if (auto peerConnectionBackend = this->peerConnectionBackend())
                        peerConnectionBackend->setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, WTFMove(errorMessage) });
                    return;
                }
                if (auto peerConnectionBackend = this->peerConnectionBackend())
                    peerConnectionBackend->setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, "Unable to set local description"_s });
                return;
            }

            GUniqueOutPtr<GstWebRTCSessionDescription> sessionDescription;
            gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &sessionDescription.outPtr(), nullptr);

            GUniquePtr<GstWebRTCSessionDescription> description;
            if (pendingRemoteDescription) {
                auto updatedAnswer = completeSDPAnswer(pendingRemoteDescription->second, sessionDescription->sdp);
                description.reset(gst_webrtc_session_description_new(sessionDescription->type, updatedAnswer.release()));
            } else
                description.reset(sessionDescription.release());

            initialDescription = RTCSessionDescription::create(RTCSdpType::Answer, sdpAsString(description->sdp));
            break;
        }
        case GST_WEBRTC_SIGNALING_STATE_CLOSED:
            if (auto peerConnectionBackend = this->peerConnectionBackend())
                peerConnectionBackend->setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, "The PeerConnection is closed."_s });
            return;
        };
    }

    auto peerConnectionBackend = this->peerConnectionBackend();
    if (!peerConnectionBackend)
        return;
    auto initialSDP = description ? description->sdp().isolatedCopy() : emptyString();
    auto remoteDescription = peerConnectionBackend->connection().remoteDescription();
    String remoteDescriptionSdp = remoteDescription ? remoteDescription->sdp() : emptyString();
    std::optional<RTCSdpType> remoteDescriptionSdpType = remoteDescription ? std::make_optional(remoteDescription->type()) : std::nullopt;

    if (!initialDescription->sdp().isEmpty()) {
        GUniqueOutPtr<GstSDPMessage> sdpMessage;
        if (gst_sdp_message_new_from_text(initialDescription->sdp().utf8().data(), &sdpMessage.outPtr()) != GST_SDP_OK) {
            peerConnectionBackend->setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, "Invalid SDP"_s });
            return;
        }

        // Make sure each outgoing media source is configured using the proposed codec and linked to webrtcbin.
        linkOutgoingSources(sdpMessage.get());
    }

    if (!m_unlinkedOutgoingSources.isEmpty())
        GST_WARNING_OBJECT(m_pipeline.get(), "Unlinked outgoing sources lingering");
    gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);

    setDescription(initialDescription.get(), DescriptionType::Local, [protectedThis = Ref(*this), this, initialSDP = WTFMove(initialSDP), remoteDescriptionSdp = WTFMove(remoteDescriptionSdp), remoteDescriptionSdpType = WTFMove(remoteDescriptionSdpType)](const GstSDPMessage& message) {
        if (protectedThis->isStopped())
            return;
        auto peerConnectionBackend = protectedThis->peerConnectionBackend();
        if (!peerConnectionBackend)
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
        auto dotFileName = makeString(unsafeSpan(GST_OBJECT_NAME(m_pipeline.get())), ".setLocalDescription"_s);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
#endif

        auto rtcTransceiverStates = transceiverStatesFromWebRTCBin(m_webrtcBin);
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

        peerConnectionBackend->setLocalDescriptionSucceeded(WTFMove(descriptions), WTFMove(transceiverStates), transport ? makeUnique<GStreamerSctpTransportBackend>(WTFMove(transport)) : nullptr, maxMessageSize);
    }, [protectedThis = Ref(*this), this](const GError* error) {
        if (protectedThis->isStopped())
            return;
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return;
        if (error) {
            if (error->code == GST_WEBRTC_ERROR_INVALID_STATE) {
                peerConnectionBackend->setLocalDescriptionFailed(Exception { ExceptionCode::InvalidStateError, "Failed to set local answer sdp: no pending remote description."_s });
                return;
            }
            peerConnectionBackend->setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, String::fromUTF8(error->message) });
        } else
            peerConnectionBackend->setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, "Unable to apply session local description"_s });
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
    auto peerConnectionBackend = this->peerConnectionBackend();
    if (!peerConnectionBackend)
        return;
    auto localDescription = peerConnectionBackend->connection().localDescription();
    String localDescriptionSdp = localDescription ? localDescription->sdp() : emptyString();
    std::optional<RTCSdpType> localDescriptionSdpType = localDescription ? std::make_optional(localDescription->type()) : std::nullopt;

    if (!initialSDP.isEmpty()) {
        GUniqueOutPtr<GstSDPMessage> sdpMessage;
        if (gst_sdp_message_new_from_text(initialSDP.utf8().data(), &sdpMessage.outPtr()) != GST_SDP_OK) {
            peerConnectionBackend->setRemoteDescriptionFailed(Exception { ExceptionCode::OperationError, "Invalid SDP"_s });
            return;
        }

        // Make sure each outgoing media source is configured using the proposed codec and linked to webrtcbin.
        linkOutgoingSources(sdpMessage.get());
    }

    setDescription(&description, DescriptionType::Remote, [protectedThis = Ref(*this), this, initialSDP = WTFMove(initialSDP), localDescriptionSdp = WTFMove(localDescriptionSdp), localDescriptionSdpType = WTFMove(localDescriptionSdpType)](const GstSDPMessage& message) {
        if (protectedThis->isStopped())
            return;
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return;
        processSDPMessage(&message, [this](unsigned, StringView mid, const auto* media) {
            auto mediaType = CStringView::unsafeFromUTF8(gst_sdp_media_get_media(media));
            m_mediaForMid.set(mid.toString(), mediaType == "audio"_s ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video);

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
        auto dotFileName = makeString(unsafeSpan(GST_OBJECT_NAME(m_pipeline.get())), ".setRemoteDescription"_s);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
#endif

        auto rtcTransceiverStates = transceiverStatesFromWebRTCBin(m_webrtcBin);
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

        peerConnectionBackend->setRemoteDescriptionSucceeded(WTFMove(descriptions), WTFMove(transceiverStates), transport ? makeUnique<GStreamerSctpTransportBackend>(WTFMove(transport)) : nullptr, maxMessageSize);
    }, [protectedThis = Ref(*this), this](const GError* error) {
        if (protectedThis->isStopped())
            return;
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return;
        if (error && error->code == GST_WEBRTC_ERROR_INVALID_STATE)
            peerConnectionBackend->setRemoteDescriptionFailed(Exception { ExceptionCode::InvalidStateError, "Failed to set remote answer sdp"_s });
        else
            peerConnectionBackend->setRemoteDescriptionFailed(Exception { ExceptionCode::InvalidAccessError, "Unable to apply session remote description"_s });
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
        if (descriptionType == DescriptionType::Remote) {
            GUniqueOutPtr<GstWebRTCSessionDescription> currentDescription;

            g_object_get(m_webrtcBin.get(), "current-remote-description", &currentDescription.outPtr(), nullptr);
            if (currentDescription && !validateRTPHeaderExtensions(currentDescription->sdp, message.get())) {
                failureCallback(nullptr);
                return;
            }
        }
    } else if (gst_sdp_message_new(&message.outPtr()) != GST_SDP_OK) {
        failureCallback(nullptr);
        return;
    }

    m_statsCollector->invalidateCache();

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
        auto mediaType = CStringView::unsafeFromUTF8(gst_sdp_media_get_media(media));
        if (mediaType != "audio"_s && mediaType != "video"_s)
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
    auto dotFileName = makeString(unsafeSpan(GST_OBJECT_NAME(m_pipeline.get())), ".outgoing"_s);
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
            payloadType = payloadTypeForEncodingName(encodingName.span());

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
    forEachTransceiver(m_webrtcBin, [&](auto&& transceiver) -> bool {
        GstWebRTCKind transceiverKind;
        g_object_get(transceiver.get(), "kind", &transceiverKind, nullptr);
        if (transceiverKind != kind)
            return false;

        bool isTransceiverAssociated = false;
        for (auto pad : GstIteratorAdaptor<GstPad>(gst_element_iterate_sink_pads(m_webrtcBin.get()))) {
            GRefPtr<GstWebRTCRTPTransceiver> padTransceiver;
            g_object_get(pad, "transceiver", &padTransceiver.outPtr(), nullptr);
            if (padTransceiver.get() == transceiver.get()) {
                isTransceiverAssociated = true;
                break;
            }
        }
        if (isTransceiverAssociated)
            return false;

        g_object_set(transceiver.get(), "codec-preferences", caps.get(), nullptr);
        GST_DEBUG_OBJECT(m_pipeline.get(), "Expecting transceiver %" GST_PTR_FORMAT " to associate to new webrtc sink pad", transceiver.get());
        return true;
    });

    auto padTemplate = gst_element_get_pad_template(m_webrtcBin.get(), "sink_%u");
    auto sinkPad = adoptGRef(gst_element_request_pad(m_webrtcBin.get(), padTemplate, nullptr, caps.get()));

    if (!mediaStreamID.isEmpty()) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Setting msid to %s on sink pad %" GST_PTR_FORMAT, mediaStreamID.ascii().data(), sinkPad.get());
        if (gstObjectHasProperty(sinkPad.get(), "msid"_s))
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
    auto mediaStreamId = mediaStreamIds.isEmpty() ? "-"_s : mediaStreamIds[0];

    String kind;
    RTCRtpTransceiverInit init;
    init.direction = RTCRtpTransceiverDirection::Sendrecv;

    for (const auto& id : mediaStreamIds)
        init.streams.append(mediaStreamFromRTCStream(id));

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
    ALWAYS_LOG(LOGIDENTIFIER, "Adding "_s, kind, " track with id "_s, track.id());

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
    onNegotiationNeeded();
}

void GStreamerMediaEndpoint::recycleTransceiverForSenderTrack(GStreamerRtpTransceiverBackend* transceiverBackend, MediaStreamTrack& track, const FixedVector<String>& mediaStreamIds)
{
    auto mediaStreamId = mediaStreamIds.isEmpty() ? emptyString() : mediaStreamIds[0];

    auto transceiver = transceiverBackend->rtcTransceiver();
    GST_DEBUG_OBJECT(m_pipeline.get(), "Recycling transceiver %" GST_PTR_FORMAT " for track %s", transceiver,  track.id().utf8().data());

    unsigned mLineIndex;
    g_object_get(transceiver, "mlineindex", &mLineIndex, nullptr);

    GUniqueOutPtr<GstWebRTCSessionDescription> remoteDescription;
    g_object_get(m_webrtcBin.get(), "remote-description", &remoteDescription.outPtr(), nullptr);
    if (!remoteDescription)
        return;

    const auto media = gst_sdp_message_get_media(remoteDescription->sdp, mLineIndex);
    auto allowedCaps = capsFromSDPMedia(media);

    if (track.privateTrack().isAudio()) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Adding outgoing audio source");
        auto source = RealtimeOutgoingAudioSourceGStreamer::create(m_ssrcGenerator, mediaStreamId, track);
        configureSource(source.get(), nullptr);

        source->configure(WTFMove(allowedCaps));
        auto rtpCaps = source->rtpCaps();
        auto sinkPad = requestPad(rtpCaps, source->mediaStreamID());
        source->setSinkPad(WTFMove(sinkPad));
    } else {
        ASSERT(track.privateTrack().isVideo());
        GST_DEBUG_OBJECT(m_pipeline.get(), "Adding outgoing video source");
        auto source = RealtimeOutgoingVideoSourceGStreamer::create(m_ssrcGenerator, mediaStreamId, track);
        configureSource(source.get(), nullptr);

        source->configure(WTFMove(allowedCaps));
        auto rtpCaps = source->rtpCaps();
        auto sinkPad = requestPad(rtpCaps, source->mediaStreamID());
        source->setSinkPad(WTFMove(sinkPad));
    }
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
    String pendingRemoteDescription;
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

    if (holder->sdpType == RTCSdpType::Answer) {
        if (auto pendingRemoteDescription = fetchDescription(m_webrtcBin.get(), "pending-remote"_s))
            holder->pendingRemoteDescription = pendingRemoteDescription->second;
    }

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

        GUniquePtr<GstWebRTCSessionDescription> description;
        if (holder->sdpType == RTCSdpType::Answer) {
            auto updatedAnswer = holder->endPoint->completeSDPAnswer(holder->pendingRemoteDescription, sessionDescription->sdp);
            description.reset(gst_webrtc_session_description_new(sessionDescription->type, updatedAnswer.release()));
        } else
            description.reset(sessionDescription.release());

        holder->endPoint->createSessionDescriptionSucceeded(WTFMove(description));
    }, holder, reinterpret_cast<GDestroyNotify>(destroyGStreamerMediaEndpointHolder)));
}

void GStreamerMediaEndpoint::getStats(const GRefPtr<GstPad>& pad, Ref<DeferredPromise>&& promise)
{
    GST_TRACE_OBJECT(m_pipeline.get(), "Getting stats on pad %" GST_PTR_FORMAT, pad.get());
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
    auto mediaStream = m_remoteStreamsById.ensure(mediaStreamId, [&]() -> RefPtr<MediaStream> {
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return nullptr;
        auto& document = downcast<Document>(*peerConnectionBackend->connection().scriptExecutionContext());
        return MediaStream::create(document, MediaStreamPrivate::create(document.logger(), { }, WTFMove(mediaStreamId)), MediaStream::AllowEventTracks::Yes);
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

    const auto& caps = data.caps;
    GST_DEBUG_OBJECT(m_pipeline.get(), "Connecting incoming track with mid '%s' and caps %" GST_PTR_FORMAT, data.mid.ascii().data(), caps.get());
    if (!gst_caps_is_empty(caps.get()) && !gst_caps_is_any(caps.get())) [[likely]] {
        const auto structure = gst_caps_get_structure(caps.get(), 0);
        if (auto encodingName = gstStructureGetString(structure, "encoding-name"_s)) {
            if (encodingName == "TELEPHONE-EVENT"_s) {
                GST_DEBUG_OBJECT(pipeline(), "Starting incoming DTMF stream");
                gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
                return;
            }
        }
    }

    auto peerConnectionBackend = this->peerConnectionBackend();
    if (!peerConnectionBackend)
        return;

    // NOTE: Here ideally we should match WebKit-side transceivers with data.transceiver but we
    // cannot because in some situations (simulcast, mostly), we can end-up with multiple webrtcbin
    // src pads associated to the same transceiver.
    RefPtr<RTCRtpTransceiver> transceiver = peerConnectionBackend->existingTransceiver([&](auto& backend) -> bool {
        GUniqueOutPtr<char> mid;
        g_object_get(backend.rtcTransceiver(), "mid", &mid.outPtr(), nullptr);
        GST_DEBUG_OBJECT(m_pipeline.get(), "Checking if transceiver with mid %s matches the track mid", mid.get());
        return data.mid == StringView::fromLatin1(mid.get());
    });
    if (!transceiver) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Transceiver not found, checking SDP");

        GRefPtr<GstWebRTCRTPTransceiver> rtcTransceiver(data.transceiver);
        unsigned mLineIndex;
        g_object_get(rtcTransceiver.get(), "mlineindex", &mLineIndex, nullptr);
        GUniqueOutPtr<GstWebRTCSessionDescription> description;
        g_object_get(m_webrtcBin.get(), "remote-description", &description.outPtr(), nullptr);
        const auto media = gst_sdp_message_get_media(description->sdp, mLineIndex);
        if (!media) [[unlikely]] {
            GST_WARNING_OBJECT(m_pipeline.get(), "SDP media for transceiver %u not found, skipping incoming track setup", mLineIndex);
            return;
        }
        const auto& trackId = data.trackId;
        transceiver = peerConnectionBackend->newRemoteTransceiver(makeUnique<GStreamerRtpTransceiverBackend>(WTFMove(rtcTransceiver)), data.type, trackId.isolatedCopy());
        GST_DEBUG_OBJECT(m_pipeline.get(), "New remote transceiver created for track");
    }

    auto mediaStreamBin = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_pipeline.get()), data.mediaStreamBinName.ascii().data()));
    auto& track = transceiver->receiver().track();
    auto& source = track.privateTrack().source();
    if (source.isIncomingAudioSource()) {
        auto& audioSource = static_cast<RealtimeIncomingAudioSourceGStreamer&>(source);
        if (!audioSource.setBin(WTFMove(mediaStreamBin)))
            return;
    } else if (source.isIncomingVideoSource()) {
        auto& videoSource = static_cast<RealtimeIncomingVideoSourceGStreamer&>(source);
        if (!videoSource.setBin(WTFMove(mediaStreamBin)))
            return;
    }

    m_pendingIncomingTracks.append(&track.privateTrack());

    unsigned totalExpectedMediaTracks = 0;
    forEachTransceiver(m_webrtcBin, [&](auto&& transceiver) -> bool {
        GstWebRTCRTPTransceiverDirection direction;
        g_object_get(transceiver.get(), "current-direction", &direction, nullptr);
        switch (direction) {
        case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE:
        case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE:
        case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY:
            break;
        case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY:
        case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV:
            totalExpectedMediaTracks++;
            break;
        }
        return false;
    });

    GST_DEBUG_OBJECT(m_pipeline.get(), "Expecting %u media tracks", totalExpectedMediaTracks);
    if (m_pendingIncomingTracks.size() < totalExpectedMediaTracks) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Only %zu track(s) received so far", m_pendingIncomingTracks.size());
        return;
    }

    GST_DEBUG_OBJECT(m_pipeline.get(), "Incoming stream %s ready, notifying observers", data.mediaStreamId.ascii().data());
    for (auto& track : m_pendingIncomingTracks) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Incoming stream has track %s", track->id().utf8().data());
        ALWAYS_LOG(LOGIDENTIFIER, "Data flow started on track "_s, track->id());
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

    GST_DEBUG_OBJECT(m_pipeline.get(), "Connecting pad %" GST_PTR_FORMAT " with caps %" GST_PTR_FORMAT, pad, caps.get());
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
    auto dotFileName = makeString(unsafeSpan(GST_OBJECT_NAME(m_pipeline.get())), ".pending-"_s, unsafeSpan(GST_OBJECT_NAME(pad)));
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
    auto holder = createPayloadTypeHolder();
    GST_DEBUG_OBJECT(m_pipeline.get(), "Looking for unused payload type in transceivers");
    forEachTransceiver(m_webrtcBin, [&](auto&& transceiver) -> bool {
        GRefPtr<GstCaps> codecPreferences;
        g_object_get(transceiver.get(), "codec-preferences", &codecPreferences.outPtr(), nullptr);
        if (!codecPreferences)
            return false;

        gst_caps_foreach(codecPreferences.get(), reinterpret_cast<GstCapsForeachFunc>(+[](GstCapsFeatures*, GstStructure* structure, gpointer data) -> gboolean {
            auto payloadType = gstStructureGet<int>(structure, "payload"_s);
            if (!payloadType)
                return TRUE;

            auto* holder = reinterpret_cast<PayloadTypeHolder*>(data);
            holder->payloadType = std::max(holder->payloadType, *payloadType);
            return TRUE;
        }), holder);
        return false;
    });

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
    for (auto& extension : rtpExtensions) {
        if (m_rtpHeaderExtensions.contains(extension.uri))
            continue;

        // https://datatracker.ietf.org/doc/html/rfc8285#section-4.2
        // The 4-bit ID is the local identifier of this element in the range 1-14 inclusive. In the
        // signaling section, this is referred to as the valid range.
        auto identifier = m_rtpHeaderExtensions.size() + 1;
        if (identifier > 14) {
            ASSERT_NOT_REACHED_WITH_MESSAGE("Attempting to use too many RTP header extensions");
            gst_printerrln("Attempting to use too many RTP header extensions, bailing out.");
            return Exception { ExceptionCode::NotSupportedError, "Attempting to use too many RTP header extensions"_s };
        }

        m_rtpHeaderExtensions.add(extension.uri, identifier);
    }

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

    if (init.streams.isEmpty()) {
        switchOn(source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
            source->setMediaStreamID("-"_s);
        }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
            source->setMediaStreamID("-"_s);
        }, [](std::nullptr_t&) { });
    }
    StringBuilder msidBuilder;
    switchOn(source, [&, rtpHeaderExtensionMapping = m_rtpHeaderExtensions](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        msidBuilder.append(source->mediaStreamID());
        if (auto track = source->track())
            msidBuilder.append(' ', track->id());
        source->setRtpHeaderExtensionMapping(rtpHeaderExtensionMapping);
    }, [&, rtpHeaderExtensionMapping = m_rtpHeaderExtensions](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        msidBuilder.append(source->mediaStreamID());
        if (auto track = source->track())
            msidBuilder.append(' ', track->id());
        source->setRtpHeaderExtensionMapping(rtpHeaderExtensionMapping);
    }, [](std::nullptr_t&) { });

    int payloadType = pickAvailablePayloadType();
    auto msid = msidBuilder.toString();
    bool msidSet = false;
    auto caps = capsFromRtpCapabilities(m_rtpHeaderExtensions, { .codecs = codecs, .headerExtensions = rtpExtensions }, [&payloadType, &msid, &msidSet](GstStructure* structure) {
        if (!gst_structure_has_field(structure, "payload"))
            gst_structure_set(structure, "payload", G_TYPE_INT, payloadType++, nullptr);
        if (msidSet)
            return;
        if (msid.isEmpty())
            return;
        gst_structure_set(structure, "a-msid", G_TYPE_STRING, msid.utf8().data(), nullptr);
        msidSet = true;
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
        else if (std::ranges::all_of(sendEncodings, [](auto& encoding) { return encoding.scaleResolutionDownBy.value_or(1) == 1; })) {
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
        if (allRids.size() > 1 && std::ranges::any_of(allRids, [](auto& rid) { return rid.isNull() || rid.isEmpty(); }))
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
            gst_printerrln("Missing video encoder / RTP payloader. Please install an H.264 encoder and/or a VP8 encoder");
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

    return GStreamerMediaEndpoint::Backends { transceiver->createSenderBackend(WeakPtr { m_peerConnectionBackend }, WTFMove(source), WTFMove(initData)), transceiver->createReceiverBackend(), WTFMove(transceiver) };
}

ExceptionOr<GStreamerMediaEndpoint::Backends> GStreamerMediaEndpoint::addTransceiver(const String& trackKind, const RTCRtpTransceiverInit& init, PeerConnectionBackend::IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    auto direction = convertEnumerationToString(init.direction);
    ALWAYS_LOG(LOGIDENTIFIER, "Adding "_s, trackKind, " ", direction, " transceiver"_s);

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
    auto direction = convertEnumerationToString(init.direction);
    ALWAYS_LOG(LOGIDENTIFIER, "Adding "_s, track.kind().string(), " ", direction, " transceiver for track "_s, track.id());
    GST_DEBUG_OBJECT(m_pipeline.get(), "Creating transceiver associated with %s track %s", track.kind().string().ascii().data(), track.id().ascii().data());
    return createTransceiverBackends(track.kind(), init, createSourceForTrack(track), ignoreNegotiationNeededFlag);
}

std::unique_ptr<GStreamerRtpTransceiverBackend> GStreamerMediaEndpoint::transceiverBackendFromSender(GStreamerRtpSenderBackend& backend)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Looking for sender %p in existing transceivers", backend.rtcSender());
    std::unique_ptr<GStreamerRtpTransceiverBackend> result;
    forEachTransceiver(m_webrtcBin, [&](auto&& transceiver) -> bool {
        GRefPtr<GstWebRTCRTPSender> sender;
        g_object_get(transceiver.get(), "sender", &sender.outPtr(), nullptr);

        if (!sender)
            return false;

        if (sender.get() == backend.rtcSender()) {
            result = WTF::makeUnique<GStreamerRtpTransceiverBackend>(WTFMove(transceiver));
            return true;
        }
        return false;
    });
    GST_DEBUG_OBJECT(m_pipeline.get(), "Result: %p", result.get());
    return result;
}

struct AddIceCandidateCallData {
    GRefPtr<GstElement> webrtcBin;
    PeerConnectionBackend::AddIceCandidateCallback callback;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(AddIceCandidateCallData)

void GStreamerMediaEndpoint::addIceCandidate(GStreamerIceCandidate& candidate, PeerConnectionBackend::AddIceCandidateCallback&& callback)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Adding ICE candidate %s", candidate.candidate.utf8().data());

    if (!candidate.candidate.startsWith("candidate:"_s) && !candidate.candidate.startsWith("a=candidate:"_s)) {
        callOnMainThread([task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(callback))]() mutable {
            task->run(Exception { ExceptionCode::OperationError, "Expect line: candidate:<candidate-str>"_s });
        });
        return;
    }

    m_statsCollector->invalidateCache();

    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/3960
    if (gst_check_version(1, 24, 0)) {
        auto* data = createAddIceCandidateCallData();
        data->webrtcBin = m_webrtcBin;
        data->callback = WTFMove(callback);

        StringView view;
        if (candidate.candidate.startsWithIgnoringASCIICase("a="_s))
            view = candidate.candidate.substring(2);
        else
            view = candidate.candidate;

        view = view.trim(isASCIIWhitespace<char8_t>);
        g_signal_emit_by_name(m_webrtcBin.get(), "add-ice-candidate-full", candidate.sdpMLineIndex, view.toStringWithoutCopying().utf8().data(), gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) {
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

            callOnMainThread([task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(data->callback)), weakWebrtcBin = GThreadSafeWeakPtr { data->webrtcBin.get() }]() mutable {
                auto webrtcBin = weakWebrtcBin.get();
                if (!webrtcBin)
                    return;
                task->run(descriptionsFromWebRTCBin(webrtcBin.get()));
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
    callOnMainThread([task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(callback)), weakWebrtcBin = GThreadSafeWeakPtr { m_webrtcBin.get() }]() mutable {
        auto webrtcBin = weakWebrtcBin.get();
        if (!webrtcBin)
            return;
        task->run(descriptionsFromWebRTCBin(webrtcBin.get()));
    });
}

std::unique_ptr<RTCDataChannelHandler> GStreamerMediaEndpoint::createDataChannel(const String& label, const RTCDataChannelInit& options)
{
    if (!m_webrtcBin)
        return nullptr;

    auto init = GStreamerDataChannelHandler::fromRTCDataChannelInit(options);
    GST_DEBUG_OBJECT(m_pipeline.get(), "Creating data channel for init options %" GST_PTR_FORMAT " and label %s", init.get(), label.utf8().data());
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
    if (!gst_check_version(1, 22, 0))
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

        if (auto peerConnectionBackend = this->peerConnectionBackend())
            peerConnectionBackend->newDataChannel(WTFMove(channelHandler), WTFMove(label), WTFMove(dataChannelInit));
    });
}

struct AuxiliarySenderDataHolder {
    ThreadSafeWeakPtr<GStreamerMediaEndpoint> endPoint;
    GRefPtr<GstWebRTCDTLSTransport> transport;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(AuxiliarySenderDataHolder)

GstElement* GStreamerMediaEndpoint::requestAuxiliarySender(GRefPtr<GstWebRTCDTLSTransport>&& transport)
{
    // Don't use makeGStreamerElement() here because it would be called mutiple times and emit an
    // error every single time if the element is not found.
    auto* estimator = gst_element_factory_make("rtpgccbwe", nullptr);
    if (!estimator) {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [] {
            gst_printerrln("gst-plugins-rs is not installed, RTP bandwidth estimation now disabled");
        });
        return nullptr;
    }

    auto holder = createAuxiliarySenderDataHolder();
    holder->endPoint = this;
    holder->transport = WTFMove(transport);

    g_signal_connect_data(estimator, "notify::estimated-bitrate", G_CALLBACK(+[](GstElement* estimator, GParamSpec*, gpointer userData) {
        auto holder = static_cast<AuxiliarySenderDataHolder*>(userData);

        uint32_t estimatedBitrate;
        g_object_get(estimator, "estimated-bitrate", &estimatedBitrate, nullptr);

        callOnMainThread([holder, estimatedBitrate] {
            RefPtr endPoint = holder->endPoint.get();
            if (!endPoint)
                return;
            if (auto peerConnectionBackend = endPoint->peerConnectionBackend())
                peerConnectionBackend->dispatchSenderBitrateRequest(holder->transport, estimatedBitrate);
        });
    }), holder, reinterpret_cast<GClosureNotify>(+[](gpointer data, GClosure*) {
        destroyAuxiliarySenderDataHolder(static_cast<AuxiliarySenderDataHolder*>(data));
    }), static_cast<GConnectFlags>(0));

    return estimator;
}

void GStreamerMediaEndpoint::prepareForClose()
{
    if (!m_pipeline || GST_STATE(m_pipeline.get()) <= GST_STATE_READY)
        return;
    gst_element_call_async(m_pipeline.get(), reinterpret_cast<GstElementCallAsyncFunc>(+[](GstElement* element, gpointer) {
        if (GST_STATE(element) <= GST_STATE_READY)
            return;
        gst_element_set_state(element, GST_STATE_READY);
    }), nullptr, nullptr);
}

void GStreamerMediaEndpoint::close()
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Closing");

    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/9379
#if GST_CHECK_VERSION(1, 27, 0)
    g_signal_emit_by_name(m_webrtcBin.get(), "close", gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) {
        auto promise = adoptGRef(rawPromise);
        auto result = gst_promise_wait(promise.get());
        if (result != GST_PROMISE_RESULT_REPLIED)
            return;

        auto weakSelf = static_cast<ThreadSafeWeakPtr<GStreamerMediaEndpoint>*>(userData);
        auto self = weakSelf->get();
        if (!self)
            return;

        const auto* reply = gst_promise_get_reply(promise.get());
        if (reply && gst_structure_has_field_typed(reply, "error", G_TYPE_ERROR)) {
            GUniqueOutPtr<GError> error;
            gst_structure_get(reply, "error", G_TYPE_ERROR, &error.outPtr(), nullptr);
            auto errorMessage = makeString("Unable to close connection, error: "_s, unsafeSpan(error->message));
            GST_ERROR_OBJECT(self->pipeline(), "%s", errorMessage.utf8().data());
        }

        callOnMainThread([weakSelf = ThreadSafeWeakPtr { *self.get() }] {
            auto self = weakSelf.get();
            if (!self)
                return;

            if (self->pipeline())
                self->teardownPipeline();
        });

    }, new ThreadSafeWeakPtr<GStreamerMediaEndpoint> { *this }, reinterpret_cast<GDestroyNotify>(+[](gpointer data) {
        delete static_cast<ThreadSafeWeakPtr<GStreamerMediaEndpoint>*>(data);
    })));
#endif

#if !RELEASE_LOG_DISABLED
    stopLoggingStats();
#endif

#if !GST_CHECK_VERSION(1, 27, 0)
    if (m_pipeline)
        teardownPipeline();
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

    GST_DEBUG_OBJECT(m_pipeline.get(), "Scheduling negotiation-needed");
    ++m_negotiationNeededEventId;
    callOnMainThread([protectedThis = Ref(*this), this] {
        if (isStopped())
            return;
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return;

        if (peerConnectionBackend->isReconfiguring()) {
            GST_DEBUG_OBJECT(m_pipeline.get(), "replaceTrack in progress, ignoring negotiation-needed signal");
            return;
        }

        GST_DEBUG_OBJECT(m_pipeline.get(), "Negotiation needed!");
        peerConnectionBackend->markAsNeedingNegotiation(m_negotiationNeededEventId);
    });
}

void GStreamerMediaEndpoint::onIceConnectionChange()
{
    GstWebRTCICEConnectionState state;
    g_object_get(m_webrtcBin.get(), "ice-connection-state", &state, nullptr);
    callOnMainThread([protectedThis = Ref(*this), this, connectionState = toRTCIceConnectionState(state)] {
        if (isStopped())
            return;
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return;
        auto& connection = peerConnectionBackend->connection();
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
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return;
        peerConnectionBackend->iceGatheringStateChanged(toRTCIceGatheringState(state));
    });
}

void GStreamerMediaEndpoint::onIceCandidate(guint sdpMLineIndex, gchararray candidate)
{
    if (isStopped())
        return;

    String candidateString = unsafeSpan(candidate);

    // webrtcbin notifies an empty ICE candidate when gathering is complete.
    if (candidateString.isEmpty())
        return;

    callOnMainThread([protectedThis = Ref(*this), this, sdp = WTFMove(candidateString).isolatedCopy(), sdpMLineIndex]() mutable {
        if (isStopped())
            return;
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return;

        m_statsCollector->invalidateCache();

        String mid;
        GUniqueOutPtr<GstWebRTCSessionDescription> description;
        g_object_get(m_webrtcBin.get(), "local-description", &description.outPtr(), nullptr);
        if (description && sdpMLineIndex < gst_sdp_message_medias_len(description->sdp)) {
            const auto media = gst_sdp_message_get_media(description->sdp, sdpMLineIndex);
            mid = unsafeSpan(gst_sdp_media_get_attribute_val(media, "mid"));
        }

        auto descriptions = descriptionsFromWebRTCBin(m_webrtcBin.get());
        GST_DEBUG_OBJECT(m_pipeline.get(), "Notifying ICE candidate: %s", sdp.ascii().data());
        peerConnectionBackend->newICECandidate(WTFMove(sdp), WTFMove(mid), sdpMLineIndex, { }, WTFMove(descriptions));
    });
}

void GStreamerMediaEndpoint::createSessionDescriptionSucceeded(GUniquePtr<GstWebRTCSessionDescription>&& description)
{
    callOnMainThread([protectedThis = Ref(*this), this, description = WTFMove(description)] {
        if (isStopped())
            return;
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return;

        auto sdpString = sdpAsString(description->sdp);
#ifndef GST_DISABLE_GST_DEBUG
        GST_DEBUG_OBJECT(pipeline(), "Created SDP %s: %s", description->type == GST_WEBRTC_SDP_TYPE_OFFER ? "offer" : "answer", sdpString.utf8().data());
#endif
        if (description->type == GST_WEBRTC_SDP_TYPE_OFFER) {
            peerConnectionBackend->createOfferSucceeded(WTFMove(sdpString));
            return;
        }

        if (description->type == GST_WEBRTC_SDP_TYPE_ANSWER) {
            peerConnectionBackend->createAnswerSucceeded(WTFMove(sdpString));
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
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return;

        auto exc = Exception { ExceptionCode::OperationError, error ? String::fromUTF8(error->message) : "Unknown Error"_s };
        if (sdpType == RTCSdpType::Offer) {
            peerConnectionBackend->createOfferFailed(WTFMove(exc));
            return;
        }
        peerConnectionBackend->createAnswerFailed(WTFMove(exc));
    });
}

void GStreamerMediaEndpoint::collectTransceivers()
{
    GUniqueOutPtr<GstWebRTCSessionDescription> description;
    g_object_get(m_webrtcBin.get(), "remote-description", &description.outPtr(), nullptr);
    if (!description)
        return;

    auto peerConnectionBackend = this->peerConnectionBackend();
    if (!peerConnectionBackend)
        return;

    GST_DEBUG_OBJECT(m_pipeline.get(), "Collecting transceivers");
    forEachTransceiver(m_webrtcBin, [&](auto&& transceiver) -> bool {
        RefPtr existingTransceiver = peerConnectionBackend->existingTransceiver([&](auto& transceiverBackend) {
            return transceiver.get() == transceiverBackend.rtcTransceiver();
        });
        if (existingTransceiver)
            return false;

        GUniqueOutPtr<char> mid;
        unsigned mLineIndex;
        g_object_get(transceiver.get(), "mid", &mid.outPtr(), "mlineindex", &mLineIndex, nullptr);
        if (!mid)
            return false;

        const auto* media = gst_sdp_message_get_media(description->sdp, mLineIndex);
        if (!media) [[unlikely]] {
            GST_WARNING_OBJECT(m_pipeline.get(), "SDP media for transceiver %u not found, skipping registration", mLineIndex);
            return false;
        }

        existingTransceiver = peerConnectionBackend->newRemoteTransceiver(WTF::makeUnique<GStreamerRtpTransceiverBackend>(WTFMove(transceiver)), m_mediaForMid.get(String::fromUTF8(mid.get())), trackIdFromSDPMedia(*media));
        return false;
    });
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
        auto peerConnectionBackend = this->peerConnectionBackend();
        if (!peerConnectionBackend)
            return nullptr;

        for (auto& sender : peerConnectionBackend->connection().getSenders()) {
            auto& backend = peerConnectionBackend->backendFromRTPSender(sender);
            GUniquePtr<GstStructure> stats;
            if (auto* videoSource = backend.videoSource())
                stats = videoSource->stats();
            else if (auto audioSource = backend.audioSource())
                stats = audioSource->stats();

            if (!stats)
                continue;

            mergeStructureInAdditionalStats(stats.get());
        }
        for (auto& receiver : peerConnectionBackend->connection().getReceivers()) {
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

    bool hasInboundAudioStats = false;
    bool hasOutboundAudioStats = false;
    bool hasInboundVideoStats = false;
    bool hasOutboundVideoStats = false;
    Seconds convertedTimestamp;
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
            if (auto framesPerSecond = gstStructureGet<double>(additionalStats.get(), "frames-per-second"_s))
                gst_structure_set(structure.get(), "frames-per-second", G_TYPE_DOUBLE, *framesPerSecond, nullptr);
            if (auto totalDecodeTime = gstStructureGet<double>(additionalStats.get(), "total-decode-time"_s))
                gst_structure_set(structure.get(), "total-decode-time", G_TYPE_DOUBLE, *totalDecodeTime, nullptr);
            auto trackIdentifier = gstStructureGetString(additionalStats.get(), "track-identifier"_s);
            if (!trackIdentifier.isEmpty())
                gst_structure_set(structure.get(), "track-identifier", G_TYPE_STRING, trackIdentifier.utf8(), nullptr);
            auto kind = gstStructureGetString(structure.get(), "kind"_s);
            if (kind == "audio"_s)
                hasInboundAudioStats = true;
            else if (kind == "video"_s)
                hasInboundVideoStats = true;
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
                gst_structure_set(structure.get(), "mid", G_TYPE_STRING, midValue.utf8(), nullptr);
            if (auto ridValue = gstStructureGetString(ssrcStats.get(), "rid"_s))
                gst_structure_set(structure.get(), "rid", G_TYPE_STRING, ridValue.utf8(), nullptr);
            auto kind = gstStructureGetString(structure.get(), "kind"_s);
            if (kind == "audio"_s)
                hasOutboundAudioStats = true;
            else if (kind == "video"_s)
                hasOutboundVideoStats = true;
            break;
        }
        default:
            break;
        };

        auto timestamp = gstStructureGet<double>(structure.get(), "timestamp"_s);
        if (timestamp) [[unlikely]] {
            auto newTimestamp = StatsTimestampConverter::singleton().convertFromMonotonicTime(Seconds::fromMilliseconds(*timestamp));
            convertedTimestamp = newTimestamp;
            gst_structure_set(structure.get(), "timestamp", G_TYPE_DOUBLE, newTimestamp.microseconds(), nullptr);
        }

        gst_value_set_structure(value, structure.get());
        return TRUE;
    });

    if (!hasInboundVideoStats) {
        GUniquePtr<GstStructure> emptyInboundStats(gst_structure_new_empty("rtp-inbound-video-stream-stats"));
        gst_structure_set(emptyInboundStats.get(), "type", GST_TYPE_WEBRTC_STATS_TYPE, GST_WEBRTC_STATS_INBOUND_RTP, "timestamp",
            G_TYPE_DOUBLE, convertedTimestamp.microseconds(), "id", G_TYPE_STRING, "rtp-inbound-video-stream-stats", "kind", G_TYPE_STRING, "video", "frames-decoded", G_TYPE_UINT64, 0, nullptr);
        gst_structure_set(result.get(), "rtp-inbound-video-stream-stats", GST_TYPE_STRUCTURE, emptyInboundStats.get(), nullptr);
    }

    if (!hasInboundAudioStats) {
        GUniquePtr<GstStructure> emptyInboundStats(gst_structure_new_empty("rtp-inbound-audio-stream-stats"));
        gst_structure_set(emptyInboundStats.get(), "type", GST_TYPE_WEBRTC_STATS_TYPE, GST_WEBRTC_STATS_INBOUND_RTP, "timestamp",
            G_TYPE_DOUBLE, convertedTimestamp.microseconds(), "id", G_TYPE_STRING, "rtp-inbound-audio-stream-stats", "kind", G_TYPE_STRING, "audio", nullptr);
        gst_structure_set(result.get(), "rtp-inbound-audio-stream-stats", GST_TYPE_STRUCTURE, emptyInboundStats.get(), nullptr);
    }

    if (!hasOutboundVideoStats) {
        GUniquePtr<GstStructure> emptyOutboundStats(gst_structure_new_empty("rtp-outbound-video-stream-stats"));
        gst_structure_set(emptyOutboundStats.get(), "type", GST_TYPE_WEBRTC_STATS_TYPE, GST_WEBRTC_STATS_OUTBOUND_RTP, "timestamp",
            G_TYPE_DOUBLE, convertedTimestamp.microseconds(), "id", G_TYPE_STRING, "rtp-outbound-video-stream-stats", "kind", G_TYPE_STRING, "video", "frames-encoded", G_TYPE_UINT64, 0, nullptr);
        gst_structure_set(result.get(), "rtp-outbound-video-stream-stats", GST_TYPE_STRUCTURE, emptyOutboundStats.get(), nullptr);
    }

    if (!hasOutboundAudioStats) {
        GUniquePtr<GstStructure> emptyOutboundStats(gst_structure_new_empty("rtp-outbound-audio-stream-stats"));
        gst_structure_set(emptyOutboundStats.get(), "type", GST_TYPE_WEBRTC_STATS_TYPE, GST_WEBRTC_STATS_OUTBOUND_RTP, "timestamp",
            G_TYPE_DOUBLE, convertedTimestamp.microseconds(), "id", G_TYPE_STRING, "rtp-outbound-audio-stream-stats", "kind", G_TYPE_STRING, "audio", nullptr);
        gst_structure_set(result.get(), "rtp-outbound-audio-stream-stats", GST_TYPE_STRUCTURE, emptyOutboundStats.get(), nullptr);
    }
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

    auto peerConnectionBackend = this->peerConnectionBackend();
    if (!peerConnectionBackend)
        return;

    if (peerConnectionBackend->isJSONLogStreamingEnabled()) {
        auto event = peerConnectionBackend->generateJSONLogEvent(gstStructureToJSONString(structure), false);
        peerConnectionBackend->emitJSONLogEvent(WTFMove(event));
    }

    if (m_isGatheringRTCLogs) {
        auto event = peerConnectionBackend->generateJSONLogEvent(gstStructureToJSONString(structure), true);
        peerConnectionBackend->provideStatLogs(WTFMove(event));
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
    auto peerConnectionBackend = this->peerConnectionBackend();
    if (!peerConnectionBackend)
        return;

    if (!WebCore::logChannels().isLogChannelEnabled("WebRTC"_s) && !peerConnectionBackend->isJSONLogStreamingEnabled() && !m_isGatheringRTCLogs)
        return;

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

        auto values = makeString(unsafeSpan(attribute->value)).split(' ');
        if (values.contains("trickle"_s))
            return true;
    }
    return false;
}


void GStreamerMediaEndpoint::updatePtDemuxSrcPadCaps(GstElement* ptDemux, GstPad* pad)
{
    GUniqueOutPtr<GstWebRTCSessionDescription> description;
    g_object_get(m_webrtcBin.get(), "current-remote-description", &description.outPtr(), nullptr);
    if (!description)
        return;

    auto currentCaps = adoptGRef(gst_pad_get_current_caps(pad));

    auto sinkPad = adoptGRef(gst_element_get_static_pad(ptDemux, "sink"));
    auto sinkCaps = adoptGRef(gst_pad_get_current_caps(sinkPad.get()));
    const auto structure = gst_caps_get_structure(sinkCaps.get(), 0);
    auto ssrc = gstStructureGet<unsigned>(structure, "ssrc"_s);
    if (!ssrc)
        return;

    auto buffer = m_inputBuffers.take(*ssrc);
    if (!buffer)
        return;

    const auto currentStructure = gst_caps_get_structure(currentCaps.get(), 0);
    if (auto encodingName = gstStructureGetString(currentStructure, "encoding-name"_s)) {
        if (encodingName == "TELEPHONE-EVENT"_s) {
            GST_DEBUG_OBJECT(pipeline(), "Incoming DTMF stream detected, no need to look for MID/RID.");
            return;
        }
    }

    GstMappedRtpBuffer rtpBuffer(buffer, GST_MAP_READ);
    if (!rtpBuffer) [[unlikely]]
        return;

    auto caps = extractMidAndRidFromRTPBuffer(rtpBuffer, description->sdp);
    if (!caps) {
        GST_DEBUG_OBJECT(pipeline(), "mid attribute not found in buffer %" GST_PTR_FORMAT, buffer.get());
        return;
    }

    // Propagate several caps fields from the previous caps to the new caps.
    auto s = gst_caps_get_structure(caps.get(), 0);
    auto s2 = gst_caps_get_structure(currentCaps.get(), 0);
    for (int j = 0; j < gst_structure_n_fields(s2); j++) {
        auto name = CStringView::unsafeFromUTF8(gst_structure_nth_field_name(s2, j));
        if (name != "media"_s && name != "payload"_s && name != "clock-rate"_s && name != "encoding-name"_s)
            continue;
        gst_structure_set_value(s, name.utf8(), gst_structure_get_value(s2, name.utf8()));
    }

    // Remove "ssrc-*" attributes matching other SSRCs.
    gstStructureFilterAndMapInPlace(s, [&](auto id, auto) -> bool {
        auto idString = gstIdToString(id);
        if (!idString.startsWith("ssrc-"_s))
            return true;

        auto value = parseInteger<unsigned>(idString.substring(5));
        if (!value)
            return true;

        return *value == *ssrc;
    });

    gst_caps_set_simple(caps.get(), "ssrc", G_TYPE_UINT, *ssrc, nullptr);

    GST_DEBUG_OBJECT(pipeline(), "mid and rid attribute set from buffer on caps %" GST_PTR_FORMAT, caps.get());
    gst_pad_set_caps(pad, caps.get());
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

GUniquePtr<GstSDPMessage> GStreamerMediaEndpoint::completeSDPAnswer(const String& pendingRemoteDescription, const GstSDPMessage* sdp)
{
    GUniqueOutPtr<GstSDPMessage> pendingSDP;
    GUniqueOutPtr<GstSDPMessage> message;
    gst_sdp_message_new_from_text(pendingRemoteDescription.utf8().data(), &pendingSDP.outPtr());

    gst_sdp_message_copy(sdp, &message.outPtr());

    // As per RFC 8829 section 5.3.1, "For each supported RTP header extension that is
    // present in the offer, an "a=extmap" line" should be added to the answer.
    unsigned totalMedias = gst_sdp_message_medias_len(message.get());
    for (unsigned i = 0; i < totalMedias; i++) {
        const auto offerMedia = gst_sdp_message_get_media(pendingSDP.get(), i);
        auto media = const_cast<GstSDPMedia*>(gst_sdp_message_get_media(message.get(), i));

        unsigned totalAttributes = gst_sdp_media_attributes_len(offerMedia);
        for (unsigned ii = 0; ii < totalAttributes; ii++) {
            const auto attribute = gst_sdp_media_get_attribute(offerMedia, ii);
            auto key = StringView::fromLatin1(attribute->key);
            if (key != "extmap"_s)
                continue;

            auto value = StringView::fromLatin1(attribute->value);
            Vector<String> tokens = value.toStringWithoutCopying().split(' ');
            if (tokens.size() < 2) [[unlikely]]
                continue;

            if (!sdpMediaHasRTPHeaderExtension(media, tokens[1]))
                gst_sdp_media_add_attribute(media, attribute->key, attribute->value);
        }
    }

    return GUniquePtr<GstSDPMessage>(message.release());
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
