/*
 * Copyright (C) 2018 Metrological Group B.V.
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

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "GStreamerCapturer.h"

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <mutex>
#include <webrtc/api/media_stream_interface.h>

GST_DEBUG_CATEGORY(webkit_capturer_debug);
#define GST_CAT_DEFAULT webkit_capturer_debug

namespace WebCore {

static void initializeGStreamerAndDebug()
{
    initializeGStreamer();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_capturer_debug, "webkitcapturer", 0, "WebKit Capturer");
    });
}

GStreamerCapturer::GStreamerCapturer(GStreamerCaptureDevice device, GRefPtr<GstCaps> caps)
    : m_device(device.device())
    , m_caps(caps)
    , m_sourceFactory(nullptr)
{
    initializeGStreamerAndDebug();
}

GStreamerCapturer::GStreamerCapturer(const char* sourceFactory, GRefPtr<GstCaps> caps)
    : m_device(nullptr)
    , m_caps(caps)
    , m_sourceFactory(sourceFactory)
{
    initializeGStreamerAndDebug();
}

GStreamerCapturer::~GStreamerCapturer()
{
    if (m_pipeline)
        disconnectSimpleBusMessageCallback(pipeline());
}

GstElement* GStreamerCapturer::createSource()
{
    if (m_sourceFactory) {
        m_src = makeElement(m_sourceFactory);
        if (GST_IS_APP_SRC(m_src.get()))
            g_object_set(m_src.get(), "is-live", true, "format", GST_FORMAT_TIME, nullptr);

        ASSERT(m_src);
        return m_src.get();
    }

    ASSERT(m_device);
    GUniquePtr<char> sourceName(g_strdup_printf("%s_%p", name(), this));
    m_src = gst_device_create_element(m_device.get(), sourceName.get());
    ASSERT(m_src);

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

    m_capsfilter = makeElement("capsfilter");
    m_tee = makeElement("tee");
    m_sink = makeElement("appsink");

    gst_app_sink_set_emit_signals(GST_APP_SINK(m_sink.get()), TRUE);
    g_object_set(m_capsfilter.get(), "caps", m_caps.get(), nullptr);

    gst_bin_add_many(GST_BIN(m_pipeline.get()), source.get(), converter.get(), m_capsfilter.get(), m_tee.get(), nullptr);
    gst_element_link_many(source.get(), converter.get(), m_capsfilter.get(), m_tee.get(), nullptr);

    addSink(m_sink.get());

    connectSimpleBusMessageCallback(pipeline());
}

GstElement* GStreamerCapturer::makeElement(const char* factoryName)
{
    auto element = gst_element_factory_make(factoryName, nullptr);
    ASSERT(element);
    GUniquePtr<char> capturerName(g_strdup_printf("%s_capturer_%s_%p", name(), GST_OBJECT_NAME(element), this));
    gst_object_set_name(GST_OBJECT(element), capturerName.get());

    return element;
}

void GStreamerCapturer::addSink(GstElement* newSink)
{
    ASSERT(m_pipeline);
    ASSERT(m_tee);

    auto queue = makeElement("queue");
    gst_bin_add_many(GST_BIN(pipeline()), queue, newSink, nullptr);
    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(newSink);

    if (!gst_element_link_pads(m_tee.get(), "src_%u", queue, "sink")) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (!gst_element_link(queue, newSink)) {
        ASSERT_NOT_REACHED();
        return;
    }

    GST_INFO_OBJECT(pipeline(), "Adding sink: %" GST_PTR_FORMAT, newSink);

    GUniquePtr<char> dumpName(g_strdup_printf("%s_sink_%s_added", GST_OBJECT_NAME(pipeline()), GST_OBJECT_NAME(newSink)));
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(pipeline()), GST_DEBUG_GRAPH_SHOW_ALL, dumpName.get());
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

} // namespace WebCore

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
