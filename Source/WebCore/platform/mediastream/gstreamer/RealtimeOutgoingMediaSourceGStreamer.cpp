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
#include "RealtimeOutgoingMediaSourceGStreamer.h"

#if USE(GSTREAMER_WEBRTC)

#include "GStreamerCommon.h"
#include "GStreamerMediaStreamSource.h"
#include "MediaStreamTrack.h"

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#undef GST_USE_UNSTABLE_API

#include <wtf/UUID.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY(webkit_webrtc_outgoing_media_debug);
#define GST_CAT_DEFAULT webkit_webrtc_outgoing_media_debug

namespace WebCore {

RealtimeOutgoingMediaSourceGStreamer::RealtimeOutgoingMediaSourceGStreamer(Type type, const RefPtr<UniqueSSRCGenerator>& ssrcGenerator, const String& mediaStreamId, MediaStreamTrack& track)
    : m_type(type)
    , m_mediaStreamId(mediaStreamId)
    , m_trackId(track.id())
    , m_ssrcGenerator(ssrcGenerator)
{
    initialize();

    if (track.isCanvas()) {
        m_liveSync = makeGStreamerElement("livesync", nullptr);
        if (!m_liveSync) {
            GST_WARNING_OBJECT(m_bin.get(), "GStreamer element livesync not found. Canvas streaming to PeerConnection will not work as expected, falling back to identity element.");
            m_liveSync = gst_element_factory_make("identity", nullptr);
        }
    } else
        m_liveSync = gst_element_factory_make("identity", nullptr);
    gst_bin_add(GST_BIN_CAST(m_bin.get()), m_liveSync.get());

    // Both livesync and identity have a single-segment property, so no need for checks here.
    g_object_set(m_liveSync.get(), "single-segment", TRUE, nullptr);

    m_track = &track.privateTrack();
    m_outgoingSource = webkitMediaStreamSrcNew();
    GST_DEBUG_OBJECT(m_bin.get(), "Created outgoing source %" GST_PTR_FORMAT, m_outgoingSource.get());
    gst_bin_add(GST_BIN_CAST(m_bin.get()), m_outgoingSource.get());
    webkitMediaStreamSrcAddTrack(WEBKIT_MEDIA_STREAM_SRC(m_outgoingSource.get()), m_track.get());
}

RealtimeOutgoingMediaSourceGStreamer::RealtimeOutgoingMediaSourceGStreamer(Type type, const RefPtr<UniqueSSRCGenerator>& ssrcGenerator)
    : m_type(type)
    , m_mediaStreamId(createVersion4UUIDString())
    , m_trackId(emptyString())
    , m_ssrcGenerator(ssrcGenerator)
{
    initialize();

    m_liveSync = gst_element_factory_make("identity", nullptr);
    g_object_set(m_liveSync.get(), "single-segment", TRUE, nullptr);
    gst_bin_add(GST_BIN_CAST(m_bin.get()), m_liveSync.get());
}

RealtimeOutgoingMediaSourceGStreamer::~RealtimeOutgoingMediaSourceGStreamer()
{
    stopUpdatingStats();
    if (m_transceiver)
        g_signal_handlers_disconnect_by_data(m_transceiver.get(), this);

    if (m_track)
        m_track->removeObserver(*this);
}

void RealtimeOutgoingMediaSourceGStreamer::initialize()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_outgoing_media_debug, "webkitwebrtcoutgoingmedia", 0, "WebKit WebRTC outgoing media");
    });

    m_bin = gst_bin_new(nullptr);

    m_tee = gst_element_factory_make("tee", nullptr);

    m_rtpFunnel = gst_element_factory_make("rtpfunnel", nullptr);
    if (gstObjectHasProperty(m_rtpFunnel.get(), "forward-unknown-ssrc"))
        g_object_set(m_rtpFunnel.get(), "forward-unknown-ssrc", TRUE, nullptr);

    m_rtpCapsfilter = gst_element_factory_make("capsfilter", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_tee.get(), m_rtpFunnel.get(), m_rtpCapsfilter.get(), nullptr);
    gst_element_link(m_rtpFunnel.get(), m_rtpCapsfilter.get());

    auto srcPad = adoptGRef(gst_element_get_static_pad(m_rtpCapsfilter.get(), "src"));
    gst_element_add_pad(m_bin.get(), gst_ghost_pad_new("src", srcPad.get()));
}

const GRefPtr<GstCaps>& RealtimeOutgoingMediaSourceGStreamer::allowedCaps() const
{
    if (m_allowedCaps)
        return m_allowedCaps;

    auto sdpMsIdLine = makeString(m_mediaStreamId, ' ', m_trackId);
    m_allowedCaps = capsFromRtpCapabilities(rtpCapabilities(), [&sdpMsIdLine](GstStructure* structure) {
        gst_structure_set(structure, "a-msid", G_TYPE_STRING, sdpMsIdLine.utf8().data(), nullptr);
    });

    GST_DEBUG_OBJECT(m_bin.get(), "Allowed caps: %" GST_PTR_FORMAT, m_allowedCaps.get());
    return m_allowedCaps;
}

GRefPtr<GstCaps> RealtimeOutgoingMediaSourceGStreamer::rtpCaps() const
{
    GRefPtr<GstCaps> caps;
    g_object_get(m_rtpCapsfilter.get(), "caps", &caps.outPtr(), nullptr);
    return caps;
}

void RealtimeOutgoingMediaSourceGStreamer::start()
{
    if (!m_isStopped) {
        GST_DEBUG_OBJECT(m_bin.get(), "Source already started");
        return;
    }

    GST_DEBUG_OBJECT(m_bin.get(), "Starting outgoing source");
    m_track->addObserver(*this);
    m_isStopped = false;

    if (m_transceiver) {
        auto pad = adoptGRef(gst_element_get_static_pad(m_liveSync.get(), "src"));
        if (!gst_pad_is_linked(pad.get())) {
            GST_DEBUG_OBJECT(m_bin.get(), "Codec preferences haven't changed before startup, ensuring source is linked");
            codecPreferencesChanged();
        }
    }

    gst_element_link(m_outgoingSource.get(), m_liveSync.get());
    gst_element_sync_state_with_parent(m_bin.get());

    startUpdatingStats();
}

void RealtimeOutgoingMediaSourceGStreamer::stop()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Stopping outgoing source");
    m_isStopped = true;
    stopOutgoingSource();
    m_track = nullptr;
}

void RealtimeOutgoingMediaSourceGStreamer::stopOutgoingSource()
{
    if (!m_track)
        return;

    GST_DEBUG_OBJECT(m_bin.get(), "Stopping outgoing source %" GST_PTR_FORMAT, m_outgoingSource.get());
    m_track->removeObserver(*this);

    if (!m_outgoingSource)
        return;

    if (WEBKIT_IS_MEDIA_STREAM_SRC(m_outgoingSource.get()))
        webkitMediaStreamSrcSignalEndOfStream(WEBKIT_MEDIA_STREAM_SRC_CAST(m_outgoingSource.get()));
    else
        gst_element_send_event(m_outgoingSource.get(), gst_event_new_eos());

    gst_element_set_locked_state(m_outgoingSource.get(), TRUE);

    gst_element_unlink(m_outgoingSource.get(), m_liveSync.get());

    gst_element_set_state(m_outgoingSource.get(), GST_STATE_NULL);
    gst_bin_remove(GST_BIN_CAST(m_bin.get()), m_outgoingSource.get());
    gst_element_set_locked_state(m_outgoingSource.get(), FALSE);
    m_outgoingSource.clear();
}

void RealtimeOutgoingMediaSourceGStreamer::sourceMutedChanged()
{
    if (!m_track)
        return;
    ASSERT(m_muted != m_track->muted());
    m_muted = m_track->muted();
    GST_DEBUG_OBJECT(m_bin.get(), "Mute state changed to %s", boolForPrinting(m_muted));
}

void RealtimeOutgoingMediaSourceGStreamer::sourceEnabledChanged()
{
    if (!m_track)
        return;

    m_enabled = m_track->enabled();
    GST_DEBUG_OBJECT(m_bin.get(), "Enabled state changed to %s", boolForPrinting(m_enabled));
    if (m_enabled)
        startUpdatingStats();
    else
        stopUpdatingStats();
}

void RealtimeOutgoingMediaSourceGStreamer::initializeSourceFromTrackPrivate()
{
    if (!m_track)
        return;
    m_muted = m_track->muted();
    m_enabled = m_track->enabled();
    GST_DEBUG_OBJECT(m_bin.get(), "Initializing from track, muted: %s, enabled: %s", boolForPrinting(m_muted), boolForPrinting(m_enabled));
}

void RealtimeOutgoingMediaSourceGStreamer::link()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Linking webrtcbin pad %" GST_PTR_FORMAT, m_webrtcSinkPad.get());

    auto srcPad = adoptGRef(gst_element_get_static_pad(m_bin.get(), "src"));
    gst_pad_link(srcPad.get(), m_webrtcSinkPad.get());
}

void RealtimeOutgoingMediaSourceGStreamer::setSinkPad(GRefPtr<GstPad>&& pad)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Associating with webrtcbin pad %" GST_PTR_FORMAT, pad.get());
    m_webrtcSinkPad = WTFMove(pad);

    if (m_transceiver)
        g_signal_handlers_disconnect_by_data(m_transceiver.get(), this);

    g_object_get(m_webrtcSinkPad.get(), "transceiver", &m_transceiver.outPtr(), nullptr);

    g_signal_connect_swapped(m_transceiver.get(), "notify::codec-preferences", G_CALLBACK(+[](RealtimeOutgoingMediaSourceGStreamer* source) {
        source->codecPreferencesChanged();
    }), this);
    g_object_get(m_transceiver.get(), "sender", &m_sender.outPtr(), nullptr);

    checkMid();
    g_signal_connect_swapped(m_transceiver.get(), "notify::mid", G_CALLBACK(+[](RealtimeOutgoingMediaSourceGStreamer* source) {
        source->checkMid();
    }), this);
}

void RealtimeOutgoingMediaSourceGStreamer::checkMid()
{
    GUniqueOutPtr<char> mid;
    g_object_get(m_transceiver.get(), "mid", &mid.outPtr(), nullptr);
    if (!mid)
        return;

    auto newMid = makeString(span(mid.get()));
    if (newMid == m_mid)
        return;

    m_mid = WTFMove(newMid);
    for (auto& packetizer : m_packetizers)
        packetizer->ensureMidExtension(m_mid);

    GRefPtr<GstCaps> rtpCaps;
    g_object_get(m_rtpCapsfilter.get(), "caps", &rtpCaps.outPtr(), nullptr);
    if (gst_caps_is_any(rtpCaps.get()) || !gst_caps_get_size(rtpCaps.get()))
        return;

    GUniquePtr<GstStructure> structure(gst_structure_copy(gst_caps_get_structure(rtpCaps.get(), 0)));
    auto lookupResults = lookupRtpExtensions(structure.get());
    if (lookupResults.hasMidExtension)
        return;

    lookupResults.lastIdentifier++;
    auto extensionIdentifier = makeString("extmap-"_s, lookupResults.lastIdentifier);
    gst_structure_set(structure.get(), extensionIdentifier.ascii().data(), G_TYPE_STRING, GST_RTP_HDREXT_BASE "sdes:mid", nullptr);

    auto newCaps = adoptGRef(gst_caps_new_full(structure.release(), nullptr));
    GST_DEBUG_OBJECT(m_bin.get(), "Setting RTP funnel caps to %" GST_PTR_FORMAT, newCaps.get());
    g_object_set(m_rtpCapsfilter.get(), "caps", newCaps.get(), nullptr);
}

GUniquePtr<GstStructure> RealtimeOutgoingMediaSourceGStreamer::parameters()
{
    if (!m_parameters)
        return nullptr;
    return GUniquePtr<GstStructure>(gst_structure_copy(m_parameters.get()));
}

void RealtimeOutgoingMediaSourceGStreamer::codecPreferencesChanged()
{
    if (GST_STATE(m_bin.get()) > GST_STATE_READY) {
        GST_WARNING_OBJECT(m_bin.get(), "Changing codec preferences on an ongoing connection is not supported");
        return;
    }

    GRefPtr<GstCaps> codecPreferences;
    g_object_get(m_transceiver.get(), "codec-preferences", &codecPreferences.outPtr(), nullptr);
    GST_DEBUG_OBJECT(m_bin.get(), "Codec preferences changed on transceiver %" GST_PTR_FORMAT " to: %" GST_PTR_FORMAT, m_transceiver.get(), codecPreferences.get());

    HashMap<int, unsigned> payloaderStates;
    while (!m_packetizers.isEmpty()) {
        RefPtr packetizer = m_packetizers.takeLast();

        int payloadType = packetizer->payloadType();
        unsigned sequenceNumber = packetizer->currentSequenceNumberOffset();
        payloaderStates.add(payloadType, sequenceNumber);

        auto bin = packetizer->bin();
        auto binSinkPad = adoptGRef(gst_element_get_static_pad(bin, "sink"));
        auto teeSrcPad = adoptGRef(gst_pad_get_peer(binSinkPad.get()));
        auto binSrcPad = adoptGRef(gst_element_get_static_pad(bin, "src"));
        auto funnelSinkPad = adoptGRef(gst_pad_get_peer(binSrcPad.get()));
        gst_element_set_state(bin, GST_STATE_NULL);
        gst_bin_remove(GST_BIN_CAST(m_bin.get()), bin);
        gst_element_release_request_pad(m_tee.get(), teeSrcPad.get());
        gst_element_release_request_pad(m_rtpFunnel.get(), funnelSinkPad.get());
    }

    if (!configurePacketizers(WTFMove(codecPreferences))) {
        GST_ERROR_OBJECT(m_bin.get(), "Unable to link encoder to webrtcbin");
        return;
    }

    for (auto& packetizer : m_packetizers) {
        int payloadType = packetizer->payloadType();
        if (!payloaderStates.contains(payloadType))
            continue;
        packetizer->setSequenceNumberOffset(payloaderStates.get(payloadType));
    }

    gst_bin_sync_children_states(GST_BIN_CAST(m_bin.get()));
    gst_element_sync_state_with_parent(m_bin.get());
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_bin.get()), GST_DEBUG_GRAPH_SHOW_ALL, "outgoing-media-new-codec-prefs");
    m_isStopped = false;
}

void RealtimeOutgoingMediaSourceGStreamer::replaceTrack(RefPtr<MediaStreamTrackPrivate>&& newTrack)
{
    if (!m_track)
        return;

    m_track->removeObserver(*this);
    webkitMediaStreamSrcReplaceTrack(WEBKIT_MEDIA_STREAM_SRC_CAST(m_outgoingSource.get()), RefPtr(newTrack));
    m_track = WTFMove(newTrack);
    m_track->addObserver(*this);
}

void RealtimeOutgoingMediaSourceGStreamer::setInitialParameters(GUniquePtr<GstStructure>&& parameters)
{
    m_parameters = WTFMove(parameters);
    GST_DEBUG_OBJECT(m_bin.get(), "Initial encoding parameters: %" GST_PTR_FORMAT, m_parameters.get());
}

void RealtimeOutgoingMediaSourceGStreamer::configure(GRefPtr<GstCaps>&& allowedCaps)
{
    const auto encodingsValue = gst_structure_get_value(m_parameters.get(), "encodings");
    RELEASE_ASSERT(GST_VALUE_HOLDS_LIST(encodingsValue));
    unsigned encodingsSize = gst_value_list_get_size(encodingsValue);
    if (UNLIKELY(!encodingsSize)) {
        GST_WARNING_OBJECT(m_bin.get(), "Encodings list is empty, cancelling configuration");
        return;
    }

    configurePacketizers(WTFMove(allowedCaps));
}

void RealtimeOutgoingMediaSourceGStreamer::setParameters(GUniquePtr<GstStructure>&& parameters)
{
    GST_DEBUG_OBJECT(m_bin.get(), "New encoding parameters: %" GST_PTR_FORMAT, parameters.get());
    const auto encodingsValue = gst_structure_get_value(parameters.get(), "encodings");
    RELEASE_ASSERT(GST_VALUE_HOLDS_LIST(encodingsValue));
    unsigned encodingsSize = gst_value_list_get_size(encodingsValue);
    if (UNLIKELY(!encodingsSize)) {
        GST_WARNING_OBJECT(m_bin.get(), "Encodings list is empty, cancelling re-configuration");
        return;
    }

    for (unsigned j = 0; j < encodingsSize; j++) {
        auto encoding = gst_value_list_get_value(encodingsValue, j);
        RELEASE_ASSERT(GST_VALUE_HOLDS_STRUCTURE(encoding));
        GUniquePtr<GstStructure> encodingParameters(gst_structure_copy(gst_value_get_structure(encoding)));
        auto rid = gstStructureGetString(encodingParameters.get(), "rid"_s);
        if (!rid)
            continue;

        auto packetizer = getPacketizerForRid(rid);
        if (!packetizer)
            continue;

        packetizer->reconfigure(WTFMove(encodingParameters));
    }
    m_parameters = WTFMove(parameters);
}

RefPtr<GStreamerRTPPacketizer> RealtimeOutgoingMediaSourceGStreamer::getPacketizerForRid(StringView rid)
{
    for (auto& packetizer : m_packetizers) {
        if (packetizer->rtpStreamId() == rid)
            return packetizer;
    }
    return nullptr;
}

bool RealtimeOutgoingMediaSourceGStreamer::linkPacketizer(RefPtr<GStreamerRTPPacketizer>&& packetizer)
{
    auto packetizerBin = packetizer->bin();
    gst_bin_add(GST_BIN_CAST(m_bin.get()), packetizerBin);

    GST_DEBUG_OBJECT(m_bin.get(), "Linking packetizer %" GST_PTR_FORMAT " to RTP funnel", packetizerBin);
    if (!gst_element_link_many(m_tee.get(), packetizerBin, m_rtpFunnel.get(), nullptr)) {
        GST_ERROR_OBJECT(m_bin.get(), "Unable to link packetizer to RTP funnel");
        gst_bin_remove(GST_BIN_CAST(m_bin.get()), packetizerBin);
        return false;
    }
    m_packetizers.append(WTFMove(packetizer));
    return true;
}

bool RealtimeOutgoingMediaSourceGStreamer::configurePacketizers(GRefPtr<GstCaps>&& codecPreferences)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Configuring packetizers for caps %" GST_PTR_FORMAT, codecPreferences.get());
    if (UNLIKELY(gst_caps_is_empty(codecPreferences.get()) || gst_caps_is_any(codecPreferences.get())))
        return false;

    if (m_outgoingSource) {
        auto srcPad = outgoingSourcePad();
        if (!gst_pad_is_linked(srcPad.get()))
            gst_element_link(m_outgoingSource.get(), m_liveSync.get());
    }

    auto teeSinkPad = adoptGRef(gst_element_get_static_pad(m_tee.get(), "sink"));
    if (!gst_pad_is_linked(teeSinkPad.get()) && !linkTee()) {
        GST_ERROR_OBJECT(m_bin.get(), "Unable to link tee");
        return false;
    }

    auto rtpCaps = adoptGRef(gst_caps_new_empty());
    unsigned totalCodecs = gst_caps_get_size(codecPreferences.get());
    for (unsigned i = 0; i < totalCodecs; i++) {
        const auto codecParameters = gst_caps_get_structure(codecPreferences.get(), i);

        if (m_parameters) {
            const auto encodingsValue = gst_structure_get_value(m_parameters.get(), "encodings");
            RELEASE_ASSERT(GST_VALUE_HOLDS_LIST(encodingsValue));
            auto totalEncodings = gst_value_list_get_size(encodingsValue);
            if (UNLIKELY(!totalEncodings)) {
                auto packetizer = createPacketizer(m_ssrcGenerator, codecParameters, nullptr);
                if (!packetizer)
                    continue;

                if (linkPacketizer(WTFMove(packetizer))) {
                    gst_caps_append_structure(rtpCaps.get(), gst_structure_copy(codecParameters));
                    break;
                }
            }

            bool codecIsValid = false;
            for (unsigned j = 0; j < totalEncodings; j++) {
                auto encoding = gst_value_list_get_value(encodingsValue, j);
                RELEASE_ASSERT(GST_VALUE_HOLDS_STRUCTURE(encoding));
                GUniquePtr<GstStructure> encodingParameters(gst_structure_copy(gst_value_get_structure(encoding)));
                auto packetizer = createPacketizer(m_ssrcGenerator, codecParameters, WTFMove(encodingParameters));
                if (!packetizer)
                    continue;

                auto rtpParameters = packetizer->rtpParameters();
                if (UNLIKELY(!rtpParameters))
                    continue;

                codecIsValid = linkPacketizer(WTFMove(packetizer));
                if (!codecIsValid)
                    break;

                gst_caps_append_structure(rtpCaps.get(), rtpParameters.release());
            }

            // TODO: Check optional "codecs" field.

            if (codecIsValid)
                break;

        } else {
            auto packetizer = createPacketizer(m_ssrcGenerator, codecParameters, nullptr);
            if (!packetizer)
                continue;

            auto rtpParameters = packetizer->rtpParameters();
            if (UNLIKELY(!rtpParameters))
                continue;
            if (linkPacketizer(WTFMove(packetizer))) {
                gst_caps_append_structure(rtpCaps.get(), rtpParameters.release());
                break;
            }
        }
    }
    if (m_packetizers.isEmpty()) {
        GST_ERROR_OBJECT(m_bin.get(), "Unable to link any packetizer");
        return false;
    }

    auto structure = gst_caps_get_structure(rtpCaps.get(), 0);

    auto payloadType = gstStructureGet<int>(structure, "payload"_s);
    if (!payloadType) {
        auto& firstPacketizer = m_packetizers.first();
        gst_structure_set(structure, "payload", G_TYPE_INT, firstPacketizer->payloadType(), nullptr);
    }

    StringBuilder simulcastBuilder;
    const char* direction = "send";
    simulcastBuilder.append(span(direction));
    simulcastBuilder.append(' ');
    unsigned totalStreams = 0;
    for (auto& packetizer : m_packetizers) {
        auto rtpStreamId = packetizer->rtpStreamId();
        if (rtpStreamId.isEmpty())
            continue;

        if (totalStreams > 0)
            simulcastBuilder.append(';');
        simulcastBuilder.append(rtpStreamId);
        gst_structure_set(structure, makeString("rid-"_s, rtpStreamId).ascii().data(), G_TYPE_STRING, direction, nullptr);
        packetizer->configureExtensions();
        totalStreams++;
    }

    auto lookupResults = lookupRtpExtensions(structure);
    if (totalStreams) {
        if (!lookupResults.hasRtpStreamIdExtension) {
            lookupResults.lastIdentifier++;
            auto extensionIdentifier = makeString("extmap-"_s, lookupResults.lastIdentifier);
            gst_structure_set(structure, extensionIdentifier.ascii().data(), G_TYPE_STRING, GST_RTP_HDREXT_BASE "sdes:rtp-stream-id", nullptr);
        }
        if (!lookupResults.hasRtpRepairedStreamIdExtension) {
            lookupResults.lastIdentifier++;
            auto extensionIdentifier = makeString("extmap-"_s, lookupResults.lastIdentifier);
            gst_structure_set(structure, extensionIdentifier.ascii().data(), G_TYPE_STRING, GST_RTP_HDREXT_BASE "sdes:repaired-rtp-stream-id", nullptr);
        }

        gst_structure_set(structure, "a-simulcast", G_TYPE_STRING, simulcastBuilder.toString().ascii().data(), nullptr);
        GST_DEBUG_OBJECT(m_bin.get(), "Simulcast parameters: %" GST_PTR_FORMAT, structure);
    }
    if (!lookupResults.hasMidExtension) {
        lookupResults.lastIdentifier++;
        auto extensionIdentifier = makeString("extmap-"_s, lookupResults.lastIdentifier);
        gst_structure_set(structure, extensionIdentifier.ascii().data(), G_TYPE_STRING, GST_RTP_HDREXT_BASE "sdes:mid", nullptr);
    }

    GST_DEBUG_OBJECT(m_bin.get(), "Setting RTP funnel caps to %" GST_PTR_FORMAT, rtpCaps.get());
    g_object_set(m_rtpCapsfilter.get(), "caps", rtpCaps.get(), nullptr);
    return true;
}

RealtimeOutgoingMediaSourceGStreamer::ExtensionLookupResults RealtimeOutgoingMediaSourceGStreamer::lookupRtpExtensions(const GstStructure* structure)
{
    ExtensionLookupResults lookupResults;
    gstStructureForeach(structure, [&](auto id, const auto value) -> bool {
        auto name = gstIdToString(id);
        if (!name.startsWith("extmap-"_s))
            return true;

        auto identifier = WTF::parseInteger<int>(name.substring(7)).value_or(0);
        if (UNLIKELY(!identifier))
            return true;

        lookupResults.lastIdentifier = std::max(lookupResults.lastIdentifier, identifier);

        StringView uri;
        if (G_VALUE_HOLDS_STRING(value))
            uri = StringView::fromLatin1(g_value_get_string(value));
        else if (GST_VALUE_HOLDS_ARRAY(value)) {
            const auto uriValue = gst_value_array_get_value(value, 1);
            uri = StringView::fromLatin1(g_value_get_string(uriValue));
        } else
            return true;

        if (uri == GST_RTP_HDREXT_BASE "sdes:rtp-stream-id"_s)
            lookupResults.hasRtpStreamIdExtension = true;
        if (uri == GST_RTP_HDREXT_BASE "sdes:repaired-rtp-stream-id"_s)
            lookupResults.hasRtpRepairedStreamIdExtension = true;
        if (uri == GST_RTP_HDREXT_BASE "sdes:mid"_s)
            lookupResults.hasMidExtension = true;

        return true;
    });
    return lookupResults;
}

GUniquePtr<GstStructure> RealtimeOutgoingMediaSourceGStreamer::stats()
{
    GUniquePtr<GstStructure> stats(gst_structure_new_empty("outgoing-media-stats"));
    for (auto& packetizer : m_packetizers) {
        auto packetizerStats = packetizer->stats();
        if (!packetizerStats)
            continue;

        auto [ssrc, structure] = *packetizerStats;
        auto ssrcString = makeString(ssrc);
        gst_structure_set(stats.get(), ssrcString.ascii().data(), GST_TYPE_STRUCTURE, structure, nullptr);
    }
    return stats;
}

void RealtimeOutgoingMediaSourceGStreamer::startUpdatingStats()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Starting buffer monitoring for stats gathering");
    forEach(m_packetizers, [](auto& packetizer) {
        packetizer->startUpdatingStats();
    });
}

void RealtimeOutgoingMediaSourceGStreamer::stopUpdatingStats()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Stopping buffer monitoring for stats gathering");
    forEach(m_packetizers, [](auto& packetizer) {
        packetizer->stopUpdatingStats();
    });
}

void RealtimeOutgoingMediaSourceGStreamer::teardown()
{
    if (m_transceiver)
        g_signal_handlers_disconnect_by_data(m_transceiver.get(), this);

    stopOutgoingSource();
    stopUpdatingStats();

    if (GST_IS_PAD(m_webrtcSinkPad.get())) {
        auto srcPad = adoptGRef(gst_element_get_static_pad(m_bin.get(), "src"));
        if (gst_pad_unlink(srcPad.get(), m_webrtcSinkPad.get())) {
            GST_DEBUG_OBJECT(m_bin.get(), "Removing webrtcbin pad %" GST_PTR_FORMAT, m_webrtcSinkPad.get());
            if (auto parent = adoptGRef(gst_pad_get_parent_element(m_webrtcSinkPad.get())))
                gst_element_release_request_pad(parent.get(), m_webrtcSinkPad.get());
        }
    }

    gst_element_set_locked_state(m_bin.get(), TRUE);
    gst_element_set_state(m_bin.get(), GST_STATE_NULL);
    if (auto pipeline = adoptGRef(gst_element_get_parent(m_bin.get())))
        gst_bin_remove(GST_BIN_CAST(pipeline.get()), m_bin.get());
    gst_element_set_locked_state(m_bin.get(), FALSE);

    m_packetizers.clear();

    m_bin.clear();
    m_liveSync.clear();
    m_tee.clear();
    m_rtpFunnel.clear();
    m_allowedCaps.clear();
    m_transceiver.clear();
    m_sender.clear();
    m_webrtcSinkPad.clear();
    m_parameters.reset();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
