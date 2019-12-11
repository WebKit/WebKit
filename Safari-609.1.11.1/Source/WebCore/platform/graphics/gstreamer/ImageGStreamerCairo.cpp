/*
 * Copyright (C) 2010 Igalia S.L
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
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ImageGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"

#include <cairo.h>
#include <gst/gst.h>
#include <gst/video/gstvideometa.h>


namespace WebCore {

ImageGStreamer::ImageGStreamer(GstSample* sample)
{
    GstCaps* caps = gst_sample_get_caps(sample);
    GstVideoInfo videoInfo;
    gst_video_info_init(&videoInfo);
    if (!gst_video_info_from_caps(&videoInfo, caps))
        return;

    // Right now the TextureMapper only supports chromas with one plane
    ASSERT(GST_VIDEO_INFO_N_PLANES(&videoInfo) == 1);

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (UNLIKELY(!GST_IS_BUFFER(buffer)))
        return;

    m_frameMapped = gst_video_frame_map(&m_videoFrame, &videoInfo, buffer, GST_MAP_READ);
    if (!m_frameMapped)
        return;

    unsigned char* bufferData = reinterpret_cast<unsigned char*>(GST_VIDEO_FRAME_PLANE_DATA(&m_videoFrame, 0));
    int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&m_videoFrame, 0);
    int width = GST_VIDEO_FRAME_WIDTH(&m_videoFrame);
    int height = GST_VIDEO_FRAME_HEIGHT(&m_videoFrame);

    RefPtr<cairo_surface_t> surface;
    cairo_format_t cairoFormat;
    cairoFormat = (GST_VIDEO_FRAME_FORMAT(&m_videoFrame) == GST_VIDEO_FORMAT_RGBA) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;

    // GStreamer doesn't use premultiplied alpha, but cairo does. So if the video format has an alpha component
    // we need to premultiply it before passing the data to cairo. This needs to be both using gstreamer-gl and not
    // using it.
    //
    // This method could be called several times for the same buffer, for example if we are rendering the video frames
    // in several non accelerated canvases. Due to this, we cannot modify the buffer, so we need to create a copy.
    if (cairoFormat == CAIRO_FORMAT_ARGB32) {
        unsigned char* surfaceData = static_cast<unsigned char*>(fastMalloc(height * stride));
        unsigned char* surfacePixel = surfaceData;

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                unsigned short alpha = bufferData[3];
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                // Video frames use RGBA in little endian.
                surfacePixel[0] = (bufferData[2] * alpha + 128) / 255;
                surfacePixel[1] = (bufferData[1] * alpha + 128) / 255;
                surfacePixel[2] = (bufferData[0] * alpha + 128) / 255;
                surfacePixel[3] = alpha;
#else
                // Video frames use RGBA in big endian.
                surfacePixel[0] = alpha;
                surfacePixel[1] = (bufferData[0] * alpha + 128) / 255;
                surfacePixel[2] = (bufferData[1] * alpha + 128) / 255;
                surfacePixel[3] = (bufferData[2] * alpha + 128) / 255;
#endif
                bufferData += 4;
                surfacePixel += 4;
            }
        }
        surface = adoptRef(cairo_image_surface_create_for_data(surfaceData, cairoFormat, width, height, stride));
        static cairo_user_data_key_t s_surfaceDataKey;
        cairo_surface_set_user_data(surface.get(), &s_surfaceDataKey, surfaceData, [](void* data) { fastFree(data); });
    } else
        surface = adoptRef(cairo_image_surface_create_for_data(bufferData, cairoFormat, width, height, stride));

    ASSERT(cairo_surface_status(surface.get()) == CAIRO_STATUS_SUCCESS);
    m_image = BitmapImage::create(WTFMove(surface));

    if (GstVideoCropMeta* cropMeta = gst_buffer_get_video_crop_meta(buffer))
        setCropRect(FloatRect(cropMeta->x, cropMeta->y, cropMeta->width, cropMeta->height));
}

ImageGStreamer::~ImageGStreamer()
{
    if (m_image)
        m_image = nullptr;

    // We keep the buffer memory mapped until the image is destroyed because the internal
    // cairo_surface_t was created using cairo_image_surface_create_for_data().
    if (m_frameMapped)
        gst_video_frame_unmap(&m_videoFrame);
}

} // namespace WebCore

#endif // USE(GSTREAMER)
