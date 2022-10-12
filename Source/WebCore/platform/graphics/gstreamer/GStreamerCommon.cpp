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

#include "AppSinkWorkaround.h"
#include "ApplicationGLib.h"
#include "DMABufVideoSinkGStreamer.h"
#include "GLVideoSinkGStreamer.h"
#include "GStreamerAudioMixer.h"
#include "GUniquePtrGStreamer.h"
#include "GstAllocatorFastMalloc.h"
#include "IntSize.h"
#include "RuntimeApplicationChecks.h"
#include "SharedBuffer.h"
#include "WebKitAudioSinkGStreamer.h"
#include <gst/audio/audio-info.h>
#include <gst/gst.h>
#include <mutex>
#include <wtf/FileSystem.h>
#include <wtf/PrintStream.h>
#include <wtf/Scope.h>
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
#include "GStreamerVideoEncoder.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)
#include "CDMThunder.h"
#include "WebKitThunderDecryptorGStreamer.h"
#endif

#if ENABLE(VIDEO)
#include "WebKitWebSourceGStreamer.h"
#endif

#if USE(GSTREAMER_WEBRTC)
#include <gst/webrtc/webrtc-enumtypes.h>
#endif

GST_DEBUG_CATEGORY(webkit_gst_common_debug);
#define GST_CAT_DEFAULT webkit_gst_common_debug

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

#if !GST_CHECK_VERSION(1, 18, 0)
void webkitGstVideoFormatInfoComponent(const GstVideoFormatInfo* info, guint plane, gint components[GST_VIDEO_MAX_COMPONENTS])
{
    guint c, i = 0;

    /* Reverse mapping of info->plane. */
    for (c = 0; c < GST_VIDEO_FORMAT_INFO_N_COMPONENTS(info); c++) {
        if (GST_VIDEO_FORMAT_INFO_PLANE(info, c) == plane) {
            components[i] = c;
            i++;
        }
    }

    for (c = i; c < GST_VIDEO_MAX_COMPONENTS; c++)
        components[c] = -1;
}
#endif

#if ENABLE(VIDEO)
bool getVideoSizeAndFormatFromCaps(const GstCaps* caps, WebCore::IntSize& size, GstVideoFormat& format, int& pixelAspectRatioNumerator, int& pixelAspectRatioDenominator, int& stride)
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
        int width = 0, height = 0;
        gst_structure_get_int(structure, "width", &width);
        gst_structure_get_int(structure, "height", &height);
        if (!gst_structure_get_fraction(structure, "pixel-aspect-ratio", &pixelAspectRatioNumerator, &pixelAspectRatioDenominator)) {
            pixelAspectRatioNumerator = 1;
            pixelAspectRatioDenominator = 1;
        }

        size.setWidth(width);
        size.setHeight(height);
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
    if (!areEncryptedCaps(caps) && (!gst_structure_has_name(structure, "video/x-raw") || gst_structure_has_field(structure, "format"))) {
        GstVideoInfo info;
        gst_video_info_init(&info);
        if (!gst_video_info_from_caps(&info, caps))
            return std::nullopt;

        width = GST_VIDEO_INFO_WIDTH(&info);
        height = GST_VIDEO_INFO_HEIGHT(&info);
        pixelAspectRatioNumerator = GST_VIDEO_INFO_PAR_N(&info);
        pixelAspectRatioDenominator = GST_VIDEO_INFO_PAR_D(&info);
    } else {
        gst_structure_get_int(structure, "width", &width);
        gst_structure_get_int(structure, "height", &height);
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
    if (gst_structure_has_name(structure, "application/x-cenc") || gst_structure_has_name(structure, "application/x-cbcs") || gst_structure_has_name(structure, "application/x-webm-enc"))
        return gst_structure_get_string(structure, "original-media-type");
#endif
    if (gst_structure_has_name(structure, "application/x-rtp"))
        return gst_structure_get_string(structure, "media");

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

static std::optional<Vector<String>> s_UIProcessCommandLineOptions;
void setGStreamerOptionsFromUIProcess(Vector<String>&& options)
{
    s_UIProcessCommandLineOptions = WTFMove(options);
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
        if (item.startsWith("--gst"_s))
            options.append(item.toString());
    });
    return options;
}

bool ensureGStreamerInitialized()
{
    RELEASE_ASSERT(isInWebProcess());
    static std::once_flag onceFlag;
    static bool isGStreamerInitialized;
    std::call_once(onceFlag, [] {
        isGStreamerInitialized = false;

        // USE_PLAYBIN3 is dangerous for us because its potential sneaky effect
        // is to register the playbin3 element under the playbin namespace. We
        // can't allow this, when we create playbin, we want playbin2, not
        // playbin3.
        if (g_getenv("USE_PLAYBIN3"))
            WTFLogAlways("The USE_PLAYBIN3 variable was detected in the environment. Expect playback issues or please unset it.");

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
        Vector<String> parameters = s_UIProcessCommandLineOptions.value_or(extractGStreamerOptionsFromCommandLine());
        s_UIProcessCommandLineOptions.reset();
        char** argv = g_new0(char*, parameters.size() + 2);
        int argc = parameters.size() + 1;
        argv[0] = g_strdup(FileSystem::currentExecutableName().data());
        for (unsigned i = 0; i < parameters.size(); i++)
            argv[i + 1] = g_strdup(parameters[i].utf8().data());

        GUniqueOutPtr<GError> error;
        isGStreamerInitialized = gst_init_check(&argc, &argv, &error.outPtr());
        ASSERT_WITH_MESSAGE(isGStreamerInitialized, "GStreamer initialization failed: %s", error ? error->message : "unknown error occurred");
        g_strfreev(argv);
        GST_DEBUG_CATEGORY_INIT(webkit_gst_common_debug, "webkitcommon", 0, "WebKit Common utilities");

        if (isFastMallocEnabled()) {
            const char* disableFastMalloc = getenv("WEBKIT_GST_DISABLE_FAST_MALLOC");
            if (!disableFastMalloc || !strcmp(disableFastMalloc, "0"))
                gst_allocator_set_default(GST_ALLOCATOR(g_object_new(gst_allocator_fast_malloc_get_type(), nullptr)));
        }

#if USE(GSTREAMER_MPEGTS)
        if (isGStreamerInitialized)
            gst_mpegts_initialize();
#endif

        // Workaround for https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/2413
        registerAppsinkWorkaroundIfNeeded();
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
    return value && equalIgnoringASCIICase(value, "Thunder"_s);
}
#endif

void registerWebKitGStreamerElements()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)
        if (!CDMFactoryThunder::singleton().supportedKeySystems().isEmpty()) {
            unsigned thunderRank = isThunderRanked() ? 300 : 100;
            gst_element_register(nullptr, "webkitthunder", GST_RANK_PRIMARY + thunderRank, WEBKIT_TYPE_MEDIA_THUNDER_DECRYPT);
        }
#ifndef NDEBUG
        else if (isThunderRanked()) {
            GST_WARNING("Thunder is up-ranked as preferred decryptor but Thunder is not supporting any encryption system. Is "
                "Thunder running? Are the plugins built?");
        }
#endif

#endif

#if ENABLE(MEDIA_STREAM)
        gst_element_register(nullptr, "mediastreamsrc", GST_RANK_PRIMARY, WEBKIT_TYPE_MEDIA_STREAM_SRC);
        gst_element_register(nullptr, "webrtcvideoencoder", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_WEBRTC_VIDEO_ENCODER);
#endif

#if ENABLE(MEDIA_SOURCE)
        gst_element_register(nullptr, "webkitmediasrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_MEDIA_SRC);
#endif

#if ENABLE(VIDEO)
        gst_element_register(0, "webkitwebsrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_WEB_SRC);
        gst_element_register(0, "webkitdmabufvideosink", GST_RANK_NONE, WEBKIT_TYPE_DMABUF_VIDEO_SINK);
#if USE(GSTREAMER_GL)
        gst_element_register(0, "webkitglvideosink", GST_RANK_NONE, WEBKIT_TYPE_GL_VIDEO_SINK);
#endif
#endif
        // We don't want autoaudiosink to autoplug our sink.
        gst_element_register(0, "webkitaudiosink", GST_RANK_NONE, WEBKIT_TYPE_AUDIO_SINK);

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

        // The new demuxers based on adaptivedemux2 cannot be used in WebKit yet because this new
        // base class does not abstract away network access. They can't work in a sandboxed
        // media process, so demote their rank in order to prevent decodebin3 from auto-plugging them.
        if (webkitGstCheckVersion(1, 21, 0)) {
            const char* const elementNames[] = { "dashdemux2", "hlsdemux2", "mssdemux2" };
            for (unsigned i = 0; i < G_N_ELEMENTS(elementNames); i++) {
                if (auto factory = adoptGRef(gst_element_factory_find(elementNames[i])))
                    gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE_CAST(factory.get()), GST_RANK_NONE);
            }
        }

        // The VAAPI plugin is not much maintained anymore and prone to rendering issues. In the
        // mid-term we will leverage the new stateless VA decoders. Disable the legacy plugin,
        // unless the WEBKIT_GST_ENABLE_LEGACY_VAAPI environment variable is set to 1.
        const char* enableLegacyVAAPIPlugin = getenv("WEBKIT_GST_ENABLE_LEGACY_VAAPI");
        if (!enableLegacyVAAPIPlugin || !strcmp(enableLegacyVAAPIPlugin, "0")) {
            auto* registry = gst_registry_get();
            if (auto vaapiPlugin = adoptGRef(gst_registry_find_plugin(registry, "vaapi")))
                gst_registry_remove_plugin(registry, vaapiPlugin.get());
        }
    });
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

void disconnectSimpleBusMessageCallback(GstElement* pipeline)
{
    auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(pipeline)));
    g_signal_handlers_disconnect_by_data(bus.get(), pipeline);
    gst_bus_remove_signal_watch(bus.get());
}

struct CustomMessageHandlerHolder {
    explicit CustomMessageHandlerHolder(Function<void(GstMessage*)>&& handler)
    {
        this->handler = WTFMove(handler);
    }
    Function<void(GstMessage*)> handler;
};

void connectSimpleBusMessageCallback(GstElement* pipeline, Function<void(GstMessage*)>&& customHandler)
{
    auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(pipeline)));
    gst_bus_add_signal_watch_full(bus.get(), RunLoopSourcePriority::RunLoopDispatcher);

    auto* holder = new CustomMessageHandlerHolder(WTFMove(customHandler));
    GQuark quark = g_quark_from_static_string("pipeline-custom-message-handler");
    g_object_set_qdata_full(G_OBJECT(pipeline), quark, holder, [](gpointer data) {
        delete reinterpret_cast<CustomMessageHandlerHolder*>(data);
    });

    g_signal_connect(bus.get(), "message", G_CALLBACK(+[](GstBus*, GstMessage* message, GstElement* pipeline) {
        switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GST_ERROR_OBJECT(pipeline, "Got message: %" GST_PTR_FORMAT, message);
            auto dotFileName = makeString(GST_OBJECT_NAME(pipeline), "_error");
            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(message) != GST_OBJECT_CAST(pipeline))
                break;

            GstState oldState;
            GstState newState;
            GstState pending;
            gst_message_parse_state_changed(message, &oldState, &newState, &pending);

            GST_INFO_OBJECT(pipeline, "State changed (old: %s, new: %s, pending: %s)", gst_element_state_get_name(oldState),
                gst_element_state_get_name(newState), gst_element_state_get_name(pending));

            auto dotFileName = makeString(GST_OBJECT_NAME(pipeline), '_', gst_element_state_get_name(oldState), '_', gst_element_state_get_name(newState));
            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
            break;
        }
        default:
            break;
        }

        GQuark quark = g_quark_from_static_string("pipeline-custom-message-handler");
        auto* holder = reinterpret_cast<CustomMessageHandlerHolder*>(g_object_get_qdata(G_OBJECT(pipeline), quark));
        if (!holder)
            return;

        holder->handler(message);
    }), pipeline);
}

Vector<uint8_t> GstMappedBuffer::createVector() const
{
    return { data(), size() };
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

bool gstElementFactoryEquals(GstElement* element, ASCIILiteral name)
{
    return name == GST_OBJECT_NAME(gst_element_get_factory(element));
}

GstElement* createAutoAudioSink(const String& role)
{
    auto* audioSink = makeGStreamerElement("autoaudiosink", nullptr);
    g_signal_connect_data(audioSink, "child-added", G_CALLBACK(+[](GstChildProxy*, GObject* object, gchar*, gpointer userData) {
        auto* role = reinterpret_cast<StringImpl*>(userData);
        auto* objectClass = G_OBJECT_GET_CLASS(object);
        if (role && g_object_class_find_property(objectClass, "stream-properties")) {
            GUniquePtr<GstStructure> properties(gst_structure_new("stream-properties", "media.role", G_TYPE_STRING, role->utf8().data(), nullptr));
            g_object_set(object, "stream-properties", properties.get(), nullptr);
            GST_DEBUG("Set media.role as %s on %" GST_PTR_FORMAT, role->utf8().data(), GST_ELEMENT_CAST(object));
        }
        if (g_object_class_find_property(objectClass, "client-name")) {
            auto& clientName = getApplicationName();
            g_object_set(object, "client-name", clientName.ascii().data(), nullptr);
        }
    }), role.isolatedCopy().releaseImpl().leakRef(), static_cast<GClosureNotify>([](gpointer userData, GClosure*) {
        reinterpret_cast<StringImpl*>(userData)->deref();
    }), static_cast<GConnectFlags>(0));
    return audioSink;
}

GstElement* createPlatformAudioSink(const String& role)
{
    GstElement* audioSink = webkitAudioSinkNew();
    if (!audioSink) {
        // This means the WebKit audio sink configuration failed. It can happen for the following reasons:
        // - audio mixing was not requested using the WEBKIT_GST_ENABLE_AUDIO_MIXER
        // - audio mixing was requested using the WEBKIT_GST_ENABLE_AUDIO_MIXER but the audio mixer
        //   runtime requirements are not fullfilled.
        // - the sink was created for the WPE port, audio mixing was not requested and no
        //   WPEBackend-FDO audio receiver has been registered at runtime.
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
        GstState currentState;
        auto result = gst_element_get_state(pipeline, &currentState, nullptr, 0);
        GST_DEBUG_OBJECT(pipeline, "Task finished, result: %s, target state reached: %s", gst_element_state_change_return_get_name(result), boolForPrinting(currentState == targetState));
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

GstBuffer* gstBufferNewWrappedFast(void* data, size_t length)
{
    return gst_buffer_new_wrapped_full(static_cast<GstMemoryFlags>(0), data, length, 0, length, data, fastFree);
}

GstElement* makeGStreamerElement(const char* factoryName, const char* name)
{
    auto* element = gst_element_factory_make(factoryName, name);
    if (!element)
        WTFLogAlways("GStreamer element %s not found. Please install it", factoryName);
    return element;
}

GstElement* makeGStreamerBin(const char* description, bool ghostUnlinkedPads)
{
    GUniqueOutPtr<GError> error;
    auto* bin = gst_parse_bin_from_description(description, ghostUnlinkedPads, &error.outPtr());
    if (!bin)
        WTFLogAlways("Unable to create bin for description: \"%s\". Error: %s", description, error->message);
    return bin;
}

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
    auto valueType = G_VALUE_TYPE(value);
    if (valueType == G_TYPE_BOOLEAN)
        return JSON::Value::create(g_value_get_boolean(value))->asValue();

    if (valueType == G_TYPE_INT)
        return JSON::Value::create(g_value_get_int(value))->asValue();

    if (valueType == G_TYPE_UINT)
        return JSON::Value::create(static_cast<int>(g_value_get_uint(value)))->asValue();

    if (valueType == G_TYPE_DOUBLE)
        return JSON::Value::create(g_value_get_double(value))->asValue();

    if (valueType == G_TYPE_FLOAT)
        return JSON::Value::create(static_cast<double>(g_value_get_float(value)))->asValue();

    // FIXME: bigint support missing in JSON.
    if (valueType == G_TYPE_UINT64)
        return JSON::Value::create(static_cast<int>(g_value_get_uint64(value)))->asValue();

    if (valueType == G_TYPE_STRING)
        return JSON::Value::create(makeString(g_value_get_string(value)))->asValue();

#if USE(GSTREAMER_WEBRTC)
    if (valueType == GST_TYPE_WEBRTC_STATS_TYPE) {
        GUniquePtr<char> statsType(g_enum_to_string(GST_TYPE_WEBRTC_STATS_TYPE, g_value_get_enum(value)));
        return JSON::Value::create(makeString(statsType.get()))->asValue();
    }
#endif

    GST_WARNING("Unhandled GValue type: %s", G_VALUE_TYPE_NAME(value));
    return { };
}

static gboolean parseGstStructureValue(GQuark fieldId, const GValue* value, gpointer userData)
{
    if (auto jsonValue = gstStructureValueToJSON(value)) {
        auto* object = reinterpret_cast<JSON::Object*>(userData);
        object->setValue(String::fromLatin1(g_quark_to_string(fieldId)), jsonValue->releaseNonNull());
    }
    return TRUE;
}

static RefPtr<JSON::Value> gstStructureToJSON(const GstStructure* structure)
{
    auto jsonObject = JSON::Object::create();
    auto resultValue = jsonObject->asObject();
    if (!resultValue)
        return nullptr;

    gst_structure_foreach(structure, parseGstStructureValue, resultValue.get());
    return resultValue;
}

String gstStructureToJSONString(const GstStructure* structure)
{
    auto value = gstStructureToJSON(structure);
    if (!value)
        return { };
    return value->toJSONString();
}

#if !GST_CHECK_VERSION(1, 18, 0)
GstClockTime webkitGstElementGetCurrentRunningTime(GstElement* element)
{
    g_return_val_if_fail(GST_IS_ELEMENT(element), GST_CLOCK_TIME_NONE);

    auto baseTime = gst_element_get_base_time(element);
    if (!GST_CLOCK_TIME_IS_VALID(baseTime)) {
        GST_DEBUG_OBJECT(element, "Could not determine base time");
        return GST_CLOCK_TIME_NONE;
    }

    auto clock = adoptGRef(gst_element_get_clock(element));
    if (!clock) {
        GST_DEBUG_OBJECT(element, "Element has no clock");
        return GST_CLOCK_TIME_NONE;
    }

    auto clockTime = gst_clock_get_time(clock.get());
    if (!GST_CLOCK_TIME_IS_VALID(clockTime))
        return GST_CLOCK_TIME_NONE;

    if (clockTime < baseTime) {
        GST_DEBUG_OBJECT(element, "Got negative current running time");
        return GST_CLOCK_TIME_NONE;
    }

    return clockTime - baseTime;
}
#endif

}

#endif // USE(GSTREAMER)
