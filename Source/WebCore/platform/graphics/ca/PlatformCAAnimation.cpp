/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PlatformCAAnimation.h"

#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, PlatformCAAnimation::AnimationType type)
{
    switch (type) {
    case PlatformCAAnimation::Basic: ts << "basic"; break;
    case PlatformCAAnimation::Group: ts << "group"; break;
    case PlatformCAAnimation::Keyframe: ts << "keyframe"; break;
    case PlatformCAAnimation::Spring: ts << "spring"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PlatformCAAnimation::FillModeType fillMode)
{
    switch (fillMode) {
    case PlatformCAAnimation::NoFillMode: ts << "none"; break;
    case PlatformCAAnimation::Forwards: ts << "forwards"; break;
    case PlatformCAAnimation::Backwards: ts << "backwards"; break;
    case PlatformCAAnimation::Both: ts << "both"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PlatformCAAnimation::ValueFunctionType valueFunctionType)
{
    switch (valueFunctionType) {
    case PlatformCAAnimation::NoValueFunction: ts << "none"; break;
    case PlatformCAAnimation::RotateX: ts << "rotateX"; break;
    case PlatformCAAnimation::RotateY: ts << "rotateY"; break;
    case PlatformCAAnimation::RotateZ: ts << "rotateX"; break;
    case PlatformCAAnimation::ScaleX: ts << "scaleX"; break;
    case PlatformCAAnimation::ScaleY: ts << "scaleY"; break;
    case PlatformCAAnimation::ScaleZ: ts << "scaleX"; break;
    case PlatformCAAnimation::Scale: ts << "scale"; break;
    case PlatformCAAnimation::TranslateX: ts << "translateX"; break;
    case PlatformCAAnimation::TranslateY: ts << "translateY"; break;
    case PlatformCAAnimation::TranslateZ: ts << "translateZ"; break;
    case PlatformCAAnimation::Translate: ts << "translate"; break;
    }
    return ts;
}

bool PlatformCAAnimation::isBasicAnimation() const
{
    return animationType() == Basic || animationType() == Spring;
}

PlatformCAAnimation::KeyPath::KeyPath(const String& stringRepresentation)
{
    if (stringRepresentation == "transform"_s)
        animatedProperty = AnimatedProperty::Transform;
    else if (stringRepresentation == "opacity"_s)
        animatedProperty = AnimatedProperty::Opacity;
    else if (stringRepresentation == "backgroundColor"_s)
        animatedProperty = AnimatedProperty::BackgroundColor;
    else if (stringRepresentation.startsWith("filters.filter_"_s)) {
        size_t underscoreIndex = 15;
        auto dotIndex = stringRepresentation.find('.', underscoreIndex);
        if (dotIndex == notFound || dotIndex <= underscoreIndex)
            return;

        auto indexString = stringRepresentation.substring(underscoreIndex, dotIndex - underscoreIndex);
        auto parsedIndex = parseInteger<int>(indexString);
        if (!parsedIndex)
            return;

        auto filterOperationTypeString = stringRepresentation.substring(dotIndex + 1);
        auto parsedFilterOperationType = PlatformCAFilters::filterOperationTypeFromAnimatedFilterPropertyName(filterOperationTypeString);
        if (parsedFilterOperationType == FilterOperation::Type::None)
            return;

        animatedProperty = AnimatedProperty::Filter;
        filterOperationType = parsedFilterOperationType;
        index = *parsedIndex;
    }
#if ENABLE(FILTERS_LEVEL_2)
    else if (stringRepresentation == "backdropFilters"_s)
        animatedProperty = AnimatedProperty::WebkitBackdropFilter;
#endif
}

String PlatformCAAnimation::KeyPath::string() const
{
    switch (animatedProperty) {
    case AnimatedProperty::Translate:
    case AnimatedProperty::Scale:
    case AnimatedProperty::Rotate:
    case AnimatedProperty::Transform:
        return "transform"_s;
    case AnimatedProperty::Opacity:
        return "opacity"_s;
    case AnimatedProperty::BackgroundColor:
        return "backgroundColor"_s;
    case AnimatedProperty::Filter:
        return makeString("filters.filter_", index, ".", PlatformCAFilters::animatedFilterPropertyName(filterOperationType));
#if ENABLE(FILTERS_LEVEL_2)
    case AnimatedProperty::WebkitBackdropFilter:
        return "backdropFilters"_s;
#endif
    case AnimatedProperty::Invalid:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

} // namespace WebCore
