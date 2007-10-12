/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

    This file is part of the KDE project

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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
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

enum SVGFilterEffectType {
    FE_DISTANT_LIGHT      = 0,
    FE_POINT_LIGHT        = 1,
    FE_SPOT_LIGHT         = 2,
    FE_BLEND              = 3,
    FE_COLOR_MATRIX       = 4,
    FE_COMPONENT_TRANSFER = 5,
    FE_COMPOSITE          = 6,
    FE_CONVOLVE_MATRIX    = 7,
    FE_DIFFUSE_LIGHTING   = 8,
    FE_DISPLACEMENT_MAP   = 9,
    FE_FLOOD              = 10,
    FE_GAUSSIAN_BLUR      = 11,
    FE_IMAGE              = 12,
    FE_MERGE              = 13,
    FE_MORPHOLOGY         = 14,
    FE_OFFSET             = 15,
    FE_SPECULAR_LIGHTING  = 16,
    FE_TILE               = 17,
    FE_TURBULENCE         = 18
};

class SVGResourceFilter;
class TextStream;

class SVGFilterEffect {
public:
    SVGFilterEffect(SVGResourceFilter*);
    virtual ~SVGFilterEffect() { }

    virtual SVGFilterEffectType effectType() const { return FE_TURBULENCE; }

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

#endif // ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)

#endif // SVGFilterEffect_h
