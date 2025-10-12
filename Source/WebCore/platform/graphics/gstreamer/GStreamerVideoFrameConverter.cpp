/*
 * Copyright (C) 2025 Igalia S.L
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
#include "GStreamerVideoFrameConverter.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <gst/allocators/gstdmabuf.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/RunLoopSourcePriority.h>

#if USE(GSTREAMER_GL)
#include <gst/gl/gl.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerVideoFrameConverter);
WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerVideoFrameConverter::Pipeline);

GST_DEBUG_CATEGORY_STATIC(webkit_gst_video_frame_converter_debug);
#define GST_CAT_DEFAULT webkit_gst_video_frame_converter_debug

static const Seconds s_releaseUnusedPipelinesTimerInterval = 30_s;

GStreamerVideoFrameConverter::Pipeline::Pipeline(Type type)
    : m_type(type)
    , m_src(makeGStreamerElement("appsrc"_s))
    , m_sink(makeGStreamerElement("appsink"_s))
{
    g_object_set(m_sink.get(), "enable-last-sample", FALSE, "max-buffers", 1, nullptr);
    switch (m_type) {
    case Type::SystemMemory: {
        auto videoconvert = makeGStreamerElement("videoconvert"_s);
        auto videoscale = makeGStreamerElement("videoscale"_s);
        m_pipeline = gst_element_factory_make("pipeline", "video-frame-converter");
        gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), m_src.get(), videoconvert, videoscale, m_sink.get(), nullptr);
        gst_element_link_many(m_src.get(), videoconvert, videoscale, m_sink.get(), nullptr);
        break;
    }
#if USE(GSTREAMER_GL)
    case Type::GLMemory: {
        auto glcolorconvert = makeGStreamerElement("glcolorconvert"_s);
        auto gldownload = makeGStreamerElement("gldownload"_s);
        auto videoscale = makeGStreamerElement("videoscale"_s);
        m_pipeline = gst_element_factory_make("pipeline", "video-frame-converter-gl");
        gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), m_src.get(), glcolorconvert, gldownload, videoscale, m_sink.get(), nullptr);
        gst_element_link_many(m_src.get(), glcolorconvert, gldownload, videoscale, m_sink.get(), nullptr);
        break;
    }
    case Type::DMABufMemory: {
        auto glupload = makeGStreamerElement("glupload"_s);
        m_capsfilter = makeGStreamerElement("capsfilter"_s);
        auto glcolorconvert = makeGStreamerElement("glcolorconvert"_s);
        auto gldownload = makeGStreamerElement("gldownload"_s);
        auto videoscale = makeGStreamerElement("videoscale"_s);
        m_pipeline = gst_element_factory_make("pipeline", "video-frame-converter-gl");
        gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), m_src.get(), glupload, m_capsfilter.get(), glcolorconvert, gldownload, videoscale, m_sink.get(), nullptr);
        gst_element_link_many(m_src.get(), glupload, m_capsfilter.get(), glcolorconvert, gldownload, videoscale, m_sink.get(), nullptr);
    }
#endif
    }
}

GStreamerVideoFrameConverter::Pipeline::~Pipeline() = default;

GRefPtr<GstSample> GStreamerVideoFrameConverter::Pipeline::run(const GRefPtr<GstSample>& sample, GstCaps* destinationCaps)
{
#if USE(GSTREAMER_GL)
    if (m_type == Type::GLMemory || m_type == Type::DMABufMemory) {
        static ASCIILiteral gstGlDisplayContextyType = ASCIILiteral::fromLiteralUnsafe(GST_GL_DISPLAY_CONTEXT_TYPE);
        if (!setGstElementGLContext(m_pipeline.get(), gstGlDisplayContextyType))
            return nullptr;
        if (!setGstElementGLContext(m_pipeline.get(), "gst.gl.app_context"_s))
            return nullptr;

        if (m_type == Type::DMABufMemory) {
            GRefPtr<GstCaps> outputCaps = adoptGRef(gst_caps_copy(destinationCaps));
            gst_caps_set_features(outputCaps.get(), 0, gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_GL_MEMORY, nullptr));
            gst_caps_set_simple(outputCaps.get(), "format", G_TYPE_STRING, "RGBA", nullptr);
            g_object_set(m_capsfilter.get(), "caps", outputCaps.get(), nullptr);
        }
    }
#endif

    unsigned capsSize = gst_caps_get_size(destinationCaps);
    auto newCaps = adoptGRef(gst_caps_new_empty());
    for (unsigned i = 0; i < capsSize; i++) {
        auto structure = gst_caps_get_structure(destinationCaps, i);
        auto modifiedStructure = gst_structure_copy(structure);
        gst_structure_remove_field(modifiedStructure, "framerate");
        gst_caps_append_structure(newCaps.get(), modifiedStructure);
    }

    GST_TRACE_OBJECT(m_pipeline.get(), "Converting sample with caps %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, gst_sample_get_caps(sample.get()), newCaps.get());
    g_object_set(m_sink.get(), "caps", newCaps.get(), nullptr);

    auto scopeExit = WTF::makeScopeExit([&] {
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
    });

    gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED);
    gst_app_src_push_sample(GST_APP_SRC_CAST(m_src.get()), sample.get());

    auto bus = adoptGRef(gst_element_get_bus(m_pipeline.get()));
    auto message = adoptGRef(gst_bus_timed_pop_filtered(bus.get(), 200 * GST_MSECOND, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE)));
    if (!message) {
        GST_ERROR_OBJECT(m_pipeline.get(), "Video frame conversion 200ms timeout expired.");
        return nullptr;
    }

    if (GST_MESSAGE_TYPE(message.get()) == GST_MESSAGE_ERROR) {
        GST_ERROR_OBJECT(m_pipeline.get(), "Unable to convert video frame. Error: %" GST_PTR_FORMAT, message.get());
        return nullptr;
    }

    return adoptGRef(gst_app_sink_pull_preroll(GST_APP_SINK_CAST(m_sink.get())));
}

GStreamerVideoFrameConverter& GStreamerVideoFrameConverter::singleton()
{
    static NeverDestroyed<GStreamerVideoFrameConverter> sharedInstance;
    return sharedInstance;
}

GStreamerVideoFrameConverter::GStreamerVideoFrameConverter()
{
    ensureGStreamerInitialized();
    GST_DEBUG_CATEGORY_INIT(webkit_gst_video_frame_converter_debug, "webkitvideoframeconverter", 0, "WebKit GStreamer Video Frame Converter");
}

GStreamerVideoFrameConverter::Pipeline& GStreamerVideoFrameConverter::ensurePipeline(GstCaps* caps)
{
#if USE(GSTREAMER_GL)
    auto* features = gst_caps_get_features(caps, 0);
    if (features && gst_caps_features_contains(features, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
        if (!m_dmabufMemoryPipeline) {
            m_dmabufMemoryPipeline = makeUnique<Pipeline>(Pipeline::Type::DMABufMemory);
            m_releaseUnusedDMABufMemoryPipelineTimer = makeUnique<RunLoop::Timer>(RunLoop::currentSingleton(), "GStreamerVideoFrameConverter::ReleaseUnusedDMABufMemoryPipelineTimer"_s, this, &GStreamerVideoFrameConverter::releaseUnusedDMABufMemoryPipelineTimerFired);
            m_releaseUnusedDMABufMemoryPipelineTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
        }
        m_releaseUnusedDMABufMemoryPipelineTimer->startOneShot(s_releaseUnusedPipelinesTimerInterval);
        return *m_dmabufMemoryPipeline;
    }

    if (features && gst_caps_features_contains(features, GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
        if (!m_glMemoryPipeline) {
            m_glMemoryPipeline = makeUnique<Pipeline>(Pipeline::Type::GLMemory);
            m_releaseUnusedGLMemoryPipelineTimer = makeUnique<RunLoop::Timer>(RunLoop::currentSingleton(), "GStreamerVideoFrameConverter::ReleaseUnusedGLMemoryPipelineTimer"_s, this, &GStreamerVideoFrameConverter::releaseUnusedGLMemoryPipelineTimerFired);
            m_releaseUnusedGLMemoryPipelineTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
        }
        m_releaseUnusedGLMemoryPipelineTimer->startOneShot(s_releaseUnusedPipelinesTimerInterval);
        return *m_glMemoryPipeline;
    }
#else
    UNUSED_PARAM(caps);
#endif

    if (!m_systemMemoryPipeline) {
        m_systemMemoryPipeline = makeUnique<Pipeline>(Pipeline::Type::SystemMemory);
        m_releaseUnusedSystemMemoryPipelineTimer = makeUnique<RunLoop::Timer>(RunLoop::currentSingleton(), "GStreamerVideoFrameConverter::ReleaseUnusedSystemMemoryPipelineTimer"_s, this, &GStreamerVideoFrameConverter::releaseUnusedSystemMemoryPipelineTimerFired);
        m_releaseUnusedSystemMemoryPipelineTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
    }
    m_releaseUnusedSystemMemoryPipelineTimer->startOneShot(s_releaseUnusedPipelinesTimerInterval);
    return *m_systemMemoryPipeline;
}

GRefPtr<GstSample> GStreamerVideoFrameConverter::convert(const GRefPtr<GstSample>& sample, const GRefPtr<GstCaps>& destinationCaps)
{
    auto inputCaps = gst_sample_get_caps(sample.get());
    if (gst_caps_is_equal(inputCaps, destinationCaps.get()))
        return GRefPtr(sample);

    auto outputSample = ensurePipeline(inputCaps).run(sample, destinationCaps.get());
    if (!outputSample)
        return nullptr;

    auto convertedSample = adoptGRef(gst_sample_make_writable(outputSample.leakRef()));
    gst_sample_set_caps(convertedSample.get(), destinationCaps.get());

    GRefPtr buffer = gst_sample_get_buffer(convertedSample.get());
IGNORE_WARNINGS_BEGIN("cast-align")
    auto writableBuffer = adoptGRef(gst_buffer_make_writable(buffer.leakRef()));
IGNORE_WARNINGS_END

    if (auto meta = gst_buffer_get_video_meta(writableBuffer.get()))
        gst_buffer_remove_meta(writableBuffer.get(), GST_META_CAST(meta));

    if (auto meta = gst_buffer_get_parent_buffer_meta(writableBuffer.get()))
        gst_buffer_remove_meta(writableBuffer.get(), GST_META_CAST(meta));

    auto structure = gst_caps_get_structure(destinationCaps.get(), 0);
    auto width = gstStructureGet<int>(structure, "width"_s);
    auto height = gstStructureGet<int>(structure, "height"_s);
    auto formatString = gstStructureGetString(structure, "format"_s);
    if (width && height && !formatString.isEmpty()) {
        auto format = gst_video_format_from_string(formatString.utf8());
        gst_buffer_add_video_meta(writableBuffer.get(), GST_VIDEO_FRAME_FLAG_NONE, format, *width, *height);
    }
    gst_sample_set_buffer(convertedSample.get(), writableBuffer.get());

    return convertedSample;
}

void GStreamerVideoFrameConverter::releaseUnusedSystemMemoryPipelineTimerFired()
{
    m_systemMemoryPipeline = nullptr;
    m_releaseUnusedSystemMemoryPipelineTimer = nullptr;
}

#if USE(GSTREAMER_GL)
void GStreamerVideoFrameConverter::releaseUnusedGLMemoryPipelineTimerFired()
{
    m_glMemoryPipeline = nullptr;
    m_releaseUnusedGLMemoryPipelineTimer = nullptr;
}

void GStreamerVideoFrameConverter::releaseUnusedDMABufMemoryPipelineTimerFired()
{
    m_dmabufMemoryPipeline = nullptr;
    m_releaseUnusedDMABufMemoryPipelineTimer = nullptr;
}
#endif

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
