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

#ifndef SVGFEBlend_h
#define SVGFEBlend_h

#if ENABLE(FILTERS)
#include "FilterEffect.h"

#include "Filter.h"

namespace WebCore {

    enum BlendModeType {
        FEBLEND_MODE_UNKNOWN  = 0,
        FEBLEND_MODE_NORMAL   = 1,
        FEBLEND_MODE_MULTIPLY = 2,
        FEBLEND_MODE_SCREEN   = 3,
        FEBLEND_MODE_DARKEN   = 4,
        FEBLEND_MODE_LIGHTEN  = 5
    };

    class FEBlend : public FilterEffect {
    public:
        static PassRefPtr<FEBlend> create(FilterEffect*, FilterEffect*, BlendModeType);

        FilterEffect* in2() const;
        void setIn2(FilterEffect*);

        BlendModeType blendMode() const;
        void setBlendMode(BlendModeType);

        virtual FloatRect uniteChildEffectSubregions(Filter* filter) { return calculateUnionOfChildEffectSubregions(filter, m_in.get(), m_in2.get()); }
        void apply(Filter*);
        void dump();

    private:
        FEBlend(FilterEffect*, FilterEffect*, BlendModeType);

        RefPtr<FilterEffect> m_in;
        RefPtr<FilterEffect> m_in2;
        BlendModeType m_mode;
    };

} // namespace WebCore

#endif // ENABLE(FILTERS)

#endif // SVGFEBlend_h
