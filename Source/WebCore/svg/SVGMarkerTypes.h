/*
 * Copyright (C) 2018 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SVGAngleValue.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

enum SVGMarkerUnitsType {
    SVGMarkerUnitsUnknown = 0,
    SVGMarkerUnitsUserSpaceOnUse,
    SVGMarkerUnitsStrokeWidth
};

enum SVGMarkerOrientType {
    SVGMarkerOrientUnknown = 0,
    SVGMarkerOrientAuto,
    SVGMarkerOrientAngle,
    SVGMarkerOrientAutoStartReverse,
    
    // Add new elements before here.
    SVGMarkerOrientMax
};
    
template<>
struct SVGPropertyTraits<SVGMarkerUnitsType> {
    static unsigned highestEnumValue() { return SVGMarkerUnitsStrokeWidth; }
    static String toString(SVGMarkerUnitsType type)
    {
        switch (type) {
        case SVGMarkerUnitsUnknown:
            return emptyString();
        case SVGMarkerUnitsUserSpaceOnUse:
            return "userSpaceOnUse"_s;
        case SVGMarkerUnitsStrokeWidth:
            return "strokeWidth"_s;
        }
        
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    static SVGMarkerUnitsType fromString(const String& value)
    {
        if (value == "userSpaceOnUse")
            return SVGMarkerUnitsUserSpaceOnUse;
        if (value == "strokeWidth")
            return SVGMarkerUnitsStrokeWidth;
        return SVGMarkerUnitsUnknown;
    }
};

template<>
struct SVGPropertyTraits<SVGMarkerOrientType> {
    static unsigned highestEnumValue() { return SVGMarkerOrientAutoStartReverse; }
    static SVGMarkerOrientType fromString(const String& value, SVGAngleValue& angle)
    {
        if (value == "auto")
            return SVGMarkerOrientAuto;
        if (value == "auto-start-reverse")
            return SVGMarkerOrientAutoStartReverse;
        auto setValueResult = angle.setValueAsString(value);
        if (setValueResult.hasException())
            return SVGMarkerOrientUnknown;
        return SVGMarkerOrientAngle;
    }
};

template<>
inline unsigned SVGIDLEnumLimits<SVGMarkerOrientType>::highestExposedEnumValue() { return SVGMarkerOrientAngle; }

template<>
struct SVGPropertyTraits<std::pair<SVGAngleValue, unsigned>> {
    static std::pair<SVGAngleValue, unsigned> initialValue() { return { }; }
    static std::pair<SVGAngleValue, unsigned> fromString(const String& string)
    {
        SVGAngleValue angle;
        SVGMarkerOrientType orientType = SVGPropertyTraits<SVGMarkerOrientType>::fromString(string, angle);
        if (orientType != SVGMarkerOrientAngle)
            angle = { };
        return std::pair<SVGAngleValue, unsigned>(angle, orientType);
    }
    static Optional<std::pair<SVGAngleValue, unsigned>> parse(const QualifiedName&, const String&) { ASSERT_NOT_REACHED(); return initialValue(); }
    static String toString(const std::pair<SVGAngleValue, unsigned>&) { ASSERT_NOT_REACHED(); return emptyString(); }
};

} // namespace WebCore
