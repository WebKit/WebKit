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

#ifndef KSVG_SVGPathSegArcImpl_H
#define KSVG_SVGPathSegArcImpl_H
#if SVG_SUPPORT

#include "SVGPathSeg.h"

namespace WebCore
{
    class SVGPathSegArcAbs : public SVGPathSeg
    {
    public:
        SVGPathSegArcAbs(const SVGStyledElement *context = 0);
        virtual ~SVGPathSegArcAbs();

        virtual unsigned short pathSegType() const { return PATHSEG_ARC_ABS; }
        virtual String pathSegTypeAsLetter() const { return "A"; }
        virtual String toString() const { return String::sprintf("A %.6lg %.6lg %.6lg %d %d %.6lg %.6lg", m_r1, m_r2, m_angle, m_largeArcFlag, m_sweepFlag, m_x, m_y); }

        void setX(double x);
        double x() const;

        void setY(double y);
        double y() const;

        void setR1(double r1);
        double r1() const;

        void setR2(double r2);
        double r2() const;

        void setAngle(double angle);
        double angle() const;

        void setLargeArcFlag(bool largeArcFlag);
        bool largeArcFlag() const;

        void setSweepFlag(bool sweepFlag);
        bool sweepFlag() const;

    private:
        double m_x;
        double m_y;
        double m_r1;
        double m_r2;
        double m_angle;

        bool m_largeArcFlag    : 1;
        bool m_sweepFlag : 1;
    };

    class SVGPathSegArcRel : public SVGPathSeg
    {
    public:
        SVGPathSegArcRel(const SVGStyledElement *context = 0);
        virtual ~SVGPathSegArcRel();

        virtual unsigned short pathSegType() const { return PATHSEG_ARC_REL; }
        virtual String pathSegTypeAsLetter() const { return "a"; }
        virtual String toString() const { return String::sprintf("a %.6lg %.6lg %.6lg %d %d %.6lg %.6lg", m_r1, m_r2, m_angle, m_largeArcFlag, m_sweepFlag, m_x, m_y); }

        void setX(double x);
        double x() const;

        void setY(double y);
        double y() const;

        void setR1(double r1);
        double r1() const;

        void setR2(double r2);
        double r2() const;

        void setAngle(double angle);
        double angle() const;

        void setLargeArcFlag(bool largeArcFlag);
        bool largeArcFlag() const;

        void setSweepFlag(bool sweepFlag);
        bool sweepFlag() const;

    private:
        double m_x;
        double m_y;
        double m_r1;
        double m_r2;
        double m_angle;

        bool m_largeArcFlag : 1;
        bool m_sweepFlag : 1;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
