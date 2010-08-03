/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
 * Copyright (C) All rights reserved.
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

#ifndef SVGFELighting_h
#define SVGFELighting_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "Color.h"
#include "Filter.h"
#include "FilterEffect.h"
#include "SVGLightSource.h"
#include <wtf/AlwaysInline.h>

// Common base class for FEDiffuseLighting and FESpecularLighting

namespace WebCore {

class CanvasPixelArray;

class FELighting : public FilterEffect {
public:
    virtual FloatRect uniteChildEffectSubregions(Filter* filter) { return calculateUnionOfChildEffectSubregions(filter, m_in.get()); }
    void apply(Filter*);

protected:
    enum LightingType {
        DiffuseLighting,
        SpecularLighting
    };

    struct LightingData {
        FloatPoint3D normalVector;
        CanvasPixelArray* pixels;
        float lightStrength;
        float surfaceScale;
        int offset;
        int widthMultipliedByPixelSize;
        int widthDecreasedByOne;
        int heightDecreasedByOne;

        ALWAYS_INLINE int upLeftPixelValue();
        ALWAYS_INLINE int upPixelValue();
        ALWAYS_INLINE int upRightPixelValue();
        ALWAYS_INLINE int leftPixelValue();
        ALWAYS_INLINE int centerPixelValue();
        ALWAYS_INLINE int rightPixelValue();
        ALWAYS_INLINE int downLeftPixelValue();
        ALWAYS_INLINE int downPixelValue();
        ALWAYS_INLINE int downRightPixelValue();
    };

    FELighting(LightingType, FilterEffect*, const Color&, float, float, float,
        float, float, float, PassRefPtr<LightSource>);

    bool drawLighting(CanvasPixelArray*, int, int);
    ALWAYS_INLINE void setPixel(LightingData&, LightSource::PaintingData&,
        int lightX, int lightY, float factorX, int normalX, float factorY, int normalY);

    LightingType m_lightingType;
    RefPtr<FilterEffect> m_in;
    RefPtr<LightSource> m_lightSource;

    Color m_lightingColor;
    float m_surfaceScale;
    float m_diffuseConstant;
    float m_specularConstant;
    float m_specularExponent;
    float m_kernelUnitLengthX;
    float m_kernelUnitLengthY;
};

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)

#endif // SVGFELighting_h
