/*
    Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
                  2009 Dirk Schulze <krit@webkit.org>

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

#ifndef FilterEffect_h
#define FilterEffect_h

#if ENABLE(FILTERS)
#include "FloatRect.h"
#include "ImageBuffer.h"
#include "SVGResourceFilter.h"
#include "TextStream.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class FilterEffect : public RefCounted<FilterEffect> {
    public:
        virtual ~FilterEffect();

        bool xBoundingBoxMode() const { return m_xBBoxMode; }
        void setXBoundingBoxMode(bool bboxMode) { m_xBBoxMode = bboxMode; }

        bool yBoundingBoxMode() const { return m_yBBoxMode; }
        void setYBoundingBoxMode(bool bboxMode) { m_yBBoxMode = bboxMode; }

        bool widthBoundingBoxMode() const { return m_widthBBoxMode; }
        void setWidthBoundingBoxMode(bool bboxMode) { m_widthBBoxMode = bboxMode; }

        bool heightBoundingBoxMode() const { return m_heightBBoxMode; }
        void setHeightBoundingBoxMode(bool bboxMode) { m_heightBBoxMode = bboxMode; }

        FloatRect subRegion() const { return m_subRegion; }
        void setSubRegion(const FloatRect& subRegion) { m_subRegion = subRegion; }

        // The result is bounded by the size of the filter primitive to save resources
        ImageBuffer* resultImage() { return m_effectBuffer.get(); }
        void setEffectBuffer(ImageBuffer* effectBuffer) { m_effectBuffer.set(effectBuffer); }

        virtual void apply(SVGResourceFilter*) = 0;
        virtual void dump() = 0;

        virtual TextStream& externalRepresentation(TextStream&) const;
    protected:
        FilterEffect();

    private:

        bool m_xBBoxMode : 1;
        bool m_yBBoxMode : 1;
        bool m_widthBBoxMode : 1;
        bool m_heightBBoxMode : 1;

        FloatRect m_subRegion;

        mutable OwnPtr<ImageBuffer> m_effectBuffer;
    };

} // namespace WebCore

#endif // ENABLE(FILTERS)

#endif // FilterEffect_h
