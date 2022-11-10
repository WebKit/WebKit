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

ImageGStreamer::ImageGStreamer(GRefPtr<GstSample>&& sample)
    : m_sample(WTFMove(sample))
{
    GstCaps* caps = gst_sample_get_caps(m_sample.get());
    GstVideoInfo videoInfo;
    gst_video_info_init(&videoInfo);
    if (!gst_video_info_from_caps(&videoInfo, caps))
        return;

    // The frame has to RGB so we can paint it.
    ASSERT(GST_VIDEO_INFO_IS_RGB(&videoInfo));

    GstBuffer* buffer = gst_sample_get_buffer(m_sample.get());
    if (UNLIKELY(!GST_IS_BUFFER(buffer)))
        return;

    m_frameMapped = gst_video_frame_map(&m_videoFrame, &videoInfo, buffer, GST_MAP_READ);
    if (!m_frameMapped)
        return;

    // The video buffer may have these formats in these cases:
    // { BGRx, BGRA }: on little endian:
    //   - When GStreamer-gl is disabled (being AC enabled or not) as VideoSinkGStreamer is used.
    //   - When GStreamer-gl is enabled, but the caps used in the sink are not RGB and it's converted by the player to paint it.
    // { xRGB, ARGB }: on big endian:
    //   - When GStreamer-gl is disabled (being AC enabled or not) as VideoSinkGStreamer is used.
    //   - When GStreamer-gl is enabled, but the caps used in the sink are not RGB and it's converted by the player to paint it.
    // { RGBx, RGBA }
    //   - When GStreamer-gl is enabled and the caps used in the sink are RGBx/RGBA.
    //
    // Internally cairo uses BGRA for CAIRO_FORMAT_ARGB32 on little endian and ARGB on big endian, so both { BGRx, BGRA }
    // and { xRGB, ARGB } can be passed directly to cairo. But for { RGBx, RGBA } we need to swap the R and B components.
    // Also, GStreamer uses straight alpha while cairo requires it to be premultiplied, so if the format has alpha
    // we need to premultiply the color components. So in these cases we need to create a modified copy of the original
    // buffer.
    m_hasAlpha = GST_VIDEO_INFO_HAS_ALPHA(&videoInfo);
    bool componentSwapRequired = GST_VIDEO_FRAME_FORMAT(&m_videoFrame) == GST_VIDEO_FORMAT_RGBA || GST_VIDEO_FRAME_FORMAT(&m_videoFrame) == GST_VIDEO_FORMAT_RGBx;
    unsigned char* bufferData = reinterpret_cast<unsigned char*>(GST_VIDEO_FRAME_PLANE_DATA(&m_videoFrame, 0));
    int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&m_videoFrame, 0);
    int width = GST_VIDEO_FRAME_WIDTH(&m_videoFrame);
    int height = GST_VIDEO_FRAME_HEIGHT(&m_videoFrame);
    RefPtr<cairo_surface_t> surface;

    if (m_hasAlpha || componentSwapRequired) {
        uint8_t* surfaceData = static_cast<uint8_t*>(fastMalloc(height * stride));
        uint8_t* surfacePixel = surfaceData;

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                // These store the source pixel components.
                uint16_t red;
                uint16_t green;
                uint16_t blue;
                uint16_t alpha;
                // These store the component offset inside the pixel for the destination surface.
                uint8_t redIndex;
                uint8_t greenIndex;
                uint8_t blueIndex;
                uint8_t alphaIndex;
                if (componentSwapRequired) {
                    // Source is RGBA or RGBx.
                    red = bufferData[0];
                    green = bufferData[1];
                    blue = bufferData[2];
                    alpha = bufferData[3];
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                    // Destination is BGRA.
                    redIndex = 2;
                    greenIndex = 1;
                    blueIndex = 0;
                    alphaIndex = 3;
#else
                    // Destination is ARGB.
                    redIndex = 1;
                    greenIndex = 2;
                    blueIndex = 3;
                    alphaIndex = 0;
#endif
                } else {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                    // BGRA or BGRx both source and destination.
                    red = bufferData[2];
                    green = bufferData[1];
                    blue = bufferData[0];
                    alpha = bufferData[3];
                    redIndex = 2;
                    greenIndex = 1;
                    blueIndex = 0;
                    alphaIndex = 3;
#else
                    // ARGB ot xRGB both source and destination.
                    red = bufferData[1];
                    green = bufferData[2];
                    blue = bufferData[3];
                    alpha = bufferData[0];
                    redIndex = 1;
                    greenIndex = 2;
                    blueIndex = 3;
                    alphaIndex = 0;
#endif
                }

                if (m_hasAlpha) {
                    surfacePixel[redIndex] = red * alpha / 255;
                    surfacePixel[greenIndex] = green * alpha / 255;
                    surfacePixel[blueIndex] = blue * alpha / 255;
                    surfacePixel[alphaIndex] = alpha;
                } else {
                    surfacePixel[redIndex] = red;
                    surfacePixel[greenIndex] = green;
                    surfacePixel[blueIndex] = blue;
                    surfacePixel[alphaIndex] = alpha;
                }

                bufferData += 4;
                surfacePixel += 4;
            }
        }
        surface = adoptRef(cairo_image_surface_create_for_data(surfaceData, CAIRO_FORMAT_ARGB32, width, height, stride));
        static cairo_user_data_key_t s_surfaceDataKey;
        cairo_surface_set_user_data(surface.get(), &s_surfaceDataKey, surfaceData, [](void* data) { fastFree(data); });
    } else
        surface = adoptRef(cairo_image_surface_create_for_data(bufferData, CAIRO_FORMAT_ARGB32, width, height, stride));

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
