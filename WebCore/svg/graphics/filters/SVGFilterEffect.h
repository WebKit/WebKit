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

#ifndef SVGFilterEffect_h
#define SVGFilterEffect_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "FloatRect.h"
#include "PlatformString.h"

#if PLATFORM(CI)
#ifdef __OBJC__
@class CIFilter;
#else
class CIFilter;
#endif
#endif

namespace WebCore {

class SVGResourceFilter;
class TextStream;

class SVGFilterEffect {
public:
    SVGFilterEffect(SVGResourceFilter*);
    virtual ~SVGFilterEffect() { }

    bool xBoundingBoxMode() const { return m_xBBoxMode; }
    void setXBoundingBoxMode(bool bboxMode) { m_xBBoxMode = bboxMode; }

    bool yBoundingBoxMode() const { return m_yBBoxMode; }
    void setYBoundingBoxMode(bool bboxMode) { m_yBBoxMode = bboxMode; }

    bool widthBoundingBoxMode() const { return m_widthBBoxMode; }
    void setWidthBoundingBoxMode(bool bboxMode) { m_widthBBoxMode = bboxMode; }

    bool heightBoundingBoxMode() const { return m_heightBBoxMode; }
    void setHeightBoundingBoxMode(bool bboxMode) { m_heightBBoxMode = bboxMode; }

    FloatRect primitiveBBoxForFilterBBox(const FloatRect& filterBBox, const FloatRect& itemBBox) const;

    FloatRect subRegion() const;
    void setSubRegion(const FloatRect&);

    String in() const;
    void setIn(const String&);

    String result() const;
    void setResult(const String&);

    SVGResourceFilter* filter() const;
    void setFilter(SVGResourceFilter*);

    virtual TextStream& externalRepresentation(TextStream&) const;

#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(const FloatRect& bbox) const;
#endif

private:
    SVGResourceFilter* m_filter;

    bool m_xBBoxMode : 1;
    bool m_yBBoxMode : 1;
    bool m_widthBBoxMode : 1;
    bool m_heightBBoxMode : 1;

    FloatRect m_subRegion;
 
    String m_in;
    String m_result;
};

TextStream& operator<<(TextStream&, const SVGFilterEffect&);

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFilterEffect_h
