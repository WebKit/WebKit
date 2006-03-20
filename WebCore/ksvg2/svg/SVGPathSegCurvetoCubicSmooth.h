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

#ifndef KSVG_SVGPathSegCurvetoCubicSmoothImpl_H
#define KSVG_SVGPathSegCurvetoCubicSmoothImpl_H
#if SVG_SUPPORT

#include "SVGPathSeg.h"

namespace WebCore
{
    class SVGPathSegCurvetoCubicSmoothAbs : public SVGPathSeg
    {
    public:
        SVGPathSegCurvetoCubicSmoothAbs(const SVGStyledElement *context = 0);
        virtual ~SVGPathSegCurvetoCubicSmoothAbs();

        virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_CUBIC_SMOOTH_ABS; }
        virtual StringImpl *pathSegTypeAsLetter() const { return new StringImpl("S"); }
        virtual DeprecatedString toString() const { return DeprecatedString::fromLatin1("S %1 %2 %3 %4").arg(m_x2).arg(m_y2).arg(m_x).arg(m_y); }

        void setX(double);
        double x() const;

        void setY(double);
        double y() const;

        void setX2(double);
        double x2() const;

        void setY2(double);
        double y2() const;

    private:
        double m_x;
        double m_y;
        double m_x2;
        double m_y2;
    };

    class SVGPathSegCurvetoCubicSmoothRel : public SVGPathSeg
    { 
    public:
        SVGPathSegCurvetoCubicSmoothRel(const SVGStyledElement *context = 0);
        virtual ~SVGPathSegCurvetoCubicSmoothRel();

        virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_CUBIC_SMOOTH_REL; }
        virtual StringImpl *pathSegTypeAsLetter() const { return new StringImpl("s"); }
        virtual DeprecatedString toString() const { return DeprecatedString::fromLatin1("s %1 %2 %3 %4").arg(m_x2).arg(m_y2).arg(m_x).arg(m_y); }

        void setX(double);
        double x() const;

        void setY(double);
        double y() const;

        void setX2(double);
        double x2() const;

        void setY2(double);
        double y2() const;

    private:
        double m_x;
        double m_y;
        double m_x2;
        double m_y2;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
