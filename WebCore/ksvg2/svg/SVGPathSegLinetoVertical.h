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

#ifndef KSVG_SVGPathSegLinetoVerticalImpl_H
#define KSVG_SVGPathSegLinetoVerticalImpl_H
#if SVG_SUPPORT

#include <ksvg2/svg/SVGPathSeg.h>

namespace WebCore
{
    class SVGPathSegLinetoVerticalAbs : public SVGPathSeg
    {
    public:
        SVGPathSegLinetoVerticalAbs(const SVGStyledElement *context = 0);
        virtual~SVGPathSegLinetoVerticalAbs();

        virtual unsigned short pathSegType() const { return PATHSEG_LINETO_VERTICAL_ABS; }
        virtual StringImpl *pathSegTypeAsLetter() const { return new StringImpl("V"); }
        virtual DeprecatedString toString() const { return DeprecatedString::fromLatin1("V %1").arg(m_y); }

        void setY(double);
        double y() const;

    private:
        double m_y;
    };

    class SVGPathSegLinetoVerticalRel : public SVGPathSeg
    {
    public:
        SVGPathSegLinetoVerticalRel(const SVGStyledElement *context = 0);
        virtual ~SVGPathSegLinetoVerticalRel();

        virtual unsigned short pathSegType() const { return PATHSEG_LINETO_VERTICAL_REL; }
        virtual StringImpl *pathSegTypeAsLetter() const { return new StringImpl("v"); }
        virtual DeprecatedString toString() const { return DeprecatedString::fromLatin1("v %1").arg(m_y); }

        void setY(double);
        double y() const;

    private:
        double m_y;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
