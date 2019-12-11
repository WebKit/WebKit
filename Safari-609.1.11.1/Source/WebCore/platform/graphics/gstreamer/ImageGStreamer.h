/*
 * Copyright (C) 2010, 2011, 2012 Igalia S.L
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

#ifndef ImageGStreamer_h
#define ImageGStreamer_h

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "BitmapImage.h"
#include "FloatRect.h"
#include "GStreamerCommon.h"

#include <gst/gst.h>
#include <gst/video/video-frame.h>

#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class IntSize;

class ImageGStreamer : public RefCounted<ImageGStreamer> {
    public:
        static RefPtr<ImageGStreamer> createImage(GstSample* sample)
        {
            auto image = adoptRef(new ImageGStreamer(sample));
            if (!image->m_image)
                return nullptr;

            return image;
        }
        ~ImageGStreamer();

        BitmapImage& image()
        {
            ASSERT(m_image);
            return *m_image.get();
        }

        void setCropRect(FloatRect rect) { m_cropRect = rect; }
        FloatRect rect()
        {
            ASSERT(m_image);
            if (!m_cropRect.isEmpty())
                return FloatRect(m_cropRect);
            return FloatRect(0, 0, m_image->size().width(), m_image->size().height());
        }

    private:
        ImageGStreamer(GstSample*);
        RefPtr<BitmapImage> m_image;
        FloatRect m_cropRect;
#if USE(CAIRO)
        GstVideoFrame m_videoFrame;
        bool m_frameMapped { false };
#endif
    };
}

#endif // USE(GSTREAMER)
#endif
