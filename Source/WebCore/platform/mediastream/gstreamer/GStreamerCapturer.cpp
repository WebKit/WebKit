/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
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

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "GStreamerCapturer.h"
#include "VideoFrameMetadataGStreamer.h"

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <mutex>
#include <wtf/HexNumber.h>
#include <wtf/MonotonicTime.h>

GST_DEBUG_CATEGORY(webkit_capturer_debug);
#define GST_CAT_DEFAULT webkit_capturer_debug

namespace WebCore {

static void initializeDebugCategory()
{
    ensureGStreamerInitialized();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_capturer_debug, "webkitcapturer", 0, "WebKit Capturer");
    });
}

GStreamerCapturer::GStreamerCapturer(GStreamerCaptureDevice device, GRefPtr<GstCaps> caps)
    : m_device(device.device())
    , m_caps(caps)
    , m_sourceFactory(nullptr)
    , m_deviceType(device.type())
{
    initializeDebugCategory();
}

GStreamerCapturer::GStreamerCapturer(const char* sourceFactory, GRefPtr<GstCaps> caps, CaptureDevice::DeviceType deviceType)
    : m_device(nullptr)
    , m_caps(caps)
    , m_sourceFactory(sourceFactory)
    , m_deviceType(deviceType)
{
    initializeDebugCategory();
}

GStreamerCapturer::~GStreamerCapturer()
{
    if (m_pipeline)
        disconnectSimpleBusMessageCallback(pipeline());
}

GStreamerCapturer::Observer::~Observer()
{
}

void GStreamerCapturer::addObserver(Observer& observer)
{
    ASSERT(isMainThread());
    m_observers.add(observer);
}

void GStreamerCapturer::removeObserver(Observer& observer)
{
    ASSERT(isMainThread());
    m_observers.remove(observer);
}

void GStreamerCapturer::forEachObserver(const Function<void(Observer&)>& apply)
{
    ASSERT(isMainThread());
    Ref protectedThis { *this };
    m_observers.forEach(apply);
}

GstElement* GStreamerCapturer::createSource()
{
    if (m_sourceFactory) {
        m_src = makeElement(m_sourceFactory);
        ASSERT(m_src);

        if (GST_IS_APP_SRC(m_src.get()))
            g_object_set(m_src.get(), "is-live", TRUE, "format", GST_FORMAT_TIME, "do-timestamp", TRUE, nullptr);

        if (m_deviceType == CaptureDevice::DeviceType::Screen) {
            auto srcPad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
            gst_pad_add_probe(srcPad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, [](GstPad*, GstPadProbeInfo* info, void* userData) -> GstPadProbeReturn {
                auto* event = gst_pad_probe_info_get_event(info);
                if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
                    return GST_PAD_PROBE_OK;

                callOnMainThread([event, capturer = reinterpret_cast<GStreamerCapturer*>(userData)] {
                    GstCaps* caps;
                    gst_event_parse_caps(event, &caps);
                    capturer->forEachObserver([caps](Observer& observer) {
                        observer.sourceCapsChanged(caps);
                    });
                });
                return GST_PAD_PROBE_OK;
            }, this, nullptr);
        }
    } else {
        ASSERT(m_device);
        auto sourceName = makeString(name(), hex(reinterpret_cast<uintptr_t>(this)));
        m_src = gst_device_create_element(m_device.get(), sourceName.ascii().data());
        ASSERT(m_src);
        g_object_set(m_src.get(), "do-timestamp", TRUE, nullptr);
    }

    if (m_deviceType == CaptureDevice::DeviceType::Camera) {
        auto srcPad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
        gst_pad_add_probe(srcPad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_PUSH | GST_PAD_PROBE_TYPE_BUFFER), [](GstPad*, GstPadProbeInfo* info, gpointer) -> GstPadProbeReturn {
            VideoFrameTimeMetadata metadata;
            metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();
            auto* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
            auto* modifiedBuffer = webkitGstBufferSetVideoFrameTimeMetadata(buffer, metadata);
            gst_buffer_replace(&buffer, modifiedBuffer);
            return GST_PAD_PROBE_OK;
        }, nullptr, nullptr);
    }

    return m_src.get();
}

GstCaps* GStreamerCapturer::caps()
{
    if (m_sourceFactory) {
        GRefPtr<GstElement> element = makeElement(m_sourceFactory);
        auto pad = adoptGRef(gst_element_get_static_pad(element.get(), "src"));

        return gst_pad_query_caps(pad.get(), nullptr);
    }

    ASSERT(m_device);
    return gst_device_get_caps(m_device.get());
}

void GStreamerCapturer::setupPipeline()
{
    if (m_pipeline)
        disconnectSimpleBusMessageCallback(pipeline());

    m_pipeline = makeElement("pipeline");

    GRefPtr<GstElement> source = createSource();
    GRefPtr<GstElement> converter = createConverter();

    m_valve = makeElement("valve");
    m_capsfilter = makeElement("capsfilter");
    m_sink = makeElement("appsink");

    gst_app_sink_set_emit_signals(GST_APP_SINK(m_sink.get()), TRUE);
    g_object_set(m_sink.get(), "enable-last-sample", FALSE, nullptr);
    g_object_set(m_capsfilter.get(), "caps", m_caps.get(), nullptr);

    gst_bin_add_many(GST_BIN(m_pipeline.get()), source.get(), converter.get(), m_capsfilter.get(), m_valve.get(), m_sink.get(), nullptr);
    gst_element_link_many(source.get(), converter.get(), m_capsfilter.get(), m_valve.get(), m_sink.get(), nullptr);

    connectSimpleBusMessageCallback(pipeline());
}

GstElement* GStreamerCapturer::makeElement(const char* factoryName)
{
    auto* element = makeGStreamerElement(factoryName, nullptr);
    auto elementName = makeString(name(), "_capturer_", GST_OBJECT_NAME(element), '_', hex(reinterpret_cast<uintptr_t>(this)));
    gst_object_set_name(GST_OBJECT(element), elementName.ascii().data());

    return element;
}

void GStreamerCapturer::play()
{
    ASSERT(m_pipeline);

    GST_INFO_OBJECT(pipeline(), "Going to PLAYING!");

    gst_element_set_state(pipeline(), GST_STATE_PLAYING);
}

void GStreamerCapturer::stop()
{
    ASSERT(m_pipeline);

    GST_INFO_OBJECT(pipeline(), "Tearing down!");

    // Make sure to remove sync handler before tearing down, avoiding
    // possible deadlocks.
    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(pipeline())));
    gst_bus_set_sync_handler(bus.get(), nullptr, nullptr, nullptr);

    gst_element_set_state(pipeline(), GST_STATE_NULL);
}

bool GStreamerCapturer::isInterrupted() const
{
    gboolean isInterrupted;
    g_object_get(m_valve.get(), "drop", &isInterrupted, nullptr);
    return isInterrupted;
}

void GStreamerCapturer::setInterrupted(bool isInterrupted)
{
    g_object_set(m_valve.get(), "drop", isInterrupted, nullptr);
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
