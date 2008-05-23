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

#ifndef SVGPathSegArc_h
#define SVGPathSegArc_h

#if ENABLE(SVG)

#include "SVGPathSeg.h"

namespace WebCore {

    class SVGPathSegArc : public SVGPathSeg {
    public:
        SVGPathSegArc(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
        : m_x(x), m_y(y), m_r1(r1), m_r2(r2), m_angle(angle), m_largeArcFlag(largeArcFlag), m_sweepFlag(sweepFlag) {}

        virtual String toString() const { return pathSegTypeAsLetter() + String::format(" %.6lg %.6lg %.6lg %d %d %.6lg %.6lg", m_r1, m_r2, m_angle, m_largeArcFlag, m_sweepFlag, m_x, m_y); }

        void setX(float x) { m_x = x; }
        float x() const { return m_x; }

        void setY(float y) { m_y = y; }
        float y() const { return m_y; }

        void setR1(float r1) { m_r1 = r1; }
        float r1() const { return m_r1; }

        void setR2(float r2) { m_r2 = r2; }
        float r2() const { return m_r2; }

        void setAngle(float angle) { m_angle = angle; }
        float angle() const { return m_angle; }

        void setLargeArcFlag(bool largeArcFlag) { m_largeArcFlag = largeArcFlag; }
        bool largeArcFlag() const { return m_largeArcFlag; }

        void setSweepFlag(bool sweepFlag) { m_sweepFlag = sweepFlag; }
        bool sweepFlag() const { return m_sweepFlag; }

    private:
        float m_x;
        float m_y;
        float m_r1;
        float m_r2;
        float m_angle;

        bool m_largeArcFlag    : 1;
        bool m_sweepFlag : 1;
    };

    class SVGPathSegArcAbs : public SVGPathSegArc {
    public:
        static PassRefPtr<SVGPathSegArcAbs> create(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
        {
            return adoptRef(new SVGPathSegArcAbs(x, y, r1, r2, angle, largeArcFlag, sweepFlag));
        }

        virtual unsigned short pathSegType() const { return PATHSEG_ARC_ABS; }
        virtual String pathSegTypeAsLetter() const { return "A"; }

    private:
        SVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag);
    };

    class SVGPathSegArcRel : public SVGPathSegArc {
    public:
        static PassRefPtr<SVGPathSegArcRel> create(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
        {
            return adoptRef(new SVGPathSegArcRel(x, y, r1, r2, angle, largeArcFlag, sweepFlag));
        }

        virtual unsigned short pathSegType() const { return PATHSEG_ARC_REL; }
        virtual String pathSegTypeAsLetter() const { return "a"; }

    private:
        SVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag);
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
