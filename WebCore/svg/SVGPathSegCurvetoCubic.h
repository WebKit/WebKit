/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>

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

#ifndef SVGPathSegCurvetoCubic_h
#define SVGPathSegCurvetoCubic_h

#if ENABLE(SVG)

#include "SVGPathSeg.h"

namespace WebCore {

    class SVGPathSegCurvetoCubic : public SVGPathSeg { 
    public:
        SVGPathSegCurvetoCubic(float x, float y, float x1, float y1, float x2, float y2) : SVGPathSeg() , m_x(x) , m_y(y) , m_x1(x1) , m_y1(y1) , m_x2(x2) , m_y2(y2) {}

        virtual String toString() const { return pathSegTypeAsLetter() + String::format(" %.6lg %.6lg %.6lg %.6lg %.6lg %.6lg", m_x1, m_y1, m_x2, m_y2, m_x, m_y); }

        void setX(float x) { m_x = x; }
        float x() const { return m_x; }

        void setY(float y) { m_y = y; }
        float y() const { return m_y; }

        void setX1(float x1) { m_x1 = x1; }
        float x1() const { return m_x1; }

        void setY1(float y1) { m_y1 = y1; }
        float y1() const { return m_y1; }

        void setX2(float x2) { m_x2 = x2; }
        float x2() const { return m_x2; }

        void setY2(float y2) { m_y2 = y2; }
        float y2() const { return m_y2; }

    private:
        float m_x;
        float m_y;
        float m_x1;
        float m_y1;
        float m_x2;
        float m_y2;
    };

    class SVGPathSegCurvetoCubicAbs : public SVGPathSegCurvetoCubic { 
    public:
        static PassRefPtr<SVGPathSegCurvetoCubicAbs> create(float x, float y, float x1, float y1, float x2, float y2)
        {
            return adoptRef(new SVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2));
        }

        virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_CUBIC_ABS; }
        virtual String pathSegTypeAsLetter() const { return "C"; }

    private:
        SVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2);
    };

    class SVGPathSegCurvetoCubicRel : public SVGPathSegCurvetoCubic { 
    public:
        static PassRefPtr<SVGPathSegCurvetoCubicRel> create(float x, float y, float x1, float y1, float x2, float y2)
        {
            return adoptRef(new SVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2));
        }

        virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_CUBIC_REL; }
        virtual String pathSegTypeAsLetter() const { return "c"; }

    private:
        SVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2);
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
