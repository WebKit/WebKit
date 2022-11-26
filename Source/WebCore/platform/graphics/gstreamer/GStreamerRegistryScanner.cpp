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
#include "RuntimeApplicationChecks.h"
#include <fnmatch.h>
#include <gst/pbutils/codec-utils.h>
#include <wtf/PrintStream.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/StringToIntegerConversion.h>

#if USE(GSTREAMER_WEBRTC)
#include <gst/rtp/rtp.h>
#endif

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_media_gst_registry_scanner_debug);
#define GST_CAT_DEFAULT webkit_media_gst_registry_scanner_debug

// We shouldn't accept media that the player can't actually play.
// AAC supports up to 96 channels.
#define MEDIA_MAX_AAC_CHANNELS 96

// Assume hardware video decoding acceleration up to 8K@60fps for the generic case. Some embedded platforms might want to tune this.
#define MEDIA_MAX_WIDTH 7680.0f
#define MEDIA_MAX_HEIGHT 4320.0f
#define MEDIA_MAX_FRAMERATE 60.0f

GStreamerRegistryScanner& GStreamerRegistryScanner::singleton()
{
    static NeverDestroyed<GStreamerRegistryScanner> sharedInstance;
    return sharedInstance;
}

void GStreamerRegistryScanner::getSupportedDecodingTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    if (isInWebProcess())
        types = singleton().mimeTypeSet(GStreamerRegistryScanner::Configuration::Decoding);
    else
        types = GStreamerRegistryScanner().mimeTypeSet(GStreamerRegistryScanner::Configuration::Decoding);
}

GStreamerRegistryScanner::ElementFactories::ElementFactories(OptionSet<ElementFactories::Type> types)
{
    if (types.contains(Type::AudioDecoder))
        audioDecoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, GST_RANK_MARGINAL);
    if (types.contains(Type::AudioParser))
        audioParserFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_PARSER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, GST_RANK_NONE);
    if (types.contains(Type::VideoDecoder))
        videoDecoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_MARGINAL);
    if (types.contains(Type::VideoParser))
        videoParserFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_PARSER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_MARGINAL);
    if (types.contains(Type::Demuxer))
        demuxerFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DEMUXER, GST_RANK_MARGINAL);
    if (types.contains(Type::AudioEncoder))
        audioEncoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, GST_RANK_MARGINAL);
    if (types.contains(Type::VideoEncoder))
        videoEncoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_MARGINAL);
    if (types.contains(Type::Muxer))
        muxerFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_MUXER, GST_RANK_MARGINAL);
    if (types.contains(Type::RtpPayloader))
        rtpPayloaderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_PAYLOADER, GST_RANK_MARGINAL);
    if (types.contains(Type::RtpDepayloader))
        rtpDepayloaderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DEPAYLOADER, GST_RANK_MARGINAL);
}

GStreamerRegistryScanner::ElementFactories::~ElementFactories()
{
    gst_plugin_feature_list_free(audioDecoderFactories);
    gst_plugin_feature_list_free(audioParserFactories);
    gst_plugin_feature_list_free(videoDecoderFactories);
    gst_plugin_feature_list_free(videoParserFactories);
    gst_plugin_feature_list_free(demuxerFactories);
    gst_plugin_feature_list_free(audioEncoderFactories);
    gst_plugin_feature_list_free(videoEncoderFactories);
    gst_plugin_feature_list_free(muxerFactories);
    gst_plugin_feature_list_free(rtpPayloaderFactories);
    gst_plugin_feature_list_free(rtpDepayloaderFactories);
}

const char* GStreamerRegistryScanner::ElementFactories::elementFactoryTypeToString(GStreamerRegistryScanner::ElementFactories::Type factoryType)
{
    switch (factoryType) {
    case Type::AudioParser:
        return "audio parser";
    case Type::AudioDecoder:
        return "audio decoder";
    case Type::VideoParser:
        return "video parser";
    case Type::VideoDecoder:
        return "video decoder";
    case Type::Demuxer:
        return "demuxer";
    case Type::AudioEncoder:
        return "audio encoder";
    case Type::VideoEncoder:
        return "video encoder";
    case Type::Muxer:
        return "muxer";
    case Type::RtpPayloader:
        return "RTP payloader";
    case Type::RtpDepayloader:
        return "RTP depayloader";
    case Type::All:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

GList* GStreamerRegistryScanner::ElementFactories::factory(GStreamerRegistryScanner::ElementFactories::Type factoryType) const
{
    switch (factoryType) {
    case GStreamerRegistryScanner::ElementFactories::Type::AudioParser:
        return audioParserFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::AudioDecoder:
        return audioDecoderFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::VideoParser:
        return videoParserFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::VideoDecoder:
        return videoDecoderFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::Demuxer:
        return demuxerFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::AudioEncoder:
        return audioEncoderFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::VideoEncoder:
        return videoEncoderFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::Muxer:
        return muxerFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::RtpPayloader:
        return rtpPayloaderFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::RtpDepayloader:
        return rtpDepayloaderFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::All:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

GStreamerRegistryScanner::RegistryLookupResult GStreamerRegistryScanner::ElementFactories::hasElementForMediaType(ElementFactories::Type factoryType, const char* capsString, ElementFactories::CheckHardwareClassifier shouldCheckHardwareClassifier, std::optional<Vector<String>> disallowedList) const
{
    auto* elementFactories = factory(factoryType);
    if (!elementFactories)
        return { };

    GstPadDirection padDirection = GST_PAD_SINK;
    if (factoryType == Type::AudioEncoder || factoryType == Type::VideoEncoder || factoryType == Type::Muxer || factoryType == Type::RtpDepayloader)
        padDirection = GST_PAD_SRC;

    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_from_string(capsString));
    GList* candidates = gst_element_factory_list_filter(elementFactories, caps.get(), padDirection, false);
    bool isSupported = candidates;
    bool isUsingHardware = false;
    GRefPtr<GstElementFactory> selectedFactory;
    if (isSupported)
        selectedFactory = GST_ELEMENT_FACTORY_CAST(candidates->data);

    if (disallowedList && !disallowedList->isEmpty()) {
        bool hasValidCandidate = false;
        for (GList* factories = candidates; factories; factories = g_list_next(factories)) {
            String name = String::fromUTF8(gst_plugin_feature_get_name(GST_PLUGIN_FEATURE_CAST(factories->data)));
            if (disallowedList->contains(name))
                continue;
            hasValidCandidate = true;
            break;
        }
        if (!hasValidCandidate) {
            GST_WARNING("All %s elements matching caps %" GST_PTR_FORMAT " are disallowed", elementFactoryTypeToString(factoryType), caps.get());
            isSupported = false;
            shouldCheckHardwareClassifier = CheckHardwareClassifier::No;
        }
    }

    if (shouldCheckHardwareClassifier == CheckHardwareClassifier::Yes) {
        for (GList* factories = candidates; factories; factories = g_list_next(factories)) {
            auto* factory = reinterpret_cast<GstElementFactory*>(factories->data);
            auto metadata = String::fromLatin1(gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_KLASS));
            auto components = metadata.split('/');
            if (components.contains("Hardware"_s)) {
                isUsingHardware = true;
                selectedFactory = factory;
                break;
            }
        }
    }

    gst_plugin_feature_list_free(candidates);
    if (!isSupported)
        selectedFactory.clear();

    GST_LOG("Lookup result for %s matching caps %" GST_PTR_FORMAT " : isSupported=%s, isUsingHardware=%s", elementFactoryTypeToString(factoryType), caps.get(), boolForPrinting(isSupported), boolForPrinting(isUsingHardware));
    return { isSupported, isUsingHardware, selectedFactory };
}

GStreamerRegistryScanner::GStreamerRegistryScanner(bool isMediaSource)
    : m_isMediaSource(isMediaSource)
{
    if (isInWebProcess())
        ensureGStreamerInitialized();
    else {
        // This is still needed, mostly because of the webkit_web_view_can_show_mime_type() public API (so
        // running from UIProcess).
        gst_init(nullptr, nullptr);
    }

    GST_DEBUG_CATEGORY_INIT(webkit_media_gst_registry_scanner_debug, "webkitregistryscanner", 0, "WebKit GStreamer registry scanner");

    ElementFactories factories(OptionSet<ElementFactories::Type>::fromRaw(static_cast<unsigned>(ElementFactories::Type::All)));
    initializeDecoders(factories);
    initializeEncoders(factories);

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

const HashSet<String, ASCIICaseInsensitiveHash>& GStreamerRegistryScanner::mimeTypeSet(Configuration configuration) const
{
    switch (configuration) {
    case Configuration::Decoding:
        return m_decoderMimeTypeSet;
    case Configuration::Encoding:
        return m_encoderMimeTypeSet;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool GStreamerRegistryScanner::isContainerTypeSupported(Configuration configuration, const String& containerType) const
{
    switch (configuration) {
    case Configuration::Decoding:
        return m_decoderMimeTypeSet.contains(containerType);
    case Configuration::Encoding:
        return m_encoderMimeTypeSet.contains(containerType);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void GStreamerRegistryScanner::fillMimeTypeSetFromCapsMapping(const GStreamerRegistryScanner::ElementFactories& factories, const Vector<GstCapsWebKitMapping>& mapping)
{
    for (const auto& current : mapping) {
        if (auto result = factories.hasElementForMediaType(current.elementType, current.capsString)) {
            if (!current.webkitCodecPatterns.isEmpty()) {
                for (const auto& pattern : current.webkitCodecPatterns)
                    m_decoderCodecMap.add(pattern, result);
            }
            if (!current.webkitMimeTypes.isEmpty()) {
                for (const auto& mimeType : current.webkitMimeTypes)
                    m_decoderMimeTypeSet.add(mimeType);
            } else
                m_decoderMimeTypeSet.add(AtomString::fromLatin1(current.capsString));
        }
    }
}

void GStreamerRegistryScanner::initializeDecoders(const GStreamerRegistryScanner::ElementFactories& factories)
{
    if (auto result = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/mpeg, mpegversion=(int)4")) {
        m_decoderMimeTypeSet.add(AtomString("audio/aac"_s));
        m_decoderMimeTypeSet.add(AtomString("audio/mp4"_s));
        m_decoderMimeTypeSet.add(AtomString("audio/x-m4a"_s));
        m_decoderMimeTypeSet.add(AtomString("audio/mpeg"_s));
        m_decoderMimeTypeSet.add(AtomString("audio/x-mpeg"_s));
        m_decoderCodecMap.add(AtomString("mpeg"_s), result);
        m_decoderCodecMap.add(AtomString("mp4a*"_s), result);
    }

    auto opusSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/x-opus");
    if (opusSupported && (!m_isMediaSource || factories.hasElementForMediaType(ElementFactories::Type::AudioParser, "audio/x-opus"))) {
        m_decoderMimeTypeSet.add(AtomString("audio/opus"_s));
        m_decoderCodecMap.add(AtomString("opus"_s), opusSupported);
        m_decoderCodecMap.add(AtomString("x-opus"_s), opusSupported);
    }

    auto vorbisSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/x-vorbis");
    if (vorbisSupported && (!m_isMediaSource || factories.hasElementForMediaType(ElementFactories::Type::AudioParser, "audio/x-vorbis"))) {
        m_decoderCodecMap.add(AtomString("vorbis"_s), vorbisSupported);
        m_decoderCodecMap.add(AtomString("x-vorbis"_s), vorbisSupported);
    }

    bool matroskaSupported = factories.hasElementForMediaType(ElementFactories::Type::Demuxer, "video/x-matroska");
    if (matroskaSupported) {
        auto vp8DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-vp8", ElementFactories::CheckHardwareClassifier::Yes);
        auto vp9DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-vp9", ElementFactories::CheckHardwareClassifier::Yes);

        if (vp8DecoderAvailable || vp9DecoderAvailable)
            m_decoderMimeTypeSet.add(AtomString("video/webm"_s));

        if (vp8DecoderAvailable) {
            m_decoderCodecMap.add(AtomString("vp8"_s), vp8DecoderAvailable);
            m_decoderCodecMap.add(AtomString("x-vp8"_s), vp8DecoderAvailable);
            m_decoderCodecMap.add(AtomString("vp8.0"_s), vp8DecoderAvailable);
        }
        if (vp9DecoderAvailable) {
            m_decoderCodecMap.add(AtomString("vp9"_s), vp9DecoderAvailable);
            m_decoderCodecMap.add(AtomString("x-vp9"_s), vp9DecoderAvailable);
            m_decoderCodecMap.add(AtomString("vp9.0"_s), vp9DecoderAvailable);
            m_decoderCodecMap.add(AtomString("vp09*"_s), vp9DecoderAvailable);
        }
        if (opusSupported)
            m_decoderMimeTypeSet.add(AtomString("audio/webm"_s));
    }

    bool shouldAddMP4Container = false;

    auto h264DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }", ElementFactories::CheckHardwareClassifier::Yes);
    auto h264AllFormatsDecoderAvailable = GStreamerRegistryScanner::RegistryLookupResult::merge(
        factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }, stream-format=(string)avc", ElementFactories::CheckHardwareClassifier::Yes),
        factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }, stream-format=(string)byte-stream", ElementFactories::CheckHardwareClassifier::Yes)
    );
    auto needsH264Parse = h264DecoderAvailable != h264AllFormatsDecoderAvailable;

    if (h264DecoderAvailable && (!needsH264Parse || factories.hasElementForMediaType(ElementFactories::Type::VideoParser, "video/x-h264"))) {
        shouldAddMP4Container = true;
        m_decoderCodecMap.add(AtomString("x-h264"_s), h264DecoderAvailable);
        m_decoderCodecMap.add(AtomString("avc*"_s), h264DecoderAvailable);
        m_decoderCodecMap.add(AtomString("mp4v*"_s), h264DecoderAvailable);
    }

    auto h265DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h265", ElementFactories::CheckHardwareClassifier::Yes);
    auto h265AllFormatsDecoderAvailable = GStreamerRegistryScanner::RegistryLookupResult::merge(
        factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h265, stream-format=(string)byte-stream", ElementFactories::CheckHardwareClassifier::Yes),
        GStreamerRegistryScanner::RegistryLookupResult::merge(
            factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h265, stream-format=(string)hev1", ElementFactories::CheckHardwareClassifier::Yes),
            factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h265, stream-format=(string)hvc1", ElementFactories::CheckHardwareClassifier::Yes)
        )
    );
    auto needsH265Parse = h265DecoderAvailable != h265AllFormatsDecoderAvailable;

    if (h265DecoderAvailable && (!needsH265Parse || factories.hasElementForMediaType(ElementFactories::Type::VideoParser, "video/x-h265"))) {
        shouldAddMP4Container = true;
        m_decoderCodecMap.add(AtomString("x-h265"_s), h265DecoderAvailable);
        m_decoderCodecMap.add(AtomString("hvc1*"_s), h265DecoderAvailable);
        m_decoderCodecMap.add(AtomString("hev1*"_s), h265DecoderAvailable);
    }

    if (shouldAddMP4Container) {
        m_decoderMimeTypeSet.add(AtomString("video/mp4"_s));
        m_decoderMimeTypeSet.add(AtomString("video/x-m4v"_s));
    }

    auto av1DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-av1", ElementFactories::CheckHardwareClassifier::Yes);
    if ((matroskaSupported || isContainerTypeSupported(Configuration::Decoding, "video/mp4"_s)) && av1DecoderAvailable) {
        m_decoderCodecMap.add(AtomString("av01*"_s), av1DecoderAvailable);
        m_decoderCodecMap.add(AtomString("av1"_s), av1DecoderAvailable);
        m_decoderCodecMap.add(AtomString("x-av1"_s), av1DecoderAvailable);
    }

    Vector<GstCapsWebKitMapping> mseCompatibleMapping = {
        { ElementFactories::Type::AudioDecoder, "audio/x-ac3", { }, { "x-ac3"_s, "ac-3"_s, "ac3"_s } },
        { ElementFactories::Type::AudioDecoder, "audio/x-eac3", { "audio/x-ac3"_s },  { "x-eac3"_s, "ec3"_s, "ec-3"_s, "eac3"_s } },
        { ElementFactories::Type::AudioDecoder, "audio/x-flac", { "audio/x-flac"_s, "audio/flac"_s }, {"x-flac"_s, "flac"_s } },
    };
    fillMimeTypeSetFromCapsMapping(factories, mseCompatibleMapping);

    if (m_isMediaSource)
        return;

    // The mime-types initialized below are not supported by the MSE backend.

    Vector<GstCapsWebKitMapping> mapping = {
        { ElementFactories::Type::AudioDecoder, "audio/midi", { "audio/midi"_s, "audio/riff-midi"_s }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-dts", { }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-sbc", { }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-sid", { }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-speex", { "audio/speex"_s, "audio/x-speex"_s }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-wavpack", { "audio/x-wavpack"_s }, { } },
        { ElementFactories::Type::VideoDecoder, "video/mpeg, mpegversion=(int){1,2}, systemstream=(boolean)false", { "video/mpeg"_s }, { "mpeg"_s } },
        { ElementFactories::Type::VideoDecoder, "video/mpegts", { }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-dirac", { }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-flash-video", { "video/flv"_s, "video/x-flv"_s }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-h263", { }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-msvideocodec", { "video/x-msvideo"_s }, { } },
        { ElementFactories::Type::Demuxer, "application/vnd.rn-realmedia", { }, { } },
        { ElementFactories::Type::Demuxer, "application/x-3gp", { }, { } },
        { ElementFactories::Type::Demuxer, "application/x-hls", { "application/vnd.apple.mpegurl"_s, "application/x-mpegurl"_s }, { } },
        { ElementFactories::Type::Demuxer, "application/x-pn-realaudio", { }, { } },
        { ElementFactories::Type::Demuxer, "application/dash+xml", { }, { } },
        { ElementFactories::Type::Demuxer, "audio/x-aiff", { }, { } },
        { ElementFactories::Type::Demuxer, "audio/x-wav", { "audio/x-wav"_s, "audio/wav"_s, "audio/vnd.wave"_s }, { "1"_s } },
        { ElementFactories::Type::Demuxer, "video/quicktime", { }, { } },
        { ElementFactories::Type::Demuxer, "video/quicktime, variant=(string)3gpp", { "video/3gpp"_s }, { } },
        { ElementFactories::Type::Demuxer, "video/x-ms-asf", { }, { } },
    };
    fillMimeTypeSetFromCapsMapping(factories, mapping);

    if (factories.hasElementForMediaType(ElementFactories::Type::Demuxer, "application/ogg")) {
        m_decoderMimeTypeSet.add(AtomString("application/ogg"_s));

        if (vorbisSupported) {
            m_decoderMimeTypeSet.add(AtomString("audio/ogg"_s));
            m_decoderMimeTypeSet.add(AtomString("audio/x-vorbis+ogg"_s));
        }

        if (auto result = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/x-speex")) {
            m_decoderMimeTypeSet.add(AtomString("audio/ogg"_s));
            m_decoderCodecMap.add(AtomString("speex"_s), result);
        }

        if (auto result = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-theora")) {
            m_decoderMimeTypeSet.add(AtomString("video/ogg"_s));
            m_decoderCodecMap.add(AtomString("theora"_s), result);
        }
    }

    bool audioMpegSupported = false;
    if (auto result = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/mpeg, mpegversion=(int)1, layer=(int)[1, 3]")) {
        audioMpegSupported = true;
        m_decoderMimeTypeSet.add(AtomString("audio/mp1"_s));
        m_decoderMimeTypeSet.add(AtomString("audio/mp3"_s));
        m_decoderMimeTypeSet.add(AtomString("audio/x-mp3"_s));
        m_decoderCodecMap.add(AtomString("audio/mp3"_s), result);
        m_decoderCodecMap.add(AtomString("mp3"_s), result);
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/mpeg, mpegversion=(int)2")) {
        audioMpegSupported = true;
        m_decoderMimeTypeSet.add(AtomString("audio/mp2"_s));
    }

    audioMpegSupported |= isContainerTypeSupported(Configuration::Decoding, "audio/mp4"_s);
    if (audioMpegSupported) {
        m_decoderMimeTypeSet.add(AtomString("audio/mpeg"_s));
        m_decoderMimeTypeSet.add(AtomString("audio/x-mpeg"_s));
    }

    if (matroskaSupported) {
        m_decoderMimeTypeSet.add(AtomString("video/x-matroska"_s));

        if (factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-vp10"))
            m_decoderMimeTypeSet.add(AtomString("video/webm"_s));
    }
}

void GStreamerRegistryScanner::initializeEncoders(const GStreamerRegistryScanner::ElementFactories& factories)
{
    // MSE is about playback, which means decoding. No need to check for encoders then.
    if (m_isMediaSource)
        return;

    auto aacSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/mpeg, mpegversion=(int)4");
    if (auto result = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/mpeg, mpegversion=(int)4")) {
        m_encoderCodecMap.add(AtomString("mpeg"_s), result);
        m_encoderCodecMap.add(AtomString("mp4a*"_s), result);
    }

    auto opusSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/x-opus");
    if (opusSupported) {
        m_encoderCodecMap.add(AtomString("opus"_s), opusSupported);
        m_encoderCodecMap.add(AtomString("x-opus"_s), opusSupported);
    }

    auto vorbisSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/x-vorbis");
    if (vorbisSupported) {
        m_encoderCodecMap.add(AtomString("vorbis"_s), vorbisSupported);
        m_encoderCodecMap.add(AtomString("x-vorbis"_s), vorbisSupported);
    }

    Vector<String> av1EncodersDisallowedList { "av1enc"_s };
    auto av1EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-av1", ElementFactories::CheckHardwareClassifier::Yes, std::make_optional(WTFMove(av1EncodersDisallowedList)));
    if (av1EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("av01*"_s), av1EncoderAvailable);
        m_encoderCodecMap.add(AtomString("av1"_s), av1EncoderAvailable);
        m_encoderCodecMap.add(AtomString("x-av1"_s), av1EncoderAvailable);
    }

    auto vp8EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-vp8", ElementFactories::CheckHardwareClassifier::Yes);
    if (vp8EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("vp8"_s), vp8EncoderAvailable);
        m_encoderCodecMap.add(AtomString("x-vp8"_s), vp8EncoderAvailable);
        m_encoderCodecMap.add(AtomString("vp8.0"_s), vp8EncoderAvailable);
    }

    auto vp9EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-vp9", ElementFactories::CheckHardwareClassifier::Yes);
    if (vp9EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("vp9"_s), vp9EncoderAvailable);
        m_encoderCodecMap.add(AtomString("x-vp9"_s), vp9EncoderAvailable);
        m_encoderCodecMap.add(AtomString("vp9.0"_s), vp9EncoderAvailable);
        m_encoderCodecMap.add(AtomString("vp09*"_s), vp9EncoderAvailable);
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "video/webm") && (vp8EncoderAvailable || vp9EncoderAvailable || av1EncoderAvailable))
        m_encoderMimeTypeSet.add(AtomString("video/webm"_s));

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "audio/webm")) {
        if (opusSupported)
            m_encoderMimeTypeSet.add(AtomString("audio/opus"_s));
        m_encoderMimeTypeSet.add(AtomString("audio/webm"_s));
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "audio/ogg") && (vorbisSupported || opusSupported))
        m_encoderMimeTypeSet.add(AtomString("audio/ogg"_s));

    auto h264EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }", ElementFactories::CheckHardwareClassifier::Yes);
    if (h264EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("h264"_s), h264EncoderAvailable);
        m_encoderCodecMap.add(AtomString("x-h264"_s), h264EncoderAvailable);
        m_encoderCodecMap.add(AtomString("avc*"_s), h264EncoderAvailable);
        m_encoderCodecMap.add(AtomString("mp4v*"_s), h264EncoderAvailable);
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "video/quicktime")) {
        if (opusSupported)
            m_encoderMimeTypeSet.add(AtomString("audio/opus"_s));
        if (aacSupported) {
            m_encoderMimeTypeSet.add(AtomString("audio/aac"_s));
            m_encoderMimeTypeSet.add(AtomString("audio/mp4"_s));
            m_encoderMimeTypeSet.add(AtomString("audio/x-m4a"_s));
        }
        if (h264EncoderAvailable) {
            m_encoderMimeTypeSet.add(AtomString("video/mp4"_s));
            m_encoderMimeTypeSet.add(AtomString("video/x-m4v"_s));
        }
    }
}

GStreamerRegistryScanner::CodecLookupResult GStreamerRegistryScanner::isCodecSupported(Configuration configuration, const String& codec, bool shouldCheckForHardwareUse) const
{
    // If the codec is named like a mimetype (eg: video/avc) remove the "video/" part.
    size_t slashIndex = codec.find('/');
    String codecName = slashIndex != notFound ? codec.substring(slashIndex + 1) : codec;

    CodecLookupResult result;
    if (codecName.startsWith("avc1"_s))
        result = isAVC1CodecSupported(configuration, codecName, shouldCheckForHardwareUse);
    else {
        auto& codecMap = configuration == Configuration::Decoding ? m_decoderCodecMap : m_encoderCodecMap;
        for (const auto& [codecId, lookupResult] : codecMap) {
            if (!fnmatch(codecId.string().utf8().data(), codecName.utf8().data(), 0)) {
                bool isSupported = shouldCheckForHardwareUse ? lookupResult.isUsingHardware : true;
                if (isSupported) {
                    result.isSupported = true;
                    result.factory = lookupResult.factory;
                    break;
                }
            }
        }
    }

    const char* configLogString = configurationNameForLogging(configuration);
    GST_LOG("Checked %s %s codec \"%s\" supported %s", shouldCheckForHardwareUse ? "hardware" : "software", configLogString, codecName.utf8().data(), boolForPrinting(result.isSupported));
    return result;
}

bool GStreamerRegistryScanner::supportsFeatures(const String& features) const
{
    // Apple TV requires this one for DD+.
    constexpr auto dolbyDigitalPlusJOC = "joc"_s;
    if (features == dolbyDigitalPlusJOC)
        return true;

    return false;
}

MediaPlayerEnums::SupportsType GStreamerRegistryScanner::isContentTypeSupported(Configuration configuration, const ContentType& contentType, const Vector<ContentType>& contentTypesRequiringHardwareSupport) const
{
    using SupportsType = MediaPlayerEnums::SupportsType;

    const auto& containerType = contentType.containerType().convertToASCIILowercase();
    if (!isContainerTypeSupported(configuration, containerType))
        return SupportsType::IsNotSupported;

    int channels = parseInteger<int>(contentType.parameter("channels"_s)).value_or(1);
    String features = contentType.parameter("features"_s);
    if (channels > MEDIA_MAX_AAC_CHANNELS || channels <= 0
        || !(features.isEmpty() || supportsFeatures(features))
        || parseInteger<unsigned>(contentType.parameter("width"_s)).value_or(0) > MEDIA_MAX_WIDTH
        || parseInteger<unsigned>(contentType.parameter("height"_s)).value_or(0) > MEDIA_MAX_HEIGHT
        || parseInteger<unsigned>(contentType.parameter("framerate"_s)).value_or(0) > MEDIA_MAX_FRAMERATE)
        return SupportsType::IsNotSupported;

    const auto& codecs = contentType.codecs();

    // Spec says we should not return "probably" if the codecs string is empty.
    if (codecs.isEmpty())
        return SupportsType::MayBeSupported;

    for (const auto& item : codecs) {
        auto codec = item.convertToASCIILowercase();
        bool requiresHardwareSupport = contentTypesRequiringHardwareSupport
            .findIf([containerType, codec](auto& hardwareContentType) -> bool {
            auto hardwareContainer = hardwareContentType.containerType();
            if (!hardwareContainer.isEmpty()
                && fnmatch(hardwareContainer.utf8().data(), containerType.utf8().data(), 0))
                return false;
            auto hardwareCodecs = hardwareContentType.codecs();
            return hardwareCodecs.isEmpty()
                || hardwareCodecs.findIf([codec](auto& hardwareCodec) -> bool {
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
    for (const auto& codec : codecs) {
        if (!isCodecSupported(configuration, codec, shouldCheckForHardwareUse))
            return false;
    }

    return true;
}

GStreamerRegistryScanner::CodecLookupResult GStreamerRegistryScanner::isAVC1CodecSupported(Configuration configuration, const String& codec, bool shouldCheckForHardwareUse) const
{
    auto checkH264Caps = [&](const char* capsString) -> CodecLookupResult {
        OptionSet<ElementFactories::Type> factoryTypes;
        switch (configuration) {
        case Configuration::Decoding:
            factoryTypes.add(ElementFactories::Type::VideoDecoder);
            break;
        case Configuration::Encoding:
            factoryTypes.add(ElementFactories::Type::VideoEncoder);
            break;
        }
        auto lookupResult = ElementFactories(factoryTypes).hasElementForMediaType(factoryTypes.toSingleValue().value(), capsString, ElementFactories::CheckHardwareClassifier::Yes);
        bool supported = lookupResult && shouldCheckForHardwareUse ? lookupResult.isUsingHardware : true;
        GST_DEBUG("%s decoding supported for codec %s: %s", shouldCheckForHardwareUse ? "Hardware" : "Software", codec.utf8().data(), boolForPrinting(supported));
        return { supported, supported ? lookupResult.factory : nullptr };
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
        return { false, nullptr };
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
            return { false, nullptr };
        }
        if (levelAsInteger > maxLevel)
            return { false, nullptr };
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

GStreamerRegistryScanner::RegistryLookupResult GStreamerRegistryScanner::isConfigurationSupported(Configuration configuration, const MediaConfiguration& mediaConfiguration) const
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
            audioConfiguration.bitrate.value_or(0), audioConfiguration.samplerate.value_or(0));
        auto contentType = ContentType(audioConfiguration.contentType);
        isSupported = isContainerTypeSupported(configuration, contentType.containerType());
    }

    return { isSupported, isUsingHardware, nullptr };
}

#if USE(GSTREAMER_WEBRTC)
RTCRtpCapabilities GStreamerRegistryScanner::audioRtpCapabilities(Configuration configuration)
{
    RTCRtpCapabilities capabilies;
    fillAudioRtpCapabilities(configuration, capabilies);
    return capabilies;
}

RTCRtpCapabilities GStreamerRegistryScanner::videoRtpCapabilities(Configuration configuration)
{
    RTCRtpCapabilities capabilies;
    fillVideoRtpCapabilities(configuration, capabilies);
    return capabilies;
}

static inline Vector<RTCRtpCapabilities::HeaderExtensionCapability> probeRtpExtensions(const Vector<const char*>& candidates)
{
    Vector<RTCRtpCapabilities::HeaderExtensionCapability> extensions;
    for (const auto& uri : candidates) {
        if (auto extension = adoptGRef(gst_rtp_header_extension_create_from_uri(uri)))
            extensions.append({ String::fromLatin1(uri) });
    }
    return extensions;
}

void GStreamerRegistryScanner::fillAudioRtpCapabilities(Configuration configuration, RTCRtpCapabilities& capabilities)
{
    if (!m_audioRtpExtensions)
        m_audioRtpExtensions = probeRtpExtensions(m_allAudioRtpExtensions);
    if (m_audioRtpExtensions)
        capabilities.headerExtensions = copyToVector(*m_audioRtpExtensions);

    auto codecElement = ElementFactories::Type::AudioDecoder;
    auto rtpElement = ElementFactories::Type::RtpDepayloader;
    if (configuration == Configuration::Encoding) {
        codecElement = ElementFactories::Type::AudioEncoder;
        rtpElement = ElementFactories::Type::RtpPayloader;
    }

    auto factories = ElementFactories({ codecElement, rtpElement });
    if (factories.hasElementForMediaType(codecElement, "audio/x-opus") && factories.hasElementForMediaType(rtpElement, "audio/x-opus"))
        capabilities.codecs.append({ .mimeType = "audio/OPUS"_s, .clockRate = 48000, .channels = 2, .sdpFmtpLine = "minptime=10;useinbandfec=1"_s });

    if (factories.hasElementForMediaType(codecElement, "audio/isac") && factories.hasElementForMediaType(rtpElement, "audio/isac")) {
        capabilities.codecs.append({ .mimeType = "audio/ISAC"_s, .clockRate = 16000, .channels = 1, .sdpFmtpLine = emptyString() });
        capabilities.codecs.append({ .mimeType = "audio/ISAC"_s, .clockRate = 32000, .channels = 1, .sdpFmtpLine = emptyString() });
    }

    if (factories.hasElementForMediaType(codecElement, "audio/G722") && factories.hasElementForMediaType(rtpElement, "audio/G722"))
        capabilities.codecs.append({ .mimeType = "audio/G722"_s, .clockRate = 8000, .channels = 1, .sdpFmtpLine = emptyString() });

    if (factories.hasElementForMediaType(codecElement, "audio/x-mulaw") && factories.hasElementForMediaType(rtpElement, "audio/x-mulaw"))
        capabilities.codecs.append({ .mimeType = "audio/PCMU"_s, .clockRate = 8000, .channels = 1, .sdpFmtpLine = emptyString() });

    if (factories.hasElementForMediaType(codecElement, "audio/x-alaw") && factories.hasElementForMediaType(rtpElement, "audio/x-alaw"))
        capabilities.codecs.append({ .mimeType = "audio/PCMA"_s, .clockRate = 8000, .channels = 1, .sdpFmtpLine = emptyString() });
}

void GStreamerRegistryScanner::fillVideoRtpCapabilities(Configuration configuration, RTCRtpCapabilities& capabilities)
{
    if (!m_videoRtpExtensions)
        m_videoRtpExtensions = probeRtpExtensions(m_allVideoRtpExtensions);
    if (m_videoRtpExtensions)
        capabilities.headerExtensions = copyToVector(*m_videoRtpExtensions);

    auto codecElement = ElementFactories::Type::VideoDecoder;
    auto rtpElement = ElementFactories::Type::RtpDepayloader;
    if (configuration == Configuration::Encoding) {
        codecElement = ElementFactories::Type::VideoEncoder;
        rtpElement = ElementFactories::Type::RtpPayloader;
    }

    auto factories = ElementFactories({ codecElement, rtpElement });

    if (factories.hasElementForMediaType(codecElement, "video/x-h264") && factories.hasElementForMediaType(rtpElement, "video/x-h264")) {
        // FIXME: Profile levels are hardcoded here for the time being. It might be a good idea to
        // actually probe those on the selected encoder.
        capabilities.codecs.append({ .mimeType = "video/H264"_s, .clockRate = 90000, .channels = { },
            .sdpFmtpLine = "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=640c1f"_s });
        capabilities.codecs.append({ .mimeType = "video/H264"_s, .clockRate = 90000, .channels = { },
            .sdpFmtpLine = "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f"_s });
        capabilities.codecs.append({ .mimeType = "video/H264"_s, .clockRate = 90000, .channels = { },
            .sdpFmtpLine = "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=640c1f"_s });
        capabilities.codecs.append({ .mimeType = "video/H264"_s, .clockRate = 90000, .channels = { },
            .sdpFmtpLine = "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f"_s });
        capabilities.codecs.append({ .mimeType = "video/H264"_s, .clockRate = 90000, .channels = { },
            .sdpFmtpLine = "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f"_s });
        capabilities.codecs.append({ .mimeType = "video/H264"_s, .clockRate = 90000, .channels = { },
            .sdpFmtpLine = "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f"_s });
        capabilities.codecs.append({ .mimeType = "video/H264"_s, .clockRate = 90000, .channels = { },
            .sdpFmtpLine = "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=4d001f"_s });
        capabilities.codecs.append({ .mimeType = "video/H264"_s, .clockRate = 90000, .channels = { },
            .sdpFmtpLine = "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=4d001f"_s });
    }

    // FIXME: Probe for video/H265 capabilies.
    // FIXME: Probe for video/AV1 capabilies.

    if (factories.hasElementForMediaType(codecElement, "video/x-vp8") && factories.hasElementForMediaType(rtpElement, "video/x-vp8"))
        capabilities.codecs.append({ .mimeType = "video/VP8"_s, .clockRate = 90000, .channels = { }, .sdpFmtpLine = { } });

    if (factories.hasElementForMediaType(codecElement, "video/x-vp9") && factories.hasElementForMediaType(rtpElement, "video/x-vp9")) {
        // FIXME: Profile levels are hardcoded here for the time being. It might be a good idea to
        // actually probe those on the selected encoder.
        capabilities.codecs.append({ .mimeType = "video/VP9"_s, .clockRate = 90000, .channels = { }, .sdpFmtpLine = "profile-id=0"_s });
        capabilities.codecs.append({ .mimeType = "video/VP9"_s, .clockRate = 90000, .channels = { }, .sdpFmtpLine = "profile-id=2"_s });
    }
}

Vector<RTCRtpCapabilities::HeaderExtensionCapability> GStreamerRegistryScanner::audioRtpExtensions()
{
    if (!m_audioRtpExtensions)
        m_audioRtpExtensions = probeRtpExtensions(m_allAudioRtpExtensions);
    return *m_audioRtpExtensions;
}

Vector<RTCRtpCapabilities::HeaderExtensionCapability> GStreamerRegistryScanner::videoRtpExtensions()
{
    if (!m_videoRtpExtensions)
        m_videoRtpExtensions = probeRtpExtensions(m_allVideoRtpExtensions);
    return *m_videoRtpExtensions;
}

#endif // USE(GSTREAMER_WEBRTC)

}
#endif
