/*
 *  Copyright (C) 2025 Igalia, S.L
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

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class GStreamerVideoFrameConverter final : public CanMakeCheckedPtr<GStreamerVideoFrameConverter> {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerVideoFrameConverter);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(GStreamerVideoFrameConverter);
    friend NeverDestroyed<GStreamerVideoFrameConverter>;

public:
    static GStreamerVideoFrameConverter& singleton();

    WARN_UNUSED_RETURN GRefPtr<GstSample> convert(const GRefPtr<GstSample>&, const GRefPtr<GstCaps>&);

private:
    GStreamerVideoFrameConverter();

    class Pipeline {
        WTF_MAKE_TZONE_ALLOCATED(Pipeline);
    public:
        enum class Type : uint8_t {
            SystemMemory,
#if USE(GSTREAMER_GL)
            GLMemory,
            DMABufMemory
#endif
        };
        Pipeline(Type);
        ~Pipeline();

        GRefPtr<GstSample> run(const GRefPtr<GstSample>&, GstCaps*);

    private:
        Type m_type { Type::SystemMemory };
        GRefPtr<GstElement> m_pipeline;
        GRefPtr<GstElement> m_src;
        GRefPtr<GstElement> m_sink;
        GRefPtr<GstElement> m_capsfilter;
    };

    Pipeline& ensurePipeline(GstCaps*);
    void releaseUnusedSystemMemoryPipelineTimerFired();
#if USE(GSTREAMER_GL)
    void releaseUnusedGLMemoryPipelineTimerFired();
    void releaseUnusedDMABufMemoryPipelineTimerFired();
#endif

    std::unique_ptr<Pipeline> m_systemMemoryPipeline;
    std::unique_ptr<RunLoop::Timer> m_releaseUnusedSystemMemoryPipelineTimer;
#if USE(GSTREAMER_GL)
    std::unique_ptr<Pipeline> m_glMemoryPipeline;
    std::unique_ptr<RunLoop::Timer> m_releaseUnusedGLMemoryPipelineTimer;
    std::unique_ptr<Pipeline> m_dmabufMemoryPipeline;
    std::unique_ptr<RunLoop::Timer> m_releaseUnusedDMABufMemoryPipelineTimer;
#endif
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
