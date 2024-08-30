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
#include "GStreamerQuirks.h"

#if USE(GSTREAMER)
#include "ContentType.h"
#include "GStreamerCodecUtilities.h"
#include "GStreamerCommon.h"
#include "RuntimeApplicationChecks.h"
#include <fnmatch.h>
#include <gst/pbutils/codec-utils.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PrintStream.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

#if USE(GSTREAMER_WEBRTC)
#include <gst/rtp/rtp.h>
#endif

#if ENABLE(VIDEO)
#include "VideoEncoderPrivateGStreamer.h"
#endif

namespace {
struct VideoDecodingLimits {
    unsigned mediaMaxWidth = 0;
    unsigned mediaMaxHeight = 0;
    unsigned mediaMaxFrameRate = 0;
    VideoDecodingLimits(unsigned mediaMaxWidth, unsigned mediaMaxHeight, unsigned mediaMaxFrameRate)
        : mediaMaxWidth(mediaMaxWidth)
        , mediaMaxHeight(mediaMaxHeight)
        , mediaMaxFrameRate(mediaMaxFrameRate)
    {
    }
};
}

#ifdef VIDEO_DECODING_LIMIT
static std::optional<VideoDecodingLimits> videoDecoderLimitsDefaults()
{
    // VIDEO_DECODING_LIMIT should be in format: WIDTHxHEIGHT@FRAMERATE.
    String videoDecodingLimit(String::fromUTF8(VIDEO_DECODING_LIMIT));

    if (videoDecodingLimit.isEmpty())
        return { };

    Vector<String> entries;

    // Extract frame rate part from the VIDEO_DECODING_LIMIT: WIDTHxHEIGHT@FRAMERATE.
    videoDecodingLimit.split('@', [&entries](StringView item) {
        entries.append(item.toString());
    });

    if (entries.size() != 2)
        return { };

    auto frameRate = parseIntegerAllowingTrailingJunk<unsigned>(entries[1]);

    if (!frameRate.has_value())
        return { };

    const auto widthAndHeight = entries[0].split('x');

    if (widthAndHeight.size() != 2)
        return { };

    const auto width = parseIntegerAllowingTrailingJunk<unsigned>(widthAndHeight[0]);

    if (!width.has_value())
        return { };

    const auto height = parseIntegerAllowingTrailingJunk<unsigned>(widthAndHeight[1]);

    if (!height.has_value())
        return { };

    return { VideoDecodingLimits(width.value(), height.value(), frameRate.value()) };
}
#endif

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_media_gst_registry_scanner_debug);
#define GST_CAT_DEFAULT webkit_media_gst_registry_scanner_debug

// We shouldn't accept media that the player can't actually play.
// AAC supports up to 96 channels.
#define MEDIA_MAX_AAC_CHANNELS 96

static bool singletonInitialized = false;

bool GStreamerRegistryScanner::singletonWasInitialized()
{
    return singletonInitialized;
}

GStreamerRegistryScanner& GStreamerRegistryScanner::singleton()
{
    static NeverDestroyed<GStreamerRegistryScanner> sharedInstance;
    singletonInitialized = true;
    return sharedInstance;
}

void teardownGStreamerRegistryScanner()
{
    if (!GStreamerRegistryScanner::singletonWasInitialized())
        return;

    auto& scanner = GStreamerRegistryScanner::singleton();
    scanner.teardown();
}

void GStreamerRegistryScanner::getSupportedDecodingTypes(HashSet<String>& types)
{
    if (isInWebProcess())
        types = singleton().mimeTypeSet(GStreamerRegistryScanner::Configuration::Decoding);
    else
        types = GStreamerRegistryScanner().mimeTypeSet(GStreamerRegistryScanner::Configuration::Decoding);
}

GStreamerRegistryScanner::ElementFactories::ElementFactories(OptionSet<ElementFactories::Type> types)
{
    auto& quirksManager = GStreamerQuirksManager::singleton();
    auto audioVideoDecoderFactory = quirksManager.audioVideoDecoderFactoryListType();
    if (types.contains(Type::AudioDecoder))
        audioDecoderFactories = gst_element_factory_list_get_elements(audioVideoDecoderFactory | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, GST_RANK_MARGINAL);
    if (types.contains(Type::VideoDecoder))
        videoDecoderFactories = gst_element_factory_list_get_elements(audioVideoDecoderFactory | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_MARGINAL);
    if (types.contains(Type::AudioParser))
        audioParserFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_PARSER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, GST_RANK_NONE);
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
    if (types.contains(Type::Decryptor))
        decryptorFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECRYPTOR, GST_RANK_MARGINAL);
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
    gst_plugin_feature_list_free(decryptorFactories);
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
    case Type::Decryptor:
        return "Decryptor";
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
    case GStreamerRegistryScanner::ElementFactories::Type::Decryptor:
        return decryptorFactories;
    case GStreamerRegistryScanner::ElementFactories::Type::All:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

GStreamerRegistryScanner::RegistryLookupResult GStreamerRegistryScanner::ElementFactories::hasElementForMediaType(ElementFactories::Type factoryType, const ASCIILiteral& capsString, ElementFactories::CheckHardwareClassifier shouldCheckHardwareClassifier, std::optional<Vector<String>> disallowedList) const
{
    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_from_string(capsString.characters()));
    if (!caps)
        return { };

    return hasElementForCaps(factoryType, caps, shouldCheckHardwareClassifier, disallowedList);
}

static Vector<GRefPtr<GstElementFactory>> findCompatibleFactories(GList* list, const GRefPtr<GstCaps>& caps, GstPadDirection direction)
{
    Vector<GRefPtr<GstElementFactory>> results;
    results.reserveInitialCapacity(g_list_length(list));
    for (; list; list = g_list_next(list)) {
        auto factory = GST_ELEMENT_FACTORY_CAST(list->data);
        const auto templates = gst_element_factory_get_static_pad_templates(factory);
        for (GList* walk = const_cast<GList*>(templates); walk; walk = g_list_next(walk)) {
            auto capsTemplate = static_cast<GstStaticPadTemplate*>(walk->data);
            if (capsTemplate->direction != direction)
                continue;

            auto templateCaps = adoptGRef(gst_static_caps_get(&capsTemplate->static_caps));
            if (gst_caps_is_any(templateCaps.get()) || !gst_caps_can_intersect(caps.get(), templateCaps.get()))
                continue;

            results.append(factory);
            break;
        }
    }

    results.shrinkToFit();
    return results;
}

GStreamerRegistryScanner::RegistryLookupResult GStreamerRegistryScanner::ElementFactories::hasElementForCaps(ElementFactories::Type factoryType, const GRefPtr<GstCaps>& caps, ElementFactories::CheckHardwareClassifier shouldCheckHardwareClassifier, std::optional<Vector<String>> disallowedList) const
{
    auto* elementFactories = factory(factoryType);
    if (!elementFactories)
        return { };

    RELEASE_ASSERT(!gst_caps_is_any(caps.get()));

    GstPadDirection padDirection = GST_PAD_SINK;
    if (factoryType == Type::AudioEncoder || factoryType == Type::VideoEncoder || factoryType == Type::Muxer || factoryType == Type::RtpDepayloader)
        padDirection = GST_PAD_SRC;

    // We can't use gst_element_factory_list_filter here because it would allow-list elements with
    // pads using ANY in their caps template.
    auto candidates = findCompatibleFactories(elementFactories, caps, padDirection);
    bool isSupported = !candidates.isEmpty();
    bool isUsingHardware = false;
    GRefPtr<GstElementFactory> selectedFactory;
    if (isSupported)
        selectedFactory = candidates.first();

    if (disallowedList && !disallowedList->isEmpty()) {
        bool hasValidCandidate = false;
        for (auto& factory : candidates) {
            String name = String::fromUTF8(gst_plugin_feature_get_name(GST_PLUGIN_FEATURE_CAST(factory.get())));
            if (disallowedList->contains(name))
                continue;

            selectedFactory = factory;
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
        for (auto& factory : candidates) {
            auto metadata = String::fromLatin1(gst_element_factory_get_metadata(factory.get(), GST_ELEMENT_METADATA_KLASS));
            auto components = metadata.split('/');
            auto& quirksManager = GStreamerQuirksManager::singleton();
            if (quirksManager.isEnabled()) {
                auto isAccelerated = quirksManager.isHardwareAccelerated(factory.get());
                if (isAccelerated && *isAccelerated) {
                    isUsingHardware = true;
                    selectedFactory = factory;
                    break;
                }
            }
            if (components.contains("Hardware"_s)) {
                isUsingHardware = true;
                selectedFactory = factory;
                break;
            }
        }
    }

    if (!isSupported)
        selectedFactory.clear();

    GST_LOG("Lookup result for %s matching caps %" GST_PTR_FORMAT " : isSupported=%s, isUsingHardware=%s, factory=%" GST_PTR_FORMAT, elementFactoryTypeToString(factoryType), caps.get(), boolForPrinting(isSupported), boolForPrinting(isUsingHardware), selectedFactory.get());
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

    refresh();
}

void GStreamerRegistryScanner::refresh()
{
#if USE(GSTREAMER_WEBRTC)
    m_audioRtpExtensions.reset();
    m_videoRtpExtensions.reset();
#endif

    ElementFactories factories(OptionSet<ElementFactories::Type>::fromRaw(static_cast<unsigned>(ElementFactories::Type::All)));
    initializeDecoders(factories);
    initializeEncoders(factories);

#ifndef GST_DISABLE_GST_DEBUG
    GST_DEBUG("%s registry scanner initialized", m_isMediaSource ? "MSE" : "Regular playback");
    for (auto& mimeType : m_decoderMimeTypeSet)
        GST_DEBUG("Decoder mime-type registered: %s", mimeType.utf8().data());
    for (auto& [codec, isHardware] : m_decoderCodecMap)
        GST_DEBUG("%s decoder codec pattern registered: %s", isHardware ? "Hardware" : "Software", codec.utf8().data());
    for (auto& mimeType : m_encoderMimeTypeSet)
        GST_DEBUG("Encoder mime-type registered: %s", mimeType.utf8().data());
    for (auto& [codec, isHardware] : m_encoderCodecMap)
        GST_DEBUG("%s encoder codec pattern registered: %s", isHardware ? "Hardware" : "Software", codec.utf8().data());
#endif
}

void GStreamerRegistryScanner::teardown()
{
    m_decoderCodecMap.clear();
    m_encoderCodecMap.clear();
}

const HashSet<String>& GStreamerRegistryScanner::mimeTypeSet(Configuration configuration) const
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
            if (!current.webkitMIMETypes.isEmpty()) {
                for (const auto& mimeType : current.webkitMIMETypes)
                    m_decoderMimeTypeSet.add(mimeType);
            } else
                m_decoderMimeTypeSet.add(current.capsString);
        }
    }
}

void GStreamerRegistryScanner::initializeDecoders(const GStreamerRegistryScanner::ElementFactories& factories)
{
    m_decoderCodecMap.clear();
    m_decoderMimeTypeSet.clear();
    if (auto result = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/mpeg, mpegversion=(int)4"_s)) {
        m_decoderMimeTypeSet.add("audio/aac"_s);
        m_decoderMimeTypeSet.add("audio/mp4"_s);
        m_decoderMimeTypeSet.add("audio/x-m4a"_s);
        m_decoderMimeTypeSet.add("audio/mpeg"_s);
        m_decoderMimeTypeSet.add("audio/x-mpeg"_s);
        m_decoderCodecMap.add("mpeg"_s, result);
        m_decoderCodecMap.add("mp4a*"_s, result);
    }

    auto opusSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/x-opus"_s);
    if (opusSupported && (!m_isMediaSource || factories.hasElementForMediaType(ElementFactories::Type::AudioParser, "audio/x-opus"_s))) {
        m_decoderMimeTypeSet.add("audio/opus"_s);
        m_decoderCodecMap.add("opus"_s, opusSupported);
        m_decoderCodecMap.add("x-opus"_s, opusSupported);
    }

    auto vorbisSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/x-vorbis"_s);
    if (vorbisSupported && (!m_isMediaSource || factories.hasElementForMediaType(ElementFactories::Type::AudioParser, "audio/x-vorbis"_s))) {
        m_decoderCodecMap.add("vorbis"_s, vorbisSupported);
        m_decoderCodecMap.add("x-vorbis"_s, vorbisSupported);
    }

    bool matroskaSupported = factories.hasElementForMediaType(ElementFactories::Type::Demuxer, "video/x-matroska"_s);
    if (matroskaSupported) {
        auto vp8DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-vp8"_s, ElementFactories::CheckHardwareClassifier::Yes, { { "vp8alphadecodebin"_s } });
        auto vp9DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-vp9"_s, ElementFactories::CheckHardwareClassifier::Yes, { { "vp9alphadecodebin"_s } });

        if (vp8DecoderAvailable || vp9DecoderAvailable)
            m_decoderMimeTypeSet.add("video/webm"_s);

        if (vp8DecoderAvailable) {
            m_decoderCodecMap.add("vp8"_s, vp8DecoderAvailable);
            m_decoderCodecMap.add("x-vp8"_s, vp8DecoderAvailable);
            m_decoderCodecMap.add("vp8.0"_s, vp8DecoderAvailable);
        }
        if (vp9DecoderAvailable) {
            m_decoderCodecMap.add("vp9"_s, vp9DecoderAvailable);
            m_decoderCodecMap.add("x-vp9"_s, vp9DecoderAvailable);
            m_decoderCodecMap.add("vp9.0"_s, vp9DecoderAvailable);
            m_decoderCodecMap.add("vp09*"_s, vp9DecoderAvailable);
        }
        if (opusSupported)
            m_decoderMimeTypeSet.add(("audio/webm"_s));
    }

    bool shouldAddMP4Container = false;

    auto h264DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }"_s, ElementFactories::CheckHardwareClassifier::Yes);
    auto h264AllFormatsDecoderAvailable = GStreamerRegistryScanner::RegistryLookupResult::merge(
        factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }, stream-format=(string)avc"_s, ElementFactories::CheckHardwareClassifier::Yes),
        factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }, stream-format=(string)byte-stream"_s, ElementFactories::CheckHardwareClassifier::Yes));
    auto needsH264Parse = h264DecoderAvailable != h264AllFormatsDecoderAvailable;

    if (h264DecoderAvailable && (!needsH264Parse || factories.hasElementForMediaType(ElementFactories::Type::VideoParser, "video/x-h264"_s))) {
        shouldAddMP4Container = true;
        m_decoderCodecMap.add("x-h264"_s, h264DecoderAvailable);
        m_decoderCodecMap.add("avc*"_s, h264DecoderAvailable);
        m_decoderCodecMap.add("mp4v*"_s, h264DecoderAvailable);
    }

    auto h265DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h265"_s, ElementFactories::CheckHardwareClassifier::Yes);
    auto h265AllFormatsDecoderAvailable = GStreamerRegistryScanner::RegistryLookupResult::merge(
        factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h265, stream-format=(string)byte-stream"_s, ElementFactories::CheckHardwareClassifier::Yes),
        GStreamerRegistryScanner::RegistryLookupResult::merge(
            factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h265, stream-format=(string)hev1"_s, ElementFactories::CheckHardwareClassifier::Yes),
            factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-h265, stream-format=(string)hvc1"_s, ElementFactories::CheckHardwareClassifier::Yes)));
    auto needsH265Parse = h265DecoderAvailable != h265AllFormatsDecoderAvailable;

    if (h265DecoderAvailable && (!needsH265Parse || factories.hasElementForMediaType(ElementFactories::Type::VideoParser, "video/x-h265"_s))) {
        shouldAddMP4Container = true;
        m_decoderCodecMap.add("x-h265"_s, h265DecoderAvailable);
        m_decoderCodecMap.add("hvc1*"_s, h265DecoderAvailable);
        m_decoderCodecMap.add("hev1*"_s, h265DecoderAvailable);
    }

    if (shouldAddMP4Container) {
        m_decoderMimeTypeSet.add("video/mp4"_s);
        m_decoderMimeTypeSet.add("video/x-m4v"_s);
    }

    auto av1DecoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-av1"_s, ElementFactories::CheckHardwareClassifier::Yes);
    if ((matroskaSupported || isContainerTypeSupported(Configuration::Decoding, "video/mp4"_s)) && av1DecoderAvailable) {
        m_decoderCodecMap.add("av01*"_s, av1DecoderAvailable);
        m_decoderCodecMap.add("av1"_s, av1DecoderAvailable);
        m_decoderCodecMap.add("x-av1"_s, av1DecoderAvailable);
    }

    Vector<GstCapsWebKitMapping> mseCompatibleMapping = {
        { ElementFactories::Type::AudioDecoder, "audio/x-ac3"_s, { }, { "x-ac3"_s, "ac-3"_s, "ac3"_s } },
        { ElementFactories::Type::AudioDecoder, "audio/x-eac3"_s, { "audio/x-ac3"_s }, { "x-eac3"_s, "ec3"_s, "ec-3"_s, "eac3"_s } },
        { ElementFactories::Type::AudioDecoder, "audio/x-flac"_s, { "audio/x-flac"_s, "audio/flac"_s }, { "x-flac"_s, "flac"_s, "fLaC"_s } },
    };
    fillMimeTypeSetFromCapsMapping(factories, mseCompatibleMapping);

    if (m_isMediaSource)
        return;

    // The mime-types initialized below are not supported by the MSE backend.

    Vector<GstCapsWebKitMapping> mapping = {
        { ElementFactories::Type::AudioDecoder, "audio/midi"_s, { "audio/midi"_s, "audio/riff-midi"_s }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-dts"_s, { }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-sbc"_s, { }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-sid"_s, { }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-alaw"_s, { }, { "alaw"_s } },
        { ElementFactories::Type::AudioDecoder, "audio/x-mulaw"_s, { }, { "ulaw"_s } },
        { ElementFactories::Type::AudioDecoder, "audio/x-speex"_s, { "audio/speex"_s, "audio/x-speex"_s }, { } },
        { ElementFactories::Type::AudioDecoder, "audio/x-wavpack"_s, { "audio/x-wavpack"_s }, { } },
        { ElementFactories::Type::VideoDecoder, "video/mpeg, mpegversion=(int){1,2}, systemstream=(boolean)false"_s, { "video/mpeg"_s }, { "mpeg"_s } },
        { ElementFactories::Type::VideoDecoder, "video/mpegts"_s, { }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-dirac"_s, { }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-flash-video"_s, { "video/flv"_s, "video/x-flv"_s }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-h263"_s, { }, { } },
        { ElementFactories::Type::VideoDecoder, "video/x-msvideocodec"_s, { "video/x-msvideo"_s }, { } },
        { ElementFactories::Type::Demuxer, "application/vnd.rn-realmedia"_s, { }, { } },
        { ElementFactories::Type::Demuxer, "application/x-3gp"_s, { }, { } },
        { ElementFactories::Type::Demuxer, "application/x-pn-realaudio"_s, { }, { } },
        { ElementFactories::Type::Demuxer, "audio/x-aiff"_s, { }, { } },
        { ElementFactories::Type::Demuxer, "audio/x-wav"_s, { "audio/x-wav"_s, "audio/wav"_s, "audio/vnd.wave"_s }, { "1"_s } },
        { ElementFactories::Type::Demuxer, "video/quicktime"_s, { }, { } },
        { ElementFactories::Type::Demuxer, "video/quicktime, variant=(string)3gpp"_s, { "video/3gpp"_s }, { } },
        { ElementFactories::Type::Demuxer, "video/x-ms-asf"_s, { }, { } },
    };

    if (const char* hlsSupport = g_getenv("WEBKIT_GST_ENABLE_HLS_SUPPORT")) {
        if (!g_strcmp0(hlsSupport, "1"))
            mapping.append({ ElementFactories::Type::Demuxer, "application/x-hls"_s, { "application/vnd.apple.mpegurl"_s, "application/x-mpegurl"_s }, { } });
    }

    if (const char* dashSupport = g_getenv("WEBKIT_GST_ENABLE_DASH_SUPPORT")) {
        if (!g_strcmp0(dashSupport, "1"))
            mapping.append({ ElementFactories::Type::Demuxer, "application/dash+xml"_s, { }, { } });
    }

    fillMimeTypeSetFromCapsMapping(factories, mapping);

    if (factories.hasElementForMediaType(ElementFactories::Type::Demuxer, "application/ogg"_s)) {
        m_decoderMimeTypeSet.add("application/ogg"_s);

        if (vorbisSupported) {
            m_decoderMimeTypeSet.add("audio/ogg"_s);
            m_decoderMimeTypeSet.add("audio/x-vorbis+ogg"_s);
        }

        if (auto result = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/x-speex"_s)) {
            m_decoderMimeTypeSet.add("audio/ogg"_s);
            m_decoderCodecMap.add("speex"_s, result);
        }

        if (auto result = factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-theora"_s)) {
            m_decoderMimeTypeSet.add("video/ogg"_s);
            m_decoderCodecMap.add("theora"_s, result);
        }
    }

    bool audioMpegSupported = false;
    if (auto result = factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/mpeg, mpegversion=(int)1, layer=(int)[1, 3]"_s)) {
        audioMpegSupported = true;
        m_decoderMimeTypeSet.add("audio/mp1"_s);
        m_decoderMimeTypeSet.add("audio/mp3"_s);
        m_decoderMimeTypeSet.add("audio/x-mp3"_s);
        m_decoderCodecMap.add("audio/mp3"_s, result);
        m_decoderCodecMap.add("mp3"_s, result);
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::AudioDecoder, "audio/mpeg, mpegversion=(int)2"_s)) {
        audioMpegSupported = true;
        m_decoderMimeTypeSet.add("audio/mp2"_s);
    }

    audioMpegSupported |= isContainerTypeSupported(Configuration::Decoding, "audio/mp4"_s);
    if (audioMpegSupported) {
        m_decoderMimeTypeSet.add("audio/mpeg"_s);
        m_decoderMimeTypeSet.add("audio/x-mpeg"_s);
    }

    if (matroskaSupported) {
        m_decoderMimeTypeSet.add("video/x-matroska"_s);

        if (factories.hasElementForMediaType(ElementFactories::Type::VideoDecoder, "video/x-vp10"_s))
            m_decoderMimeTypeSet.add("video/webm"_s);
    }
}

void GStreamerRegistryScanner::initializeEncoders(const GStreamerRegistryScanner::ElementFactories& factories)
{
    // MSE is about playback, which means decoding. No need to check for encoders then.
    if (m_isMediaSource)
        return;

    m_encoderCodecMap.clear();
    m_encoderMimeTypeSet.clear();

    auto aacSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/mpeg, mpegversion=(int)4"_s);
    if (auto result = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/mpeg, mpegversion=(int)4"_s)) {
        m_encoderCodecMap.add("mpeg"_s, result);
        m_encoderCodecMap.add("mp4a*"_s, result);
    }

    if (auto alawSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/x-alaw"_s))
        m_encoderCodecMap.add("alaw"_s, alawSupported);

    if (auto ulawSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/x-mulaw"_s))
        m_encoderCodecMap.add("ulaw"_s, ulawSupported);

    if (auto flacSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/x-flac"_s))
        m_encoderCodecMap.add("flac"_s, flacSupported);

    if (auto mp3Supported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/mpeg, mpegversion=(int)1, layer=(int)3"_s))
        m_encoderCodecMap.add("mp3"_s, mp3Supported);

    auto opusSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/x-opus"_s);
    if (opusSupported) {
        m_encoderCodecMap.add("opus"_s, opusSupported);
        m_encoderCodecMap.add("x-opus"_s, opusSupported);
    }

    auto vorbisSupported = factories.hasElementForMediaType(ElementFactories::Type::AudioEncoder, "audio/x-vorbis"_s);
    if (vorbisSupported) {
        m_encoderCodecMap.add("vorbis"_s, vorbisSupported);
        m_encoderCodecMap.add("x-vorbis"_s, vorbisSupported);
    }

    Vector<String> av1EncodersDisallowedList { "av1enc"_s };
    auto av1EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-av1"_s, ElementFactories::CheckHardwareClassifier::Yes, std::make_optional(WTFMove(av1EncodersDisallowedList)));
    if (av1EncoderAvailable) {
        m_encoderCodecMap.add("av01*"_s, av1EncoderAvailable);
        m_encoderCodecMap.add("av1"_s, av1EncoderAvailable);
        m_encoderCodecMap.add("x-av1"_s, av1EncoderAvailable);
    }

    auto vp8EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-vp8"_s, ElementFactories::CheckHardwareClassifier::Yes);
    if (vp8EncoderAvailable) {
        m_encoderCodecMap.add("vp8"_s, vp8EncoderAvailable);
        m_encoderCodecMap.add("x-vp8"_s, vp8EncoderAvailable);
        m_encoderCodecMap.add("vp8.0"_s, vp8EncoderAvailable);
    }

    auto vp9EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-vp9"_s, ElementFactories::CheckHardwareClassifier::Yes);
    if (vp9EncoderAvailable) {
        m_encoderCodecMap.add("vp9"_s, vp9EncoderAvailable);
        m_encoderCodecMap.add("x-vp9"_s, vp9EncoderAvailable);
        m_encoderCodecMap.add("vp9.0"_s, vp9EncoderAvailable);
        m_encoderCodecMap.add("vp09*"_s, vp9EncoderAvailable);
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "video/webm"_s) && (vp8EncoderAvailable || vp9EncoderAvailable || av1EncoderAvailable))
        m_encoderMimeTypeSet.add("video/webm"_s);

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "audio/webm"_s)) {
        if (opusSupported)
            m_encoderMimeTypeSet.add("audio/opus"_s);
        m_encoderMimeTypeSet.add("audio/webm"_s);
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "audio/ogg"_s) && (vorbisSupported || opusSupported))
        m_encoderMimeTypeSet.add("audio/ogg"_s);

    auto h264EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-h264, profile=(string){ constrained-baseline, baseline, high }"_s, ElementFactories::CheckHardwareClassifier::Yes);
    if (h264EncoderAvailable) {
        m_encoderCodecMap.add("h264"_s, h264EncoderAvailable);
        m_encoderCodecMap.add("x-h264"_s, h264EncoderAvailable);
        m_encoderCodecMap.add("avc*"_s, h264EncoderAvailable);
        m_encoderCodecMap.add("mp4v*"_s, h264EncoderAvailable);
    }

    auto h265EncoderAvailable = factories.hasElementForMediaType(ElementFactories::Type::VideoEncoder, "video/x-h265, profile=(string){ main, high }"_s, ElementFactories::CheckHardwareClassifier::Yes);
    if (h265EncoderAvailable) {
        m_encoderCodecMap.add("hev1*"_s, h265EncoderAvailable);
        m_encoderCodecMap.add("hvc1*"_s, h265EncoderAvailable);
    }

    if (factories.hasElementForMediaType(ElementFactories::Type::Muxer, "video/quicktime"_s)) {
        if (opusSupported)
            m_encoderMimeTypeSet.add("audio/opus"_s);
        if (aacSupported) {
            m_encoderMimeTypeSet.add("audio/aac"_s);
            m_encoderMimeTypeSet.add("audio/mp4"_s);
            m_encoderMimeTypeSet.add("audio/x-m4a"_s);
        }
        if (h264EncoderAvailable) {
            m_encoderMimeTypeSet.add("video/mp4"_s);
            m_encoderMimeTypeSet.add("video/x-m4v"_s);
        }
    }
}

GStreamerRegistryScanner::CodecLookupResult GStreamerRegistryScanner::isHEVCCodecSupported(Configuration configuration, const String& codec, bool shouldCheckForHardwareUse) const
{
    auto h265Caps = adoptGRef(gst_caps_new_empty_simple("video/x-h265"));
    if (codec.find('.') == notFound) {
        GST_DEBUG("Codec has no profile/level, falling back to unconstrained caps");
        return areCapsSupported(configuration, h265Caps, shouldCheckForHardwareUse);
    }

    if (!GStreamerCodecUtilities::parseHEVCProfile(codec)) {
        GST_ERROR("HEVC codec string is invalid: %s", codec.utf8().data());
        return { false, nullptr };
    }

    return areCapsSupported(configuration, h265Caps, shouldCheckForHardwareUse);
}

GStreamerRegistryScanner::CodecLookupResult GStreamerRegistryScanner::isCodecSupported(Configuration configuration, const String& codec, bool shouldCheckForHardwareUse) const
{
    // If the codec is named like a mimetype (eg: video/avc) remove the "video/" part.
    size_t slashIndex = codec.find('/');
    String codecName = slashIndex != notFound ? codec.substring(slashIndex + 1) : codec;

    CodecLookupResult result;
    if (codecName.startsWith("avc1"_s))
        result = isAVC1CodecSupported(configuration, codecName, shouldCheckForHardwareUse);
    else if (codecName.startsWith("hev1"_s) || codecName.startsWith("hvc1"_s))
        result = isHEVCCodecSupported(configuration, codecName, shouldCheckForHardwareUse);
    else {
        auto& codecMap = configuration == Configuration::Decoding ? m_decoderCodecMap : m_encoderCodecMap;
        for (const auto& [codecId, lookupResult] : codecMap) {
            if (!fnmatch(codecId.utf8().data(), codecName.utf8().data(), 0)) {
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
    if (equalIgnoringASCIICase(features, dolbyDigitalPlusJOC))
        return true;

    return false;
}

MediaPlayerEnums::SupportsType GStreamerRegistryScanner::isContentTypeSupported(Configuration configuration, const ContentType& contentType, const Vector<ContentType>& contentTypesRequiringHardwareSupport) const
{
    VideoDecodingLimits* videoDecodingLimits = nullptr;
#ifdef VIDEO_DECODING_LIMIT
    static std::optional<VideoDecodingLimits> videoDecodingLimitsDefaults;
    static std::once_flag onceFlag;
    if (configuration == Configuration::Decoding) {
        std::call_once(onceFlag, [] {
            videoDecodingLimitsDefaults = videoDecoderLimitsDefaults();
            if (!videoDecodingLimitsDefaults) {
                GST_WARNING("Parsing VIDEO_DECODING_LIMIT failed");
                ASSERT_NOT_REACHED();
                return;
            }
        });
        if (videoDecodingLimitsDefaults)
            videoDecodingLimits = &*videoDecodingLimitsDefaults;
    }
#endif

    using SupportsType = MediaPlayerEnums::SupportsType;

    const auto containerType = contentType.containerType().convertToASCIILowercase();
    if (!isContainerTypeSupported(configuration, containerType))
        return SupportsType::IsNotSupported;

    int channels = parseInteger<int>(contentType.parameter("channels"_s)).value_or(1);
    String features = contentType.parameter("features"_s);
    if (channels > MEDIA_MAX_AAC_CHANNELS || channels <= 0
        || !(features.isEmpty() || supportsFeatures(features)))
        return SupportsType::IsNotSupported;

    bool ok;
    float width = contentType.parameter("width"_s).toFloat(&ok);
    if (!ok)
        width = 0;
    float height = contentType.parameter("height"_s).toFloat(&ok);
    if (!ok)
        height = 0;

    if (videoDecodingLimits && (width > videoDecodingLimits->mediaMaxWidth || height > videoDecodingLimits->mediaMaxHeight))
        return SupportsType::IsNotSupported;

    float frameRate = contentType.parameter("framerate"_s).toFloat(&ok);
    // Limit frameRate only in case of highest supported resolution.
    if (ok && videoDecodingLimits && width == videoDecodingLimits->mediaMaxWidth && height == videoDecodingLimits->mediaMaxHeight && frameRate > videoDecodingLimits->mediaMaxFrameRate)
        return SupportsType::IsNotSupported;

    const auto& codecs = contentType.codecs();

#if ENABLE(ENCRYPTED_MEDIA)
    String cryptoblockformat = contentType.parameter("cryptoblockformat"_s);
    if (!cryptoblockformat.isEmpty()) {
        OptionSet<ElementFactories::Type> factoryTypes;
        factoryTypes.add(ElementFactories::Type::Decryptor);
        auto factories = ElementFactories(factoryTypes);

        // https://developers.google.com/youtube/devices/living-room/certification-requirements
        // section 15.1.3: The isTypeSupported method MUST support the cryptoblockformat mime-type
        // parameter, and MUST support subsample as a parameter value.
        if (cryptoblockformat != "subsample"_s)
            return SupportsType::IsNotSupported;

        if (!containerType.endsWith("webm"_s))
            return SupportsType::IsNotSupported;

#if GST_CHECK_VERSION(1, 22, 0)
        for (const auto& mimeCodec : codecs) {
            auto codecCaps = adoptGRef(gst_codec_utils_caps_from_mime_codec(mimeCodec.ascii().data()));
            if (!codecCaps) {
                GST_WARNING("Unable to convert codec %s to caps", mimeCodec.ascii().data());
                continue;
            }
            auto structure = gst_caps_get_structure(codecCaps.get(), 0);
            auto name = gstStructureGetName(structure);
            auto caps = adoptGRef(gst_caps_new_simple("application/x-webm-enc", "original-media-type", G_TYPE_STRING, reinterpret_cast<const char*>(name.rawCharacters()), nullptr));
            if (!factories.hasElementForCaps(ElementFactories::Type::Decryptor, caps))
                return SupportsType::IsNotSupported;
        }
#else
        if (!factories.hasElementForMediaType(ElementFactories::Type::Decryptor, "application/x-webm-enc"_s))
            return SupportsType::IsNotSupported;
#endif // GST_CHECK_VERSION(1, 22, 0)
    }
#endif // ENABLE(ENCRYPTED_MEDIA)

    // Spec says we should not return "probably" if the codecs string is empty.
    if (codecs.isEmpty())
        return SupportsType::MayBeSupported;

    for (const auto& codec : codecs) {
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

GStreamerRegistryScanner::CodecLookupResult GStreamerRegistryScanner::areCapsSupported(Configuration configuration, const GRefPtr<GstCaps>& caps, bool shouldCheckForHardwareUse) const
{
    OptionSet<ElementFactories::Type> factoryTypes;
    switch (configuration) {
    case Configuration::Decoding:
        factoryTypes.add(ElementFactories::Type::VideoDecoder);
        break;
    case Configuration::Encoding:
        factoryTypes.add(ElementFactories::Type::VideoEncoder);
        break;
    }
    auto lookupResult = ElementFactories(factoryTypes).hasElementForCaps(factoryTypes.toSingleValue().value(), caps, ElementFactories::CheckHardwareClassifier::Yes);
    bool supported = lookupResult && (shouldCheckForHardwareUse ? lookupResult.isUsingHardware : true);
    GST_DEBUG("%s decoding supported for caps %" GST_PTR_FORMAT ": %s", shouldCheckForHardwareUse ? "Hardware" : "Software", caps.get(), boolForPrinting(supported));
    return { supported, supported ? lookupResult.factory : nullptr };
}

GStreamerRegistryScanner::CodecLookupResult GStreamerRegistryScanner::isAVC1CodecSupported(Configuration configuration, const String& codec, bool shouldCheckForHardwareUse) const
{
    auto h264Caps = adoptGRef(gst_caps_new_empty_simple("video/x-h264"));
    if (codec.find('.') == notFound) {
        GST_DEBUG("Codec has no profile/level, falling back to unconstrained caps");
        return areCapsSupported(configuration, h264Caps, shouldCheckForHardwareUse);
    }

    auto [profile, level] = GStreamerCodecUtilities::parseH264ProfileAndLevel(codec);
    if (!profile || !level) {
        GST_ERROR("H.264 profile / level was not recognised in codec %s", codec.utf8().data());
        return { false, nullptr };
    }

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

        gst_caps_set_simple(h264Caps.get(), "level", G_TYPE_STRING, maxLevelString, nullptr);
        return areCapsSupported(configuration, h264Caps, shouldCheckForHardwareUse);
    }

    GST_DEBUG("Checking video decoders for constrained caps");
    gst_caps_set_simple(h264Caps.get(), "level", G_TYPE_STRING, level, "profile", G_TYPE_STRING, profile, nullptr);
    return areCapsSupported(configuration, h264Caps, shouldCheckForHardwareUse);
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

static inline Vector<RTCRtpCapabilities::HeaderExtensionCapability> probeRtpExtensions(const Vector<ASCIILiteral>& candidates)
{
    Vector<RTCRtpCapabilities::HeaderExtensionCapability> extensions;
    for (const auto& uri : candidates) {
        if (auto extension = adoptGRef(gst_rtp_header_extension_create_from_uri(uri.characters())))
            extensions.append(makeString(span(uri)));
    }
    return extensions;
}

void GStreamerRegistryScanner::fillAudioRtpCapabilities(Configuration configuration, RTCRtpCapabilities& capabilities)
{
    if (!m_audioRtpExtensions) {
        auto extensions = m_commonRtpExtensions;
        extensions.appendVector(m_allAudioRtpExtensions);
        m_audioRtpExtensions = probeRtpExtensions(extensions);
    }
    if (m_audioRtpExtensions)
        capabilities.headerExtensions = copyToVector(*m_audioRtpExtensions);

    auto codecElement = ElementFactories::Type::AudioDecoder;
    auto rtpElement = ElementFactories::Type::RtpDepayloader;
    if (configuration == Configuration::Encoding) {
        codecElement = ElementFactories::Type::AudioEncoder;
        rtpElement = ElementFactories::Type::RtpPayloader;
    }

    auto factories = ElementFactories({ codecElement, rtpElement });
    if (factories.hasElementForMediaType(codecElement, "audio/x-opus"_s) && factories.hasElementForMediaType(rtpElement, "audio/x-opus"_s))
        capabilities.codecs.append({ .mimeType = "audio/opus"_s, .clockRate = 48000, .channels = 2, .sdpFmtpLine = "minptime=10;useinbandfec=1"_s });

    if (factories.hasElementForMediaType(codecElement, "audio/G722"_s) && factories.hasElementForMediaType(rtpElement, "audio/G722"_s))
        capabilities.codecs.append({ .mimeType = "audio/G722"_s, .clockRate = 8000, .channels = 1, .sdpFmtpLine = emptyString() });

    if (factories.hasElementForMediaType(codecElement, "audio/x-mulaw"_s) && factories.hasElementForMediaType(rtpElement, "audio/x-mulaw"_s))
        capabilities.codecs.append({ .mimeType = "audio/PCMU"_s, .clockRate = 8000, .channels = 1, .sdpFmtpLine = emptyString() });

    if (factories.hasElementForMediaType(codecElement, "audio/x-alaw"_s) && factories.hasElementForMediaType(rtpElement, "audio/x-alaw"_s))
        capabilities.codecs.append({ .mimeType = "audio/PCMA"_s, .clockRate = 8000, .channels = 1, .sdpFmtpLine = emptyString() });
}

void GStreamerRegistryScanner::fillVideoRtpCapabilities(Configuration configuration, RTCRtpCapabilities& capabilities)
{
    if (!m_videoRtpExtensions) {
        auto extensions = m_commonRtpExtensions;
        extensions.appendVector(m_allVideoRtpExtensions);
        m_videoRtpExtensions = probeRtpExtensions(extensions);
    }
    if (m_videoRtpExtensions)
        capabilities.headerExtensions = copyToVector(*m_videoRtpExtensions);

    auto codecElement = ElementFactories::Type::VideoDecoder;
    auto rtpElement = ElementFactories::Type::RtpDepayloader;
    if (configuration == Configuration::Encoding) {
        codecElement = ElementFactories::Type::VideoEncoder;
        rtpElement = ElementFactories::Type::RtpPayloader;
    }

    auto factories = ElementFactories({ codecElement, rtpElement });
    auto codecLookupResult = factories.hasElementForMediaType(codecElement, "video/x-h264"_s);
    if (codecLookupResult && factories.hasElementForMediaType(rtpElement, "video/x-h264"_s)) {
        GRefPtr<GstElement> element;
        if (configuration == Configuration::Decoding)
            element = gst_element_factory_create(codecLookupResult.factory.get(), nullptr);
        else
            element = gst_element_factory_make("webkitvideoencoder", nullptr);

        if (element) {
            Vector<ASCIILiteral> profiles = {
                "42e01f"_s,
                "640c1f"_s,
                "42001f"_s,
                "4d001f"_s,
            };

            for (auto& profileLevelId : profiles) {
                if (WEBKIT_IS_VIDEO_ENCODER(element.get())) {
                    auto codec = makeString("avc1."_s, profileLevelId);
                    if (!videoEncoderSupportsCodec(WEBKIT_VIDEO_ENCODER(element.get()), codec))
                        continue;
                } else {
                    auto spsAsInteger = parseInteger<uint64_t>(profileLevelId, 16).value_or(0);
                    uint8_t sps[3];
                    sps[0] = spsAsInteger >> 16;
                    sps[1] = (spsAsInteger >> 8) & 0xff;
                    sps[2] = spsAsInteger & 0xff;

                    auto caps = adoptGRef(gst_caps_new_empty_simple("video/x-h264"));
                    gst_codec_utils_h264_caps_set_level_and_profile(caps.get(), sps, 3);
                    if (!gst_element_factory_can_sink_any_caps(gst_element_get_factory(element.get()), caps.get()))
                        continue;
                }

                capabilities.codecs.append({ .mimeType = "video/H264"_s, .clockRate = 90000, .channels = { }, .sdpFmtpLine = makeString("level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="_s, profileLevelId) });
                capabilities.codecs.append({ .mimeType = "video/H264"_s, .clockRate = 90000, .channels = { }, .sdpFmtpLine = makeString("level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="_s, profileLevelId) });
            }
        }
    }

    if (factories.hasElementForMediaType(codecElement, "video/x-h265"_s) && factories.hasElementForMediaType(rtpElement, "video/x-h265"_s)) {
        // FIXME: Probe for video/H265 encoder capabilities.
        capabilities.codecs.append({ .mimeType = "video/H265"_s, .clockRate = 90000, .channels = { }, .sdpFmtpLine = { } });
    }

    if (factories.hasElementForMediaType(codecElement, "video/x-av1"_s) && factories.hasElementForMediaType(rtpElement, "video/x-av1"_s))
        capabilities.codecs.append({ .mimeType = "video/AV1"_s, .clockRate = 90000, .channels = { }, .sdpFmtpLine = { } });

    if (factories.hasElementForMediaType(codecElement, "video/x-vp8"_s) && factories.hasElementForMediaType(rtpElement, "video/x-vp8"_s))
        capabilities.codecs.append({ .mimeType = "video/VP8"_s, .clockRate = 90000, .channels = { }, .sdpFmtpLine = { } });

    if (factories.hasElementForMediaType(codecElement, "video/x-vp9"_s) && factories.hasElementForMediaType(rtpElement, "video/x-vp9"_s)) {
        // FIXME: Profile levels are hardcoded here for the time being. It might be a good idea to
        // actually probe those on the selected encoder.
        capabilities.codecs.append({ .mimeType = "video/VP9"_s, .clockRate = 90000, .channels = { }, .sdpFmtpLine = "profile-id=0"_s });
        capabilities.codecs.append({ .mimeType = "video/VP9"_s, .clockRate = 90000, .channels = { }, .sdpFmtpLine = "profile-id=2"_s });
    }
}

Vector<RTCRtpCapabilities::HeaderExtensionCapability> GStreamerRegistryScanner::audioRtpExtensions()
{
    if (!m_audioRtpExtensions) {
        auto extensions = m_commonRtpExtensions;
        extensions.appendVector(m_allAudioRtpExtensions);
        m_audioRtpExtensions = probeRtpExtensions(extensions);
    }
    return *m_audioRtpExtensions;
}

Vector<RTCRtpCapabilities::HeaderExtensionCapability> GStreamerRegistryScanner::videoRtpExtensions()
{
    if (!m_videoRtpExtensions) {
        auto extensions = m_commonRtpExtensions;
        extensions.appendVector(m_allVideoRtpExtensions);
        m_videoRtpExtensions = probeRtpExtensions(extensions);
    }
    return *m_videoRtpExtensions;
}

GStreamerRegistryScanner::RegistryLookupResult GStreamerRegistryScanner::isRtpPacketizerSupported(const String& encoding)
{
    static HashMap<String, ASCIILiteral> mapping = { { "h264"_s, "video/x-h264"_s }, { "vp8"_s, "video/x-vp8"_s },
        { "vp9"_s, "video/x-vp9"_s }, { "av1"_s, "video/x-av1"_s }, { "h265"_s, "video/x-h265"_s }, { "opus"_s, "audio/x-opus"_s },
        { "g722"_s, "audio/G722"_s }, { "pcma"_s, "audio/x-alaw"_s }, { "pcmu"_s, "audio/x-mulaw"_s } };
    auto gstCapsName = mapping.getOptional(encoding);
    if (!gstCapsName) {
        GST_WARNING("Unhandled RTP encoding-name: %s", encoding.ascii().data());
        return { };
    }

    ElementFactories factories(ElementFactories::Type::RtpPayloader);
    return factories.hasElementForMediaType(ElementFactories::Type::RtpPayloader, *gstCapsName);
}

#endif // USE(GSTREAMER_WEBRTC)

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
