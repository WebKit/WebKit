/*
 * Copyright (C) 2019 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerRegistryScanner.h"

#if USE(GSTREAMER)
#include "ContentType.h"
#include "GStreamerCommon.h"
#include <fnmatch.h>
#include <gst/pbutils/codec-utils.h>
#include <wtf/PrintStream.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_media_gst_registry_scanner_debug);
#define GST_CAT_DEFAULT webkit_media_gst_registry_scanner_debug

GStreamerRegistryScanner& GStreamerRegistryScanner::singleton()
{
    static NeverDestroyed<GStreamerRegistryScanner> sharedInstance;
    return sharedInstance;
}

GStreamerRegistryScanner::GStreamerRegistryScanner(bool isMediaSource)
    : m_isMediaSource(isMediaSource)
{
    GST_DEBUG_CATEGORY_INIT(webkit_media_gst_registry_scanner_debug, "webkitregistryscanner", 0, "WebKit GStreamer registry scanner");
    m_audioDecoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, GST_RANK_MARGINAL);
    m_audioParserFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_PARSER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, GST_RANK_NONE);
    m_videoDecoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_MARGINAL);
    m_videoParserFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_PARSER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_MARGINAL);
    m_demuxerFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DEMUXER, GST_RANK_MARGINAL);

    m_audioEncoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, GST_RANK_MARGINAL);
    m_videoEncoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_MARGINAL);
    m_muxerFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_MUXER, GST_RANK_MARGINAL);

    initializeDecoders();
    initializeEncoders();
#ifndef GST_DISABLE_GST_DEBUG
    GST_DEBUG("%s registry scanner initialized", m_isMediaSource ? "MSE" : "Regular playback");
    for (auto& mimeType : m_decoderMimeTypeSet)
        GST_DEBUG("Decoder mime-type registered: %s", mimeType.utf8().data());
    for (auto& item : m_decoderCodecMap)
        GST_DEBUG("%s decoder codec pattern registered: %s", item.value ? "Hardware" : "Software", item.key.string().utf8().data());
    for (auto& mimeType : m_encoderMimeTypeSet)
        GST_DEBUG("Encoder mime-type registered: %s", mimeType.utf8().data());
    for (auto& item : m_encoderCodecMap)
        GST_DEBUG("%s encoder codec pattern registered: %s", item.value ? "Hardware" : "Software", item.key.string().utf8().data());
#endif
}

GStreamerRegistryScanner::~GStreamerRegistryScanner()
{
    gst_plugin_feature_list_free(m_audioDecoderFactories);
    gst_plugin_feature_list_free(m_audioParserFactories);
    gst_plugin_feature_list_free(m_videoDecoderFactories);
    gst_plugin_feature_list_free(m_videoParserFactories);
    gst_plugin_feature_list_free(m_demuxerFactories);
    gst_plugin_feature_list_free(m_audioEncoderFactories);
    gst_plugin_feature_list_free(m_videoEncoderFactories);
    gst_plugin_feature_list_free(m_muxerFactories);
}

const HashSet<String, ASCIICaseInsensitiveHash>& GStreamerRegistryScanner::mimeTypeSet(Configuration configuration)
{
    switch (configuration) {
    case Configuration::Decoding:
        return m_decoderMimeTypeSet;
    case Configuration::Encoding:
        return m_encoderMimeTypeSet;
    }
    ASSERT_NOT_REACHED();
    return m_decoderMimeTypeSet;
}

bool GStreamerRegistryScanner::isContainerTypeSupported(Configuration configuration, String containerType) const
{
    switch (configuration) {
    case Configuration::Decoding:
        return m_decoderMimeTypeSet.contains(containerType);
    case Configuration::Encoding:
        return m_encoderMimeTypeSet.contains(containerType);
    }
    ASSERT_NOT_REACHED();
    return false;
}

GStreamerRegistryScanner::RegistryLookupResult GStreamerRegistryScanner::hasElementForMediaType(GList* elementFactories, const char* capsString, bool shouldCheckHardwareClassifier, Optional<Vector<String>> blackList) const
{
    GstPadDirection padDirection = GST_PAD_SINK;
    if (elementFactories == m_audioEncoderFactories || elementFactories == m_videoEncoderFactories || elementFactories == m_muxerFactories)
        padDirection = GST_PAD_SRC;
    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_from_string(capsString));
    GList* candidates = gst_element_factory_list_filter(elementFactories, caps.get(), padDirection, false);
    bool isSupported = candidates;
    bool isUsingHardware = false;

    const char* elementType = "";
    if (elementFactories == m_audioParserFactories)
        elementType = "audio parser";
    else if (elementFactories == m_audioDecoderFactories)
        elementType = "audio decoder";
    else if (elementFactories == m_videoParserFactories)
        elementType = "video parser";
    else if (elementFactories == m_videoDecoderFactories)
        elementType = "video decoder";
    else if (elementFactories == m_demuxerFactories)
        elementType = "demuxer";
    else if (elementFactories == m_audioEncoderFactories)
        elementType = "audio encoder";
    else if (elementFactories == m_videoEncoderFactories)
        elementType = "video encoder";
    else if (elementFactories == m_muxerFactories)
        elementType = "muxer";
    else
        ASSERT_NOT_REACHED();

    if (blackList.hasValue() && !blackList->isEmpty()) {
        bool hasValidCandidate = false;
        for (GList* factories = candidates; factories; factories = g_list_next(factories)) {
            String name(gst_plugin_feature_get_name(GST_PLUGIN_FEATURE_CAST(factories->data)));
            if (blackList->contains(name))
                continue;
            hasValidCandidate = true;
            break;
        }
        if (!hasValidCandidate) {
            GST_WARNING("All %s elements matching caps %" GST_PTR_FORMAT " are blacklisted", elementType, caps.get());
            isSupported = false;
            shouldCheckHardwareClassifier = false;
        }
    }

    if (shouldCheckHardwareClassifier) {
        for (GList* factories = candidates; factories; factories = g_list_next(factories)) {
            auto* factory = reinterpret_cast<GstElementFactory*>(factories->data);
            String metadata = gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_KLASS);
            auto components = metadata.split('/');
            if (components.contains("Hardware")) {
                isUsingHardware = true;
                break;
            }
        }
    }

    gst_plugin_feature_list_free(candidates);
    GST_LOG("Lookup result for %s matching caps %" GST_PTR_FORMAT " : isSupported=%s, isUsingHardware=%s", elementType, caps.get(), boolForPrinting(isSupported), boolForPrinting(isUsingHardware));
    return GStreamerRegistryScanner::RegistryLookupResult { isSupported, isUsingHardware };
}

void GStreamerRegistryScanner::fillMimeTypeSetFromCapsMapping(Vector<GstCapsWebKitMapping>& mapping)
{
    for (auto& current : mapping) {
        GList* factories;
        switch (current.elementType) {
        case Demuxer:
            factories = m_demuxerFactories;
            break;
        case AudioDecoder:
            factories = m_audioDecoderFactories;
            break;
        case VideoDecoder:
            factories = m_videoDecoderFactories;
            break;
        }

        if (hasElementForMediaType(factories, current.capsString)) {
            if (!current.webkitCodecPatterns.isEmpty()) {
                for (const auto& pattern : current.webkitCodecPatterns)
                    m_decoderCodecMap.add(pattern, false);
            }
            if (!current.webkitMimeTypes.isEmpty()) {
                for (const auto& mimeType : current.webkitMimeTypes)
                    m_decoderMimeTypeSet.add(mimeType);
            } else
                m_decoderMimeTypeSet.add(AtomString(current.capsString));
        }
    }
}

void GStreamerRegistryScanner::initializeDecoders()
{
    if (hasElementForMediaType(m_audioDecoderFactories, "audio/mpeg, mpegversion=(int)4")) {
        m_decoderMimeTypeSet.add(AtomString("audio/aac"));
        m_decoderMimeTypeSet.add(AtomString("audio/mp4"));
        m_decoderMimeTypeSet.add(AtomString("audio/x-m4a"));
        m_decoderCodecMap.add(AtomString("mpeg"), false);
        m_decoderCodecMap.add(AtomString("mp4a*"), false);
    }

    auto opusSupported = hasElementForMediaType(m_audioDecoderFactories, "audio/x-opus");
    if (opusSupported && (!m_isMediaSource || hasElementForMediaType(m_audioParserFactories, "audio/x-opus"))) {
        m_decoderMimeTypeSet.add(AtomString("audio/opus"));
        m_decoderCodecMap.add(AtomString("opus"), false);
        m_decoderCodecMap.add(AtomString("x-opus"), false);
    }

    auto vorbisSupported = hasElementForMediaType(m_audioDecoderFactories, "audio/x-vorbis");
    if (vorbisSupported && (!m_isMediaSource || hasElementForMediaType(m_audioParserFactories, "audio/x-vorbis"))) {
        m_decoderCodecMap.add(AtomString("vorbis"), false);
        m_decoderCodecMap.add(AtomString("x-vorbis"), false);
    }

    bool matroskaSupported = hasElementForMediaType(m_demuxerFactories, "video/x-matroska");
    if (matroskaSupported) {
        auto vp8DecoderAvailable = hasElementForMediaType(m_videoDecoderFactories, "video/x-vp8", true);
        auto vp9DecoderAvailable = hasElementForMediaType(m_videoDecoderFactories, "video/x-vp9", true);

        if (vp8DecoderAvailable || vp9DecoderAvailable)
            m_decoderMimeTypeSet.add(AtomString("video/webm"));

        if (vp8DecoderAvailable) {
            m_decoderCodecMap.add(AtomString("vp8"), vp8DecoderAvailable.isUsingHardware);
            m_decoderCodecMap.add(AtomString("x-vp8"), vp8DecoderAvailable.isUsingHardware);
            m_decoderCodecMap.add(AtomString("vp8.0"), vp8DecoderAvailable.isUsingHardware);
        }
        if (vp9DecoderAvailable) {
            m_decoderCodecMap.add(AtomString("vp9"), vp9DecoderAvailable.isUsingHardware);
            m_decoderCodecMap.add(AtomString("x-vp9"), vp9DecoderAvailable.isUsingHardware);
            m_decoderCodecMap.add(AtomString("vp9.0"), vp9DecoderAvailable.isUsingHardware);
            m_decoderCodecMap.add(AtomString("vp09*"), vp9DecoderAvailable.isUsingHardware);
        }
        if (opusSupported)
            m_decoderMimeTypeSet.add(AtomString("audio/webm"));
    }

    auto h264DecoderAvailable = hasElementForMediaType(m_videoDecoderFactories, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }", true);
    if (h264DecoderAvailable && (!m_isMediaSource || hasElementForMediaType(m_videoParserFactories, "video/x-h264"))) {
        m_decoderMimeTypeSet.add(AtomString("video/mp4"));
        m_decoderMimeTypeSet.add(AtomString("video/x-m4v"));
        m_decoderCodecMap.add(AtomString("x-h264"), h264DecoderAvailable.isUsingHardware);
        m_decoderCodecMap.add(AtomString("avc*"), h264DecoderAvailable.isUsingHardware);
        m_decoderCodecMap.add(AtomString("mp4v*"), h264DecoderAvailable.isUsingHardware);
    }

    Vector<String> av1DecodersBlacklist { "av1dec"_s };
    if ((matroskaSupported || isContainerTypeSupported(Configuration::Decoding, "video/mp4")) && hasElementForMediaType(m_videoDecoderFactories, "video/x-av1", false, makeOptional(WTFMove(av1DecodersBlacklist)))) {
        m_decoderCodecMap.add(AtomString("av01*"), false);
        m_decoderCodecMap.add(AtomString("av1"), false);
        m_decoderCodecMap.add(AtomString("x-av1"), false);
    }

    if (m_isMediaSource)
        return;

    // The mime-types initialized below are not supported by the MSE backend.

    Vector<GstCapsWebKitMapping> mapping = {
        {AudioDecoder, "audio/midi", {"audio/midi", "audio/riff-midi"}, { }},
        {AudioDecoder, "audio/x-ac3", { }, { }},
        {AudioDecoder, "audio/x-dts", { }, { }},
        {AudioDecoder, "audio/x-eac3", {"audio/x-ac3"}, { }},
        {AudioDecoder, "audio/x-flac", {"audio/x-flac", "audio/flac"}, { }},
        {AudioDecoder, "audio/x-sbc", { }, { }},
        {AudioDecoder, "audio/x-sid", { }, { }},
        {AudioDecoder, "audio/x-speex", {"audio/speex", "audio/x-speex"}, { }},
        {AudioDecoder, "audio/x-wavpack", {"audio/x-wavpack"}, { }},
        {VideoDecoder, "video/mpeg, mpegversion=(int){1,2}, systemstream=(boolean)false", {"video/mpeg"}, {"mpeg"}},
        {VideoDecoder, "video/mpegts", { }, { }},
        {VideoDecoder, "video/x-dirac", { }, { }},
        {VideoDecoder, "video/x-flash-video", {"video/flv", "video/x-flv"}, { }},
        {VideoDecoder, "video/x-h263", { }, { }},
        {VideoDecoder, "video/x-msvideocodec", {"video/x-msvideo"}, { }},
        {Demuxer, "application/vnd.rn-realmedia", { }, { }},
        {Demuxer, "application/x-3gp", { }, { }},
        {Demuxer, "application/x-hls", {"application/vnd.apple.mpegurl", "application/x-mpegurl"}, { }},
        {Demuxer, "application/x-pn-realaudio", { }, { }},
        {Demuxer, "audio/x-aiff", { }, { }},
        {Demuxer, "audio/x-wav", {"audio/x-wav", "audio/wav", "audio/vnd.wave"}, {"1"}},
        {Demuxer, "video/quicktime", { }, { }},
        {Demuxer, "video/quicktime, variant=(string)3gpp", {"video/3gpp"}, { }},
        {Demuxer, "video/x-ms-asf", { }, { }},
    };
    fillMimeTypeSetFromCapsMapping(mapping);

    if (hasElementForMediaType(m_demuxerFactories, "application/ogg")) {
        m_decoderMimeTypeSet.add(AtomString("application/ogg"));

        if (vorbisSupported) {
            m_decoderMimeTypeSet.add(AtomString("audio/ogg"));
            m_decoderMimeTypeSet.add(AtomString("audio/x-vorbis+ogg"));
        }

        if (hasElementForMediaType(m_audioDecoderFactories, "audio/x-speex")) {
            m_decoderMimeTypeSet.add(AtomString("audio/ogg"));
            m_decoderCodecMap.add(AtomString("speex"), false);
        }

        if (hasElementForMediaType(m_videoDecoderFactories, "video/x-theora")) {
            m_decoderMimeTypeSet.add(AtomString("video/ogg"));
            m_decoderCodecMap.add(AtomString("theora"), false);
        }
    }

    bool audioMpegSupported = false;
    if (hasElementForMediaType(m_audioDecoderFactories, "audio/mpeg, mpegversion=(int)1, layer=(int)[1, 3]")) {
        audioMpegSupported = true;
        m_decoderMimeTypeSet.add(AtomString("audio/mp1"));
        m_decoderMimeTypeSet.add(AtomString("audio/mp3"));
        m_decoderMimeTypeSet.add(AtomString("audio/x-mp3"));
        m_decoderCodecMap.add(AtomString("audio/mp3"), false);
        m_decoderCodecMap.add(AtomString("mp3"), false);
    }

    if (hasElementForMediaType(m_audioDecoderFactories, "audio/mpeg, mpegversion=(int)2")) {
        audioMpegSupported = true;
        m_decoderMimeTypeSet.add(AtomString("audio/mp2"));
    }

    audioMpegSupported |= isContainerTypeSupported(Configuration::Decoding, "audio/mp4");
    if (audioMpegSupported) {
        m_decoderMimeTypeSet.add(AtomString("audio/mpeg"));
        m_decoderMimeTypeSet.add(AtomString("audio/x-mpeg"));
    }

    if (matroskaSupported) {
        m_decoderMimeTypeSet.add(AtomString("video/x-matroska"));

        if (hasElementForMediaType(m_videoDecoderFactories, "video/x-vp10"))
            m_decoderMimeTypeSet.add(AtomString("video/webm"));
    }
}

void GStreamerRegistryScanner::initializeEncoders()
{
    // MSE is about playback, which means decoding. No need to check for encoders then.
    if (m_isMediaSource)
        return;

    auto aacSupported = hasElementForMediaType(m_audioEncoderFactories, "audio/mpeg, mpegversion=(int)4");
    if (hasElementForMediaType(m_audioEncoderFactories, "audio/mpeg, mpegversion=(int)4")) {
        m_encoderCodecMap.add(AtomString("mpeg"), false);
        m_encoderCodecMap.add(AtomString("mp4a*"), false);
    }

    auto opusSupported = hasElementForMediaType(m_audioEncoderFactories, "audio/x-opus");
    if (opusSupported) {
        m_encoderCodecMap.add(AtomString("opus"), false);
        m_encoderCodecMap.add(AtomString("x-opus"), false);
    }

    auto vorbisSupported = hasElementForMediaType(m_audioEncoderFactories, "audio/x-vorbis");
    if (vorbisSupported) {
        m_encoderCodecMap.add(AtomString("vorbis"), false);
        m_encoderCodecMap.add(AtomString("x-vorbis"), false);
    }

    Vector<String> av1EncodersBlacklist { "av1enc"_s };
    auto av1EncoderAvailable = hasElementForMediaType(m_videoEncoderFactories, "video/x-av1", true, makeOptional(WTFMove(av1EncodersBlacklist)));
    if (av1EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("av01*"), false);
        m_encoderCodecMap.add(AtomString("av1"), false);
        m_encoderCodecMap.add(AtomString("x-av1"), false);
    }

    auto vp8EncoderAvailable = hasElementForMediaType(m_videoEncoderFactories, "video/x-vp8", true);
    if (vp8EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("vp8"), vp8EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("x-vp8"), vp8EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("vp8.0"), vp8EncoderAvailable.isUsingHardware);
    }

    auto vp9EncoderAvailable = hasElementForMediaType(m_videoEncoderFactories, "video/x-vp9", true);
    if (vp9EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("vp9"), vp9EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("x-vp9"), vp9EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("vp9.0"), vp9EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("vp09*"), vp9EncoderAvailable.isUsingHardware);
    }

    if (hasElementForMediaType(m_muxerFactories, "video/webm") && (vp8EncoderAvailable || vp9EncoderAvailable || av1EncoderAvailable))
        m_encoderMimeTypeSet.add(AtomString("video/webm"));

    if (hasElementForMediaType(m_muxerFactories, "audio/webm")) {
        if (opusSupported)
            m_encoderMimeTypeSet.add(AtomString("audio/opus"));
        m_encoderMimeTypeSet.add(AtomString("audio/webm"));
    }

    if (hasElementForMediaType(m_muxerFactories, "audio/ogg") && (vorbisSupported || opusSupported))
        m_encoderMimeTypeSet.add(AtomString("audio/ogg"));

    auto h264EncoderAvailable = hasElementForMediaType(m_videoEncoderFactories, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }", true);
    if (h264EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("x-h264"), h264EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("avc*"), h264EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("mp4v*"), h264EncoderAvailable.isUsingHardware);
    }

    if (hasElementForMediaType(m_muxerFactories, "video/quicktime")) {
        if (opusSupported)
            m_encoderMimeTypeSet.add(AtomString("audio/opus"));
        if (aacSupported) {
            m_encoderMimeTypeSet.add(AtomString("audio/aac"));
            m_encoderMimeTypeSet.add(AtomString("audio/mp4"));
            m_encoderMimeTypeSet.add(AtomString("audio/x-m4a"));
        }
        if (h264EncoderAvailable) {
            m_encoderMimeTypeSet.add(AtomString("video/mp4"));
            m_encoderMimeTypeSet.add(AtomString("video/x-m4v"));
        }
    }
}

bool GStreamerRegistryScanner::isCodecSupported(Configuration configuration, String codec, bool shouldCheckForHardwareUse) const
{
    // If the codec is named like a mimetype (eg: video/avc) remove the "video/" part.
    size_t slashIndex = codec.find('/');
    if (slashIndex != WTF::notFound)
        codec = codec.substring(slashIndex + 1);

    bool supported = false;
    if (codec.startsWith("avc1"))
        supported = isAVC1CodecSupported(configuration, codec, shouldCheckForHardwareUse);
    else {
        auto& codecMap = configuration == Configuration::Decoding ? m_decoderCodecMap : m_encoderCodecMap;
        for (const auto& item : codecMap) {
            if (!fnmatch(item.key.string().utf8().data(), codec.utf8().data(), 0)) {
                supported = shouldCheckForHardwareUse ? item.value : true;
                if (supported)
                    break;
            }
        }
    }

    const char* configLogString = configurationNameForLogging(configuration);
    GST_LOG("Checked %s %s codec \"%s\" supported %s", shouldCheckForHardwareUse ? "hardware" : "software", configLogString, codec.utf8().data(), boolForPrinting(supported));
    return supported;
}

MediaPlayerEnums::SupportsType GStreamerRegistryScanner::isContentTypeSupported(Configuration configuration, const ContentType& contentType, const Vector<ContentType>& contentTypesRequiringHardwareSupport) const
{
    using SupportsType = MediaPlayerEnums::SupportsType;

    const auto& containerType = contentType.containerType().convertToASCIILowercase();
    if (!isContainerTypeSupported(configuration, containerType))
        return SupportsType::IsNotSupported;

    const auto& codecs = contentType.codecs();

    // Spec says we should not return "probably" if the codecs string is empty.
    if (codecs.isEmpty())
        return SupportsType::MayBeSupported;

    for (const auto& item : codecs) {
        auto codec = item.convertToASCIILowercase();
        bool requiresHardwareSupport = contentTypesRequiringHardwareSupport
            .findMatching([containerType, codec](auto& hardwareContentType) -> bool {
            auto hardwareContainer = hardwareContentType.containerType();
            if (!hardwareContainer.isEmpty()
                && fnmatch(hardwareContainer.utf8().data(), containerType.utf8().data(), 0))
                return false;
            auto hardwareCodecs = hardwareContentType.codecs();
            return hardwareCodecs.isEmpty()
                || hardwareCodecs.findMatching([codec](auto& hardwareCodec) -> bool {
                    return !fnmatch(hardwareCodec.utf8().data(), codec.utf8().data(), 0);
            }) != notFound;
        }) != notFound;
        if (!isCodecSupported(configuration, codec, requiresHardwareSupport))
            return SupportsType::IsNotSupported;
    }
    return SupportsType::IsSupported;
}

bool GStreamerRegistryScanner::areAllCodecsSupported(Configuration configuration, const Vector<String>& codecs, bool shouldCheckForHardwareUse) const
{
    for (String codec : codecs) {
        if (!isCodecSupported(configuration, codec, shouldCheckForHardwareUse))
            return false;
    }

    return true;
}

bool GStreamerRegistryScanner::isAVC1CodecSupported(Configuration configuration, const String& codec, bool shouldCheckForHardwareUse) const
{
    auto checkH264Caps = [&](const char* capsString) {
        bool supported = false;
        RegistryLookupResult lookupResult;
        switch (configuration) {
        case Configuration::Decoding:
            lookupResult = hasElementForMediaType(m_videoDecoderFactories, capsString, true);
            break;
        case Configuration::Encoding:
            lookupResult = hasElementForMediaType(m_videoEncoderFactories, capsString, true);
            break;
        }
        supported = lookupResult;
        if (shouldCheckForHardwareUse)
            supported = lookupResult.isUsingHardware;
        GST_DEBUG("%s decoding supported for codec %s: %s", shouldCheckForHardwareUse ? "Hardware" : "Software", codec.utf8().data(), boolForPrinting(supported));
        return supported;
    };

    if (codec.find('.') == notFound) {
        GST_DEBUG("Codec has no profile/level, falling back to unconstrained caps");
        return checkH264Caps("video/x-h264");
    }

    auto components = codec.split('.');
    long int spsAsInteger = strtol(components[1].utf8().data(), nullptr, 16);
    uint8_t sps[3];
    sps[0] = spsAsInteger >> 16;
    sps[1] = spsAsInteger >> 8;
    sps[2] = spsAsInteger;

    const char* profile = gst_codec_utils_h264_get_profile(sps, 3);
    const char* level = gst_codec_utils_h264_get_level(sps, 3);

    // To avoid going through a class hierarchy for such a simple
    // string conversion, we use a little trick here: See
    // https://bugs.webkit.org/show_bug.cgi?id=201870.
    char levelAsStringFallback[2] = { '\0', '\0' };
    if (!level && sps[2] > 0 && sps[2] <= 5) {
        levelAsStringFallback[0] = static_cast<char>('0' + sps[2]);
        level = levelAsStringFallback;
    }

    if (!profile || !level) {
        GST_ERROR("H.264 profile / level was not recognised in codec %s", codec.utf8().data());
        return false;
    }

    GST_DEBUG("Codec %s translates to H.264 profile %s and level %s", codec.utf8().data(), profile, level);

    if (const char* maxVideoResolution = g_getenv("WEBKIT_GST_MAX_AVC1_RESOLUTION")) {
        uint8_t levelAsInteger = gst_codec_utils_h264_get_level_idc(level);
        GST_DEBUG("Maximum video resolution requested: %s, supplied codec level IDC: %u", maxVideoResolution, levelAsInteger);
        uint8_t maxLevel = 0;
        const char* maxLevelString = "";
        if (!g_strcmp0(maxVideoResolution, "1080P")) {
            maxLevel = 40;
            maxLevelString = "4";
        } else if (!g_strcmp0(maxVideoResolution, "720P")) {
            maxLevel = 31;
            maxLevelString = "3.1";
        } else if (!g_strcmp0(maxVideoResolution, "480P")) {
            maxLevel = 30;
            maxLevelString = "3";
        } else {
            g_warning("Invalid value for WEBKIT_GST_MAX_AVC1_RESOLUTION. Currently supported, 1080P, 720P and 480P.");
            return false;
        }
        if (levelAsInteger > maxLevel)
            return false;
        return checkH264Caps(makeString("video/x-h264, level=(string)", maxLevelString).utf8().data());
    }

    if (webkitGstCheckVersion(1, 18, 0)) {
        GST_DEBUG("Checking video decoders for constrained caps");
        return checkH264Caps(makeString("video/x-h264, level=(string)", level, ", profile=(string)", profile).utf8().data());
    }

    GST_DEBUG("Falling back to unconstrained caps");
    return checkH264Caps("video/x-h264");
}

const char* GStreamerRegistryScanner::configurationNameForLogging(Configuration configuration) const
{
    const char* configLogString = "";

    switch (configuration) {
    case Configuration::Encoding:
        configLogString = "encoding";
        break;
    case Configuration::Decoding:
        configLogString = "decoding";
        break;
    }
    return configLogString;
}

GStreamerRegistryScanner::RegistryLookupResult GStreamerRegistryScanner::isConfigurationSupported(Configuration configuration, MediaConfiguration& mediaConfiguration) const
{
    bool isSupported = false;
    bool isUsingHardware = false;
    const char* configLogString = configurationNameForLogging(configuration);

    if (mediaConfiguration.video) {
        auto& videoConfiguration = mediaConfiguration.video.value();
        GST_DEBUG("Checking %s support for video configuration: \"%s\" size: %ux%u bitrate: %" G_GUINT64_FORMAT " framerate: %f", configLogString,
            videoConfiguration.contentType.utf8().data(),
            videoConfiguration.width, videoConfiguration.height,
            videoConfiguration.bitrate, videoConfiguration.framerate);

        auto contentType = ContentType(videoConfiguration.contentType);
        isSupported = isContainerTypeSupported(configuration, contentType.containerType());
        auto codecs = contentType.codecs();
        if (!codecs.isEmpty())
            isUsingHardware = areAllCodecsSupported(configuration, codecs, true);
    }

    if (mediaConfiguration.audio) {
        auto& audioConfiguration = mediaConfiguration.audio.value();
        GST_DEBUG("Checking %s support for audio configuration: \"%s\" %s channels, bitrate: %" G_GUINT64_FORMAT " samplerate: %u", configLogString,
            audioConfiguration.contentType.utf8().data(), audioConfiguration.channels.utf8().data(),
            audioConfiguration.bitrate, audioConfiguration.samplerate);
        auto contentType = ContentType(audioConfiguration.contentType);
        isSupported = isContainerTypeSupported(configuration, contentType.containerType());
    }

    return GStreamerRegistryScanner::RegistryLookupResult { isSupported, isUsingHardware };
}

}
#endif
