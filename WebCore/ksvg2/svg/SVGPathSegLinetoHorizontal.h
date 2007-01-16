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

#ifndef SVGPathSegLinetoHorizontal_h
#define SVGPathSegLinetoHorizontal_h

#ifdef SVG_SUPPORT

#include "SVGPathSeg.h"

namespace WebCore
{
    class SVGPathSegLinetoHorizontalAbs : public SVGPathSeg
    {
    public:
        SVGPathSegLinetoHorizontalAbs(double x);
        virtual ~SVGPathSegLinetoHorizontalAbs();

        virtual unsigned short pathSegType() const { return PATHSEG_LINETO_HORIZONTAL_ABS; }
        virtual String pathSegTypeAsLetter() const { return "H"; }
        virtual String toString() const { return String::format("H %.6lg", m_x); }

        void setX(double);
        double x() const;

    private:
        double m_x;
    };

    class SVGPathSegLinetoHorizontalRel : public SVGPathSeg
    {
    public:
        SVGPathSegLinetoHorizontalRel(double x);
        virtual ~SVGPathSegLinetoHorizontalRel();

        virtual unsigned short pathSegType() const { return PATHSEG_LINETO_HORIZONTAL_REL; }
        virtual String pathSegTypeAsLetter() const { return "h"; }
        virtual String toString() const { return String::format("h %.6lg", m_x); }

        void setX(double);
        double x() const;

    private:
        double m_x;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
