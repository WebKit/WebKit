/*
 *  Copyright (C) 2011, 2012 Igalia S.L
 *  Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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

#include "WebKitWebAudioSourceGStreamer.h"

#if ENABLE(WEB_AUDIO) && USE(GSTREAMER)

#include "AudioBus.h"
#include "AudioIOCallback.h"
#include "GStreamerCommon.h"
#include <gst/app/gstappsrc.h>
#include <gst/audio/audio-info.h>
#include <gst/pbutils/missing-plugins.h>
#include <wtf/glib/GUniquePtr.h>

using namespace WebCore;

typedef struct _WebKitWebAudioSrcClass   WebKitWebAudioSrcClass;
typedef struct _WebKitWebAudioSourcePrivate WebKitWebAudioSourcePrivate;

struct _WebKitWebAudioSrc {
    GstBin parent;

    WebKitWebAudioSourcePrivate* priv;
};

struct _WebKitWebAudioSrcClass {
    GstBinClass parentClass;
};

#define WEBKIT_WEB_AUDIO_SRC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEBAUDIO_SRC, WebKitWebAudioSourcePrivate))
struct _WebKitWebAudioSourcePrivate {
    gfloat sampleRate;
    AudioBus* bus;
    AudioIOCallback* provider;
    guint framesToPull;
    guint bufferSize;

    GRefPtr<GstElement> interleave;

    GRefPtr<GstTask> task;
    GRecMutex mutex;

    // List of appsrc. One appsrc for each planar audio channel.
    Vector<GRefPtr<GstElement>> sources;

    // src pad of the element, interleaved wav data is pushed to it.
    GstPad* sourcePad;

    guint64 numberOfSamples;

    GRefPtr<GstBufferPool> pool;

    bool enableGapBufferSupport;
};

enum {
    PROP_RATE = 1,
    PROP_BUS,
    PROP_PROVIDER,
    PROP_FRAMES
};

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(GST_AUDIO_CAPS_MAKE(GST_AUDIO_NE(F32))));

GST_DEBUG_CATEGORY_STATIC(webkit_web_audio_src_debug);
#define GST_CAT_DEFAULT webkit_web_audio_src_debug

static void webKitWebAudioSrcConstructed(GObject*);
static void webKitWebAudioSrcFinalize(GObject*);
static void webKitWebAudioSrcSetProperty(GObject*, guint propertyId, const GValue*, GParamSpec*);
static void webKitWebAudioSrcGetProperty(GObject*, guint propertyId, GValue*, GParamSpec*);
static GstStateChangeReturn webKitWebAudioSrcChangeState(GstElement*, GstStateChange);
static void webKitWebAudioSrcLoop(WebKitWebAudioSrc*);

static GstCaps* getGStreamerMonoAudioCaps(float sampleRate)
{
    return gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, static_cast<int>(sampleRate),
        "channels", G_TYPE_INT, 1,
        "format", G_TYPE_STRING, GST_AUDIO_NE(F32),
        "layout", G_TYPE_STRING, "interleaved", nullptr);
}

static GstAudioChannelPosition webKitWebAudioGStreamerChannelPosition(int channelIndex)
{
    GstAudioChannelPosition position = GST_AUDIO_CHANNEL_POSITION_NONE;

    switch (channelIndex) {
    case AudioBus::ChannelLeft:
        position = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
        break;
    case AudioBus::ChannelRight:
        position = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
        break;
    case AudioBus::ChannelCenter:
        position = GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER;
        break;
    case AudioBus::ChannelLFE:
        position = GST_AUDIO_CHANNEL_POSITION_LFE1;
        break;
    case AudioBus::ChannelSurroundLeft:
        position = GST_AUDIO_CHANNEL_POSITION_REAR_LEFT;
        break;
    case AudioBus::ChannelSurroundRight:
        position = GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT;
        break;
    default:
        break;
    };

    return position;
}

#define webkit_web_audio_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(WebKitWebAudioSrc, webkit_web_audio_src, GST_TYPE_BIN, GST_DEBUG_CATEGORY_INIT(webkit_web_audio_src_debug, \
                            "webkitwebaudiosrc", \
                            0, \
                            "webaudiosrc element"));

static void webkit_web_audio_src_class_init(WebKitWebAudioSrcClass* webKitWebAudioSrcClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(webKitWebAudioSrcClass);
    GstElementClass* elementClass = GST_ELEMENT_CLASS(webKitWebAudioSrcClass);

    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&srcTemplate));
    gst_element_class_set_metadata(elementClass, "WebKit WebAudio source element", "Source", "Handles WebAudio data from WebCore", "Philippe Normand <pnormand@igalia.com>");

    objectClass->constructed = webKitWebAudioSrcConstructed;
    objectClass->finalize = webKitWebAudioSrcFinalize;
    elementClass->change_state = webKitWebAudioSrcChangeState;

    objectClass->set_property = webKitWebAudioSrcSetProperty;
    objectClass->get_property = webKitWebAudioSrcGetProperty;

    GParamFlags flags = static_cast<GParamFlags>(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
    g_object_class_install_property(objectClass,
                                    PROP_RATE,
                                    g_param_spec_float("rate", "rate",
                                                       "Sample rate", G_MINDOUBLE, G_MAXDOUBLE,
                                                       44100.0, flags));

    g_object_class_install_property(objectClass,
                                    PROP_BUS,
                                    g_param_spec_pointer("bus", "bus",
                                                         "Bus", flags));

    g_object_class_install_property(objectClass,
                                    PROP_PROVIDER,
                                    g_param_spec_pointer("provider", "provider",
                                                         "Provider", flags));

    g_object_class_install_property(objectClass,
                                    PROP_FRAMES,
                                    g_param_spec_uint("frames", "frames",
                                                      "Number of audio frames to pull at each iteration",
                                                      0, G_MAXUINT8, 128, flags));

    g_type_class_add_private(webKitWebAudioSrcClass, sizeof(WebKitWebAudioSourcePrivate));
}

static void webkit_web_audio_src_init(WebKitWebAudioSrc* src)
{
    WebKitWebAudioSourcePrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(src, WEBKIT_TYPE_WEB_AUDIO_SRC, WebKitWebAudioSourcePrivate);
    src->priv = priv;
    new (priv) WebKitWebAudioSourcePrivate();

    priv->sourcePad = webkitGstGhostPadFromStaticTemplate(&srcTemplate, "src", nullptr);
    gst_element_add_pad(GST_ELEMENT(src), priv->sourcePad);

    priv->provider = nullptr;
    priv->bus = nullptr;

    g_rec_mutex_init(&priv->mutex);
    priv->task = adoptGRef(gst_task_new(reinterpret_cast<GstTaskFunction>(webKitWebAudioSrcLoop), src, nullptr));

    // GAP buffer support is enabled only for GStreamer 1.12.5 because of a
    // memory leak that was fixed in that version.
    // https://bugzilla.gnome.org/show_bug.cgi?id=793067
    priv->enableGapBufferSupport = webkitGstCheckVersion(1, 12, 5);

    gst_task_set_lock(priv->task.get(), &priv->mutex);
}

static void webKitWebAudioSrcConstructed(GObject* object)
{
    WebKitWebAudioSrc* src = WEBKIT_WEB_AUDIO_SRC(object);
    WebKitWebAudioSourcePrivate* priv = src->priv;

    ASSERT(priv->bus);
    ASSERT(priv->provider);
    ASSERT(priv->sampleRate);

    priv->interleave = gst_element_factory_make("interleave", nullptr);

    if (!priv->interleave) {
        GST_ERROR_OBJECT(src, "Failed to create interleave");
        return;
    }

    gst_bin_add(GST_BIN(src), priv->interleave.get());

    // For each channel of the bus create a new upstream branch for interleave, like:
    // appsrc ! . which is plugged to a new interleave request sinkpad.
    for (unsigned channelIndex = 0; channelIndex < priv->bus->numberOfChannels(); channelIndex++) {
        GUniquePtr<gchar> appsrcName(g_strdup_printf("webaudioSrc%u", channelIndex));
        GRefPtr<GstElement> appsrc = gst_element_factory_make("appsrc", appsrcName.get());
        GRefPtr<GstCaps> monoCaps = adoptGRef(getGStreamerMonoAudioCaps(priv->sampleRate));

        GstAudioInfo info;
        gst_audio_info_from_caps(&info, monoCaps.get());
        GST_AUDIO_INFO_POSITION(&info, 0) = webKitWebAudioGStreamerChannelPosition(channelIndex);
        GRefPtr<GstCaps> caps = adoptGRef(gst_audio_info_to_caps(&info));

        // Configure the appsrc for minimal latency.
        g_object_set(appsrc.get(), "max-bytes", static_cast<guint64>(2 * priv->bufferSize), "block", TRUE,
            "blocksize", priv->bufferSize,
            "format", GST_FORMAT_TIME, "caps", caps.get(), nullptr);

        priv->sources.append(appsrc);

        gst_bin_add(GST_BIN(src), appsrc.get());
        gst_element_link_pads_full(appsrc.get(), "src", priv->interleave.get(), "sink_%u", GST_PAD_LINK_CHECK_NOTHING);
    }

    // interleave's src pad is the only visible pad of our element.
    GRefPtr<GstPad> targetPad = adoptGRef(gst_element_get_static_pad(priv->interleave.get(), "src"));
    gst_ghost_pad_set_target(GST_GHOST_PAD(priv->sourcePad), targetPad.get());
}

static void webKitWebAudioSrcFinalize(GObject* object)
{
    WebKitWebAudioSrc* src = WEBKIT_WEB_AUDIO_SRC(object);
    WebKitWebAudioSourcePrivate* priv = src->priv;

    g_rec_mutex_clear(&priv->mutex);

    priv->~WebKitWebAudioSourcePrivate();
    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, ((GObject* )(src)));
}

static void webKitWebAudioSrcSetProperty(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitWebAudioSrc* src = WEBKIT_WEB_AUDIO_SRC(object);
    WebKitWebAudioSourcePrivate* priv = src->priv;

    switch (propertyId) {
    case PROP_RATE:
        priv->sampleRate = g_value_get_float(value);
        break;
    case PROP_BUS:
        priv->bus = static_cast<AudioBus*>(g_value_get_pointer(value));
        break;
    case PROP_PROVIDER:
        priv->provider = static_cast<AudioIOCallback*>(g_value_get_pointer(value));
        break;
    case PROP_FRAMES:
        priv->framesToPull = g_value_get_uint(value);
        priv->bufferSize = sizeof(float) * priv->framesToPull;
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webKitWebAudioSrcGetProperty(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitWebAudioSrc* src = WEBKIT_WEB_AUDIO_SRC(object);
    WebKitWebAudioSourcePrivate* priv = src->priv;

    switch (propertyId) {
    case PROP_RATE:
        g_value_set_float(value, priv->sampleRate);
        break;
    case PROP_BUS:
        g_value_set_pointer(value, priv->bus);
        break;
    case PROP_PROVIDER:
        g_value_set_pointer(value, priv->provider);
        break;
    case PROP_FRAMES:
        g_value_set_uint(value, priv->framesToPull);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static Optional<Vector<GRefPtr<GstBuffer>>> webKitWebAudioSrcAllocateBuffersAndRenderAudio(WebKitWebAudioSrc* src)
{
    WebKitWebAudioSourcePrivate* priv = src->priv;

    ASSERT(priv->bus);
    ASSERT(priv->provider);
    if (!priv->provider || !priv->bus) {
        GST_ELEMENT_ERROR(src, CORE, FAILED, ("Internal WebAudioSrc error"), ("Can't start without provider or bus"));
        gst_task_stop(src->priv->task.get());
        return WTF::nullopt;
    }

    ASSERT(priv->pool);
    GstClockTime timestamp = gst_util_uint64_scale(priv->numberOfSamples, GST_SECOND, priv->sampleRate);
    priv->numberOfSamples += priv->framesToPull;
    GstClockTime duration = gst_util_uint64_scale(priv->numberOfSamples, GST_SECOND, priv->sampleRate) - timestamp;

    Vector<GRefPtr<GstBuffer>> channelBufferList;
    channelBufferList.reserveInitialCapacity(priv->sources.size());
    Vector<RefPtr<GstMappedBuffer>> mappedBuffers;
    mappedBuffers.reserveInitialCapacity(priv->sources.size());
    for (unsigned i = 0; i < priv->sources.size(); ++i) {
        GRefPtr<GstBuffer> buffer;
        GstFlowReturn ret = gst_buffer_pool_acquire_buffer(priv->pool.get(), &buffer.outPtr(), nullptr);
        if (ret != GST_FLOW_OK) {
            // FLUSHING and EOS are not errors.
            if (ret < GST_FLOW_EOS || ret == GST_FLOW_NOT_LINKED)
                GST_ELEMENT_ERROR(src, CORE, PAD, ("Internal WebAudioSrc error"), ("Failed to allocate buffer for flow: %s", gst_flow_get_name(ret)));
            return WTF::nullopt;
        }

        ASSERT(buffer);
        GST_BUFFER_TIMESTAMP(buffer.get()) = timestamp;
        GST_BUFFER_DURATION(buffer.get()) = duration;
        auto mappedBuffer = GstMappedBuffer::create(buffer.get(), GST_MAP_READWRITE);
        ASSERT(mappedBuffer);
        mappedBuffers.uncheckedAppend(WTFMove(mappedBuffer));
        priv->bus->setChannelMemory(i, reinterpret_cast<float*>(mappedBuffers[i]->data()), priv->framesToPull);
        channelBufferList.uncheckedAppend(WTFMove(buffer));
    }

    // FIXME: Add support for local/live audio input.
    priv->provider->render(nullptr, priv->bus, priv->framesToPull);

    return makeOptional(channelBufferList);
}

static void webKitWebAudioSrcLoop(WebKitWebAudioSrc* src)
{
    WebKitWebAudioSourcePrivate* priv = src->priv;

    Optional<Vector<GRefPtr<GstBuffer>>> channelBufferList = webKitWebAudioSrcAllocateBuffersAndRenderAudio(src);
    if (!channelBufferList) {
        gst_task_stop(src->priv->task.get());
        return;
    }

    ASSERT(channelBufferList->size() == priv->sources.size());

    bool failed = false;
    for (unsigned i = 0; i < priv->sources.size(); ++i) {
        auto& buffer = channelBufferList.value()[i];

        if (priv->enableGapBufferSupport && priv->bus->channel(i)->isSilent())
            GST_BUFFER_FLAG_SET(buffer.get(), GST_BUFFER_FLAG_GAP);

        if (failed)
            continue;

        auto& appsrc = priv->sources[i];
        // Leak the buffer ref, because gst_app_src_push_buffer steals it.
        GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc.get()), buffer.leakRef());
        if (ret != GST_FLOW_OK) {
            // FLUSHING and EOS are not errors.
            if (ret < GST_FLOW_EOS || ret == GST_FLOW_NOT_LINKED)
                GST_ELEMENT_ERROR(src, CORE, PAD, ("Internal WebAudioSrc error"), ("Failed to push buffer on %s flow: %s", GST_OBJECT_NAME(appsrc.get()), gst_flow_get_name(ret)));
            gst_task_stop(src->priv->task.get());
            failed = true;
        }
    }
}

static GstStateChangeReturn webKitWebAudioSrcChangeState(GstElement* element, GstStateChange transition)
{
    GstStateChangeReturn returnValue = GST_STATE_CHANGE_SUCCESS;
    WebKitWebAudioSrc* src = WEBKIT_WEB_AUDIO_SRC(element);

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!src->priv->interleave) {
            gst_element_post_message(element, gst_missing_element_message_new(element, "interleave"));
            GST_ELEMENT_ERROR(src, CORE, MISSING_PLUGIN, (nullptr), ("no interleave"));
            return GST_STATE_CHANGE_FAILURE;
        }
        src->priv->numberOfSamples = 0;
        break;
    default:
        break;
    }

    returnValue = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (UNLIKELY(returnValue == GST_STATE_CHANGE_FAILURE)) {
        GST_DEBUG_OBJECT(src, "State change failed");
        return returnValue;
    }

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED: {
        GST_DEBUG_OBJECT(src, "READY->PAUSED");

        src->priv->pool = gst_buffer_pool_new();
        GstStructure* config = gst_buffer_pool_get_config(src->priv->pool.get());
        gst_buffer_pool_config_set_params(config, nullptr, src->priv->bufferSize, 0, 0);
        gst_buffer_pool_set_config(src->priv->pool.get(), config);
        if (!gst_buffer_pool_set_active(src->priv->pool.get(), TRUE))
            returnValue = GST_STATE_CHANGE_FAILURE;
        else if (!gst_task_start(src->priv->task.get()))
            returnValue = GST_STATE_CHANGE_FAILURE;
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        GST_DEBUG_OBJECT(src, "PAUSED->READY");

        gst_buffer_pool_set_flushing(src->priv->pool.get(), TRUE);
        if (!gst_task_join(src->priv->task.get()))
            returnValue = GST_STATE_CHANGE_FAILURE;
        gst_buffer_pool_set_active(src->priv->pool.get(), FALSE);
        src->priv->pool = nullptr;
        break;
    default:
        break;
    }

    return returnValue;
}

#endif // ENABLE(WEB_AUDIO) && USE(GSTREAMER)
