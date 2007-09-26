/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
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

#ifndef SVGAngle_h
#define SVGAngle_h
#if ENABLE(SVG)

#include "PlatformString.h"

namespace WebCore {

    class SVGStyledElement;

    class SVGAngle : public Shared<SVGAngle> {
    public:
        SVGAngle(const SVGStyledElement* context);
        virtual ~SVGAngle();
        
        enum SVGAngleType {
            SVG_ANGLETYPE_UNKNOWN           = 0,
            SVG_ANGLETYPE_UNSPECIFIED       = 1,
            SVG_ANGLETYPE_DEG               = 2,
            SVG_ANGLETYPE_RAD               = 3,
            SVG_ANGLETYPE_GRAD              = 4
        };

        SVGAngleType unitType() const;

        void setValue(float);
        float value() const; 

        void setValueInSpecifiedUnits(float valueInSpecifiedUnits);
        float valueInSpecifiedUnits() const;

        void setValueAsString(const String&);
        String valueAsString() const;

        void newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits);
        void convertToSpecifiedUnits(unsigned short unitType);

        // Helpers
        static double todeg(double rad);
        static double torad(double deg);

        // Returns the angle that divides the shortest arc between the two angles.
        static double shortestArcBisector(double angle1, double angle2);

        const SVGStyledElement* context() const;
        void setContext(const SVGStyledElement*);

    private:
        SVGAngleType m_unitType;
        float m_value;
        float m_valueInSpecifiedUnits;
        mutable String m_valueAsString;
        const SVGStyledElement* m_context;

        void calculate();
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAngle_h

// vim:ts=4:noet
