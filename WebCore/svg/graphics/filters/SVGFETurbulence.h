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

#ifndef SVGFETurbulence_h
#define SVGFETurbulence_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "FilterEffect.h"

namespace WebCore {

    enum TurbulanceType {
        FETURBULENCE_TYPE_UNKNOWN      = 0,
        FETURBULENCE_TYPE_FRACTALNOISE = 1,
        FETURBULENCE_TYPE_TURBULENCE   = 2
    };

    class FETurbulence : public FilterEffect {
    public:
        static PassRefPtr<FETurbulence> create(TurbulanceType, const float&, const float&, const int&, const float&,
                bool);

        TurbulanceType type() const;
        void setType(TurbulanceType);

        float baseFrequencyY() const;
        void setBaseFrequencyY(float);

        float baseFrequencyX() const;
        void setBaseFrequencyX(float);

        float seed() const;
        void setSeed(float);

        int numOctaves() const;
        void setNumOctaves(bool);

        bool stitchTiles() const;
        void setStitchTiles(bool);

        virtual void apply();
        virtual void dump();
        TextStream& externalRepresentation(TextStream& ts) const;

    private:
        FETurbulence(TurbulanceType, const float&, const float&, const int&, const float&,
                bool);

        TurbulanceType m_type;
        float m_baseFrequencyX;
        float m_baseFrequencyY;
        int m_numOctaves;
        float m_seed;
        bool m_stitchTiles;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFETurbulence_h
