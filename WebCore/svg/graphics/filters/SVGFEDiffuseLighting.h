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

#ifndef SVGFEDiffuseLighting_h
#define SVGFEDiffuseLighting_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "Color.h"
#include "FilterEffect.h"
#include "Filter.h"

namespace WebCore {

    class LightSource;

    class FEDiffuseLighting : public FilterEffect {
    public:
        static PassRefPtr<FEDiffuseLighting> create(FilterEffect*, const Color&, const float&, const float&,
                const float&, const float&, LightSource*);
        virtual ~FEDiffuseLighting();

        Color lightingColor() const;
        void setLightingColor(const Color&);

        float surfaceScale() const;
        void setSurfaceScale(float);

        float diffuseConstant() const;
        void setDiffuseConstant(float);

        float kernelUnitLengthX() const;
        void setKernelUnitLengthX(float);

        float kernelUnitLengthY() const;
        void setKernelUnitLengthY(float);

        const LightSource* lightSource() const;
        void setLightSource(LightSource*);

        virtual FloatRect uniteChildEffectSubregions(Filter* filter) { return calculateUnionOfChildEffectSubregions(filter, m_in.get()); }
        void apply(Filter*);
        void dump();
        TextStream& externalRepresentation(TextStream& ts) const;
        
    private:
        FEDiffuseLighting(FilterEffect*, const Color&, const float&, const float&,
                const float&, const float&, LightSource*);

        RefPtr<FilterEffect> m_in;
        Color m_lightingColor;
        float m_surfaceScale;
        float m_diffuseConstant;
        float m_kernelUnitLengthX;
        float m_kernelUnitLengthY;
        RefPtr<LightSource> m_lightSource;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)

#endif // SVGFEDiffuseLighting_h
