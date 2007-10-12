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

#ifndef SVGLength_h
#define SVGLength_h

#if ENABLE(SVG)

#include "PlatformString.h"

namespace WebCore {

    class SVGStyledElement;

    enum SVGLengthType {
        LengthTypeUnknown = 0,
        LengthTypeNumber = 1,
        LengthTypePercentage = 2,
        LengthTypeEMS = 3,
        LengthTypeEXS = 4,
        LengthTypePX = 5,
        LengthTypeCM = 6,
        LengthTypeMM = 7,
        LengthTypeIN = 8,
        LengthTypePT = 9,
        LengthTypePC = 10
    };

    enum SVGLengthMode {
        LengthModeWidth = 0,
        LengthModeHeight,
        LengthModeOther
    };

    class SVGLength {
    public:
        // Forward declare these enums in the w3c naming scheme, for IDL generation
        enum {
            SVG_LENGTHTYPE_UNKNOWN = LengthTypeUnknown,
            SVG_LENGTHTYPE_NUMBER = LengthTypeNumber,
            SVG_LENGTHTYPE_PERCENTAGE = LengthTypePercentage,
            SVG_LENGTHTYPE_EMS = LengthTypeEMS,
            SVG_LENGTHTYPE_EXS = LengthTypeEXS,
            SVG_LENGTHTYPE_PX = LengthTypePX,
            SVG_LENGTHTYPE_CM = LengthTypeCM,
            SVG_LENGTHTYPE_MM = LengthTypeMM,
            SVG_LENGTHTYPE_IN = LengthTypeIN,
            SVG_LENGTHTYPE_PT = LengthTypePT,
            SVG_LENGTHTYPE_PC = LengthTypePC
        };

        SVGLength(const SVGStyledElement* context = 0, SVGLengthMode mode = LengthModeOther, const String& valueAsString = String());

        // 'SVGLength' functions
        SVGLengthType unitType() const;

        float value() const;
        void setValue(float);

        float valueInSpecifiedUnits() const;
        void setValueInSpecifiedUnits(float);
        
        float valueAsPercentage() const;

        String valueAsString() const;
        bool setValueAsString(const String&);

        void newValueSpecifiedUnits(unsigned short, float valueInSpecifiedUnits);
        void convertToSpecifiedUnits(unsigned short);

        // Helper functions
        static float PercentageOfViewport(float value, const SVGStyledElement*, SVGLengthMode);

        inline bool isRelative() const
        {
            SVGLengthType type = unitType();
            return (type == LengthTypePercentage || type == LengthTypeEMS || type == LengthTypeEXS);
        }
 
    private:
        float m_valueInSpecifiedUnits;
        unsigned int m_unit;

        const SVGStyledElement* m_context;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGLength_h

// vim:ts=4:noet
