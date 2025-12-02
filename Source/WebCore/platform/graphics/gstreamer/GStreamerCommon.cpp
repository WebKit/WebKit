/*
 *  Copyright (C) 2012, 2015, 2016 Igalia S.L
 *  Copyright (C) 2015, 2016 Metrological Group B.V.
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
#include "GStreamerCommon.h"

#if USE(GSTREAMER)

#include "ApplicationGLib.h"
#include "FloatSize.h"
#include "GLVideoSinkGStreamer.h"
#include "GStreamerAudioMixer.h"
#include "GStreamerQuirks.h"
#include "GStreamerRegistryScanner.h"
#include "GStreamerSinksWorkarounds.h"
#include "GUniquePtrGStreamer.h"
#include "GstAllocatorFastMalloc.h"
#include "IntSize.h"
#include "PlatformDisplay.h"
#include "PlatformVideoColorSpace.h"
#include "SharedBuffer.h"
#include "VideoSinkGStreamer.h"
#include "WebKitAudioSinkGStreamer.h"
#include <fnmatch.h>
#include <gst/audio/audio-info.h>
#include <gst/gst.h>
#include <mutex>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/MallocSpan.h>
#include <wtf/MediaTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PrintStream.h>
#include <wtf/RecursiveLockAdapter.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/glib/GSpanExtras.h>
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringToIntegerConversion.h>

#if USE(GSTREAMER_MPEGTS)
#define GST_USE_UNSTABLE_API
#include <gst/mpegts/mpegts.h>
#undef GST_USE_UNSTABLE_API
#endif

#if ENABLE(MEDIA_SOURCE)
#include "GStreamerRegistryScannerMSE.h"
#include "WebKitMediaSourceGStreamer.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "GStreamerCaptureDeviceManager.h"
#include "GStreamerMediaStreamSource.h"
#endif

#if USE(FLITE)
#include "WebKitFliteSourceGStreamer.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)
#include "CDMThunder.h"
#include "WebKitThunderDecryptorGStreamer.h"
#include "WebKitThunderParser.h"
#endif

#if ENABLE(VIDEO)
#include "ImageDecoderGStreamer.h"
#include "VideoEncoderPrivateGStreamer.h"
#include "WebKitWebSourceGStreamer.h"
#endif

#if USE(GSTREAMER_WEBRTC)
#include "GStreamerRTPVideoRotationHeaderExtension.h"
#include <gst/webrtc/webrtc-enumtypes.h>
#endif

#if USE(GSTREAMER_FULL) && GST_CHECK_VERSION(1, 18, 0) && !GST_CHECK_VERSION(1, 20, 0)
#define IS_GST_FULL_1_18 1
#include <gst/gstinitstaticplugins.h>
#else
#define IS_GST_FULL_1_18 0
#endif

#if USE(GBM)
#include "DRMDeviceManager.h"
#include <drm_fourcc.h>
#endif

GST_DEBUG_CATEGORY(webkit_gst_common_debug);
#define GST_CAT_DEFAULT webkit_gst_common_debug

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GstMappedFrame);
WTF_MAKE_TZONE_ALLOCATED_IMPL(WebCoreLogObserver);

static GstClockTime s_webkitGstInitTime;

WARN_UNUSED_RETURN GstPad* webkitGstGhostPadFromStaticTemplate(GstStaticPadTemplate* staticPadTemplate, CStringView name, GstPad* target)
{
    GstPad* pad;
    GRefPtr padTemplate = gst_static_pad_template_get(staticPadTemplate);

    if (target)
        pad = gst_ghost_pad_new_from_template(name.utf8(), target, padTemplate.get());
    else
        pad = gst_ghost_pad_new_no_target_from_template(name.utf8(), padTemplate.get());

    return pad;
}

#if ENABLE(VIDEO)
bool getVideoSizeAndFormatFromCaps(const GstCaps* caps, WebCore::IntSize& size, GstVideoFormat& format, int& pixelAspectRatioNumerator, int& pixelAspectRatioDenominator, int& stride, double& frameRate, PlatformVideoColorSpace& colorSpace)
{
    if (!doCapsHaveType(caps, GST_VIDEO_CAPS_TYPE_PREFIX)) {
        GST_WARNING("Failed to get the video size and format, these are not a video caps");
        return false;
    }

    GstStructure* structure = gst_caps_get_structure(caps, 0);
    if (!areEncryptedCaps(caps) && (!gst_structure_has_name(structure, "video/x-raw") || gst_structure_has_field(structure, "format"))) {
        GstVideoInfo info;
        gst_video_info_init(&info);
        if (!gst_video_info_from_caps(&info, caps))
            return false;

        if (GST_VIDEO_INFO_FPS_N(&info))
            gst_util_fraction_to_double(GST_VIDEO_INFO_FPS_N(&info), GST_VIDEO_INFO_FPS_D(&info), &frameRate);

        colorSpace = videoColorSpaceFromInfo(info);

        format = GST_VIDEO_INFO_FORMAT(&info);
        size.setWidth(GST_VIDEO_INFO_WIDTH(&info));
        size.setHeight(GST_VIDEO_INFO_HEIGHT(&info));
        pixelAspectRatioNumerator = GST_VIDEO_INFO_PAR_N(&info);
        pixelAspectRatioDenominator = GST_VIDEO_INFO_PAR_D(&info);
        stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
    } else {
        if (areEncryptedCaps(caps))
            format = GST_VIDEO_FORMAT_ENCODED;
        else
            format = GST_VIDEO_FORMAT_UNKNOWN;
        stride = 0;
        frameRate = 1;

        auto width = gstStructureGet<int>(structure, "width"_s);
        if (!width) {
            GST_WARNING("Missing width field in %" GST_PTR_FORMAT, caps);
            return false;
        }
        auto height = gstStructureGet<int>(structure, "height"_s);
        if (!height) {
            GST_WARNING("Missing height field in %" GST_PTR_FORMAT, caps);
            return false;
        }

        if (!gst_structure_get_fraction(structure, "pixel-aspect-ratio", &pixelAspectRatioNumerator, &pixelAspectRatioDenominator)) {
            pixelAspectRatioNumerator = 1;
            pixelAspectRatioDenominator = 1;
        }

        size.setWidth(*width);
        size.setHeight(*height);
    }

    return true;
}

std::optional<FloatSize> getVideoResolutionFromCaps(const GstCaps* caps)
{
    if (!doCapsHaveType(caps, GST_VIDEO_CAPS_TYPE_PREFIX)) {
        GST_WARNING("Failed to get the video resolution, these are not a video caps");
        return std::nullopt;
    }

    int width = 0, height = 0;
    int pixelAspectRatioNumerator = 1, pixelAspectRatioDenominator = 1;

    GstStructure* structure = gst_caps_get_structure(caps, 0);
    if (!areEncryptedCaps(caps) && (gst_structure_has_name(structure, "video/x-raw") || gst_structure_has_field(structure, "format"))) {
        GstVideoInfo info;
        gst_video_info_init(&info);
        if (!gst_video_info_from_caps(&info, caps))
            return std::nullopt;

        width = GST_VIDEO_INFO_WIDTH(&info);
        height = GST_VIDEO_INFO_HEIGHT(&info);
        pixelAspectRatioNumerator = GST_VIDEO_INFO_PAR_N(&info);
        pixelAspectRatioDenominator = GST_VIDEO_INFO_PAR_D(&info);
    } else {
        auto widthField = gstStructureGet<int>(structure, "width"_s);
        if (!widthField) {
            GST_WARNING("Missing width field in %" GST_PTR_FORMAT, caps);
            return std::nullopt;
        }
        auto heightField = gstStructureGet<int>(structure, "height"_s);
        if (!heightField) {
            GST_WARNING("Missing height field in %" GST_PTR_FORMAT, caps);
            return std::nullopt;
        }

        width = *widthField;
        height = *heightField;
        gst_structure_get_fraction(structure, "pixel-aspect-ratio", &pixelAspectRatioNumerator, &pixelAspectRatioDenominator);
    }

    return std::make_optional(FloatSize(width, height * (static_cast<float>(pixelAspectRatioDenominator) / static_cast<float>(pixelAspectRatioNumerator))));
}

bool getSampleVideoInfo(GstSample* sample, GstVideoInfo& videoInfo)
{
    if (!GST_IS_SAMPLE(sample))
        return false;

    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps)
        return false;

    gst_video_info_init(&videoInfo);
    if (!gst_video_info_from_caps(&videoInfo, caps))
        return false;

    return true;
}

std::optional<WebCore::IntSize> getDisplaySize(WebCore::IntSize originalSize, int pixelAspectRatioNumerator, int pixelAspectRatioDenominator)
{
    WebCore::IntSize computedSize { 0, 0 };
    unsigned width = 0, height = 0;

    // Calculate DAR based on PAR and video size.
    // Assume regular display (1:1).
    if (!gst_video_calculate_display_ratio(&width, &height, originalSize.width(), originalSize.height(), pixelAspectRatioNumerator, pixelAspectRatioDenominator, 1, 1))
        return std::nullopt;

    // Apply DAR to original video size. This is the same behavior as in xvimagesink's setcaps function.
    if (!(originalSize.height() % height)) {
        GST_DEBUG("Keeping video original height");
        width = gst_util_uint64_scale_int(originalSize.height(), width, height);
        height = originalSize.height();
    } else if (!(originalSize.width() % width)) {
        GST_DEBUG("Keeping video original width");
        height = gst_util_uint64_scale_int(originalSize.width(), height, width);
        width = originalSize.width();
    } else {
        GST_DEBUG("Approximating while keeping original video height");
        width = gst_util_uint64_scale_int(originalSize.height(), width, height);
        height = originalSize.height();
    }

    computedSize.setWidth(width);
    computedSize.setHeight(height);

    return computedSize;
}

bool isProtocolAllowed(const WTF::URL& url)
{
    HashSet<String> allowedProtocols = { "blob"_s, "data"_s, "file"_s, "http"_s, "https"_s };
#if ENABLE(MEDIA_SOURCE)
    allowedProtocols.add("mediasourceblob"_s);
#endif
#if ENABLE(MEDIA_STREAM)
    allowedProtocols.add("mediastream"_s);
#endif

    // Parse and add protocols from environment variable
    auto additionalProtocols = String::fromLatin1(std::getenv("WEBKIT_GST_ALLOWED_URI_PROTOCOLS"));
    if (!additionalProtocols.isEmpty()) {
        for (auto protocols : additionalProtocols.split(',')) {
            auto trimmedProtocol = protocols.trim(deprecatedIsSpaceOrNewline).convertToLowercaseWithoutLocale();
            if (!trimmedProtocol.isEmpty())
                allowedProtocols.add(trimmedProtocol);
        }
    }

    auto protocol = url.protocol().toString().convertToLowercaseWithoutLocale();
    bool isAllowed = allowedProtocols.contains(protocol);

    GST_DEBUG("URL: %s", url.string().utf8().data());
    GST_DEBUG("Requested protocol: %s (allowed: %s)", protocol.utf8().data(), isAllowed ? "yes" : "no");

    return isAllowed;
}
#endif

std::optional<TrackID> getStreamIdFromPad(const GRefPtr<GstPad>& pad)
{
    GUniquePtr<gchar> streamIdAsCharacters(gst_pad_get_stream_id(pad.get()));
    if (!streamIdAsCharacters) {
        GST_DEBUG_OBJECT(pad.get(), "Failed to get stream-id from pad");
        return std::nullopt;
    }

    std::optional<TrackID> streamId(parseStreamId(StringView::fromLatin1(streamIdAsCharacters.get())));
    if (!streamId)
        GST_WARNING_OBJECT(pad.get(), "Got invalid stream-id from pad: %s", streamIdAsCharacters.get());

    return streamId;
}

std::optional<TrackID> getStreamIdFromStream(const GRefPtr<GstStream>& stream)
{
    const gchar* streamIdAsCharacters = gst_stream_get_stream_id(stream.get());
    if (!streamIdAsCharacters) {
        GST_DEBUG_OBJECT(stream.get(), "Failed to get stream-id from stream");
        return std::nullopt;
    }

    std::optional<TrackID> streamId(parseStreamId(StringView::fromLatin1(streamIdAsCharacters)));
    if (!streamId)
        GST_WARNING_OBJECT(stream.get(), "Got invalid stream-id from stream: %s", streamIdAsCharacters);

    return streamId;
}

std::optional<TrackID> parseStreamId(StringView stringId)
{
    auto maybeUUID = WTF::UUID::parse(stringId);
    if (maybeUUID.has_value())
        return maybeUUID.value().low();

    // GStreamer docs advise against interpreting contents of a stream-id,
    // however this is the format qtdemux uses for stream-id creation,
    // so we can reasonably rely on it.
    size_t position = stringId.find('/');
    if (position == notFound || position + 1 == stringId.length())
        return parseIntegerAllowingTrailingJunk<TrackID>(stringId);

    return parseIntegerAllowingTrailingJunk<TrackID>(stringId.substring(position + 1));
}

CStringView capsMediaType(const GstCaps* caps)
{
    ASSERT(caps);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    if (!structure) {
        GST_WARNING("caps are empty");
        return nullptr;
    }
#if ENABLE(ENCRYPTED_MEDIA)
    if (gst_structure_has_name(structure, "application/x-cenc") || gst_structure_has_name(structure, "application/x-cbcs") || gst_structure_has_name(structure, "application/x-webm-enc"))
        return gstStructureGetString(structure, "original-media-type"_s);
#endif
    if (gst_structure_has_name(structure, "application/x-rtp"))
        return gstStructureGetString(structure, "media"_s);

    return gstStructureGetName(structure);
}

bool doCapsHaveType(const GstCaps* caps, ASCIILiteral type)
{
    auto mediaType = capsMediaType(caps);
    if (!mediaType) {
        GST_WARNING("Failed to get MediaType");
        return false;
    }
    return startsWith(mediaType.span(), type);
}

bool areEncryptedCaps(const GstCaps* caps)
{
    ASSERT(caps);
#if ENABLE(ENCRYPTED_MEDIA)
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    if (!structure) {
        GST_WARNING("caps are empty");
        return false;
    }
    return gst_structure_has_name(structure, "application/x-cenc") || gst_structure_has_name(structure, "application/x-webm-enc");
#else
    UNUSED_PARAM(caps);
    return false;
#endif
}

static std::optional<Vector<String>> s_UIProcessCommandLineOptions;
void setGStreamerOptionsFromUIProcess(Vector<String>&& options)
{
    s_UIProcessCommandLineOptions = WTFMove(options);
}

Vector<String> extractGStreamerOptionsFromCommandLine()
{
    GUniqueOutPtr<GError> error;
    GMallocSpan<char> contents = gFileGetContents("/proc/self/cmdline", error);
    if (!contents)
        return { };

    Vector<String> options;
    auto optionsString = String::fromUTF8(contents.span());
    optionsString.split('\0', [&options](StringView item) {
        if (item.startsWith("--gst"_s))
            options.append(item.toString());
    });
    return options;
}

bool ensureGStreamerInitialized()
{
    // WARNING: Please note this function can be called from any thread, for instance when creating
    // a WebCodec element from a JS Worker.
    RELEASE_ASSERT(isInWebProcess());
    static std::once_flag onceFlag;
    static bool isGStreamerInitialized;
    std::call_once(onceFlag, [] {
        isGStreamerInitialized = false;

        // USE_PLAYBIN3 is dangerous for us because its potential sneaky effect
        // is to register the playbin3 element under the playbin namespace. We
        // can't allow this, when we create playbin, we want playbin2, not
        // playbin3.
        // The USE_PLAYBIN3 environment variable is no longer supported.
        // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/6255
        if (!gst_check_version(1, 24, 0) && g_getenv("USE_PLAYBIN3"))
            WTFLogAlways("The USE_PLAYBIN3 variable was detected in the environment. Expect playback issues or please unset it.");

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
        Vector<String> parameters = s_UIProcessCommandLineOptions.value_or(extractGStreamerOptionsFromCommandLine());
        s_UIProcessCommandLineOptions.reset();
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
        char** argv = g_new0(char*, parameters.size() + 2);
        int argc = parameters.size() + 1;
        argv[0] = g_strdup(FileSystem::currentExecutableName().data());
        for (unsigned i = 0; i < parameters.size(); i++)
            argv[i + 1] = g_strdup(parameters[i].utf8().data());

        GUniqueOutPtr<GError> error;
        isGStreamerInitialized = gst_init_check(&argc, &argv, &error.outPtr());
        s_webkitGstInitTime = gst_util_get_timestamp();
        ASSERT_WITH_MESSAGE(isGStreamerInitialized, "GStreamer initialization failed: %s", error ? error->message : "unknown error occurred");
        g_strfreev(argv);
        GST_DEBUG_CATEGORY_INIT(webkit_gst_common_debug, "webkitcommon", 0, "WebKit Common utilities");

        if (isFastMallocEnabled()) {
            const char* disableFastMalloc = getenv("WEBKIT_GST_DISABLE_FAST_MALLOC");
            if (!disableFastMalloc || !strcmp(disableFastMalloc, "0"))
                gst_allocator_set_default(GST_ALLOCATOR(g_object_new(gst_allocator_fast_malloc_get_type(), nullptr)));
        }
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#if USE(GSTREAMER_MPEGTS)
        if (isGStreamerInitialized)
            gst_mpegts_initialize();
#endif

        registerAppsinkWithWorkaroundsIfNeeded();
#endif
    });
    return isGStreamerInitialized;
}

static bool registerInternalVideoEncoder()
{
#if ENABLE(VIDEO)
    if (auto factory = adoptGRef(gst_element_factory_find("webkitvideoencoder")))
        return false;
    return gst_element_register(nullptr, "webkitvideoencoder", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_VIDEO_ENCODER);
#endif
    return false;
}

void registerWebKitGStreamerElements()
{
    static std::once_flag onceFlag;
    bool registryWasUpdated = false;
    std::call_once(onceFlag, [&registryWasUpdated] {

#if IS_GST_FULL_1_18
        gst_init_static_plugins();
#endif

        // Rank guidelines are as following:
        // - Use GST_RANK_PRIMARY for elements meant to be auto-plugged and for which we know
        //   there's no other alternative outside of WebKit.
        // - Use GST_RANK_PRIMARY+100 for elements meant to be auto-plugged and that we know there
        //   is an alternative outside of WebKit.
        // - Use GST_RANK_NONE for elements explicitely created by WebKit (no auto-plugging).

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)
        if (!CDMFactoryThunder::singleton().supportedKeySystems().isEmpty()) {
            // The Thunder parser is auto-plugged by parsebin and its internal parsebin can
            // auto-plug the Thunder decryptor.
            gst_element_register(nullptr, "webkitthunder", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_MEDIA_THUNDER_DECRYPT);
            gst_element_register(nullptr, "webkitthunderparser", GST_RANK_PRIMARY + 101, WEBKIT_TYPE_MEDIA_THUNDER_PARSER);
        }
#endif

#if ENABLE(MEDIA_STREAM)
        gst_element_register(nullptr, "mediastreamsrc", GST_RANK_PRIMARY, WEBKIT_TYPE_MEDIA_STREAM_SRC);
#endif
        registerInternalVideoEncoder();

#if USE(GSTREAMER_WEBRTC)
        gst_element_register(nullptr, "webkitrtpvideorotationheaderextension", GST_RANK_MARGINAL, WEBKIT_TYPE_GST_RTP_VIDEO_ROTATION_HEADER_EXTENSION);
#endif

#if ENABLE(MEDIA_SOURCE)
        gst_element_register(nullptr, "webkitmediasrc", GST_RANK_PRIMARY, WEBKIT_TYPE_MEDIA_SRC);
#endif

#if USE(FLITE)
        gst_element_register(nullptr, "webkitflitesrc", GST_RANK_NONE, WEBKIT_TYPE_FLITE_SRC);
#endif

#if ENABLE(VIDEO)
        gst_element_register(0, "webkitwebsrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_WEB_SRC);
        gst_element_register(0, "webkitvideosink", GST_RANK_NONE, WEBKIT_TYPE_VIDEO_SINK);
#if USE(GSTREAMER_GL)
        gst_element_register(0, "webkitglvideosink", GST_RANK_NONE, WEBKIT_TYPE_GL_VIDEO_SINK);
#endif
#endif
        // We don't want autoaudiosink to autoplug our sink.
        gst_element_register(0, "webkitaudiosink", GST_RANK_NONE, WEBKIT_TYPE_AUDIO_SINK);

        // Prevent decodebin(3) from auto-plugging hlsdemux if it was disabled. UAs should be able
        // to fallback to MSE when this happens.
        const char* hlsSupport = g_getenv("WEBKIT_GST_ENABLE_HLS_SUPPORT");
        if (!hlsSupport || !g_strcmp0(hlsSupport, "0")) {
            if (auto factory = adoptGRef(gst_element_factory_find("hlsdemux")))
                gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE_CAST(factory.get()), GST_RANK_NONE);
        }

        // Prevent decodebin(3) from auto-plugging dashdemux if it was disabled. UAs should be able
        // to fallback to MSE when this happens.
        const char* dashSupport = g_getenv("WEBKIT_GST_ENABLE_DASH_SUPPORT");
        if (!dashSupport || !g_strcmp0(dashSupport, "0")) {
            if (auto factory = adoptGRef(gst_element_factory_find("dashdemux")))
                gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE_CAST(factory.get()), GST_RANK_NONE);
        }

        // The new demuxers based on adaptivedemux2 cannot be used in WebKit yet because this new
        // base class does not abstract away network access. They can't work in a sandboxed
        // media process, so demote their rank in order to prevent decodebin3 from auto-plugging them.
        if (gst_check_version(1, 22, 0)) {
            std::array<ASCIILiteral, 3> elementNames = { "dashdemux2"_s, "hlsdemux2"_s, "mssdemux2"_s };
            for (auto& elementName : elementNames) {
                if (auto factory = adoptGRef(gst_element_factory_find(elementName)))
                    gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE_CAST(factory.get()), GST_RANK_NONE);
            }
        }

        // Make sure isofmp4mux is auto-plugged in transcodebin pipelines.
        if (auto factory = adoptGRef(gst_element_factory_find("isofmp4mux")))
            gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE_CAST(factory.get()), GST_RANK_PRIMARY + 1);

        // The VAAPI plugin is not much maintained anymore and prone to rendering issues. In the
        // mid-term we will leverage the new stateless VA decoders. Disable the legacy plugin,
        // unless the WEBKIT_GST_ENABLE_LEGACY_VAAPI environment variable is set to 1.
        auto enableLegacyVAAPIPlugin = CStringView::unsafeFromUTF8(g_getenv("WEBKIT_GST_ENABLE_LEGACY_VAAPI"));
        if (enableLegacyVAAPIPlugin.isEmpty() || enableLegacyVAAPIPlugin == "0"_s) {
            auto* registry = gst_registry_get();
            if (auto vaapiPlugin = adoptGRef(gst_registry_find_plugin(registry, "vaapi")))
                gst_registry_remove_plugin(registry, vaapiPlugin.get());
        }

        // Make sure the quirks are created as early as possible.
        [[maybe_unused]] auto& quirksManager = GStreamerQuirksManager::singleton();

        registryWasUpdated = true;
    });

    // The GStreamer registry might be updated after the scanner was initialized, so in this situation
    // we need to reset the internal state of the registry scanner.
    if (registryWasUpdated && GStreamerRegistryScanner::singletonWasInitialized())
        GStreamerRegistryScanner::singleton().refresh();
}

void registerWebKitGStreamerVideoEncoder()
{
    static std::once_flag onceFlag;
    bool registryWasUpdated = false;
    std::call_once(onceFlag, [&registryWasUpdated] {
        registryWasUpdated = registerInternalVideoEncoder();
    });

    // The video encoder might be registered after the scanner was initialized, so in this situation
    // we need to reset the internal state of the registry scanner.
    if (registryWasUpdated && GStreamerRegistryScanner::singletonWasInitialized())
        GStreamerRegistryScanner::singleton().refresh();
}

// We use a recursive lock because the removal of a pipeline can trigger the removal of another one,
// from the same thread, specially when using chained element harnesses.
static RecursiveLock s_activePipelinesMapLock;
static HashMap<String, GRefPtr<GstElement>>& activePipelinesMap()
{
    static NeverDestroyed<HashMap<String, GRefPtr<GstElement>>> activePipelines;
    return activePipelines.get();
}

void registerActivePipeline(const GRefPtr<GstElement>& pipeline)
{
    GUniquePtr<gchar> name(gst_object_get_name(GST_OBJECT_CAST(pipeline.get())));
    Locker locker { s_activePipelinesMapLock };
    activePipelinesMap().add(unsafeSpan(name.get()), GRefPtr<GstElement>(pipeline));
}

void unregisterPipeline(const GRefPtr<GstElement>& pipeline)
{
    GUniquePtr<gchar> name(gst_object_get_name(GST_OBJECT_CAST(pipeline.get())));
    Locker locker { s_activePipelinesMapLock };
    activePipelinesMap().remove(unsafeSpan(name.get()));
}

void WebCoreLogObserver::didLogMessage(const WTFLogChannel& channel, WTFLogLevel level, Vector<JSONLogValue>&& values)
{
#ifndef GST_DISABLE_GST_DEBUG
    if (!shouldEmitLogMessage(channel))
        return;

    StringBuilder builder;
    for (auto& [_, value] : values)
        builder.append(value);

    auto logString = builder.toString();
    GstDebugLevel gstDebugLevel;
    switch (level) {
    case WTFLogLevel::Error:
        gstDebugLevel = GST_LEVEL_ERROR;
        break;
    case WTFLogLevel::Debug:
        gstDebugLevel = GST_LEVEL_DEBUG;
        break;
    case WTFLogLevel::Always:
    case WTFLogLevel::Info:
        gstDebugLevel = GST_LEVEL_INFO;
        break;
    case WTFLogLevel::Warning:
        gstDebugLevel = GST_LEVEL_WARNING;
        break;
    };
    gst_debug_log(debugCategory(), gstDebugLevel, __FILE__, __FUNCTION__, __LINE__, nullptr, "%s", logString.utf8().data());
#else
    UNUSED_PARAM(channel);
    UNUSED_PARAM(level);
    UNUSED_PARAM(values);
#endif
}

void WebCoreLogObserver::addWatch(const Logger& logger)
{
    auto totalObservers = m_totalObservers.exchangeAdd(1);
    if (!totalObservers)
        logger.addObserver(*this);
}

void WebCoreLogObserver::removeWatch(const Logger& logger)
{
    auto totalObservers = m_totalObservers.exchangeSub(1);
    if (totalObservers <= 1)
        logger.removeObserver(*this);
}

void deinitializeGStreamer()
{
#if USE(GSTREAMER_GL)
    if (auto* sharedDisplay = PlatformDisplay::sharedDisplayIfExists())
        sharedDisplay->clearGStreamerGLState();
#endif
#if ENABLE(MEDIA_STREAM)
    teardownGStreamerCaptureDeviceManagers();
#endif
#if ENABLE(MEDIA_SOURCE)
    teardownGStreamerRegistryScannerMSE();
#endif
    teardownGStreamerRegistryScanner();
#if ENABLE(VIDEO)
    teardownVideoEncoderSingleton();
    teardownGStreamerImageDecoders();
#endif

    bool isLeaksTracerActive = false;
    auto activeTracers = gst_tracing_get_active_tracers();
    while (activeTracers) {
        auto tracer = adoptGRef(GST_TRACER_CAST(activeTracers->data));
        if (!isLeaksTracerActive && !g_strcmp0(G_OBJECT_TYPE_NAME(G_OBJECT(tracer.get())), "GstLeaksTracer"))
            isLeaksTracerActive = true;
        activeTracers = g_list_delete_link(activeTracers, activeTracers);
    }

    if (!isLeaksTracerActive)
        return;

    // Make sure there is no active pipeline left. Those might trigger deadlocks during gst_deinit().
    {
        Locker locker { s_activePipelinesMapLock };
        for (auto& pipeline : activePipelinesMap().values()) {
            GST_DEBUG("Pipeline %" GST_PTR_FORMAT " was left running. Forcing clean-up.", pipeline.get());
            disconnectSimpleBusMessageCallback(pipeline.get());
            gst_element_set_state(pipeline.get(), GST_STATE_NULL);
        }
        activePipelinesMap().clear();
    }

    gst_deinit();
}

unsigned getGstPlayFlag(const char* nick)
{
    static GFlagsClass* flagsClass = static_cast<GFlagsClass*>(g_type_class_ref(g_type_from_name("GstPlayFlags")));
    ASSERT(flagsClass);

    GFlagsValue* flag = g_flags_get_value_by_nick(flagsClass, nick);
    if (!flag)
        return 0;

    return flag->value;
}

// Convert a MediaTime in seconds to a GstClockTime. Note that we can get MediaTime objects with a time scale that isn't a GST_SECOND, since they can come to
// us through the internal testing API, the DOM and internally. It would be nice to assert the format of the incoming time, but all the media APIs assume time
// is passed around in fractional seconds, so we'll just have to assume the same.
uint64_t toGstUnsigned64Time(const MediaTime& mediaTime)
{
    MediaTime time = mediaTime.toTimeScale(GST_SECOND);
    if (time.isInvalid())
        return GST_CLOCK_TIME_NONE;
    return time.timeValue();
}

GstClockTime toGstClockTime(const Seconds& seconds)
{
    return toGstClockTime(MediaTime::createWithDouble(seconds.seconds()));
}

MediaTime fromGstClockTime(GstClockTime time)
{
    if (!GST_CLOCK_TIME_IS_VALID(time))
        return WTF::MediaTime::invalidTime();

    return WTF::MediaTime(GST_TIME_AS_USECONDS(time), G_USEC_PER_SEC);
}

RefPtr<GstMappedOwnedBuffer> GstMappedOwnedBuffer::create(GRefPtr<GstBuffer>&& buffer)
{
    auto* mappedBuffer = new GstMappedOwnedBuffer(WTFMove(buffer));
    if (!mappedBuffer->isValid()) {
        delete mappedBuffer;
        return nullptr;
    }

    return adoptRef(mappedBuffer);
}

RefPtr<GstMappedOwnedBuffer> GstMappedOwnedBuffer::create(const GRefPtr<GstBuffer>& buffer)
{
    return GstMappedOwnedBuffer::create(GRefPtr(buffer));
}

// This GstBuffer is [ transfer none ], meaning the reference
// count is increased during the life of this object.
RefPtr<GstMappedOwnedBuffer> GstMappedOwnedBuffer::create(GstBuffer* buffer)
{
    return GstMappedOwnedBuffer::create(GRefPtr(buffer));
}

GstMappedOwnedBuffer::~GstMappedOwnedBuffer()
{
    unmapEarly();
}

Ref<SharedBuffer> GstMappedOwnedBuffer::createSharedBuffer()
{
    return SharedBuffer::create(*this);
}

GstMappedFrame::GstMappedFrame(GstBuffer* buffer, const GstVideoInfo* info, GstMapFlags flags)
{
    // This cast can be removed once the GStreamer minimum version is raised to 1.20
    gst_video_frame_map(&m_frame, const_cast<GstVideoInfo*>(info), buffer, flags);
}

GstMappedFrame::GstMappedFrame(const GRefPtr<GstSample>& sample, GstMapFlags flags)
{
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, gst_sample_get_caps(sample.get())))
        return;

    gst_video_frame_map(&m_frame, &info, gst_sample_get_buffer(sample.get()), flags);
}

GstMappedFrame::~GstMappedFrame()
{
    // FIXME: Make this un-conditional when the minimum GStreamer dependency version is >= 1.22.
    if (m_frame.buffer)
        gst_video_frame_unmap(&m_frame);
}

GstVideoFrame* GstMappedFrame::get()
{
    RELEASE_ASSERT(isValid());
    return &m_frame;
}

std::span<uint8_t> GstMappedFrame::componentData(int comp) const
{
    RELEASE_ASSERT(isValid());
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN; // GLib port
    auto data = byteCast<uint8_t>(GST_VIDEO_FRAME_COMP_DATA(&m_frame, comp));
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END;
    return unsafeMakeSpan(data, componentStride(comp));
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN; // GLib port
int GstMappedFrame::componentStride(int stride) const
{
    RELEASE_ASSERT(isValid());
    return GST_VIDEO_FRAME_COMP_STRIDE(&m_frame, stride);
}

int GstMappedFrame::componentWidth(int index) const
{
    RELEASE_ASSERT(isValid());
    return GST_VIDEO_FRAME_COMP_WIDTH(&m_frame, index);
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END;

GstVideoInfo* GstMappedFrame::info()
{
    RELEASE_ASSERT(isValid());
    return &m_frame.info;
}

int GstMappedFrame::width() const
{
    RELEASE_ASSERT(isValid());
    return GST_VIDEO_FRAME_WIDTH(&m_frame);
}

int GstMappedFrame::height() const
{
    RELEASE_ASSERT(isValid());
    return GST_VIDEO_FRAME_HEIGHT(&m_frame);
}

int GstMappedFrame::format() const
{
    RELEASE_ASSERT(isValid());
    return GST_VIDEO_FRAME_FORMAT(&m_frame);
}

std::span<uint8_t> GstMappedFrame::planeData(uint32_t planeIndex) const
{
    RELEASE_ASSERT(isValid());
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN; // GLib port
    auto data = reinterpret_cast<uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&m_frame, planeIndex));
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END;
    return unsafeMakeSpan(data, height() * planeStride(planeIndex));
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN; // GLib port
int GstMappedFrame::planeStride(uint32_t planeIndex) const
{
    RELEASE_ASSERT(isValid());
    return GST_VIDEO_FRAME_PLANE_STRIDE(&m_frame, planeIndex);
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END;

GstMappedAudioBuffer::GstMappedAudioBuffer(GstBuffer* buffer, GstAudioInfo info, GstMapFlags flags)
{
    m_isValid = gst_audio_buffer_map(&m_buffer, &info, buffer, flags);
}

GstMappedAudioBuffer::GstMappedAudioBuffer(const GRefPtr<GstSample>& sample, GstMapFlags flags)
{
    GstAudioInfo info;

    if (!gst_audio_info_from_caps(&info, gst_sample_get_caps(sample.get())))
        return;

    m_isValid = gst_audio_buffer_map(&m_buffer, &info, gst_sample_get_buffer(sample.get()), flags);
}

GstMappedAudioBuffer::~GstMappedAudioBuffer()
{
    if (!m_isValid)
        return;

    gst_audio_buffer_unmap(&m_buffer);
    m_isValid = false;
}

GstAudioBuffer* GstMappedAudioBuffer::get()
{
    if (!m_isValid) {
        GST_INFO("Invalid buffer, returning NULL");
        return nullptr;
    }

    return &m_buffer;
}

GstAudioInfo* GstMappedAudioBuffer::info()
{
    if (!m_isValid) {
        GST_INFO("Invalid frame, returning NULL");
        return nullptr;
    }

    return &m_buffer.info;
}

template<typename T> Vector<std::span<T>> GstMappedAudioBuffer::samples(size_t offset) const
{
    RELEASE_ASSERT(m_isValid);
    if (!m_isValid)
        return Vector<std::span<T>> { };

    auto planeCount = static_cast<uint32_t>(GST_AUDIO_INFO_CHANNELS(&m_buffer.info));
    auto planeSizeTotal = m_buffer.n_samples * GST_AUDIO_INFO_BPS(&m_buffer.info);

    if (GST_AUDIO_INFO_LAYOUT(&m_buffer.info) == GST_AUDIO_LAYOUT_INTERLEAVED) {
        auto planeSizeInBytes = (m_buffer.n_samples - offset) * GST_AUDIO_INFO_BPS(&m_buffer.info);
        Vector<std::span<T>> result;
        result.reserveInitialCapacity(planeCount);
        for (uint32_t i = 0; i < planeCount; i++) {
            auto plane = MallocSpan<T>::malloc(planeSizeInBytes);
            result.append(plane.leakSpan());
        }

        auto inputSpan = unsafeMakeSpan(reinterpret_cast<T*>(m_buffer.planes[0]), planeSizeTotal * planeCount);
        for (uint32_t s = offset; s < m_buffer.n_samples; s++) {
            for (uint32_t c = 0; c < planeCount; c++)
                result[c][s] = inputSpan[s * planeCount + c];
        }
        return result;
    }

    RELEASE_ASSERT(GST_AUDIO_INFO_LAYOUT(&m_buffer.info) == GST_AUDIO_LAYOUT_NON_INTERLEAVED);
    return Vector<std::span<T>> { planeCount, [&](auto index) {
        auto totalData = unsafeMakeSpan(reinterpret_cast<T*>(m_buffer.planes[index]), planeSizeTotal);
        return totalData.subspan(offset);
    } };
}

template Vector<std::span<uint8_t>> GstMappedAudioBuffer::samples(size_t) const;
template Vector<std::span<int16_t>> GstMappedAudioBuffer::samples(size_t) const;
template Vector<std::span<int32_t>> GstMappedAudioBuffer::samples(size_t) const;
template Vector<std::span<float>> GstMappedAudioBuffer::samples(size_t) const;

static GQuark customMessageHandlerQuark()
{
    static GQuark quark = g_quark_from_static_string("pipeline-custom-message-handler");
    return quark;
}

void disconnectSimpleBusMessageCallback(GstElement* pipeline)
{
    auto handler = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(pipeline), customMessageHandlerQuark()));
    if (!handler)
        return;

    auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(pipeline)));
    g_signal_handler_disconnect(bus.get(), handler);
    gst_bus_remove_signal_watch(bus.get());
    g_object_set_qdata(G_OBJECT(pipeline), customMessageHandlerQuark(), nullptr);
}

struct MessageBusData {
    GThreadSafeWeakPtr<GstElement> pipeline;
    Function<void(GstMessage*)> handler;
    AsynchronousPipelineDumping asynchronousPipelineDumping;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(MessageBusData)

struct AsyncPipelineDumpData {
    String dotFileName;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(AsyncPipelineDumpData)

static void dumpPipeline(const GRefPtr<GstElement>& pipeline, String&& dotFileName, AsynchronousPipelineDumping asynchronousPipelineDumping)
{
    if (asynchronousPipelineDumping == AsynchronousPipelineDumping::No) {
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
        return;
    }

    auto data = createAsyncPipelineDumpData();
    data->dotFileName = WTFMove(dotFileName);
    gst_element_call_async(pipeline.get(), reinterpret_cast<GstElementCallAsyncFunc>(+[](GstElement* pipeline, gpointer userData) {
        auto data = reinterpret_cast<AsyncPipelineDumpData*>(userData);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, data->dotFileName.utf8().data());
    }), data, reinterpret_cast<GDestroyNotify>(destroyAsyncPipelineDumpData));
}

void connectSimpleBusMessageCallback(GstElement* pipeline, Function<void(GstMessage*)>&& customHandler, AsynchronousPipelineDumping asynchronousPipelineDumping)
{
    auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(pipeline)));
    gst_bus_add_signal_watch_full(bus.get(), RunLoopSourcePriority::RunLoopDispatcher);

    auto data = createMessageBusData();
    data->pipeline.reset(pipeline);
    data->handler = WTFMove(customHandler);
    data->asynchronousPipelineDumping = asynchronousPipelineDumping;
    auto handler = g_signal_connect_data(bus.get(), "message", G_CALLBACK(+[](GstBus*, GstMessage* message, gpointer userData) {
        auto data = reinterpret_cast<MessageBusData*>(userData);
        auto pipeline = data->pipeline.get();
        if (!pipeline)
            return;

        switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GST_ERROR_OBJECT(pipeline.get(), "Got message: %" GST_PTR_FORMAT, message);
            auto dotFileName = makeString(unsafeSpan(GST_OBJECT_NAME(pipeline.get())), "_error"_s);
            dumpPipeline(pipeline, WTFMove(dotFileName), data->asynchronousPipelineDumping);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(message) != GST_OBJECT_CAST(pipeline.get()))
                break;

            GstState oldState;
            GstState newState;
            GstState pending;
            gst_message_parse_state_changed(message, &oldState, &newState, &pending);

            GST_INFO_OBJECT(pipeline.get(), "State changed (old: %s, new: %s, pending: %s)", gst_element_state_get_name(oldState),
                gst_element_state_get_name(newState), gst_element_state_get_name(pending));

            auto dotFileName = makeString(unsafeSpan(GST_OBJECT_NAME(pipeline.get())), '_', unsafeSpan(gst_element_state_get_name(oldState)), '_', unsafeSpan(gst_element_state_get_name(newState)));
            dumpPipeline(pipeline, WTFMove(dotFileName), data->asynchronousPipelineDumping);
            break;
        }
        case GST_MESSAGE_LATENCY:
            // Recalculate the latency, we don't need any special handling
            // here other than the GStreamer default.
            // This can happen if the latency of live elements changes, or
            // for one reason or another a new live element is added or
            // removed from the pipeline.
            gst_element_call_async(pipeline.get(), reinterpret_cast<GstElementCallAsyncFunc>(+[](GstElement* pipeline, gpointer userData) {
                UNUSED_PARAM(userData);
                gst_bin_recalculate_latency(GST_BIN_CAST(pipeline));
            }), nullptr, nullptr);
            break;
        default:
            break;
        }

        data->handler(message);
    }), data, reinterpret_cast<GClosureNotify>(+[](gpointer data, GClosure*) {
        destroyMessageBusData(reinterpret_cast<MessageBusData*>(data));
    }), static_cast<GConnectFlags>(0));
    g_object_set_qdata(G_OBJECT(pipeline), customMessageHandlerQuark(), GUINT_TO_POINTER(handler));
}

template<>
Vector<uint8_t> GstMappedBuffer::createVector() const
{
    return span<uint8_t>();
}

bool isGStreamerPluginAvailable(ASCIILiteral name)
{
    GRefPtr<GstPlugin> plugin = adoptGRef(gst_registry_find_plugin(gst_registry_get(), name.characters()));
    if (!plugin)
        GST_WARNING("Plugin %s not found. Please check your GStreamer installation", name.characters());
    return plugin;
}

bool gstElementFactoryEquals(GstElement* element, ASCIILiteral name)
{
    return name == GST_OBJECT_NAME(gst_element_get_factory(element));
}

GstElement* /* (transfer floating) */ createAutoAudioSink(const String& role)
{
    auto* audioSink = makeGStreamerElement("autoaudiosink"_s);
    g_signal_connect_data(audioSink, "child-added", G_CALLBACK(+[](GstChildProxy*, GObject* object, gchar*, gpointer userData) {
        auto* role = reinterpret_cast<StringImpl*>(userData);
        auto* objectClass = G_OBJECT_GET_CLASS(object);
        if (role && g_object_class_find_property(objectClass, "stream-properties")) {
            GUniquePtr<GstStructure> properties(gst_structure_new("stream-properties", "media.role", G_TYPE_STRING, role->utf8().data(), nullptr));
            g_object_set(object, "stream-properties", properties.get(), nullptr);
IGNORE_WARNINGS_BEGIN("cast-align")
            GST_DEBUG("Set media.role as %s on %" GST_PTR_FORMAT, role->utf8().data(), GST_ELEMENT_CAST(object));
IGNORE_WARNINGS_END
        }
        if (g_object_class_find_property(objectClass, "client-name")) {
            auto& clientName = getApplicationName();
            g_object_set(object, "client-name", clientName.ascii().data(), nullptr);
        }
    }), role.isolatedCopy().releaseImpl().leakRef(), static_cast<GClosureNotify>([](gpointer userData, GClosure*) {
        reinterpret_cast<StringImpl*>(userData)->deref();
    }), static_cast<GConnectFlags>(0));
    ASSERT(g_object_is_floating(audioSink));
    return audioSink;
}

GstElement* /* (transfer floating) */ createPlatformAudioSink(const String& role)
{
    GstElement* audioSink = webkitAudioSinkNew(role);
    if (!audioSink) {
        // This means the WebKit audio sink configuration failed. It can happen for the following reasons:
        // - audio mixing was not requested using the WEBKIT_GST_ENABLE_AUDIO_MIXER
        // - audio mixing was requested using the WEBKIT_GST_ENABLE_AUDIO_MIXER but the audio mixer
        //   runtime requirements are not fullfilled.
        audioSink = createAutoAudioSink(role);
    }

    return audioSink;
}

bool webkitGstSetElementStateSynchronously(GstElement* pipeline, GstState targetState, Function<bool(GstMessage*)>&& messageHandler)
{
    GST_DEBUG_OBJECT(pipeline, "Setting state to %s", gst_element_state_get_name(targetState));

    GstState currentState;
    auto result = gst_element_get_state(pipeline, &currentState, nullptr, 10);
    if (result == GST_STATE_CHANGE_SUCCESS && currentState == targetState) {
        GST_DEBUG_OBJECT(pipeline, "Target state already reached");
        return true;
    }

    auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(pipeline)));
    gst_bus_enable_sync_message_emission(bus.get());

    auto cleanup = makeScopeExit([bus = GRefPtr<GstBus>(bus), pipeline, targetState] {
        gst_bus_disable_sync_message_emission(bus.get());
#ifdef GST_DISABLE_GST_DEBUG
        UNUSED_VARIABLE(pipeline);
        UNUSED_VARIABLE(targetState);
#else
        GstState currentState;
        auto result = gst_element_get_state(pipeline, &currentState, nullptr, 0);
        GST_DEBUG_OBJECT(pipeline, "Task finished, result: %s, target state reached: %s", gst_element_state_change_return_get_name(result), boolForPrinting(currentState == targetState));
#endif
    });

    result = gst_element_set_state(pipeline, targetState);
    if (result == GST_STATE_CHANGE_FAILURE)
        return false;

    if (result == GST_STATE_CHANGE_ASYNC) {
        while (auto message = adoptGRef(gst_bus_timed_pop_filtered(bus.get(), GST_CLOCK_TIME_NONE, GST_MESSAGE_STATE_CHANGED))) {
            if (!messageHandler(message.get()))
                return false;

            result = gst_element_get_state(pipeline, &currentState, nullptr, 10);
            if (result == GST_STATE_CHANGE_FAILURE)
                return false;

            if (currentState == targetState)
                return true;
        }
    }
    return true;
}

GstBuffer* /* (transfer full) */ gstBufferNewWrappedFast(void* data, size_t length)
{
    return gst_buffer_new_wrapped_full(static_cast<GstMemoryFlags>(0), data, length, 0, length, data, fastFree);
}

GstElement* /* (transfer floating) */ makeGStreamerElement(ASCIILiteral factoryName, const String& name)
{
    static Lock lock;
    static Vector<String> cache WTF_GUARDED_BY_LOCK(lock);
    auto* element = gst_element_factory_make(factoryName.characters(), name.isEmpty() ? nullptr : name.ascii().data());
    Locker locker { lock };
    if (!element && !cache.contains(factoryName)) {
        cache.append(factoryName);
        WTFLogAlways("GStreamer element %s not found. Please install it", factoryName.characters());
        ASSERT_NOT_REACHED_WITH_MESSAGE("GStreamer element %s not found. Please install it", factoryName.characters());
    }
    ASSERT(g_object_is_floating(element));
    return element;
}

#if USE(GSTREAMER_WEBRTC)
static ASCIILiteral webrtcStatsTypeName(int value)
{
    switch (value) {
    case GST_WEBRTC_STATS_CODEC:
        return "codec"_s;
    case GST_WEBRTC_STATS_INBOUND_RTP:
        return "inbound-rtp"_s;
    case GST_WEBRTC_STATS_OUTBOUND_RTP:
        return "outbound-rtp"_s;
    case GST_WEBRTC_STATS_REMOTE_INBOUND_RTP:
        return "remote-inbound-rtp"_s;
    case GST_WEBRTC_STATS_REMOTE_OUTBOUND_RTP:
        return "remote-outbound-rtp"_s;
    case GST_WEBRTC_STATS_CSRC:
        return "csrc"_s;
    case GST_WEBRTC_STATS_PEER_CONNECTION:
        return "peer-connection"_s;
    case GST_WEBRTC_STATS_TRANSPORT:
        return "transport"_s;
    case GST_WEBRTC_STATS_STREAM:
        return "stream"_s;
    case GST_WEBRTC_STATS_DATA_CHANNEL:
        return "data-channel"_s;
    case GST_WEBRTC_STATS_LOCAL_CANDIDATE:
        return "local-candidate"_s;
    case GST_WEBRTC_STATS_REMOTE_CANDIDATE:
        return "remote-candidate"_s;
    case GST_WEBRTC_STATS_CANDIDATE_PAIR:
        return "candidate-pair"_s;
    case GST_WEBRTC_STATS_CERTIFICATE:
        return "certificate"_s;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

#if GST_CHECK_VERSION(1, 27, 0)
static ASCIILiteral webrtcIceTcpCandidateTypeName(int value)
{
    switch (value) {
    case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_NONE:
        return "none"_s;
    case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_ACTIVE:
        return "active"_s;
    case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_PASSIVE:
        return "passive"_s;
    case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_SO:
        return "so"_s;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}
#endif // GST_CHECK_VERSION
#endif // USE(GSTREAMER_WEBRTC)

template<typename T>
std::optional<T> gstStructureGet(const GstStructure* structure, CStringView key)
{
    if (!structure) {
        ASSERT_NOT_REACHED_WITH_MESSAGE("tried to access a field of a null GstStructure");
        return std::nullopt;
    }

    T value;
    if constexpr(std::is_same_v<T, int>) {
        if (gst_structure_get_int(structure, key.utf8(), &value))
            return value;
    } else if constexpr(std::is_same_v<T, int64_t>) {
        if (gst_structure_get_int64(structure, key.utf8(), &value))
            return value;
    } else if constexpr(std::is_same_v<T, unsigned>) {
        if (gst_structure_get_uint(structure, key.utf8(), &value))
            return value;
    } else if constexpr(std::is_same_v<T, uint64_t>) {
        if (gst_structure_get_uint64(structure, key.utf8(), &value))
            return value;
    } else if constexpr(std::is_same_v<T, double>) {
        if (gst_structure_get_double(structure, key.utf8(), &value))
            return value;
    } else if constexpr(std::is_same_v<T, bool>) {
        gboolean gstValue;
        if (gst_structure_get_boolean(structure, key.utf8(), &gstValue)) {
            value = gstValue;
            return value;
        }
    } else
        static_assert(!std::is_same_v<T, T>, "type not implemented for gstStructureGet");
    return std::nullopt;
}

template std::optional<int> gstStructureGet(const GstStructure*, CStringView key);
template std::optional<int64_t> gstStructureGet(const GstStructure*, CStringView key);
template std::optional<unsigned> gstStructureGet(const GstStructure*, CStringView key);
template std::optional<uint64_t> gstStructureGet(const GstStructure*, CStringView key);
template std::optional<double> gstStructureGet(const GstStructure*, CStringView key);
template std::optional<bool> gstStructureGet(const GstStructure*, CStringView key);

CStringView gstStructureGetString(const GstStructure* structure, CStringView key)
{
    if (!structure) {
        ASSERT_NOT_REACHED_WITH_MESSAGE("tried to access a field of a null GstStructure");
        return { };
    }

    const GValue* value = gst_structure_get_value(structure, key.utf8());
    if (!value || !G_VALUE_HOLDS_STRING(value))
        return { };
    return CStringView::unsafeFromUTF8(g_value_get_string(value));
}

CStringView gstStructureGetName(const GstStructure* structure)
{
    if (!structure) {
        ASSERT_NOT_REACHED_WITH_MESSAGE("tried to access a field of a null GstStructure");
        return { };
    }

    return CStringView::unsafeFromUTF8(gst_structure_get_name(structure));
}

template<typename T>
Vector<T> gstStructureGetArray(const GstStructure* structure, CStringView key)
{
    static_assert(std::is_same_v<T, int> || std::is_same_v<T, int64_t> || std::is_same_v<T, unsigned>
        || std::is_same_v<T, uint64_t> || std::is_same_v<T, double> || std::is_same_v<T, const GstStructure*>);
    Vector<T> result;
    if (!structure)
        return result;
    const GValue* array = gst_structure_get_value(structure, key.utf8());
    if (!GST_VALUE_HOLDS_ARRAY (array))
        return result;
    unsigned size = gst_value_array_get_size(array);
    for (unsigned i = 0; i < size; i++) {
        const GValue* item = gst_value_array_get_value(array, i);
        if constexpr(std::is_same_v<T, int>)
            result.append(g_value_get_int(item));
        else if constexpr(std::is_same_v<T, int64_t>)
            result.append(g_value_get_int64(item));
        else if constexpr(std::is_same_v<T, unsigned>)
            result.append(g_value_get_uint(item));
        else if constexpr(std::is_same_v<T, uint64_t>)
            result.append(g_value_get_uint64(item));
        else if constexpr(std::is_same_v<T, double>)
            result.append(g_value_get_double(item));
        else if constexpr(std::is_same_v<T, const GstStructure*>)
            result.append(gst_value_get_structure(item));
    }
    return result;
}

template Vector<const GstStructure*> gstStructureGetArray(const GstStructure*, CStringView key);

template<typename T>
Vector<T> gstStructureGetList(const GstStructure* structure, CStringView key)
{
    static_assert(std::is_same_v<T, int> || std::is_same_v<T, int64_t> || std::is_same_v<T, unsigned>
        || std::is_same_v<T, uint64_t> || std::is_same_v<T, double> || std::is_same_v<T, const GstStructure*>);
    Vector<T> result;
    if (!structure)
        return result;
    const GValue* list = gst_structure_get_value(structure, key.utf8());
    RELEASE_ASSERT(GST_VALUE_HOLDS_LIST(list));
    if (!GST_VALUE_HOLDS_LIST(list)) {
        GST_WARNING("Structure field %s does not hold a list", key.utf8());
        return result;
    }
    unsigned size = gst_value_list_get_size(list);
    for (unsigned i = 0; i < size; i++) {
        const GValue* item = gst_value_list_get_value(list, i);
        if constexpr(std::is_same_v<T, int>)
            result.append(g_value_get_int(item));
        else if constexpr(std::is_same_v<T, int64_t>)
            result.append(g_value_get_int64(item));
        else if constexpr(std::is_same_v<T, unsigned>)
            result.append(g_value_get_uint(item));
        else if constexpr(std::is_same_v<T, uint64_t>)
            result.append(g_value_get_uint64(item));
        else if constexpr(std::is_same_v<T, double>) {
            if (G_VALUE_TYPE(item) == GST_TYPE_FRACTION) {
                double doubleValue;
                gst_util_fraction_to_double(gst_value_get_fraction_numerator(item), gst_value_get_fraction_denominator(item), &doubleValue);
                result.append(doubleValue);
            } else
                result.append(g_value_get_double(item));
        } else if constexpr(std::is_same_v<T, const GstStructure*>)
            result.append(gst_value_get_structure(item));
    }
    return result;
}

template Vector<int> gstStructureGetList(const GstStructure*, CStringView key);
template Vector<int64_t> gstStructureGetList(const GstStructure*, CStringView key);
template Vector<unsigned> gstStructureGetList(const GstStructure*, CStringView key);
template Vector<uint64_t> gstStructureGetList(const GstStructure*, CStringView key);
template Vector<double> gstStructureGetList(const GstStructure*, CStringView key);
template Vector<const GstStructure*> gstStructureGetList(const GstStructure*, CStringView key);

static RefPtr<JSON::Value> gstStructureToJSON(const GstStructure*);

static std::optional<RefPtr<JSON::Value>> gstStructureValueToJSON(const GValue* value)
{
    if (GST_VALUE_HOLDS_STRUCTURE(value))
        return gstStructureToJSON(gst_value_get_structure(value));

    if (GST_VALUE_HOLDS_ARRAY(value)) {
        unsigned size = gst_value_array_get_size(value);
        auto array = JSON::Array::create();
        for (unsigned i = 0; i < size; i++) {
            if (auto innerJson = gstStructureValueToJSON(gst_value_array_get_value(value, i)))
                array->pushValue(innerJson->releaseNonNull());
        }
        return array->asArray()->asValue();
    }
    if (GST_VALUE_HOLDS_LIST(value)) {
        unsigned size = gst_value_list_get_size(value);
        auto array = JSON::Array::create();
        for (unsigned i = 0; i < size; i++) {
            if (auto innerJson = gstStructureValueToJSON(gst_value_list_get_value(value, i)))
                array->pushValue(innerJson->releaseNonNull());
        }
        return array->asArray()->asValue();
    }
    auto valueType = G_VALUE_TYPE(value);
    if (valueType == G_TYPE_BOOLEAN)
        return JSON::Value::create(g_value_get_boolean(value))->asValue();

    if (valueType == G_TYPE_INT)
        return JSON::Value::create(g_value_get_int(value))->asValue();

    if (valueType == G_TYPE_UINT)
        return JSON::Value::create(static_cast<double>(g_value_get_uint(value)))->asValue();

    if (valueType == G_TYPE_DOUBLE)
        return JSON::Value::create(g_value_get_double(value))->asValue();

    if (valueType == G_TYPE_FLOAT)
        return JSON::Value::create(static_cast<double>(g_value_get_float(value)))->asValue();

    // BigInt is not officially supported in JSON, so the workaround is to serialize to a string. See:
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigInt#use_within_json
    if (valueType == G_TYPE_UINT64) {
        auto jsonObject = JSON::Object::create();
        auto resultValue = jsonObject->asObject();
        if (!resultValue)
            return nullptr;
        auto bigIntValue = JSON::Value::create(makeString(g_value_get_uint64(value)));
        resultValue->setValue("$biguint"_s, bigIntValue->asValue().releaseNonNull());
        return resultValue;
    }

    if (valueType == G_TYPE_INT64) {
        auto jsonObject = JSON::Object::create();
        auto resultValue = jsonObject->asObject();
        if (!resultValue)
            return nullptr;
        auto bigIntValue = JSON::Value::create(makeString(g_value_get_int64(value)));
        resultValue->setValue("$bigint"_s, bigIntValue->asValue().releaseNonNull());
        return resultValue;
    }

    if (valueType == G_TYPE_STRING)
        return JSON::Value::create(makeString(unsafeSpan(g_value_get_string(value))))->asValue();

#if USE(GSTREAMER_WEBRTC)
    if (valueType == GST_TYPE_WEBRTC_STATS_TYPE) {
        auto name = webrtcStatsTypeName(g_value_get_enum(value));
        if (!name.isEmpty()) [[likely]]
            return JSON::Value::create(makeString(name))->asValue();
    }
#if GST_CHECK_VERSION(1, 27, 0)
    if (valueType == GST_TYPE_WEBRTC_ICE_TCP_CANDIDATE_TYPE) {
        auto name = webrtcIceTcpCandidateTypeName(g_value_get_enum(value));
        if (!name.isEmpty()) [[likely]]
            return JSON::Value::create(makeString(name))->asValue();
    }
#endif // GST_CHECK_VERSION
#endif // USE(GSTREAMER_WEBRTC)

    GST_WARNING("Unhandled GValue type: %s", G_VALUE_TYPE_NAME(value));
    return { };
}

static RefPtr<JSON::Value> gstStructureToJSON(const GstStructure* structure)
{
    auto jsonObject = JSON::Object::create();
    auto resultValue = jsonObject->asObject();
    if (!resultValue)
        return nullptr;

    gstStructureForeach(structure, [&](auto id, auto value) -> bool {
        if (auto jsonValue = gstStructureValueToJSON(value)) {
            auto fieldId = gstIdToString(id);
            resultValue->setValue(fieldId.toString(), jsonValue->releaseNonNull());
        }
        return TRUE;
    });
    return resultValue;
}

String gstStructureToJSONString(const GstStructure* structure)
{
    auto value = gstStructureToJSON(structure);
    if (!value)
        return { };
    return value->toJSONString();
}

GstClockTime webkitGstInitTime()
{
    return s_webkitGstInitTime;
}

PlatformVideoColorSpace videoColorSpaceFromCaps(const GstCaps* caps)
{
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps))
        return { };

    return videoColorSpaceFromInfo(info);
}

PlatformVideoColorSpace videoColorSpaceFromInfo(const GstVideoInfo& info)
{
    ensureGStreamerInitialized();
#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> colorimetry(gst_video_colorimetry_to_string(&GST_VIDEO_INFO_COLORIMETRY(&info)));
#endif
    PlatformVideoColorSpace colorSpace;
    switch (GST_VIDEO_INFO_COLORIMETRY(&info).matrix) {
    case GST_VIDEO_COLOR_MATRIX_RGB:
        colorSpace.matrix = PlatformVideoMatrixCoefficients::Rgb;
        break;
    case GST_VIDEO_COLOR_MATRIX_BT709:
        colorSpace.matrix = PlatformVideoMatrixCoefficients::Bt709;
        break;
    case GST_VIDEO_COLOR_MATRIX_BT601:
        colorSpace.matrix = PlatformVideoMatrixCoefficients::Smpte170m;
        break;
    case GST_VIDEO_COLOR_MATRIX_SMPTE240M:
        colorSpace.matrix = PlatformVideoMatrixCoefficients::Smpte240m;
        break;
    case GST_VIDEO_COLOR_MATRIX_FCC:
        colorSpace.matrix = PlatformVideoMatrixCoefficients::Fcc;
        break;
    case GST_VIDEO_COLOR_MATRIX_BT2020:
        colorSpace.matrix = PlatformVideoMatrixCoefficients::Bt2020Ncl;
        break;
    case GST_VIDEO_COLOR_MATRIX_UNKNOWN:
        colorSpace.matrix = PlatformVideoMatrixCoefficients::Unspecified;
        break;
    default:
#ifndef GST_DISABLE_GST_DEBUG
        GST_WARNING("Unhandled colorspace matrix from %s", colorimetry.get());
#endif
        break;
    }

    switch (GST_VIDEO_INFO_COLORIMETRY(&info).transfer) {
    case GST_VIDEO_TRANSFER_SRGB:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Iec6196621;
        break;
    case GST_VIDEO_TRANSFER_BT709:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Bt709;
        break;
    case GST_VIDEO_TRANSFER_BT601:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Smpte170m;
        break;
    case GST_VIDEO_TRANSFER_SMPTE2084:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::SmpteSt2084;
        break;
    case GST_VIDEO_TRANSFER_ARIB_STD_B67:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::AribStdB67Hlg;
        break;
    case GST_VIDEO_TRANSFER_BT2020_10:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Bt2020_10bit;
        break;
    case GST_VIDEO_TRANSFER_BT2020_12:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Bt2020_12bit;
        break;
    case GST_VIDEO_TRANSFER_GAMMA10:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Linear;
        break;
    case GST_VIDEO_TRANSFER_GAMMA22:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Gamma22curve;
        break;
    case GST_VIDEO_TRANSFER_GAMMA28:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Gamma28curve;
        break;
    case GST_VIDEO_TRANSFER_SMPTE240M:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Smpte240m;
        break;
    case GST_VIDEO_TRANSFER_LOG100:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Log;
        break;
    case GST_VIDEO_TRANSFER_LOG316:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::LogSqrt;
        break;
    case GST_VIDEO_TRANSFER_UNKNOWN:
        colorSpace.transfer = PlatformVideoTransferCharacteristics::Unspecified;
        break;
    default:
#ifndef GST_DISABLE_GST_DEBUG
        GST_WARNING("Unhandled colorspace transfer from %s", colorimetry.get());
#endif
        break;
    }

    switch (GST_VIDEO_INFO_COLORIMETRY(&info).primaries) {
    case GST_VIDEO_COLOR_PRIMARIES_BT709:
        colorSpace.primaries = PlatformVideoColorPrimaries::Bt709;
        break;
    case GST_VIDEO_COLOR_PRIMARIES_BT470BG:
        colorSpace.primaries = PlatformVideoColorPrimaries::Bt470bg;
        break;
    case GST_VIDEO_COLOR_PRIMARIES_BT470M:
        colorSpace.primaries = PlatformVideoColorPrimaries::Bt470m;
        break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTE170M:
        colorSpace.primaries = PlatformVideoColorPrimaries::Smpte170m;
        break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTERP431:
        colorSpace.primaries = PlatformVideoColorPrimaries::SmpteRp431;
        break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTEEG432:
        colorSpace.primaries = PlatformVideoColorPrimaries::SmpteEg432;
        break;
    case GST_VIDEO_COLOR_PRIMARIES_FILM:
        colorSpace.primaries = PlatformVideoColorPrimaries::Film;
        break;
    case GST_VIDEO_COLOR_PRIMARIES_BT2020:
        colorSpace.primaries = PlatformVideoColorPrimaries::Bt2020;
        break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTE240M:
        colorSpace.primaries = PlatformVideoColorPrimaries::Smpte240m;
        break;
    case GST_VIDEO_COLOR_PRIMARIES_EBU3213:
        colorSpace.primaries = PlatformVideoColorPrimaries::JedecP22Phosphors;
        break;
    case GST_VIDEO_COLOR_PRIMARIES_UNKNOWN:
        colorSpace.primaries = PlatformVideoColorPrimaries::Unspecified;
        break;
    default:
#ifndef GST_DISABLE_GST_DEBUG
        GST_WARNING("Unhandled colorspace primaries from %s", colorimetry.get());
#endif
        break;
    }

    if (GST_VIDEO_INFO_COLORIMETRY(&info).range != GST_VIDEO_COLOR_RANGE_UNKNOWN)
        colorSpace.fullRange = GST_VIDEO_INFO_COLORIMETRY(&info).range == GST_VIDEO_COLOR_RANGE_0_255;

    return colorSpace;
}

void fillVideoInfoColorimetryFromColorSpace(GstVideoInfo* info, const PlatformVideoColorSpace& colorSpace)
{
    ensureGStreamerInitialized();
    if (colorSpace.matrix) {
        switch (*colorSpace.matrix) {
        case PlatformVideoMatrixCoefficients::Rgb:
            GST_VIDEO_INFO_COLORIMETRY(info).matrix = GST_VIDEO_COLOR_MATRIX_RGB;
            break;
        case PlatformVideoMatrixCoefficients::Bt709:
            GST_VIDEO_INFO_COLORIMETRY(info).matrix = GST_VIDEO_COLOR_MATRIX_BT709;
            break;
        case PlatformVideoMatrixCoefficients::Smpte170m:
            GST_VIDEO_INFO_COLORIMETRY(info).matrix = GST_VIDEO_COLOR_MATRIX_BT601;
            break;
        case PlatformVideoMatrixCoefficients::Smpte240m:
            GST_VIDEO_INFO_COLORIMETRY(info).matrix = GST_VIDEO_COLOR_MATRIX_SMPTE240M;
            break;
        case PlatformVideoMatrixCoefficients::Fcc:
            GST_VIDEO_INFO_COLORIMETRY(info).matrix = GST_VIDEO_COLOR_MATRIX_FCC;
            break;
        case PlatformVideoMatrixCoefficients::Bt2020NonconstantLuminance:
            GST_VIDEO_INFO_COLORIMETRY(info).matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
            break;
        case PlatformVideoMatrixCoefficients::Unspecified:
            GST_VIDEO_INFO_COLORIMETRY(info).matrix = GST_VIDEO_COLOR_MATRIX_UNKNOWN;
            break;
        default:
            break;
        };
    } else
        GST_VIDEO_INFO_COLORIMETRY(info).matrix = GST_VIDEO_COLOR_MATRIX_UNKNOWN;

    if (colorSpace.transfer) {
        switch (*colorSpace.transfer) {
        case PlatformVideoTransferCharacteristics::Iec6196621:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_SRGB;
            break;
        case PlatformVideoTransferCharacteristics::Bt709:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_BT709;
            break;
        case PlatformVideoTransferCharacteristics::Smpte170m:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_BT601;
            break;
        case PlatformVideoTransferCharacteristics::SmpteSt2084:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_SMPTE2084;
            break;
        case PlatformVideoTransferCharacteristics::AribStdB67Hlg:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_ARIB_STD_B67;
            break;
        case PlatformVideoTransferCharacteristics::Bt2020_10bit:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_BT2020_10;
            break;
        case PlatformVideoTransferCharacteristics::Bt2020_12bit:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_BT2020_12;
            break;
        case PlatformVideoTransferCharacteristics::Linear:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_GAMMA10;
            break;
        case PlatformVideoTransferCharacteristics::Gamma22curve:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_GAMMA22;
            break;
        case PlatformVideoTransferCharacteristics::Gamma28curve:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_GAMMA28;
            break;
        case PlatformVideoTransferCharacteristics::Smpte240m:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_SMPTE240M;
            break;
        case PlatformVideoTransferCharacteristics::Log:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_LOG100;
            break;
        case PlatformVideoTransferCharacteristics::LogSqrt:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_LOG316;
            break;
        case PlatformVideoTransferCharacteristics::Unspecified:
            GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_UNKNOWN;
            break;
        default:
            break;
        }
    } else
        GST_VIDEO_INFO_COLORIMETRY(info).transfer = GST_VIDEO_TRANSFER_UNKNOWN;

    if (colorSpace.primaries) {
        switch (*colorSpace.primaries) {
        case PlatformVideoColorPrimaries::Bt709:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
            break;
        case PlatformVideoColorPrimaries::Bt470bg:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT470BG;
            break;
        case PlatformVideoColorPrimaries::Bt470m:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT470M;
            break;
        case PlatformVideoColorPrimaries::Smpte170m:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTE170M;
            break;
        case PlatformVideoColorPrimaries::SmpteRp431:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTERP431;
            break;
        case PlatformVideoColorPrimaries::SmpteEg432:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTEEG432;
            break;
        case PlatformVideoColorPrimaries::Film:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_FILM;
            break;
        case PlatformVideoColorPrimaries::Bt2020:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
            break;
        case PlatformVideoColorPrimaries::Smpte240m:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTE240M;
            break;
        case PlatformVideoColorPrimaries::JedecP22Phosphors:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_EBU3213;
            break;
        case PlatformVideoColorPrimaries::Unspecified:
            GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;
            break;
        default:
            break;
        }
    } else
        GST_VIDEO_INFO_COLORIMETRY(info).primaries = GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;

    if (colorSpace.fullRange)
        GST_VIDEO_INFO_COLORIMETRY(info).range = *colorSpace.fullRange ? GST_VIDEO_COLOR_RANGE_0_255 : GST_VIDEO_COLOR_RANGE_16_235;
    else
        GST_VIDEO_INFO_COLORIMETRY(info).range = GST_VIDEO_COLOR_RANGE_UNKNOWN;
}

void configureAudioDecoderForHarnessing(const GRefPtr<GstElement>& element)
{
    if (gstObjectHasProperty(element.get(), "max-errors"_s))
        g_object_set(element.get(), "max-errors", 0, nullptr);

    // rawaudioparse-specific:
    if (gstElementMatchesFactoryAndHasProperty(element.get(), "rawaudioparse"_s, "use-sink-caps"_s))
        g_object_set(element.get(), "use-sink-caps", TRUE, nullptr);
}

void configureVideoDecoderForHarnessing(const GRefPtr<GstElement>& element)
{
    if (gstObjectHasProperty(element.get(), "max-threads"_s))
        g_object_set(element.get(), "max-threads", 1, nullptr);

    if (gstObjectHasProperty(element.get(), "max-errors"_s))
        g_object_set(element.get(), "max-errors", 0, nullptr);

    // avdec-specific:
    if (gstElementMatchesFactoryAndHasProperty(element.get(), "avdec*"_s, "std-compliance"_s))
        gst_util_set_object_arg(G_OBJECT(element.get()), "std-compliance", "strict");

    if (gstElementMatchesFactoryAndHasProperty(element.get(), "avdec*"_s, "output-corrupt"_s))
        g_object_set(element.get(), "output-corrupt", FALSE, nullptr);

    // dav1ddec-specific:
    if (gstElementMatchesFactoryAndHasProperty(element.get(), "dav1ddec"_s, "n-threads"_s))
        g_object_set(element.get(), "n-threads", 1, nullptr);
}

void configureMediaStreamAudioDecoder(GstElement* element)
{
    // Currently implemented only in opusdec.
    if (gstObjectHasProperty(element, "plc"_s))
        g_object_set(element, "plc", TRUE, nullptr);

    // Currently implemented only in opusdec.
    if (gstObjectHasProperty(element, "use-inband-fec"_s))
        g_object_set(element, "use-inband-fec", TRUE, nullptr);

    if (gstObjectHasProperty(element, "max-errors"_s))
        g_object_set(element, "max-errors", -1, nullptr);
}

void configureMediaStreamVideoDecoder(GstElement* element)
{
    if (gstObjectHasProperty(element, "automatic-request-sync-points"_s))
        g_object_set(element, "automatic-request-sync-points", TRUE, nullptr);

    if (gstObjectHasProperty(element, "discard-corrupted-frames"_s))
        g_object_set(element, "discard-corrupted-frames", TRUE, nullptr);

    if (gstObjectHasProperty(element, "output-corrupt"_s))
        g_object_set(element, "output-corrupt", FALSE, nullptr);

    if (gstObjectHasProperty(element, "max-errors"_s))
        g_object_set(element, "max-errors", -1, nullptr);
}

void configureVideoRTPDepayloader(GstElement* element)
{
    if (gstObjectHasProperty(element, "request-keyframe"_s))
        g_object_set(element, "request-keyframe", TRUE, nullptr);

    if (gstObjectHasProperty(element, "wait-for-keyframe"_s))
        g_object_set(element, "wait-for-keyframe", TRUE, nullptr);
}

bool gstObjectHasProperty(GstObject* gstObject, ASCIILiteral name)
{
    return g_object_class_find_property(G_OBJECT_GET_CLASS(gstObject), name.characters());
}

bool gstObjectHasProperty(GstElement* element, ASCIILiteral name)
{
    return gstObjectHasProperty(GST_OBJECT_CAST(element), name);
}

bool gstObjectHasProperty(GstPad* pad, ASCIILiteral name)
{
    return gstObjectHasProperty(GST_OBJECT_CAST(pad), name);
}

bool gstElementMatchesFactoryAndHasProperty(GstElement* element, ASCIILiteral factoryNamePattern, ASCIILiteral propertyName)
{
    auto factory = gst_element_get_factory(element);
    if (!factory)
        return gstObjectHasProperty(element, propertyName);

    auto nameView = StringView::fromLatin1(GST_OBJECT_NAME(factory));
    if (fnmatch(factoryNamePattern.characters(), nameView.toStringWithoutCopying().ascii().data(), 0))
        return false;

    return gstObjectHasProperty(element, propertyName);
}

GRefPtr<GstBuffer> wrapSpanData(const std::span<const uint8_t>& span)
{
    if (span.empty())
        return nullptr;

    Vector<uint8_t> data { span };
    auto bufferSize = data.size();
    auto bufferData = data.mutableSpan().data();
    auto buffer = adoptGRef(gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, bufferData, bufferSize, 0, bufferSize, new Vector<uint8_t>(WTFMove(data)), [](gpointer data) {
        delete static_cast<Vector<uint8_t>*>(data);
    }));
    return buffer;
}

std::optional<unsigned> gstGetAutoplugSelectResult(ASCIILiteral nick)
{
    static auto enumClass = static_cast<GEnumClass*>(g_type_class_ref(g_type_from_name("GstAutoplugSelectResult")));
    RELEASE_ASSERT(enumClass);
    auto enumValue = g_enum_get_value_by_nick(enumClass, nick.characters());
    if (!enumValue)
        return std::nullopt;
    return enumValue->value;
}

bool gstStructureForeach(const GstStructure* structure, Function<bool(GstId, const GValue*)>&& callback)
{
#if GST_CHECK_VERSION(1, 26, 0)
    return gst_structure_foreach_id_str(structure, [](GstId id, const GValue* value, gpointer userData) -> gboolean {
        auto& callback = *reinterpret_cast<Function<bool(GstId, const GValue*)>*>(userData);
        return callback(id, value);
    }, &callback);
#else
    return gst_structure_foreach(structure, [](GQuark quark, const GValue* value, gpointer userData) -> gboolean {
        auto& callback = *reinterpret_cast<Function<bool(GQuark, const GValue*)>*>(userData);
        return callback(quark, value);
    }, &callback);
#endif
}

void gstStructureIdSetValue(GstStructure* structure, GstId id, const GValue* value)
{
#if GST_CHECK_VERSION(1, 26, 0)
    gst_structure_id_str_set_value(structure, id, value);
#else
    gst_structure_set_value(structure, g_quark_to_string(id), value);
#endif
}

bool gstStructureMapInPlace(GstStructure* structure, Function<bool(GstId, GValue*)>&& callback)
{
#if GST_CHECK_VERSION(1, 26, 0)
    return gst_structure_map_in_place_id_str(structure, [](GstId id, GValue* value, gpointer userData) -> gboolean {
        auto& callback = *reinterpret_cast<Function<bool(GstId, GValue*)>*>(userData);
        return callback(id, value);
    }, &callback);
#else
    return gst_structure_map_in_place(structure, [](GQuark quark, GValue* value, gpointer userData) -> gboolean {
        auto& callback = *reinterpret_cast<Function<bool(GQuark, GValue*)>*>(userData);
        return callback(quark, value);
    }, &callback);
#endif
}

StringView gstIdToString(GstId id)
{
#if GST_CHECK_VERSION(1, 26, 0)
    return StringView::fromLatin1(gst_id_str_as_str(id));
#else
    return StringView::fromLatin1(g_quark_to_string(id));
#endif
}

void gstStructureFilterAndMapInPlace(GstStructure* structure, Function<bool(GstId, GValue*)>&& callback)
{
#if GST_CHECK_VERSION(1, 26, 0)
    gst_structure_filter_and_map_in_place_id_str(structure, [](GstId id, GValue* value, gpointer userData) -> gboolean {
        auto& callback = *reinterpret_cast<Function<bool(GstId, GValue*)>*>(userData);
        return callback(id, value);
    }, &callback);
#else
    gst_structure_filter_and_map_in_place(structure, [](GQuark quark, GValue* value, gpointer userData) -> gboolean {
        auto& callback = *reinterpret_cast<Function<bool(GQuark, GValue*)>*>(userData);
        return callback(quark, value);
    }, &callback);
#endif
}

#if USE(GBM) && !GST_CHECK_VERSION(1, 24, 0)
static GstVideoFormat drmFourccToGstVideoFormat(uint32_t fourcc)
{
    switch (fourcc) {
    case DRM_FORMAT_XRGB8888:
        return GST_VIDEO_FORMAT_BGRx;
    case DRM_FORMAT_XBGR8888:
        return GST_VIDEO_FORMAT_RGBx;
    case DRM_FORMAT_ARGB8888:
        return GST_VIDEO_FORMAT_BGRA;
    case DRM_FORMAT_ABGR8888:
        return GST_VIDEO_FORMAT_RGBA;
    case DRM_FORMAT_YUV420:
        return GST_VIDEO_FORMAT_I420;
    case DRM_FORMAT_YVU420:
        return GST_VIDEO_FORMAT_YV12;
    case DRM_FORMAT_NV12:
        return GST_VIDEO_FORMAT_NV12;
    case DRM_FORMAT_NV21:
        return GST_VIDEO_FORMAT_NV21;
    case DRM_FORMAT_YUV444:
        return GST_VIDEO_FORMAT_Y444;
    case DRM_FORMAT_YUV411:
        return GST_VIDEO_FORMAT_Y41B;
    case DRM_FORMAT_YUV422:
        return GST_VIDEO_FORMAT_Y42B;
    case DRM_FORMAT_P010:
        return GST_VIDEO_FORMAT_P010_10LE;
    default:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return GST_VIDEO_FORMAT_UNKNOWN;
}
#endif // USE(GBM) && !GST_CHECK_VERSION(1, 24, 0)

#if USE(GBM)
GRefPtr<GstCaps> buildDMABufCaps()
{
    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_from_string("video/x-raw(memory:DMABuf), width = " GST_VIDEO_SIZE_RANGE ", height = " GST_VIDEO_SIZE_RANGE ", framerate = " GST_VIDEO_FPS_RANGE));
#if GST_CHECK_VERSION(1, 24, 0)
    gst_caps_set_simple(caps.get(), "format", G_TYPE_STRING, "DMA_DRM", nullptr);

    static const char* formats = g_getenv("WEBKIT_GST_DMABUF_FORMATS");
    if (formats && *formats) {
        auto formatsString = StringView::fromLatin1(formats);
        GValue drmSupportedFormats = G_VALUE_INIT;
        g_value_init(&drmSupportedFormats, GST_TYPE_LIST);
        for (auto token : formatsString.split(',')) {
            GValue value = G_VALUE_INIT;
            g_value_init(&value, G_TYPE_STRING);
            g_value_set_string(&value, token.toStringWithoutCopying().ascii().data());
            gst_value_list_append_and_take_value(&drmSupportedFormats, &value);
        }
        gst_caps_set_value(caps.get(), "drm-format", &drmSupportedFormats);
        g_value_unset(&drmSupportedFormats);
        return caps;
    }
#endif

    GValue supportedFormats = G_VALUE_INIT;
    g_value_init(&supportedFormats, GST_TYPE_LIST);
    const auto& bufferFormats = PlatformDisplay::sharedDisplay().bufferFormatsForVideo();
    for (const auto& format : bufferFormats) {
#if GST_CHECK_VERSION(1, 24, 0)
        if (format.modifiers.isEmpty() || format.modifiers[0] == DRM_FORMAT_MOD_INVALID) {
            GValue value = G_VALUE_INIT;
            g_value_init(&value, G_TYPE_STRING);
            g_value_take_string(&value, gst_video_dma_drm_fourcc_to_string(format.fourcc.value, DRM_FORMAT_MOD_LINEAR));
            gst_value_list_append_and_take_value(&supportedFormats, &value);
        } else {
            for (auto modifier : format.modifiers) {
                GValue value = G_VALUE_INIT;
                g_value_init(&value, G_TYPE_STRING);
                g_value_take_string(&value, gst_video_dma_drm_fourcc_to_string(format.fourcc.value, modifier));
                gst_value_list_append_and_take_value(&supportedFormats, &value);
            }
        }
#else
        GValue value = G_VALUE_INIT;
        g_value_init(&value, G_TYPE_STRING);
        g_value_set_string(&value, gst_video_format_to_string(drmFourccToGstVideoFormat(format.fourcc.value)));
        gst_value_list_append_and_take_value(&supportedFormats, &value);
#endif
    }

#if GST_CHECK_VERSION(1, 24, 0)
    gst_caps_set_value(caps.get(), "drm-format", &supportedFormats);
#else
    gst_caps_set_value(caps.get(), "format", &supportedFormats);
#endif
    g_value_unset(&supportedFormats);

    return caps;
}
#endif // USE(GBM)

#if USE(GSTREAMER_GL)
static std::optional<GRefPtr<GstContext>> requestGLContext(const char* contextType)
{
    auto& sharedDisplay = PlatformDisplay::sharedDisplay();
    auto* gstGLDisplay = sharedDisplay.gstGLDisplay();
    auto* gstGLContext = sharedDisplay.gstGLContext();

    if (!gstGLDisplay || !gstGLContext)
        return std::nullopt;

    if (!g_strcmp0(contextType, GST_GL_DISPLAY_CONTEXT_TYPE)) {
        GRefPtr<GstContext> displayContext = adoptGRef(gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, FALSE));
        gst_context_set_gl_display(displayContext.get(), gstGLDisplay);
        return displayContext;
    }

    if (!g_strcmp0(contextType, "gst.gl.app_context")) {
        GRefPtr<GstContext> appContext = adoptGRef(gst_context_new("gst.gl.app_context", FALSE));
        GstStructure* structure = gst_context_writable_structure(appContext.get());
        gst_structure_set(structure, "context", GST_TYPE_GL_CONTEXT, gstGLContext, nullptr);
        return appContext;
    }

    return std::nullopt;
}

bool setGstElementGLContext(GstElement* element, ASCIILiteral contextType)
{
    GRefPtr<GstContext> oldContext = adoptGRef(gst_element_get_context(element, contextType.characters()));
    if (!oldContext) {
        auto newContext = requestGLContext(contextType.characters());
        if (!newContext)
            return false;
        gst_element_set_context(element, newContext->get());
    }
    return true;
}
#endif

GstStateChangeReturn gstElementLockAndSetState(GstElement* element, GstState state)
{
    auto parent = adoptGRef(gst_element_get_parent(element));
    if (parent)
        GST_STATE_LOCK(parent.get());

    gst_element_set_locked_state(element, TRUE);
    auto result = gst_element_set_state(element, state);
    gst_element_set_locked_state(element, FALSE);

    if (parent)
        GST_STATE_UNLOCK(parent.get());
    return result;
}

GRefPtr<GstElement> createVideoConvertScaleElement(const String& name)
{
    // Keep videoconvertscale disabled for now due to some performance issues.
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/3815
    auto useVideoConvertScale = StringView::fromLatin1(std::getenv("WEBKIT_GST_USE_VIDEOCONVERT_SCALE"));
    if (useVideoConvertScale == "1"_s && gst_check_version(1, 22, 0)) {
        GRefPtr videoConvertScale = makeGStreamerElement("videoconvertscale"_s, name);
        if (!videoConvertScale)
            return nullptr;

        // Enable multi-threading in the converter.
        g_object_set(videoConvertScale.get(), "n-threads", 0U, nullptr);
        return videoConvertScale;
    }

    auto videoScale = makeGStreamerElement("videoscale"_s);
    if (!videoScale)
        return nullptr;

    auto videoConvert = makeGStreamerElement("videoconvert"_s);
    if (!videoConvert)
        return nullptr;

    // Enable multi-threading in the converter.
    g_object_set(videoConvert, "n-threads", 0U, nullptr);

    GRefPtr bin = gst_bin_new(name.utf8().data());
    gst_bin_add_many(GST_BIN_CAST(bin.get()), videoScale, videoConvert, nullptr);
    gst_element_link(videoScale, videoConvert);

    auto pad = adoptGRef(gst_element_get_static_pad(videoScale, "sink"));
    gst_element_add_pad(bin.get(), gst_ghost_pad_new("sink", pad.get()));
    pad = adoptGRef(gst_element_get_static_pad(videoConvert, "src"));
    gst_element_add_pad(bin.get(), gst_ghost_pad_new("src", pad.get()));
    return bin;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#undef IS_GST_FULL_1_18

#if !GST_CHECK_VERSION(1, 20, 0)
GstBuffer* gst_buffer_new_memdup(gconstpointer data, gsize size)
{
    gpointer copiedData = nullptr;

    if (data && size) {
        copiedData = g_malloc(size);
        memcpy(copiedData, data, size);
    }

    return gst_buffer_new_wrapped_full(static_cast<GstMemoryFlags>(0), copiedData, size, 0, size, copiedData, g_free);
}
#endif

#if !GST_CHECK_VERSION_FULL(1, 27, 2, 1) && !GST_CHECK_VERSION(1, 27, 3) && !GST_CHECK_VERSION(1, 28, 0)
void gst_pad_probe_info_set_buffer(GstPadProbeInfo* info, GstBuffer* buffer)
{
    g_return_if_fail(info->type & GST_PAD_PROBE_TYPE_BUFFER);

    gst_clear_mini_object(&info->data);
    info->data = buffer;
}

void gst_pad_probe_info_set_event(GstPadProbeInfo* info, GstEvent* event)
{
    g_return_if_fail(info->type & (GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_EVENT_UPSTREAM));

    gst_clear_mini_object(&info->data);
    info->data = event;
}
#endif

#endif // USE(GSTREAMER)
