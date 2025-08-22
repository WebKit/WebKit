/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "CommonAtomStrings.h"
#include "SVGAngleValue.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

enum class SVGMarkerUnitsType : uint8_t {
    Unknown = 0,
    UserSpaceOnUse,
    StrokeWidth,
};

enum SVGMarkerOrientType {
    SVGMarkerOrientUnknown = 0,
    SVGMarkerOrientAuto,
    SVGMarkerOrientAngle,
    SVGMarkerOrientAutoStartReverse
};
    
template<>
struct SVGPropertyTraits<SVGMarkerUnitsType> {
    static unsigned highestEnumValue() { return std::underlying_type_t<SVGMarkerUnitsType>(SVGMarkerUnitsType::StrokeWidth); }
    static String toString(SVGMarkerUnitsType type)
    {
        switch (type) {
        case SVGMarkerUnitsType::Unknown:
            return emptyString();
        case SVGMarkerUnitsType::UserSpaceOnUse:
            return "userSpaceOnUse"_s;
        case SVGMarkerUnitsType::StrokeWidth:
            return "strokeWidth"_s;
        }
        
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    static SVGMarkerUnitsType fromString(const String& value)
    {
        if (value == "userSpaceOnUse"_s)
            return SVGMarkerUnitsType::UserSpaceOnUse;
        if (value == "strokeWidth"_s)
            return SVGMarkerUnitsType::StrokeWidth;
        return SVGMarkerUnitsType::Unknown;
    }
};

template<>
struct SVGPropertyTraits<SVGMarkerOrientType> {
    static const String autoStartReverseString()
    {
        static const NeverDestroyed<String> autoStartReverseString = MAKE_STATIC_STRING_IMPL("auto-start-reverse");
        return autoStartReverseString;
    }
    static unsigned highestEnumValue() { return SVGMarkerOrientAutoStartReverse; }
    static SVGMarkerOrientType fromString(const String& string)
    {
        if (string == autoAtom())
            return SVGMarkerOrientAuto;
        if (string == autoStartReverseString())
            return SVGMarkerOrientAutoStartReverse;
        return SVGMarkerOrientUnknown;
    }
    static String toString(SVGMarkerOrientType type)
    {
        if (type == SVGMarkerOrientAuto)
            return autoAtom();
        if (type == SVGMarkerOrientAutoStartReverse)
            return autoStartReverseString();
        return emptyString();
    }
};

template<>
struct SVGPropertyTraits<std::pair<SVGAngleValue, SVGMarkerOrientType>> {
    static std::pair<SVGAngleValue, SVGMarkerOrientType> fromString(const String& string)
    {
        SVGAngleValue angle;
        SVGMarkerOrientType orientType = SVGPropertyTraits<SVGMarkerOrientType>::fromString(string);
        if (orientType == SVGMarkerOrientUnknown) {
            auto result = angle.setValueAsString(string);
            if (!result.hasException())
                orientType = SVGMarkerOrientAngle;
        }
        return std::make_pair(angle, orientType);
    }
};

} // namespace WebCore
