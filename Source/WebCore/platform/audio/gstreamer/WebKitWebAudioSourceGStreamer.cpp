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
#include "AudioDestinationGStreamer.h"
#include "AudioIOCallback.h"
#include "AudioUtilities.h"
#include "GStreamerCommon.h"
#include <gst/app/gstappsrc.h>
#include <gst/audio/audio-info.h>
#include <gst/pbutils/missing-plugins.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

typedef struct _WebKitWebAudioSrcClass   WebKitWebAudioSrcClass;
typedef struct _WebKitWebAudioSrcPrivate WebKitWebAudioSrcPrivate;

struct _WebKitWebAudioSrc {
    GstBin parent;

    WebKitWebAudioSrcPrivate* priv;
};

struct _WebKitWebAudioSrcClass {
    GstBinClass parentClass;
};

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(GST_AUDIO_CAPS_MAKE(GST_AUDIO_NE(F32))));

struct _WebKitWebAudioSrcPrivate {
    gfloat sampleRate;
    RefPtr<AudioBus> bus;
    AudioDestinationGStreamer* destination;
    guint framesToPull;
    guint bufferSize;

    GRefPtr<GstTask> task;
    GRecMutex mutex;

    GRefPtr<GstElement> source;
    GRefPtr<GstCaps> caps;
    GstAudioInfo info;

    // src pad of the element, interleaved wav data is pushed to it.
    GstPad* sourcePad;

    guint64 numberOfSamples;

    GRefPtr<GstBufferPool> pool;

    bool hasRenderedAudibleFrame { false };

    Lock dispatchToRenderThreadLock;
    Function<void(Function<void()>&&)> dispatchToRenderThreadFunction WTF_GUARDED_BY_LOCK(dispatchToRenderThreadLock);

    bool dispatchDone WTF_GUARDED_BY_LOCK(dispatchLock);
    Lock dispatchLock;
    Condition dispatchCondition;

    _WebKitWebAudioSrcPrivate()
    {
        sourcePad = webkitGstGhostPadFromStaticTemplate(&srcTemplate, "src", nullptr);

        g_rec_mutex_init(&mutex);
    }

    ~_WebKitWebAudioSrcPrivate()
    {
        g_rec_mutex_clear(&mutex);
    }
};

enum {
    PROP_RATE = 1,
    PROP_DESTINATION,
    PROP_FRAMES
};

GST_DEBUG_CATEGORY_STATIC(webkit_web_audio_src_debug);
#define GST_CAT_DEFAULT webkit_web_audio_src_debug

static void webKitWebAudioSrcConstructed(GObject*);
static void webKitWebAudioSrcSetProperty(GObject*, guint propertyId, const GValue*, GParamSpec*);
static void webKitWebAudioSrcGetProperty(GObject*, guint propertyId, GValue*, GParamSpec*);
static GstStateChangeReturn webKitWebAudioSrcChangeState(GstElement*, GstStateChange);
static void webKitWebAudioSrcRenderIteration(WebKitWebAudioSrc*);

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

static GstCaps* getGStreamerAudioCaps(float sampleRate, unsigned numberOfChannels)
{
    guint64 channelMask = 0;
    Vector<GstAudioChannelPosition> positions(numberOfChannels, [&](size_t channelIndex) -> GstAudioChannelPosition {
        return webKitWebAudioGStreamerChannelPosition(channelIndex);
    });

    gst_audio_channel_positions_to_mask(reinterpret_cast<GstAudioChannelPosition*>(positions.data()),
        numberOfChannels, FALSE, &channelMask);

    return gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, static_cast<int>(sampleRate),
        "channels", G_TYPE_INT, static_cast<int>(numberOfChannels),
        "channel-mask", GST_TYPE_BITMASK, channelMask,
        "format", G_TYPE_STRING, GST_AUDIO_NE(F32),
        "layout", G_TYPE_STRING, "non-interleaved", nullptr);
}

#define webkit_web_audio_src_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitWebAudioSrc, webkit_web_audio_src, GST_TYPE_BIN, GST_DEBUG_CATEGORY_INIT(webkit_web_audio_src_debug, "webkitwebaudiosrc", 0, "webaudiosrc element"))

static void webkit_web_audio_src_class_init(WebKitWebAudioSrcClass* webKitWebAudioSrcClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(webKitWebAudioSrcClass);
    GstElementClass* elementClass = GST_ELEMENT_CLASS(webKitWebAudioSrcClass);

    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&srcTemplate));
    gst_element_class_set_metadata(elementClass, "WebKit WebAudio source element", "Source", "Handles WebAudio data from WebCore", "Philippe Normand <pnormand@igalia.com>");

    objectClass->constructed = webKitWebAudioSrcConstructed;
    elementClass->change_state = webKitWebAudioSrcChangeState;

    objectClass->set_property = webKitWebAudioSrcSetProperty;
    objectClass->get_property = webKitWebAudioSrcGetProperty;

    GParamFlags flags = static_cast<GParamFlags>(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
    g_object_class_install_property(objectClass,
                                    PROP_RATE,
                                    g_param_spec_float("rate",
                                                       nullptr, nullptr,
                                                       G_MINDOUBLE, G_MAXDOUBLE,
                                                       44100.0, flags));

    g_object_class_install_property(objectClass, PROP_DESTINATION, g_param_spec_pointer("destination", "destination", "Destination", G_PARAM_READWRITE));

    g_object_class_install_property(objectClass,
                                    PROP_FRAMES,
                                    g_param_spec_uint("frames",
                                                      nullptr, nullptr,
                                                      0, G_MAXUINT8, AudioUtilities::renderQuantumSize, flags));
}

static void webKitWebAudioSrcConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));

    WebKitWebAudioSrc* src = WEBKIT_WEB_AUDIO_SRC(object);
    WebKitWebAudioSrcPrivate* priv = src->priv;

    ASSERT(priv->sampleRate);

    GST_OBJECT_FLAG_SET(GST_OBJECT_CAST(src), GST_ELEMENT_FLAG_SOURCE);
    gst_bin_set_suppressed_flags(GST_BIN_CAST(src), static_cast<GstElementFlags>(GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK));

    gst_element_add_pad(GST_ELEMENT(src), priv->sourcePad);

    priv->task = adoptGRef(gst_task_new(reinterpret_cast<GstTaskFunction>(webKitWebAudioSrcRenderIteration), src, nullptr));
    gst_task_set_lock(priv->task.get(), &priv->mutex);

    priv->source = makeGStreamerElement("appsrc", "webaudioSrc");

    // Configure the appsrc for minimal latency.
    g_object_set(priv->source.get(), "block", TRUE, "blocksize", priv->bufferSize, "format", GST_FORMAT_TIME, "is-live", TRUE, nullptr);

    gst_bin_add(GST_BIN(src), priv->source.get());
    // appsrc's src pad is the only visible pad of our element.
    GRefPtr<GstPad> targetPad = adoptGRef(gst_element_get_static_pad(priv->source.get(), "src"));
    gst_ghost_pad_set_target(GST_GHOST_PAD(priv->sourcePad), targetPad.get());
}

static void webKitWebAudioSrcSetProperty(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitWebAudioSrc* src = WEBKIT_WEB_AUDIO_SRC(object);
    WebKitWebAudioSrcPrivate* priv = src->priv;

    switch (propertyId) {
    case PROP_RATE:
        priv->sampleRate = g_value_get_float(value);
        break;
    case PROP_DESTINATION:
        priv->destination = static_cast<AudioDestinationGStreamer*>(g_value_get_pointer(value));
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
    WebKitWebAudioSrcPrivate* priv = src->priv;

    switch (propertyId) {
    case PROP_RATE:
        g_value_set_float(value, priv->sampleRate);
        break;
    case PROP_DESTINATION:
        g_value_set_pointer(value, priv->destination);
        break;
    case PROP_FRAMES:
        g_value_set_uint(value, priv->framesToPull);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GRefPtr<GstBuffer> webKitWebAudioSrcAllocateBuffer(WebKitWebAudioSrc* src)
{
    WebKitWebAudioSrcPrivate* priv = src->priv;

    ASSERT(priv->bus);
    ASSERT(priv->destination);
    if (!priv->destination || !priv->bus) {
        GST_ELEMENT_ERROR(src, CORE, FAILED, ("Internal WebAudioSrc error"), ("Can't start without destination or bus"));
        gst_task_stop(src->priv->task.get());
        return nullptr;
    }

    ASSERT(priv->pool);

    GRefPtr<GstBuffer> buffer;
    GstFlowReturn ret = gst_buffer_pool_acquire_buffer(priv->pool.get(), &buffer.outPtr(), nullptr);
    if (ret != GST_FLOW_OK) {
        // FLUSHING and EOS are not errors.
        if (ret < GST_FLOW_EOS || ret == GST_FLOW_NOT_LINKED)
            GST_ELEMENT_ERROR(src, CORE, PAD, ("Internal WebAudioSrc error"), ("Failed to allocate buffer for flow: %s", gst_flow_get_name(ret)));
        return nullptr;
    }

    ASSERT(buffer);
    gst_buffer_add_audio_meta(buffer.get(), &priv->info, priv->framesToPull, nullptr);

    {
        GstMappedBuffer mappedBuffer(buffer.get(), GST_MAP_READ);
        ASSERT(mappedBuffer);
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
        for (unsigned channelIndex = 0; channelIndex < priv->bus->numberOfChannels(); channelIndex++)
            priv->bus->setChannelMemory(channelIndex, spanReinterpretCast<float>(mappedBuffer.mutableSpan().subspan(channelIndex * priv->bufferSize)).first(priv->framesToPull));
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    return buffer;
}

static void webKitWebAudioSrcRenderAndPushFrames(const GRefPtr<GstElement>& element, GRefPtr<GstBuffer>&& buffer)
{
    auto* src = WEBKIT_WEB_AUDIO_SRC(element.get());
    auto* priv = src->priv;

    auto notifyDispatchOnExit = makeScopeExit([priv] {
        Locker locker { priv->dispatchLock };
        priv->dispatchDone = true;
        priv->dispatchCondition.notifyOne();
    });

    if (GST_STATE(element.get()) < GST_STATE_PAUSED)
        return;

    if (!priv->destination)
        return;

    GST_TRACE_OBJECT(element.get(), "Playing: %d", priv->destination->isPlaying());
    if (priv->hasRenderedAudibleFrame && !priv->destination->isPlaying())
        return;

    GstClockTime timestamp = gst_util_uint64_scale(priv->numberOfSamples, GST_SECOND, priv->sampleRate);
    priv->numberOfSamples += priv->framesToPull;
    GstClockTime duration = gst_util_uint64_scale(priv->framesToPull, GST_SECOND, priv->sampleRate);

    AudioIOPosition outputTimestamp;
    outputTimestamp.position = Seconds::fromNanoseconds(timestamp);
    auto now = static_cast<double>((g_get_monotonic_time() + GST_TIME_AS_USECONDS(timestamp)) / 1000000.0);
    outputTimestamp.timestamp = MonotonicTime::fromRawSeconds(now);

    // FIXME: Add support for local/live audio input.
    if (priv->bus)
        priv->destination->callRenderCallback(nullptr, priv->bus.get(), priv->framesToPull, outputTimestamp);

    if (!priv->hasRenderedAudibleFrame && !priv->bus->isSilent()) {
        priv->destination->notifyIsPlaying(true);
        priv->hasRenderedAudibleFrame = true;
    }

    GST_BUFFER_TIMESTAMP(buffer.get()) = timestamp;
    GST_BUFFER_DURATION(buffer.get()) = duration;

    if (priv->bus->isSilent()) {
        GST_BUFFER_FLAG_SET(buffer.get(), GST_BUFFER_FLAG_GAP);
        GST_BUFFER_FLAG_SET(buffer.get(), GST_BUFFER_FLAG_DROPPABLE);
    }

    // Leak the buffer ref, because gst_app_src_push_buffer steals it.
    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(priv->source.get()), buffer.leakRef());
    if (ret != GST_FLOW_OK) {
        // FLUSHING and EOS are not errors.
        if (ret < GST_FLOW_EOS || ret == GST_FLOW_NOT_LINKED)
            GST_ELEMENT_ERROR(src, CORE, PAD, ("Internal WebAudioSrc error"), ("Failed to push buffer on %s flow: %s", GST_OBJECT_NAME(priv->source.get()), gst_flow_get_name(ret)));
        gst_task_stop(priv->task.get());
    }
}

static void webKitWebAudioSrcRenderIteration(WebKitWebAudioSrc* src)
{
    auto* priv = src->priv;
    auto buffer = webKitWebAudioSrcAllocateBuffer(src);
    if (!buffer) {
        gst_task_stop(priv->task.get());
        return;
    }

    {
        Locker locker { priv->dispatchLock };
        priv->dispatchDone = false;
    }

    if (!priv->dispatchToRenderThreadLock.tryLock())
        return;

    Locker locker { AdoptLock, priv->dispatchToRenderThreadLock };

    if (!priv->dispatchToRenderThreadFunction)
        webKitWebAudioSrcRenderAndPushFrames(GRefPtr<GstElement>(GST_ELEMENT_CAST(src)), WTFMove(buffer));
    else {
        priv->dispatchToRenderThreadFunction([buffer = WTFMove(buffer), protectedThis = GRefPtr<GstElement>(GST_ELEMENT_CAST(src))]() mutable {
            if (!protectedThis)
                return;
            webKitWebAudioSrcRenderAndPushFrames(protectedThis, WTFMove(buffer));
        });
    }

    {
        Locker locker { priv->dispatchLock };
        if (!priv->dispatchDone)
            priv->dispatchCondition.wait(priv->dispatchLock);
    }
}

static GstStateChangeReturn webKitWebAudioSrcChangeState(GstElement* element, GstStateChange transition)
{
    GstStateChangeReturn returnValue = GST_STATE_CHANGE_SUCCESS;
    auto* src = WEBKIT_WEB_AUDIO_SRC(element);
    auto* priv = src->priv;

    GST_DEBUG_OBJECT(element, "%s", gst_state_change_get_name(transition));

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        priv->numberOfSamples = 0;
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED: {
        priv->pool = adoptGRef(gst_buffer_pool_new());
        GstStructure* config = gst_buffer_pool_get_config(priv->pool.get());
        gst_buffer_pool_config_set_params(config, nullptr, priv->bufferSize * priv->bus->numberOfChannels(), 0, 0);
        gst_buffer_pool_set_config(priv->pool.get(), config);
        if (!gst_buffer_pool_set_active(priv->pool.get(), TRUE))
            return GST_STATE_CHANGE_FAILURE;
        webKitWebAudioSrcRenderIteration(src);
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING: {
        if (!gst_task_start(priv->task.get()))
            return GST_STATE_CHANGE_FAILURE;
        break;
    }
    default:
        break;
    }

    returnValue = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (UNLIKELY(returnValue == GST_STATE_CHANGE_FAILURE)) {
        GST_DEBUG_OBJECT(src, "State change failed");
        return returnValue;
    }

    switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        {
            Locker locker { priv->dispatchLock };
            priv->dispatchCondition.notifyAll();
        }
        gst_buffer_pool_set_flushing(priv->pool.get(), TRUE);
        if (!gst_task_join(priv->task.get()))
            returnValue = GST_STATE_CHANGE_FAILURE;

        gst_buffer_pool_set_active(priv->pool.get(), FALSE);
        priv->pool = nullptr;
        priv->hasRenderedAudibleFrame = false;
        break;
    default:
        break;
    }

    return returnValue;
}

void webkitWebAudioSourceSetBus(WebKitWebAudioSrc* src, RefPtr<AudioBus> bus)
{
    auto priv = src->priv;
    priv->bus = bus;
    priv->caps = adoptGRef(getGStreamerAudioCaps(priv->sampleRate, priv->bus->numberOfChannels()));
    gst_audio_info_from_caps(&priv->info, priv->caps.get());
    g_object_set(priv->source.get(), "max-bytes", static_cast<guint64>(2 * priv->bufferSize * priv->bus->numberOfChannels()), "caps", priv->caps.get(), nullptr);
}

void webkitWebAudioSourceSetDispatchToRenderThreadFunction(WebKitWebAudioSrc* src, Function<void(Function<void()>&&)>&& function)
{
    Locker locker { src->priv->dispatchToRenderThreadLock };
    src->priv->dispatchToRenderThreadFunction = WTFMove(function);
}

#undef GST_CAT_DEFAULT

#endif // ENABLE(WEB_AUDIO) && USE(GSTREAMER)
