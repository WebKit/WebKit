/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGFEImage_h
#define SVGFEImage_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "CachedImage.h"
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "FilterEffect.h"

namespace WebCore {

    class FEImage : public FilterEffect
                     , public CachedResourceClient {
    public:
        static PassRefPtr<FEImage> create(CachedImage*);
        virtual ~FEImage();

        // FIXME: We need to support <svg> (RenderObject*) as well as image data.

        CachedImage* cachedImage() const;
        void setCachedImage(CachedImage*);
        
        virtual void apply();
        virtual void dump();
        TextStream& externalRepresentation(TextStream& ts) const;
        
    private:
        FEImage(CachedImage*);

        CachedResourceHandle<CachedImage> m_cachedImage;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFEImage_h
