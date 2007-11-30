/*
    Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>

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

#ifndef RadialGradientAttributes_h
#define RadialGradientAttributes_h

#include "GradientAttributes.h"

#if ENABLE(SVG)

namespace WebCore
{
    struct RadialGradientAttributes : GradientAttributes {
        RadialGradientAttributes()
            : m_cx(0.5)
            , m_cy(0.5)
            , m_r(0.5)
            , m_fx(0.0)
            , m_fy(0.0)
            , m_cxSet(false)
            , m_cySet(false)
            , m_rSet(false)
            , m_fxSet(false)
            , m_fySet(false) 
        {
        }

        double cx() const { return m_cx; }
        double cy() const { return m_cy; }
        double r() const { return m_r; }
        double fx() const { return m_fx; }
        double fy() const { return m_fy; }

        void setCx(double value) { m_cx = value; m_cxSet = true; }
        void setCy(double value) { m_cy = value; m_cySet = true; }
        void setR(double value) { m_r = value; m_rSet = true; }
        void setFx(double value) { m_fx = value; m_fxSet = true; }
        void setFy(double value) { m_fy = value; m_fySet = true; }

        bool hasCx() const { return m_cxSet; }
        bool hasCy() const { return m_cySet; }
        bool hasR() const { return m_rSet; }
        bool hasFx() const { return m_fxSet; }
        bool hasFy() const { return m_fySet; }

    private:
        // Properties
        double m_cx;
        double m_cy;
        double m_r;
        double m_fx;
        double m_fy;

        // Property states
        bool m_cxSet : 1;
        bool m_cySet : 1;
        bool m_rSet : 1;
        bool m_fxSet : 1;
        bool m_fySet : 1;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
