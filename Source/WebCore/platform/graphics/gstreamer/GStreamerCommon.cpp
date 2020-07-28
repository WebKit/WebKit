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

#include "GLVideoSinkGStreamer.h"
#include "GstAllocatorFastMalloc.h"
#include "IntSize.h"
#include "SharedBuffer.h"
#include <gst/audio/audio-info.h>
#include <gst/gst.h>
#include <mutex>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>

#if USE(GSTREAMER_MPEGTS)
#define GST_USE_UNSTABLE_API
#include <gst/mpegts/mpegts.h>
#undef GST_USE_UNSTABLE_API
#endif

#if ENABLE(MEDIA_SOURCE)
#include "WebKitMediaSourceGStreamer.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "GStreamerMediaStreamSource.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "WebKitClearKeyDecryptorGStreamer.h"
#if ENABLE(THUNDER)
#include "WebKitThunderDecryptorGStreamer.h"
#endif
#endif

#if ENABLE(VIDEO)
#include "WebKitWebSourceGStreamer.h"
#endif

namespace WebCore {

GstPad* webkitGstGhostPadFromStaticTemplate(GstStaticPadTemplate* staticPadTemplate, const gchar* name, GstPad* target)
{
    GstPad* pad;
    GstPadTemplate* padTemplate = gst_static_pad_template_get(staticPadTemplate);

    if (target)
        pad = gst_ghost_pad_new_from_template(name, target, padTemplate);
    else
        pad = gst_ghost_pad_new_no_target_from_template(name, padTemplate);

    gst_object_unref(padTemplate);

    return pad;
}

#if ENABLE(VIDEO)
bool getVideoSizeAndFormatFromCaps(GstCaps* caps, WebCore::IntSize& size, GstVideoFormat& format, int& pixelAspectRatioNumerator, int& pixelAspectRatioDenominator, int& stride)
{
    if (!doCapsHaveType(caps, GST_VIDEO_CAPS_TYPE_PREFIX)) {
        GST_WARNING("Failed to get the video size and format, these are not a video caps");
        return false;
    }

    if (areEncryptedCaps(caps)) {
        GstStructure* structure = gst_caps_get_structure(caps, 0);
        format = GST_VIDEO_FORMAT_ENCODED;
        stride = 0;
        int width = 0, height = 0;
        gst_structure_get_int(structure, "width", &width);
        gst_structure_get_int(structure, "height", &height);
        if (!gst_structure_get_fraction(structure, "pixel-aspect-ratio", &pixelAspectRatioNumerator, &pixelAspectRatioDenominator)) {
            pixelAspectRatioNumerator = 1;
            pixelAspectRatioDenominator = 1;
        }

        size.setWidth(width);
        size.setHeight(height);
    } else {
        GstVideoInfo info;
        gst_video_info_init(&info);
        if (!gst_video_info_from_caps(&info, caps))
            return false;

        format = GST_VIDEO_INFO_FORMAT(&info);
        size.setWidth(GST_VIDEO_INFO_WIDTH(&info));
        size.setHeight(GST_VIDEO_INFO_HEIGHT(&info));
        pixelAspectRatioNumerator = GST_VIDEO_INFO_PAR_N(&info);
        pixelAspectRatioDenominator = GST_VIDEO_INFO_PAR_D(&info);
        stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
    }

    return true;
}

Optional<FloatSize> getVideoResolutionFromCaps(const GstCaps* caps)
{
    if (!doCapsHaveType(caps, GST_VIDEO_CAPS_TYPE_PREFIX)) {
        GST_WARNING("Failed to get the video resolution, these are not a video caps");
        return WTF::nullopt;
    }

    int width = 0, height = 0;
    int pixelAspectRatioNumerator = 1, pixelAspectRatioDenominator = 1;

    if (areEncryptedCaps(caps)) {
        GstStructure* structure = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(structure, "width", &width);
        gst_structure_get_int(structure, "height", &height);
        gst_structure_get_fraction(structure, "pixel-aspect-ratio", &pixelAspectRatioNumerator, &pixelAspectRatioDenominator);
    } else {
        GstVideoInfo info;
        gst_video_info_init(&info);
        if (!gst_video_info_from_caps(&info, caps))
            return WTF::nullopt;

        width = GST_VIDEO_INFO_WIDTH(&info);
        height = GST_VIDEO_INFO_HEIGHT(&info);
        pixelAspectRatioNumerator = GST_VIDEO_INFO_PAR_N(&info);
        pixelAspectRatioDenominator = GST_VIDEO_INFO_PAR_D(&info);
    }

    return makeOptional(FloatSize(width, height * (static_cast<float>(pixelAspectRatioDenominator) / static_cast<float>(pixelAspectRatioNumerator))));
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
#endif


const char* capsMediaType(const GstCaps* caps)
{
    ASSERT(caps);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    if (!structure) {
        GST_WARNING("caps are empty");
        return nullptr;
    }
#if ENABLE(ENCRYPTED_MEDIA)
    if (gst_structure_has_name(structure, "application/x-cenc") || gst_structure_has_name(structure, "application/x-webm-enc"))
        return gst_structure_get_string(structure, "original-media-type");
#endif
    return gst_structure_get_name(structure);
}

bool doCapsHaveType(const GstCaps* caps, const char* type)
{
    const char* mediaType = capsMediaType(caps);
    if (!mediaType) {
        GST_WARNING("Failed to get MediaType");
        return false;
    }
    return g_str_has_prefix(mediaType, type);
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

Vector<String> extractGStreamerOptionsFromCommandLine()
{
    GUniqueOutPtr<char> contents;
    gsize length;
    if (!g_file_get_contents("/proc/self/cmdline", &contents.outPtr(), &length, nullptr))
        return { };

    Vector<String> options;
    auto optionsString = String::fromUTF8(contents.get(), length);
    optionsString.split('\0', [&options](StringView item) {
        if (item.startsWith("--gst"))
            options.append(item.toString());
    });
    return options;
}

bool initializeGStreamer(Optional<Vector<String>>&& options)
{
    static std::once_flag onceFlag;
    static bool isGStreamerInitialized;
    std::call_once(onceFlag, [options = WTFMove(options)] {
        isGStreamerInitialized = false;

        // USE_PLAYBIN3 is dangerous for us because its potential sneaky effect
        // is to register the playbin3 element under the playbin namespace. We
        // can't allow this, when we create playbin, we want playbin2, not
        // playbin3.
        if (g_getenv("USE_PLAYBIN3"))
            WTFLogAlways("The USE_PLAYBIN3 variable was detected in the environment. Expect playback issues or please unset it.");

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
        Vector<String> parameters = options.valueOr(extractGStreamerOptionsFromCommandLine());
        char** argv = g_new0(char*, parameters.size() + 2);
        int argc = parameters.size() + 1;
        argv[0] = g_strdup(getCurrentExecutableName().data());
        for (unsigned i = 0; i < parameters.size(); i++)
            argv[i + 1] = g_strdup(parameters[i].utf8().data());

        GUniqueOutPtr<GError> error;
        isGStreamerInitialized = gst_init_check(&argc, &argv, &error.outPtr());
        ASSERT_WITH_MESSAGE(isGStreamerInitialized, "GStreamer initialization failed: %s", error ? error->message : "unknown error occurred");
        g_strfreev(argv);

        if (isFastMallocEnabled()) {
            const char* disableFastMalloc = getenv("WEBKIT_GST_DISABLE_FAST_MALLOC");
            if (!disableFastMalloc || !strcmp(disableFastMalloc, "0"))
                gst_allocator_set_default(GST_ALLOCATOR(g_object_new(gst_allocator_fast_malloc_get_type(), nullptr)));
        }

#if USE(GSTREAMER_MPEGTS)
        if (isGStreamerInitialized)
            gst_mpegts_initialize();
#endif

        // If the FDK-AAC decoder is available, promote it and downrank the
        // libav AAC decoders, due to their broken LC support, as reported in:
        // https://ffmpeg.org/pipermail/ffmpeg-devel/2019-July/247063.html
        GRefPtr<GstElementFactory> elementFactory = adoptGRef(gst_element_factory_find("fdkaacdec"));
        if (elementFactory) {
            gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE_CAST(elementFactory.get()), GST_RANK_PRIMARY);

            const char* const elementNames[] = {"avdec_aac", "avdec_aac_fixed", "avdec_aac_latm"};
            for (unsigned i = 0; i < G_N_ELEMENTS(elementNames); i++) {
                GRefPtr<GstElementFactory> avAACDecoderFactory = adoptGRef(gst_element_factory_find(elementNames[i]));
                if (avAACDecoderFactory)
                    gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE_CAST(avAACDecoderFactory.get()), GST_RANK_MARGINAL);
            }
        }

#endif
    });
    return isGStreamerInitialized;
}

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)
// WebM does not specify a protection system ID so it can happen that
// the ClearKey decryptor is chosen instead of the Thunder one for
// Widevine (and viceversa) which can can create chaos. This is an
// environment variable to set in run time if we prefer to rank higher
// Thunder or ClearKey. If we want to run tests with Thunder, we need
// to set this environment variable to Thunder and that decryptor will
// be ranked higher when there is no protection system set (as in
// WebM).
// FIXME: In https://bugs.webkit.org/show_bug.cgi?id=214826 we say we
// should migrate to use GST_PLUGIN_FEATURE_RANK but we can't yet
// because our lowest dependency is 1.16.
bool isThunderRanked()
{
    const char* value = g_getenv("WEBKIT_GST_EME_RANK_PRIORITY");
    return value && equalIgnoringASCIICase(value, "Thunder");
}
#endif

bool initializeGStreamerAndRegisterWebKitElements()
{
    if (!initializeGStreamer())
        return false;

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if ENABLE(ENCRYPTED_MEDIA)
        gst_element_register(nullptr, "webkitclearkey", GST_RANK_PRIMARY + 200, WEBKIT_TYPE_MEDIA_CK_DECRYPT);
#if ENABLE(THUNDER)
        unsigned thunderRank = isThunderRanked() ? 300 : 100;
        gst_element_register(nullptr, "webkitthunder", GST_RANK_PRIMARY + thunderRank, WEBKIT_TYPE_MEDIA_THUNDER_DECRYPT);
#endif
#endif

#if ENABLE(MEDIA_STREAM)
        gst_element_register(nullptr, "mediastreamsrc", GST_RANK_PRIMARY, WEBKIT_TYPE_MEDIA_STREAM_SRC);
#endif

#if ENABLE(MEDIA_SOURCE)
        gst_element_register(nullptr, "webkitmediasrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_MEDIA_SRC);
#endif

#if ENABLE(VIDEO)
        gst_element_register(0, "webkitwebsrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_WEB_SRC);
#if USE(GSTREAMER_GL)
        gst_element_register(0, "webkitglvideosink", GST_RANK_PRIMARY, WEBKIT_TYPE_GL_VIDEO_SINK);
#endif
#endif
    });
    return true;
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

static void simpleBusMessageCallback(GstBus*, GstMessage* message, GstBin* pipeline)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
        GST_ERROR_OBJECT(pipeline, "Got message: %" GST_PTR_FORMAT, message);
        {
            WTF::String dotFileName = makeString(GST_OBJECT_NAME(pipeline), "_error");
            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(pipeline, GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
        }
        break;
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(pipeline)) {
            GstState oldState, newState, pending;
            gst_message_parse_state_changed(message, &oldState, &newState, &pending);

            GST_INFO_OBJECT(pipeline, "State changed (old: %s, new: %s, pending: %s)",
                gst_element_state_get_name(oldState),
                gst_element_state_get_name(newState),
                gst_element_state_get_name(pending));

            WTF::String dotFileName = makeString(
                GST_OBJECT_NAME(pipeline), '_',
                gst_element_state_get_name(oldState), '_',
                gst_element_state_get_name(newState));

            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
        }
        break;
    default:
        break;
    }
}

void disconnectSimpleBusMessageCallback(GstElement* pipeline)
{
    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(pipeline)));
    g_signal_handlers_disconnect_by_func(bus.get(), reinterpret_cast<gpointer>(simpleBusMessageCallback), pipeline);
}

void connectSimpleBusMessageCallback(GstElement* pipeline)
{
    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(pipeline)));
    gst_bus_add_signal_watch_full(bus.get(), RunLoopSourcePriority::RunLoopDispatcher);
    g_signal_connect(bus.get(), "message", G_CALLBACK(simpleBusMessageCallback), pipeline);
}

Vector<uint8_t> GstMappedBuffer::createVector() const
{
    Vector<uint8_t> vector;
    vector.append(data(), size());
    return vector;
}

Ref<SharedBuffer> GstMappedOwnedBuffer::createSharedBuffer()
{
    return SharedBuffer::create(*this);
}

bool isGStreamerPluginAvailable(const char* name)
{
    GRefPtr<GstPlugin> plugin = adoptGRef(gst_registry_find_plugin(gst_registry_get(), name));
    if (!plugin)
        GST_WARNING("Plugin %s not found. Please check your GStreamer installation", name);
    return plugin;
}

}

#endif // USE(GSTREAMER)
