/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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

#ifndef SVGImageCache_h
#define SVGImageCache_h

#if ENABLE(SVG)
#include "Image.h"
#include "IntSize.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CachedImage;
class CachedImageClient;
class ImageBuffer;
class SVGImage;
class RenderObject;

class SVGImageCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~SVGImageCache();

    static PassOwnPtr<SVGImageCache> create(SVGImage* image)
    {
        return adoptPtr(new SVGImageCache(image));
    }

    struct SizeAndScales {
        SizeAndScales()
            : zoom(1)
            , scale(0)
        {
        }

        SizeAndScales(const IntSize& newSize, float newZoom, float newScale)
            : size(newSize)
            , zoom(newZoom)
            , scale(newScale)
        {
        }

        SizeAndScales(const IntSize& newSize, float newZoom)
            : size(newSize)
            , zoom(newZoom)
            , scale(0)
        {
        }

        IntSize size;
        float zoom;
        float scale; // A scale of 0 indicates that the default scale should be used.
    };

    void removeClientFromCache(const CachedImageClient*);

    void setRequestedSizeAndScales(const CachedImageClient*, const SizeAndScales&);
    SizeAndScales requestedSizeAndScales(const CachedImageClient*) const;

    Image* lookupOrCreateBitmapImageForRenderer(const RenderObject*);
    void imageContentChanged();

private:
    SVGImageCache(SVGImage*);
    void redraw();
    void redrawTimerFired(Timer<SVGImageCache>*);
    void cacheClearTimerFired(DeferrableOneShotTimer<SVGImageCache>*);
    void clearBitmapCache();

    struct ImageData {
        ImageData()
            : imageNeedsUpdate(false)
            , buffer(0)
        {
        }

        ImageData(ImageBuffer* newBuffer, PassRefPtr<Image> newImage, const SizeAndScales& newSizeAndScales)
            : imageNeedsUpdate(false)
            , sizeAndScales(newSizeAndScales)
            , buffer(newBuffer)
            , image(newImage)
        {
        }

        bool imageNeedsUpdate;
        SizeAndScales sizeAndScales;

        ImageBuffer* buffer;
        RefPtr<Image> image;
    };

    typedef HashMap<const CachedImageClient*, SizeAndScales> SizeAndScalesMap;
    typedef HashMap<const CachedImageClient*, ImageData> ImageDataMap;

    SVGImage* m_svgImage;
    SizeAndScalesMap m_sizeAndScalesMap;
    ImageDataMap m_imageDataMap;
    Timer<SVGImageCache> m_redrawTimer;
    DeferrableOneShotTimer<SVGImageCache> m_cacheClearTimer;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGImageCache_h
