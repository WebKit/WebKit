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

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_media_gst_registry_scanner_debug);
#define GST_CAT_DEFAULT webkit_media_gst_registry_scanner_debug

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
    if (factoryType == Type::AudioEncoder || factoryType == Type::VideoEncoder || factoryType == Type::Muxer)
        padDirection = GST_PAD_SRC;

    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_from_string(capsString));
    GList* candidates = gst_element_factory_list_filter(elementFactories, caps.get(), padDirection, false);
    bool isSupported = candidates;
    bool isUsingHardware = false;

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
            String metadata = gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_KLASS);
            auto components = metadata.split('/');
            if (components.contains("Hardware")) {
                isUsingHardware = true;
                break;
            }
        }
    }

    gst_plugin_feature_list_free(candidates);
    GST_LOG("Lookup result for %s matching caps %" GST_PTR_FORMAT " : isSupported=%s, isUsingHardware=%s", elementFactoryTypeToString(factoryType), caps.get(), boolForPrinting(isSupported), boolForPrinting(isUsingHardware));
    return { isSupported, isUsingHardware };
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
        if (factories.hasElementForMediaType(current.elementType, current.capsString)) {
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

void GStreamerRegistryScanner::initializeDecoders(const GStreamerRegistryScanner::ElementFactories& factories)
{
    if (factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/mpeg, mpegversion=(int)4")) {
        m_decoderMimeTypeSet.add(AtomString("audio/aac"));
        m_decoderMimeTypeSet.add(AtomString("audio/mp4"));
        m_decoderMimeTypeSet.add(AtomString("audio/x-m4a"));
        m_decoderMimeTypeSet.add(AtomString("audio/mpeg"));
        m_decoderMimeTypeSet.add(AtomString("audio/x-mpeg"));
        m_decoderCodecMap.add(AtomString("mpeg"), false);
        m_decoderCodecMap.add(AtomString("mp4a*"), false);
    }

    auto opusSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/x-opus");
    if (opusSupported && (!m_isMediaSource || factories.hasElementForMediaType(ElementFactories::Type::AudioParser, "audio/x-opus"))) {
        m_decoderMimeTypeSet.add(AtomString("audio/opus"));
        m_decoderCodecMap.add(AtomString("opus"), false);
        m_decoderCodecMap.add(AtomString("x-opus"), false);
    }

    auto vorbisSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/x-vorbis");
    if (vorbisSupported && (!m_isMediaSource || factories.hasElementForMediaType(ElementFactories::Type::AudioParser, "audio/x-vorbis"))) {
        m_decoderCodecMap.add(AtomString("vorbis"), false);
        m_decoderCodecMap.add(AtomString("x-vorbis"), false);
    }

    bool matroskaSupported = factories.hasElementForMediaType(ElementFactories::Type::Demuxer, "video/x-matroska");
    if (matroskaSupported) {
        auto vp8DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-vp8", ElementFactories::CheckHardwareClassifier::Yes);
        auto vp9DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-vp9", ElementFactories::CheckHardwareClassifier::Yes);

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

    auto h264DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }", ElementFactories::CheckHardwareClassifier::Yes);
    if (h264DecoderAvailable && (!m_isMediaSource || factories.hasElementForMediaType(ElementFactories::Type::VideoParser, "video/x-h264"))) {
        m_decoderMimeTypeSet.add(AtomString("video/mp4"));
        m_decoderMimeTypeSet.add(AtomString("video/x-m4v"));
        m_decoderCodecMap.add(AtomString("x-h264"), h264DecoderAvailable.isUsingHardware);
        m_decoderCodecMap.add(AtomString("avc*"), h264DecoderAvailable.isUsingHardware);
        m_decoderCodecMap.add(AtomString("mp4v*"), h264DecoderAvailable.isUsingHardware);
    }

    Vector<String> av1DecodersDisallowedList { "av1dec"_s };
    if ((matroskaSupported || isContainerTypeSupported(Configuration::Decoding, "video/mp4")) && factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-av1", ElementFactories::CheckHardwareClassifier::No, std::make_optional(WTFMove(av1DecodersDisallowedList)))) {
        m_decoderCodecMap.add(AtomString("av01*"), false);
        m_decoderCodecMap.add(AtomString("av1"), false);
        m_decoderCodecMap.add(AtomString("x-av1"), false);
    }

    if (m_isMediaSource)
        return;

    // The mime-types initialized below are not supported by the MSE backend.

    Vector<GstCapsWebKitMapping> mapping = {
        { ElementFactories::Type::AudioDecoder, "audio/midi", { "audio/midi", "audio/riff-midi" }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-ac3", { }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-dts", { }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-eac3", { "audio/x-ac3" }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-flac", { "audio/x-flac", "audio/flac" }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-sbc", { }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-sid", { }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-speex", { "audio/speex", "audio/x-speex" }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-wavpack", { "audio/x-wavpack" }, { } },
        { ElementFactories::Type::VideoDecoder, "video/mpeg, mpegversion=(int){1,2}, systemstream=(boolean)false", { "video/mpeg" }, { "mpeg" } },
        { ElementFactories::Type::VideoDecoder, "video/mpegts", { }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-dirac", { }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-flash-video", { "video/flv", "video/x-flv" }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-h263", { }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-msvideocodec", { "video/x-msvideo" }, { } },
        { ElementFactories::Type::Demuxer, "application/vnd.rn-realmedia", { }, { } },
        { ElementFactories::Type::Demuxer, "application/x-3gp", { }, { } },
        { ElementFactories::Type::Demuxer, "application/x-hls", { "application/vnd.apple.mpegurl", "application/x-mpegurl" }, { } },
        { ElementFactories::Type::Demuxer, "application/x-pn-realaudio", { }, { } },
        { ElementFactories::Type::Demuxer, "application/dash+xml", { }, { } },
        { ElementFactories::Type::Demuxer, "audio/x-aiff", { }, { } },
        { ElementFactories::Type::Demuxer, "audio/x-wav", { "audio/x-wav", "audio/wav", "audio/vnd.wave" }, { "1" } },
        { ElementFactories::Type::Demuxer, "video/quicktime", { }, { } },
        { ElementFactories::Type::Demuxer, "video/quicktime, variant=(string)3gpp", { "video/3gpp" }, { } },
        { ElementFactories::Type::Demuxer, "video/x-ms-asf", { }, { } },
    };
    fillMimeTypeSetFromCapsMapping(factories, mapping);

    if (factories.hasElementForMediaType(ElementFactories::Type::Demuxer, "application/ogg")) {
        m_decoderMimeTypeSet.add(AtomString("application/ogg"));

        if (vorbisSupported) {
            m_decoderMimeTypeSet.add(AtomString("audio/ogg"));
            m_decoderMimeTypeSet.add(AtomString("audio/x-vorbis+ogg"));
        }

        if (factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/x-speex")) {
            m_decoderMimeTypeSet.add(AtomString("audio/ogg"));
            m_decoderCodecMap.add(AtomString("speex"), false);
        }

        if (factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-theora")) {
            m_decoderMimeTypeSet.add(AtomString("video/ogg"));
            m_decoderCodecMap.add(AtomString("theora"), false);
        }
    }

    bool audioMpegSupported = false;
    if (factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/mpeg, mpegversion=(int)1, layer=(int)[1, 3]")) {
        audioMpegSupported = true;
        m_decoderMimeTypeSet.add(AtomString("audio/mp1"));
        m_decoderMimeTypeSet.add(AtomString("audio/mp3"));
        m_decoderMimeTypeSet.add(AtomString("audio/x-mp3"));
        m_decoderCodecMap.add(AtomString("audio/mp3"), false);
        m_decoderCodecMap.add(AtomString("mp3"), false);
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/mpeg, mpegversion=(int)2")) {
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

        if (factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-vp10"))
            m_decoderMimeTypeSet.add(AtomString("video/webm"));
    }
}

void GStreamerRegistryScanner::initializeEncoders(const GStreamerRegistryScanner::ElementFactories& factories)
{
    // MSE is about playback, which means decoding. No need to check for encoders then.
    if (m_isMediaSource)
        return;

    auto aacSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/mpeg, mpegversion=(int)4");
    if (factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/mpeg, mpegversion=(int)4")) {
        m_encoderCodecMap.add(AtomString("mpeg"), false);
        m_encoderCodecMap.add(AtomString("mp4a*"), false);
    }

    auto opusSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/x-opus");
    if (opusSupported) {
        m_encoderCodecMap.add(AtomString("opus"), false);
        m_encoderCodecMap.add(AtomString("x-opus"), false);
    }

    auto vorbisSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/x-vorbis");
    if (vorbisSupported) {
        m_encoderCodecMap.add(AtomString("vorbis"), false);
        m_encoderCodecMap.add(AtomString("x-vorbis"), false);
    }

    Vector<String> av1EncodersDisallowedList { "av1enc"_s };
    auto av1EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-av1", ElementFactories::CheckHardwareClassifier::Yes, std::make_optional(WTFMove(av1EncodersDisallowedList)));
    if (av1EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("av01*"), false);
        m_encoderCodecMap.add(AtomString("av1"), false);
        m_encoderCodecMap.add(AtomString("x-av1"), false);
    }

    auto vp8EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-vp8", ElementFactories::CheckHardwareClassifier::Yes);
    if (vp8EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("vp8"), vp8EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("x-vp8"), vp8EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("vp8.0"), vp8EncoderAvailable.isUsingHardware);
    }

    auto vp9EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-vp9", ElementFactories::CheckHardwareClassifier::Yes);
    if (vp9EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("vp9"), vp9EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("x-vp9"), vp9EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("vp9.0"), vp9EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("vp09*"), vp9EncoderAvailable.isUsingHardware);
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "video/webm") && (vp8EncoderAvailable || vp9EncoderAvailable || av1EncoderAvailable))
        m_encoderMimeTypeSet.add(AtomString("video/webm"));

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "audio/webm")) {
        if (opusSupported)
            m_encoderMimeTypeSet.add(AtomString("audio/opus"));
        m_encoderMimeTypeSet.add(AtomString("audio/webm"));
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "audio/ogg") && (vorbisSupported || opusSupported))
        m_encoderMimeTypeSet.add(AtomString("audio/ogg"));

    auto h264EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }", ElementFactories::CheckHardwareClassifier::Yes);
    if (h264EncoderAvailable) {
        m_encoderCodecMap.add(AtomString("x-h264"), h264EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("avc*"), h264EncoderAvailable.isUsingHardware);
        m_encoderCodecMap.add(AtomString("mp4v*"), h264EncoderAvailable.isUsingHardware);
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "video/quicktime")) {
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

bool GStreamerRegistryScanner::isCodecSupported(Configuration configuration, const String& codec, bool shouldCheckForHardwareUse) const
{
    // If the codec is named like a mimetype (eg: video/avc) remove the "video/" part.
    size_t slashIndex = codec.find('/');
    String codecName = slashIndex != WTF::notFound ? codec.substring(slashIndex + 1) : codec;

    bool supported = false;
    if (codecName.startsWith("avc1"))
        supported = isAVC1CodecSupported(configuration, codecName, shouldCheckForHardwareUse);
    else {
        auto& codecMap = configuration == Configuration::Decoding ? m_decoderCodecMap : m_encoderCodecMap;
        for (const auto& item : codecMap) {
            if (!fnmatch(item.key.string().utf8().data(), codecName.utf8().data(), 0)) {
                supported = shouldCheckForHardwareUse ? item.value : true;
                if (supported)
                    break;
            }
        }
    }

    const char* configLogString = configurationNameForLogging(configuration);
    GST_LOG("Checked %s %s codec \"%s\" supported %s", shouldCheckForHardwareUse ? "hardware" : "software", configLogString, codecName.utf8().data(), boolForPrinting(supported));
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
    for (const auto& codec : codecs) {
        if (!isCodecSupported(configuration, codec, shouldCheckForHardwareUse))
            return false;
    }

    return true;
}

bool GStreamerRegistryScanner::isAVC1CodecSupported(Configuration configuration, const String& codec, bool shouldCheckForHardwareUse) const
{
    auto checkH264Caps = [&](const char* capsString) {
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

    return { isSupported, isUsingHardware };
}

}
#endif
