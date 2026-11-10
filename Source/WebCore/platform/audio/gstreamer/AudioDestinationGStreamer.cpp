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

#if ENABLE(WEB_AUDIO)

#include "AudioDestinationGStreamer.h"

#include "AudioSourceProvider.h"
#include "AudioUtilities.h"
#include "GStreamerCommon.h"
#include "GStreamerQuirks.h"
#include "WebKitWebAudioSourceGStreamer.h"
#include <gst/audio/gstaudiobasesink.h>
#include <gst/gst.h>
#include <wtf/PrintStream.h>
#include <wtf/Scope.h>
#include <wtf/glib/GMallocString.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

// The audio sink buffer time (in microseconds). It is used to compute the
// maximum latency of the audio sink. It is set to 50ms by default which is a
// good tradeoff between latency and avoiding underruns.
static constexpr gint64 audioSinkBufferTime = 50000;

GST_DEBUG_CATEGORY(webkit_audio_destination_debug);
#define GST_CAT_DEFAULT webkit_audio_destination_debug

static void initializeAudioDestinationDebugCategory()
{
    ensureGStreamerInitialized();
    registerWebKitGStreamerElements();

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_audio_destination_debug, "webkitaudiodestination", 0, "WebKit WebAudio Destination");
    });
}

static unsigned long maximumNumberOfOutputChannels()
{
    initializeAudioDestinationDebugCategory();

    static int count = 0;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        auto maxFromEnvironment = StringView::fromLatin1(g_getenv("WEBKIT_GST_MAX_NUMBER_OF_AUDIO_OUTPUT_CHANNELS"));
        if (!maxFromEnvironment.isEmpty()) {
            if (auto value = WTF::parseInteger<int>(maxFromEnvironment)) {
                count = *value;
                return;
            }
        }

        GRefPtr monitor = adoptGRef(gst_device_monitor_new());
        GRefPtr caps = adoptGRef(gst_caps_new_empty_simple("audio/x-raw"));
        gst_device_monitor_add_filter(monitor.get(), "Audio/Sink", caps.get());
        bool started = gst_device_monitor_start(monitor.get());
#if GST_CHECK_VERSION(1, 28, 0)
        if (started) {
            GRefPtr bus = adoptGRef(gst_device_monitor_get_bus(monitor.get()));
            [[maybe_unused]] GRefPtr message = adoptGRef(gst_bus_timed_pop_filtered(bus.get(), GST_CLOCK_TIME_NONE, GST_MESSAGE_DEVICE_MONITOR_STARTED));
            GST_DEBUG_OBJECT(monitor.get(), "Device monitor startup completed");
        }
#endif
        auto* devices = gst_device_monitor_get_devices(monitor.get());
        while (devices) {
            GRefPtr device = adoptGRef(GST_DEVICE_CAST(devices->data));
            GRefPtr caps = adoptGRef(gst_device_get_caps(device.get()));
            unsigned size = gst_caps_get_size(caps.get());
            for (unsigned i = 0; i < size; i++) {
                auto* structure = gst_caps_get_structure(caps.get(), i);
                if (gstStructureGetName(structure) != "audio/x-raw"_s)
                    continue;
                if (auto value = gstStructureGet<int>(structure, "channels"_s))
                    count = std::max(count, *value);
            }
            devices = g_list_delete_link(devices, devices);
        }
        GST_DEBUG("maximumNumberOfOutputChannels: %d", count);
        if (started)
            gst_device_monitor_stop(monitor.get());
    });

    return count;
}

Ref<AudioDestination> AudioDestination::create(const CreationOptions& options)
{
    initializeAudioDestinationDebugCategory();
    // FIXME: make use of inputDeviceId as appropriate.

    // FIXME: Add support for local/live audio input.
    if (options.numberOfInputChannels)
        WTFLogAlways("AudioDestination::create(%u, %u, %f) - unhandled input channels", options.numberOfInputChannels, options.numberOfOutputChannels, options.sampleRate);

    return adoptRef(*new AudioDestinationGStreamer(options));
}

float AudioDestination::hardwareSampleRate()
{
    return 44100;
}

unsigned long AudioDestination::maxChannelCount()
{
    return maximumNumberOfOutputChannels();
}

AudioDestinationGStreamer::AudioDestinationGStreamer(const CreationOptions& options)
    : AudioDestination(options)
    , m_renderBus(AudioBus::create(options.numberOfOutputChannels, AudioUtilities::renderQuantumSize, false))
{
}

AudioDestinationGStreamer::~AudioDestinationGStreamer()
{
    auto scopeExit = makeScopeExit([&] {
        notifyStopResult(true);
    });

    if (!m_pipeline)
        return;

    GST_DEBUG_OBJECT(m_pipeline.get(), "Disposing");
    unregisterPipeline(m_pipeline);
    disconnectSimpleBusMessageCallback(m_pipeline.get());
    gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
}

void AudioDestinationGStreamer::initializePipeline()
{
    if (m_pipeline)
        return;

    static Atomic<uint32_t> pipelineId;
    m_pipeline = gst_pipeline_new(ascii(makeString("audio-destination-"_s, pipelineId.exchangeAdd(1))).data());
    registerActivePipeline(m_pipeline);
    connectSimpleBusMessageCallback(m_pipeline.get(), [weakThis = ThreadSafeWeakPtr { *this }](GstMessage* message) {
        RefPtr destination = weakThis.get();
        if (!destination)
            return;
        destination->handleMessage(message, false);
    });

    m_src = GST_ELEMENT_CAST(g_object_new(WEBKIT_TYPE_WEB_AUDIO_SRC, nullptr));
    webkitWebAudioSourceSetDestination(WEBKIT_WEB_AUDIO_SRC(m_src.get()), this);
    webkitWebAudioSourceSetSilent(WEBKIT_WEB_AUDIO_SRC(m_src.get()), isSilent());

    auto& quirksManager = GStreamerQuirksManager::singleton();
    if (quirksManager.isEnabled()) {
#if ENABLE(MEDIA_STREAM)
        if (!m_audioSink && !isEmpty(outputDeviceId())) {
            auto [resolvedId, device] = gstResolveAudioOutputDevice(outputDeviceId());
            if (device)
                m_audioSink = quirksManager.createWebAudioSink(resolvedId, device);

            if (!m_audioSink)
                GST_WARNING_OBJECT(m_pipeline.get(), "Failed to create a GStreamer audio sink for device '%s'. Falling back to the default device.", utf8(outputDeviceId()).legacyCStringPointer());
        }
#endif

        if (!m_audioSink)
            m_audioSink = quirksManager.createWebAudioSink();
    }

#if ENABLE(MEDIA_STREAM)
    if (!m_audioSink && !isEmpty(outputDeviceId())) {
        auto [resolvedId, device] = gstResolveAudioOutputDevice(outputDeviceId());
        if (device)
            m_audioSink = createPlatformAudioSink("webaudio"_si, resolvedId, device);

        if (!m_audioSink)
            GST_WARNING_OBJECT(m_pipeline.get(), "Failed to create a GStreamer audio sink for device '%s'. Falling back to the default device.", utf8(outputDeviceId()).legacyCStringPointer());
    }
#endif

    if (!m_audioSink)
        m_audioSink = createPlatformAudioSink("webaudio"_si);

    if (!m_audioSink) {
        GST_ERROR("Failed to create GStreamer audio sink element");
        return;
    }

    if (GST_IS_AUDIO_BASE_SINK(m_audioSink.get()))
        g_object_set(m_audioSink.get(), "buffer-time", audioSinkBufferTime, nullptr);
    else {
        // Probe platform early on for a working audio output device in autoaudiosink.
        auto nameView = StringView::fromLatin1(GST_OBJECT_NAME(m_audioSink.get()));
        if (nameView.startsWith("autoaudiosink"_s)) {
            g_signal_connect(m_audioSink.get(), "child-added", G_CALLBACK(+[](GstChildProxy*, GObject* object, gchar*, gpointer) {
                if (GST_IS_AUDIO_BASE_SINK(object))
                    g_object_set(object, "buffer-time", audioSinkBufferTime, nullptr);
            }), nullptr);

            // Autoaudiosink does the real sink detection in the GST_STATE_NULL->READY transition
            // so it's best to roll it to READY as soon as possible to ensure the underlying platform
            // audiosink was loaded correctly.
            GstStateChangeReturn stateChangeReturn = gst_element_set_state(m_audioSink.get(), GST_STATE_READY);
            if (stateChangeReturn == GST_STATE_CHANGE_FAILURE) {
                GST_ERROR("Failed to change autoaudiosink element state");
                gst_element_set_state(m_audioSink.get(), GST_STATE_NULL);
                m_audioSink = nullptr;
                return;
            }
        }
    }

    GstElement* clockSync = gst_element_factory_make("clocksync", nullptr);
    GstElement* audioConvert = makeGStreamerElement("audioconvert"_s);
    GstElement* audioResample = makeGStreamerElement("audioresample"_s);

    gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), m_src.get(), clockSync, audioConvert, audioResample, m_audioSink.get(), nullptr);

    // Link src pads from webkitAudioSrc to clocksync ! audioConvert ! audioResample ! audiosink.
    gst_element_link_pads_full(m_src.get(), "src", clockSync, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(clockSync, "src", audioConvert, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioConvert, "src", audioResample, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioResample, "src", m_audioSink.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);
}

void AudioDestinationGStreamer::setSinkId(StringImpl* persistentDeviceId, bool isSilent, CompletionHandler<void(bool)>&& completionHandler)
{
    if (equal(outputDeviceId(), persistentDeviceId) && this->isSilent() == isSilent) {
        completionHandler(true);
        return;
    }

    RefPtr previousDeviceId = outputDeviceId();
    bool previousIsSilent = this->isSilent();
    setSinkSelection(persistentDeviceId, isSilent);

    // If pipeline is not created yet, the selection will be applied during its initialization.
    if (!m_pipeline || !m_audioSink) {
        completionHandler(true);
        return;
    }

    if (previousIsSilent != isSilent)
        webkitWebAudioSourceSetSilent(WEBKIT_WEB_AUDIO_SRC(m_src.get()), isSilent);

    if (isSilent) {
        completionHandler(true);
        return;
    }

#if ENABLE(MEDIA_STREAM)
    bool changed = false;
    if (isEmpty(persistentDeviceId)) {
        // No device set, route back to the default output.
        changed = gstApplyAudioSinkDevice(m_audioSink.get(), { }, nullptr);
    } else {
        auto [resolvedId, device] = gstResolveAudioOutputDevice(persistentDeviceId);
        if (device) {
            auto deviceName = GMallocString::unsafeAdoptFromUTF8(gst_device_get_display_name(device.get()));
            GST_DEBUG_OBJECT(m_pipeline.get(), "Switching to %s<%p>, output '%s'.", GST_OBJECT_NAME(device.get()), device.get(), deviceName.utf8());
            changed = gstApplyAudioSinkDevice(m_audioSink.get(), device, resolvedId);
        } else
            GST_WARNING_OBJECT(m_pipeline.get(), "Could not obtain GstDevice for '%s'.", utf8(resolvedId).legacyCStringPointer());
    }

    if (!changed) {
        setSinkSelection(previousDeviceId, previousIsSilent);
        if (previousIsSilent)
            webkitWebAudioSourceSetSilent(WEBKIT_WEB_AUDIO_SRC(m_src.get()), true);

        completionHandler(false);
        return;
    }
#endif

    completionHandler(true);
}

unsigned AudioDestinationGStreamer::framesPerBuffer() const
{
    return webkitWebAudioSourceFramesPerBuffer();
}

MediaTime AudioDestinationGStreamer::outputLatency() const
{
    if (!m_src)
        return MediaTime::zeroTime();
    return fromGstClockTime(webkitWebAudioSourceGetOutputLatency(WEBKIT_WEB_AUDIO_SRC(m_src.get())));
}

bool AudioDestinationGStreamer::handleMessage(GstMessage* message, bool handleLatencyMessage)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
        notifyIsPlaying(false);
        break;
    case GST_MESSAGE_LATENCY:
        if (!handleLatencyMessage)
            break;
        gst_element_call_async(m_pipeline.get(), reinterpret_cast<GstElementCallAsyncFunc>(+[](GstElement* pipeline, gpointer) {
            gst_bin_recalculate_latency(GST_BIN_CAST(pipeline));
        }), nullptr, nullptr);
        break;
    default:
        break;
    }
    return true;
}

void AudioDestinationGStreamer::start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_pipeline)
        initializePipeline();
    webkitWebAudioSourceSetDispatchToRenderThreadFunction(WEBKIT_WEB_AUDIO_SRC(m_src.get()), WTF::move(dispatchToRenderThread));
    startRendering(WTF::move(completionHandler));
}

void AudioDestinationGStreamer::startRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(m_audioSink);
    ASSERT(m_pipeline);
    m_startupCompletionHandler = WTF::move(completionHandler);
    GST_DEBUG_OBJECT(m_pipeline.get(), "Starting audio rendering, sink %s", m_audioSink ? "available" : "not available");

    if (m_isPlaying) {
        notifyStartupResult(true);
        return;
    }

    if (!m_audioSink) {
        notifyStartupResult(false);
        return;
    }

    notifyStartupResult(webkitGstSetElementStateSynchronously(m_pipeline.get(), GST_STATE_PLAYING, [weakThis = ThreadSafeWeakPtr { *this }](GstMessage* message) -> bool {
        RefPtr destination = weakThis.get();
        if (!destination)
            return false;
        return destination->handleMessage(message, true);
    }));
}

void AudioDestinationGStreamer::stop(CompletionHandler<void(bool)>&& completionHandler)
{
    stopRendering(WTF::move(completionHandler));
    if (m_src)
        webkitWebAudioSourceSetDispatchToRenderThreadFunction(WEBKIT_WEB_AUDIO_SRC(m_src.get()), nullptr);
}

void AudioDestinationGStreamer::stopRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    m_stopCompletionHandler = WTF::move(completionHandler);
    if (!m_pipeline) {
        notifyStopResult(true);
        return;
    }

    ASSERT(m_audioSink);
    GST_DEBUG_OBJECT(m_pipeline.get(), "Stopping audio rendering, sink %s", m_audioSink ? "available" : "not available");

    if (!m_isPlaying) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Already stopped");
        notifyStopResult(true);
        return;
    }

    if (!m_audioSink) {
        notifyStopResult(false);
        return;
    }

    notifyStopResult(webkitGstSetElementStateSynchronously(m_pipeline.get(), GST_STATE_READY, [weakThis = ThreadSafeWeakPtr { *this }](GstMessage* message) -> bool {
        RefPtr destination = weakThis.get();
        if (!destination)
            return false;
        return destination->handleMessage(message, true);
    }));
}

void AudioDestinationGStreamer::notifyStartupResult(bool success)
{
    if (success)
        notifyIsPlaying(true);

    callOnMainThreadAndWait([this, completionHandler = WTF::move(m_startupCompletionHandler), success]() mutable {
#ifdef GST_DISABLE_GST_DEBUG
        UNUSED_VARIABLE(this);
#endif
        GST_DEBUG_OBJECT(m_pipeline.get(), "Has start completion handler: %s", boolForPrinting(!!completionHandler));
        if (completionHandler)
            completionHandler(success);
    });
}

void AudioDestinationGStreamer::notifyStopResult(bool success)
{
    if (success)
        notifyIsPlaying(false);

    callOnMainThreadAndWait([this, completionHandler = WTF::move(m_stopCompletionHandler), success]() mutable {
#ifdef GST_DISABLE_GST_DEBUG
        UNUSED_VARIABLE(this);
#endif
        if (m_pipeline)
            GST_DEBUG_OBJECT(m_pipeline.get(), "Has stop completion handler: %s", boolForPrinting(!!completionHandler));
        if (completionHandler)
            completionHandler(success);
    });
}

void AudioDestinationGStreamer::notifyIsPlaying(bool isPlaying)
{
    if (m_isPlaying == isPlaying)
        return;

    GST_DEBUG("Is playing: %s", boolForPrinting(isPlaying));
    m_isPlaying = isPlaying;

    {
        Locker locker { m_callbackLock };
        if (m_callback)
            m_callback->isPlayingDidChange();
    }
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
