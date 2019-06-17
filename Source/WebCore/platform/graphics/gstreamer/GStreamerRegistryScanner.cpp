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

    initialize();
#ifndef GST_DISABLE_GST_DEBUG
    GST_DEBUG("%s registry scanner initialized", m_isMediaSource ? "MSE" : "Regular playback");
    for (auto& mimeType : m_mimeTypeSet)
        GST_DEBUG("Mime-type registered: %s", mimeType.utf8().data());
    for (auto& item : m_codecMap)
        GST_DEBUG("%s codec pattern registered: %s", item.value ? "Hardware" : "Software", item.key.string().utf8().data());
#endif
}

GStreamerRegistryScanner::~GStreamerRegistryScanner()
{
    gst_plugin_feature_list_free(m_audioDecoderFactories);
    gst_plugin_feature_list_free(m_audioParserFactories);
    gst_plugin_feature_list_free(m_videoDecoderFactories);
    gst_plugin_feature_list_free(m_videoParserFactories);
    gst_plugin_feature_list_free(m_demuxerFactories);
}

GStreamerRegistryScanner::RegistryLookupResult GStreamerRegistryScanner::hasElementForMediaType(GList* elementFactories, const char* capsString, bool shouldCheckHardwareClassifier) const
{
    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_from_string(capsString));
    GList* candidates = gst_element_factory_list_filter(elementFactories, caps.get(), GST_PAD_SINK, false);
    bool isSupported = candidates;
    bool isUsingHardware = false;

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
#ifndef GST_DISABLE_GST_DEBUG
    const char* elementType = "";
    if (elementFactories == m_audioParserFactories)
        elementType = "Audio parser";
    else if (elementFactories == m_audioDecoderFactories)
        elementType = "Audio decoder";
    else if (elementFactories == m_videoParserFactories)
        elementType = "Video parser";
    else if (elementFactories == m_videoDecoderFactories)
        elementType = "Video decoder";
    else if (elementFactories == m_demuxerFactories)
        elementType = "Demuxer";
    else
        ASSERT_NOT_REACHED();
    GST_LOG("%s lookup result for caps %" GST_PTR_FORMAT " : isSupported=%s, isUsingHardware=%s", elementType, caps.get(), boolForPrinting(isSupported), boolForPrinting(isUsingHardware));
#endif
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
                    m_codecMap.add(pattern, false);
            }
            if (!current.webkitMimeTypes.isEmpty()) {
                for (const auto& mimeType : current.webkitMimeTypes)
                    m_mimeTypeSet.add(mimeType);
            } else
                m_mimeTypeSet.add(AtomString(current.capsString));
        }
    }
}

void GStreamerRegistryScanner::initialize()
{
    if (hasElementForMediaType(m_audioDecoderFactories, "audio/mpeg, mpegversion=(int)4")) {
        m_mimeTypeSet.add(AtomString("audio/aac"));
        m_mimeTypeSet.add(AtomString("audio/mp4"));
        m_mimeTypeSet.add(AtomString("audio/x-m4a"));
        m_codecMap.add(AtomString("mpeg"), false);
        m_codecMap.add(AtomString("mp4a*"), false);
    }

    auto opusSupported = hasElementForMediaType(m_audioDecoderFactories, "audio/x-opus");
    if (opusSupported && (!m_isMediaSource || hasElementForMediaType(m_audioParserFactories, "audio/x-opus"))) {
        m_mimeTypeSet.add(AtomString("audio/opus"));
        m_codecMap.add(AtomString("opus"), false);
        m_codecMap.add(AtomString("x-opus"), false);
    }

    auto vorbisSupported = hasElementForMediaType(m_audioDecoderFactories, "audio/x-vorbis");
    if (vorbisSupported && (!m_isMediaSource || hasElementForMediaType(m_audioParserFactories, "audio/x-vorbis"))) {
        m_codecMap.add(AtomString("vorbis"), false);
        m_codecMap.add(AtomString("x-vorbis"), false);
    }

    if (hasElementForMediaType(m_demuxerFactories, "video/x-matroska")) {
        auto vp8DecoderAvailable = hasElementForMediaType(m_videoDecoderFactories, "video/x-vp8", true);
        auto vp9DecoderAvailable = hasElementForMediaType(m_videoDecoderFactories, "video/x-vp9", true);

        if (vp8DecoderAvailable || vp9DecoderAvailable)
            m_mimeTypeSet.add(AtomString("video/webm"));

        if (vp8DecoderAvailable) {
            m_codecMap.add(AtomString("vp8"), vp8DecoderAvailable.isUsingHardware);
            m_codecMap.add(AtomString("x-vp8"), vp8DecoderAvailable.isUsingHardware);
            m_codecMap.add(AtomString("vp8.0"), vp8DecoderAvailable.isUsingHardware);
        }
        if (vp9DecoderAvailable) {
            m_codecMap.add(AtomString("vp9"), vp9DecoderAvailable.isUsingHardware);
            m_codecMap.add(AtomString("x-vp9"), vp9DecoderAvailable.isUsingHardware);
            m_codecMap.add(AtomString("vp9.0"), vp9DecoderAvailable.isUsingHardware);
        }
        if (opusSupported)
            m_mimeTypeSet.add(AtomString("audio/webm"));
    }

    auto h264DecoderAvailable = hasElementForMediaType(m_videoDecoderFactories, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }", true);
    if (h264DecoderAvailable && (!m_isMediaSource || hasElementForMediaType(m_videoParserFactories, "video/x-h264"))) {
        m_mimeTypeSet.add(AtomString("video/mp4"));
        m_mimeTypeSet.add(AtomString("video/x-m4v"));
        m_codecMap.add(AtomString("x-h264"), h264DecoderAvailable.isUsingHardware);
        m_codecMap.add(AtomString("avc*"), h264DecoderAvailable.isUsingHardware);
        m_codecMap.add(AtomString("mp4v*"), h264DecoderAvailable.isUsingHardware);
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
        m_mimeTypeSet.add(AtomString("application/ogg"));

        if (vorbisSupported) {
            m_mimeTypeSet.add(AtomString("audio/ogg"));
            m_mimeTypeSet.add(AtomString("audio/x-vorbis+ogg"));
        }

        if (hasElementForMediaType(m_audioDecoderFactories, "audio/x-speex")) {
            m_mimeTypeSet.add(AtomString("audio/ogg"));
            m_codecMap.add(AtomString("speex"), false);
        }

        if (hasElementForMediaType(m_videoDecoderFactories, "video/x-theora")) {
            m_mimeTypeSet.add(AtomString("video/ogg"));
            m_codecMap.add(AtomString("theora"), false);
        }
    }

    bool audioMpegSupported = false;
    if (hasElementForMediaType(m_audioDecoderFactories, "audio/mpeg, mpegversion=(int)1, layer=(int)[1, 3]")) {
        audioMpegSupported = true;
        m_mimeTypeSet.add(AtomString("audio/mp1"));
        m_mimeTypeSet.add(AtomString("audio/mp3"));
        m_mimeTypeSet.add(AtomString("audio/x-mp3"));
        m_codecMap.add(AtomString("audio/mp3"), false);
    }

    if (hasElementForMediaType(m_audioDecoderFactories, "audio/mpeg, mpegversion=(int)2")) {
        audioMpegSupported = true;
        m_mimeTypeSet.add(AtomString("audio/mp2"));
    }

    audioMpegSupported |= isContainerTypeSupported("audio/mp4");
    if (audioMpegSupported) {
        m_mimeTypeSet.add(AtomString("audio/mpeg"));
        m_mimeTypeSet.add(AtomString("audio/x-mpeg"));
    }

    bool matroskaSupported = hasElementForMediaType(m_demuxerFactories, "video/x-matroska");
    if (matroskaSupported) {
        m_mimeTypeSet.add(AtomString("video/x-matroska"));

        if (hasElementForMediaType(m_videoDecoderFactories, "video/x-vp10"))
            m_mimeTypeSet.add(AtomString("video/webm"));
    }

    if ((matroskaSupported || isContainerTypeSupported("video/mp4")) && hasElementForMediaType(m_videoDecoderFactories, "video/x-av1"))
        m_codecMap.add(AtomString("av01*"), false);
}

bool GStreamerRegistryScanner::isCodecSupported(String codec, bool shouldCheckForHardwareUse) const
{
    // If the codec is named like a mimetype (eg: video/avc) remove the "video/" part.
    size_t slashIndex = codec.find('/');
    if (slashIndex != WTF::notFound)
        codec = codec.substring(slashIndex + 1);

    bool supported = false;
    if (codec.startsWith("avc1"))
        supported = isAVC1CodecSupported(codec, shouldCheckForHardwareUse);
    else {
        for (const auto& item : m_codecMap) {
            if (!fnmatch(item.key.string().utf8().data(), codec.utf8().data(), 0)) {
                supported = shouldCheckForHardwareUse ? item.value : true;
                if (supported)
                    break;
            }
        }
    }

    GST_LOG("Checked %s codec \"%s\" supported %s", shouldCheckForHardwareUse ? "hardware" : "software", codec.utf8().data(), boolForPrinting(supported));
    return supported;
}

bool GStreamerRegistryScanner::areAllCodecsSupported(const Vector<String>& codecs, bool shouldCheckForHardwareUse) const
{
    for (String codec : codecs) {
        if (!isCodecSupported(codec, shouldCheckForHardwareUse))
            return false;
    }

    return true;
}

bool GStreamerRegistryScanner::isAVC1CodecSupported(const String& codec, bool shouldCheckForHardwareUse) const
{
    auto components = codec.split('.');
    long int spsAsInteger = strtol(components[1].utf8().data(), nullptr, 16);
    uint8_t sps[3];
    sps[0] = spsAsInteger >> 16;
    sps[1] = spsAsInteger >> 8;
    sps[2] = spsAsInteger;

    const char* profile = gst_codec_utils_h264_get_profile(sps, 3);
    const char* level = gst_codec_utils_h264_get_level(sps, 3);
    GST_DEBUG("Codec %s translates to H.264 profile %s and level %s", codec.utf8().data(), profile, level);

    auto checkH264Caps = [&](const char* capsString) {
        bool supported = false;
        auto lookupResult = hasElementForMediaType(m_videoDecoderFactories, capsString, true);
        supported = lookupResult;
        if (shouldCheckForHardwareUse)
            supported = lookupResult.isUsingHardware;
        GST_DEBUG("%s decoding supported for codec %s: %s", shouldCheckForHardwareUse ? "Hardware" : "Software", codec.utf8().data(), boolForPrinting(supported));
        return supported;
    };

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

    if (webkitGstCheckVersion(1, 17, 0)) {
        GST_DEBUG("Checking video decoders for constrained caps");
        return checkH264Caps(makeString("video/x-h264, level=(string)", level, ", profile=(string)", profile).utf8().data());
    }

    GST_DEBUG("Falling back to unconstrained caps");
    return checkH264Caps("video/x-h264");
}

GStreamerRegistryScanner::RegistryLookupResult GStreamerRegistryScanner::isDecodingSupported(MediaConfiguration& configuration) const
{
    bool isSupported = false;
    bool isUsingHardware = false;

    if (configuration.video) {
        auto& videoConfiguration = configuration.video.value();
        GST_DEBUG("Checking support for video configuration: \"%s\" size: %ux%u bitrate: %" G_GUINT64_FORMAT " framerate: %f",
            videoConfiguration.contentType.utf8().data(),
            videoConfiguration.width, videoConfiguration.height,
            videoConfiguration.bitrate, videoConfiguration.framerate);

        auto contentType = ContentType(videoConfiguration.contentType);
        isSupported = isContainerTypeSupported(contentType.containerType());
        auto codecs = contentType.codecs();
        if (!codecs.isEmpty())
            isUsingHardware = areAllCodecsSupported(codecs, true);
    }

    if (configuration.audio) {
        auto& audioConfiguration = configuration.audio.value();
        GST_DEBUG("Checking support for audio configuration: \"%s\" %s channels, bitrate: %" G_GUINT64_FORMAT " samplerate: %u",
            audioConfiguration.contentType.utf8().data(), audioConfiguration.channels.utf8().data(),
            audioConfiguration.bitrate, audioConfiguration.samplerate);
        auto contentType = ContentType(audioConfiguration.contentType);
        isSupported = isContainerTypeSupported(contentType.containerType());
    }

    return GStreamerRegistryScanner::RegistryLookupResult { isSupported, isUsingHardware };
}

}
#endif
