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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGPathSegCurvetoCubicSmooth_h
#define SVGPathSegCurvetoCubicSmooth_h

#if ENABLE(SVG)

#include "SVGPathSeg.h"

namespace WebCore {

    class SVGPathSegCurvetoCubicSmoothAbs : public SVGPathSeg {
    public:
        SVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2);
        virtual ~SVGPathSegCurvetoCubicSmoothAbs();

        virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_CUBIC_SMOOTH_ABS; }
        virtual String pathSegTypeAsLetter() const { return "S"; }
        virtual String toString() const { return String::format("S %.6lg %.6lg %.6lg %.6lg", m_x2, m_y2, m_x, m_y); }

        void setX(float);
        float x() const;

        void setY(float);
        float y() const;

        void setX2(float);
        float x2() const;

        void setY2(float);
        float y2() const;

    private:
        float m_x;
        float m_y;
        float m_x2;
        float m_y2;
    };

    class SVGPathSegCurvetoCubicSmoothRel : public SVGPathSeg { 
    public:
        SVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2);
        virtual ~SVGPathSegCurvetoCubicSmoothRel();

        virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_CUBIC_SMOOTH_REL; }
        virtual String pathSegTypeAsLetter() const { return "s"; }
        virtual String toString() const { return String::format("s %.6lg %.6lg %.6lg %.6lg", m_x2, m_y2, m_x, m_y); }

        void setX(float);
        float x() const;

        void setY(float);
        float y() const;

        void setX2(float);
        float x2() const;

        void setY2(float);
        float y2() const;

    private:
        float m_x;
        float m_y;
        float m_x2;
        float m_y2;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
