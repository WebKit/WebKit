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

#ifndef KSVG_SVGMatrix_H
#define KSVG_SVGMatrix_H

#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
    class SVGMatrixImpl;
    class SVGMatrix
    { 
    public:
        SVGMatrix();
        explicit SVGMatrix(SVGMatrixImpl *i);
        SVGMatrix(const SVGMatrix &other);
        virtual ~SVGMatrix();

        // Operators
        SVGMatrix &operator=(const SVGMatrix &other);
        bool operator==(const SVGMatrix &other) const;
        bool operator!=(const SVGMatrix &other) const;

        // 'SVGMatrix' functions
        void setA(double);
        double a() const;

        void setB(double);
        double b() const;

        void setC(double);
        double c() const;

        void setD(double);
        double d() const;

        void setE(double);
        double e() const;

        void setF(double);
        double f() const;

        SVGMatrix multiply(const SVGMatrix &secondMatrix);
        SVGMatrix inverse();
        SVGMatrix translate(double x, double y);
        SVGMatrix scale(double scaleFactor);
        SVGMatrix scaleNonUniform(double scaleFactorX, double scaleFactorY);
        SVGMatrix rotate(double angle);
        SVGMatrix rotateFromVector(double x, double y);
        SVGMatrix flipX();
        SVGMatrix flipY();
        SVGMatrix skewX(double angle);
        SVGMatrix skewY(double angle);

        // Internal
        KSVG_INTERNAL_BASE(SVGMatrix)

    protected:
        SVGMatrixImpl *impl;

    public: // EcmaScript section
        KDOM_BASECLASS_GET
        KDOM_PUT
        KDOM_CAST

        KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
        void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
    };

    KDOM_DEFINE_CAST(SVGMatrix)
};

KSVG_DEFINE_PROTOTYPE(SVGMatrixProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGMatrixProtoFunc, SVGMatrix)

#endif

// vim:ts=4:noet
