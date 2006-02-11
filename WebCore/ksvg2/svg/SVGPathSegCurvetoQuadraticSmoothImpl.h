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

#ifndef KSVG_SVGPathSegCurvetoQuadraticSmoothImpl_H
#define KSVG_SVGPathSegCurvetoQuadraticSmoothImpl_H
#if SVG_SUPPORT

#include "SVGPathSegImpl.h"

namespace WebCore
{
    class SVGPathSegCurvetoQuadraticSmoothAbsImpl : public SVGPathSegImpl
    { 
    public:
        SVGPathSegCurvetoQuadraticSmoothAbsImpl(const SVGStyledElementImpl *context = 0);
        virtual ~SVGPathSegCurvetoQuadraticSmoothAbsImpl();

        virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS; }
        virtual DOMStringImpl *pathSegTypeAsLetter() const { return new DOMStringImpl("T"); }
        virtual QString toString() const { return QString::fromLatin1("T %1 %2").arg(m_x).arg(m_y); }

        void setX(double);
        double x() const;

        void setY(double);
        double y() const;

    private:
        double m_x;
        double m_y;
    };

    class SVGPathSegCurvetoQuadraticSmoothRelImpl : public SVGPathSegImpl 
    { 
    public:
        SVGPathSegCurvetoQuadraticSmoothRelImpl(const SVGStyledElementImpl *context = 0);
        virtual ~SVGPathSegCurvetoQuadraticSmoothRelImpl();

        virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL; }
        virtual DOMStringImpl *pathSegTypeAsLetter() const { return new DOMStringImpl("t"); }
        virtual QString toString() const { return QString::fromLatin1("t %1 %2").arg(m_x).arg(m_y); }

        void setX(double);
        double x() const;

        void setY(double);
        double y() const;

    private:
        double m_x;
        double m_y;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
