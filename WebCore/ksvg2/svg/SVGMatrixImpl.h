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

namespace KSVG
{
    class SVGMatrixImpl : public Shared<SVGMatrixImpl>
    { 
    public:
        SVGMatrixImpl();
        SVGMatrixImpl(double a, double b, double c, double d, double e, double f);
        SVGMatrixImpl(QMatrix mat);
        virtual ~SVGMatrixImpl();

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

        void copy(const SVGMatrixImpl *other);

        SVGMatrixImpl *inverse();

        // Pre-multiplied operations, as per the specs.
        SVGMatrixImpl *multiply(const SVGMatrixImpl *secondMatrix);
        SVGMatrixImpl *translate(double x, double y);
        SVGMatrixImpl *scale(double scaleFactor);
        SVGMatrixImpl *scaleNonUniform(double scaleFactorX, double scaleFactorY);
        SVGMatrixImpl *rotate(double angle);
        SVGMatrixImpl *rotateFromVector(double x, double y);
        SVGMatrixImpl *flipX();
        SVGMatrixImpl *flipY();
        SVGMatrixImpl *skewX(double angle);
        SVGMatrixImpl *skewY(double angle);

        // Post-multiplied operations
        SVGMatrixImpl *postMultiply(const SVGMatrixImpl *secondMatrix);
        SVGMatrixImpl *postTranslate(double x, double y);
        SVGMatrixImpl *postScale(double scaleFactor);
        SVGMatrixImpl *postScaleNonUniform(double scaleFactorX, double scaleFactorY);
        SVGMatrixImpl *postRotate(double angle);
        SVGMatrixImpl *postRotateFromVector(double x, double y);
        SVGMatrixImpl *postFlipX();
        SVGMatrixImpl *postFlipY();
        SVGMatrixImpl *postSkewX(double angle);
        SVGMatrixImpl *postSkewY(double angle);

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
