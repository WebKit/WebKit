/*
 * Copyright (C) 2022 Igalia S.L
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

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "VideoFrame.h"
#include "VideoFrameMetadataGStreamer.h"
#include <gst/video/video-format.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _GstSample GstSample;

namespace WebCore {

class PixelBuffer;
class IntSize;

class VideoFrameGStreamer final : public VideoFrame {
public:

    enum class CanvasContentType {
        WebGL,
        Canvas2D
    };

    static Ref<VideoFrameGStreamer> create(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize, const MediaTime& presentationTime = MediaTime::invalidTime(), Rotation videoRotation = Rotation::None, bool videoMirrored = false, std::optional<VideoFrameTimeMetadata>&& metadata = std::nullopt)
    {
        return adoptRef(*new VideoFrameGStreamer(WTFMove(sample), presentationSize, presentationTime, videoRotation, videoMirrored, WTFMove(metadata)));
    }

    static Ref<VideoFrameGStreamer> createWrappedSample(const GRefPtr<GstSample>& sample, const MediaTime& presentationTime, Rotation videoRotation = Rotation::None)
    {
        return adoptRef(*new VideoFrameGStreamer(sample, presentationTime, videoRotation));
    }

    static Ref<VideoFrameGStreamer> createFromPixelBuffer(Ref<PixelBuffer>&&, CanvasContentType canvasContentType, Rotation videoRotation, const MediaTime& presentationTime = MediaTime::invalidTime(), const IntSize& destinationSize = { }, double frameRate = 1, bool videoMirrored = false, std::optional<VideoFrameTimeMetadata>&& metadata = std::nullopt);


    RefPtr<VideoFrameGStreamer> resizeTo(const IntSize&);

    GstSample* sample() const { return m_sample.get(); }
    RefPtr<JSC::Uint8ClampedArray> computeRGBAImageData() const;

    FloatSize presentationSize() const final { return m_presentationSize; }
    uint32_t pixelFormat() const final;
private:
    VideoFrameGStreamer(GRefPtr<GstSample>&&, const FloatSize& presentationSize, const MediaTime& presentationTime = MediaTime::invalidTime(), Rotation = Rotation::None, bool videoMirrored = false, std::optional<VideoFrameTimeMetadata>&& = std::nullopt);
    VideoFrameGStreamer(const GRefPtr<GstSample>&, const MediaTime& presentationTime, Rotation = Rotation::None);

    bool isGStreamer() const final { return true; }

    GRefPtr<GstSample> m_sample;
    FloatSize m_presentationSize;
    mutable GstVideoFormat m_cachedVideoFormat { GST_VIDEO_FORMAT_UNKNOWN };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::VideoFrameGStreamer)
static bool isType(const WebCore::VideoFrame& frame) { return frame.isGStreamer(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
