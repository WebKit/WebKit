/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGPathSegLinetoHorizontal_h
#define SVGPathSegLinetoHorizontal_h

#if ENABLE(SVG)

#include "SVGPathSeg.h"

namespace WebCore {

    class SVGPathSegLinetoHorizontal : public SVGPathSeg {
    public:
        SVGPathSegLinetoHorizontal(float x) : SVGPathSeg(), m_x(x) {}

        virtual String toString() const { return pathSegTypeAsLetter() + String::format(" %.6lg", m_x); }

        void setX(float x) { m_x = x; }
        float x() const { return m_x; }

    private:
        float m_x;
    };

    class SVGPathSegLinetoHorizontalAbs : public SVGPathSegLinetoHorizontal {
    public:
        static PassRefPtr<SVGPathSegLinetoHorizontalAbs> create(float x) { return adoptRef(new SVGPathSegLinetoHorizontalAbs(x)); }

        virtual unsigned short pathSegType() const { return PATHSEG_LINETO_HORIZONTAL_ABS; }
        virtual String pathSegTypeAsLetter() const { return "H"; }

    private:
        SVGPathSegLinetoHorizontalAbs(float x);
    };

    class SVGPathSegLinetoHorizontalRel : public SVGPathSegLinetoHorizontal {
    public:
        static PassRefPtr<SVGPathSegLinetoHorizontalRel> create(float x) { return adoptRef(new SVGPathSegLinetoHorizontalRel(x)); }

        virtual unsigned short pathSegType() const { return PATHSEG_LINETO_HORIZONTAL_REL; }
        virtual String pathSegTypeAsLetter() const { return "h"; }

    private:
        SVGPathSegLinetoHorizontalRel(float x);
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
