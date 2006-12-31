/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#ifndef SVGPathSegLinetoVertical_H
#define SVGPathSegLinetoVertical_H

#ifdef SVG_SUPPORT

#include "SVGPathSeg.h"

namespace WebCore
{
    class SVGPathSegLinetoVerticalAbs : public SVGPathSeg
    {
    public:
        SVGPathSegLinetoVerticalAbs(double y);
        virtual~SVGPathSegLinetoVerticalAbs();

        virtual unsigned short pathSegType() const { return PATHSEG_LINETO_VERTICAL_ABS; }
        virtual String pathSegTypeAsLetter() const { return "V"; }
        virtual String toString() const { return String::format("V %.6lg", m_y); }

        void setY(double);
        double y() const;

    private:
        double m_y;
    };

    class SVGPathSegLinetoVerticalRel : public SVGPathSeg
    {
    public:
        SVGPathSegLinetoVerticalRel(double y);
        virtual ~SVGPathSegLinetoVerticalRel();

        virtual unsigned short pathSegType() const { return PATHSEG_LINETO_VERTICAL_REL; }
        virtual String pathSegTypeAsLetter() const { return "v"; }
        virtual String toString() const { return String::format("v %.6lg", m_y); }

        void setY(double);
        double y() const;

    private:
        double m_y;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
