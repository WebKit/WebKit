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

#ifndef SVGFEColorMatrix_h
#define SVGFEColorMatrix_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "FilterEffect.h"
#include <wtf/Vector.h>

namespace WebCore {

    enum ColorMatrixType {
        FECOLORMATRIX_TYPE_UNKNOWN          = 0,
        FECOLORMATRIX_TYPE_MATRIX           = 1,
        FECOLORMATRIX_TYPE_SATURATE         = 2,
        FECOLORMATRIX_TYPE_HUEROTATE        = 3,
        FECOLORMATRIX_TYPE_LUMINANCETOALPHA = 4
    };

    class FEColorMatrix : public FilterEffect {
    public:
        static PassRefPtr<FEColorMatrix> create(FilterEffect*, ColorMatrixType, const Vector<float>&);

        ColorMatrixType type() const;
        void setType(ColorMatrixType);

        const Vector<float>& values() const;
        void setValues(const Vector<float>&);
        
        virtual void apply();
        virtual void dump();

    private:
        FEColorMatrix(FilterEffect*, ColorMatrixType, const Vector<float>&);

        RefPtr<FilterEffect> m_in;
        ColorMatrixType m_type;
        Vector<float> m_values;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFEColorMatrix_h
