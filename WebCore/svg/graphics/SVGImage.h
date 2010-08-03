/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef SVGImage_h
#define SVGImage_h

#if ENABLE(SVG)

#include "Image.h"

namespace WebCore {

    class ImageBuffer;
    class Page;
    class SVGImageChromeClient;
    
    class SVGImage : public Image {
    public:
        static PassRefPtr<SVGImage> create(ImageObserver* observer)
        {
            return adoptRef(new SVGImage(observer));
        }

    private:
        virtual ~SVGImage();

        virtual String filenameExtension() const;

        virtual void setContainerSize(const IntSize&);
        virtual bool usesContainerSize() const;
        virtual bool hasRelativeWidth() const;
        virtual bool hasRelativeHeight() const;

        virtual IntSize size() const;
        
        virtual bool dataChanged(bool allDataReceived);

        // FIXME: SVGImages are underreporting decoded sizes and will be unable
        // to prune because these functions are not implemented yet.
        virtual void destroyDecodedData(bool) { }
        virtual unsigned decodedSize() const { return 0; }

        virtual NativeImagePtr frameAtIndex(size_t) { return 0; }
        
        SVGImage(ImageObserver*);
        virtual void draw(GraphicsContext*, const FloatRect& fromRect, const FloatRect& toRect, ColorSpace styleColorSpace, CompositeOperator);
        
        virtual NativeImagePtr nativeImageForCurrentFrame();
        
        OwnPtr<SVGImageChromeClient> m_chromeClient;
        OwnPtr<Page> m_page;
        OwnPtr<ImageBuffer> m_frameCache;
    };
}

#endif // ENABLE(SVG)

#endif
