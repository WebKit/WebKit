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

#ifndef KSVG_SVGAngleImpl_H
#define KSVG_SVGAngleImpl_H

#include "Shared.h"
#include <kdom/DOMString.h>

namespace KSVG
{
    class SVGStyledElementImpl;

    class SVGAngleImpl : public Shared<SVGAngleImpl>
    {
    public:
        SVGAngleImpl(const SVGStyledElementImpl *context);
        virtual ~SVGAngleImpl();

        unsigned short unitType() const;

        void setValue(float value);
        float value() const; 

        void setValueInSpecifiedUnits(float valueInSpecifiedUnits);
        float valueInSpecifiedUnits() const;

        void setValueAsString(KDOM::DOMStringImpl *valueAsString);
        KDOM::DOMStringImpl *valueAsString() const;

        void newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits);
        void convertToSpecifiedUnits(unsigned short unitType);

        // Helpers
        static double todeg(double rad);
        static double torad(double deg);

        // Returns the angle that divides the shortest arc between the two angles.
        static double shortestArcBisector(double angle1, double angle2);

        const SVGStyledElementImpl *context() const;
        void setContext(const SVGStyledElementImpl *context);

    private:
        unsigned short m_unitType;
        float m_value;
        float m_valueInSpecifiedUnits;
        KDOM::DOMString m_valueAsString;
        const SVGStyledElementImpl *m_context;

        void calculate();
    };
};

#endif

// vim:ts=4:noet
