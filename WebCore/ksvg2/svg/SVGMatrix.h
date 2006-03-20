/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGMatrixImpl_H
#define KSVG_SVGMatrixImpl_H
#if SVG_SUPPORT

#include <qmatrix.h>

#include "Shared.h"

namespace WebCore
{
    class SVGMatrix : public Shared<SVGMatrix>
    { 
    public:
        SVGMatrix();
        SVGMatrix(double a, double b, double c, double d, double e, double f);
        SVGMatrix(QMatrix mat);
        virtual ~SVGMatrix();

        void setA(double a);
        double a() const;

        void setB(double b);
        double b() const;

        void setC(double c);
        double c() const;

        void setD(double d);
        double d() const;

        void setE(double e);
        double e() const;

        void setF(double f);
        double f() const;

        void copy(const SVGMatrix *other);

        SVGMatrix *inverse();

        // Pre-multiplied operations, as per the specs.
        SVGMatrix *multiply(const SVGMatrix *secondMatrix);
        SVGMatrix *translate(double x, double y);
        SVGMatrix *scale(double scaleFactor);
        SVGMatrix *scaleNonUniform(double scaleFactorX, double scaleFactorY);
        SVGMatrix *rotate(double angle);
        SVGMatrix *rotateFromVector(double x, double y);
        SVGMatrix *flipX();
        SVGMatrix *flipY();
        SVGMatrix *skewX(double angle);
        SVGMatrix *skewY(double angle);

        // Post-multiplied operations
        SVGMatrix *postMultiply(const SVGMatrix *secondMatrix);
        SVGMatrix *postTranslate(double x, double y);
        SVGMatrix *postScale(double scaleFactor);
        SVGMatrix *postScaleNonUniform(double scaleFactorX, double scaleFactorY);
        SVGMatrix *postRotate(double angle);
        SVGMatrix *postRotateFromVector(double x, double y);
        SVGMatrix *postFlipX();
        SVGMatrix *postFlipY();
        SVGMatrix *postSkewX(double angle);
        SVGMatrix *postSkewY(double angle);

        void reset();

        // KSVG helper method
        QMatrix &qmatrix();
        const QMatrix &qmatrix() const;

        // Determine the scaling component of the matrix and factor it out. After
        // this operation, the matrix has x and y scale of one.
        void removeScale(double *xScale, double *yScale);

    private:
        void setMatrix(QMatrix mat);
        QMatrix m_mat;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
