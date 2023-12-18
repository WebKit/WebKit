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
    case PlatformCAAnimation::AnimationType::Basic: ts << "basic"; break;
    case PlatformCAAnimation::AnimationType::Group: ts << "group"; break;
    case PlatformCAAnimation::AnimationType::Keyframe: ts << "keyframe"; break;
    case PlatformCAAnimation::AnimationType::Spring: ts << "spring"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PlatformCAAnimation::FillModeType fillMode)
{
    switch (fillMode) {
    case PlatformCAAnimation::FillModeType::NoFillMode: ts << "none"; break;
    case PlatformCAAnimation::FillModeType::Forwards: ts << "forwards"; break;
    case PlatformCAAnimation::FillModeType::Backwards: ts << "backwards"; break;
    case PlatformCAAnimation::FillModeType::Both: ts << "both"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PlatformCAAnimation::ValueFunctionType valueFunctionType)
{
    switch (valueFunctionType) {
    case PlatformCAAnimation::ValueFunctionType::NoValueFunction: ts << "none"; break;
    case PlatformCAAnimation::ValueFunctionType::RotateX: ts << "rotateX"; break;
    case PlatformCAAnimation::ValueFunctionType::RotateY: ts << "rotateY"; break;
    case PlatformCAAnimation::ValueFunctionType::RotateZ: ts << "rotateZ"; break;
    case PlatformCAAnimation::ValueFunctionType::ScaleX: ts << "scaleX"; break;
    case PlatformCAAnimation::ValueFunctionType::ScaleY: ts << "scaleY"; break;
    case PlatformCAAnimation::ValueFunctionType::ScaleZ: ts << "scaleZ"; break;
    case PlatformCAAnimation::ValueFunctionType::Scale: ts << "scale"; break;
    case PlatformCAAnimation::ValueFunctionType::TranslateX: ts << "translateX"; break;
    case PlatformCAAnimation::ValueFunctionType::TranslateY: ts << "translateY"; break;
    case PlatformCAAnimation::ValueFunctionType::TranslateZ: ts << "translateZ"; break;
    case PlatformCAAnimation::ValueFunctionType::Translate: ts << "translate"; break;
    }
    return ts;
}

bool PlatformCAAnimation::isBasicAnimation() const
{
    return animationType() == AnimationType::Basic || animationType() == AnimationType::Spring;
}

static constexpr auto transformKeyPath = "transform"_s;
static constexpr auto opacityKeyPath = "opacity"_s;
static constexpr auto backgroundColorKeyPath = "backgroundColor"_s;
static constexpr auto filterKeyPathPrefix = "filters.filter_"_s;
static constexpr auto backdropFiltersKeyPath = "backdropFilters"_s;

String PlatformCAAnimation::makeGroupKeyPath()
{
    return emptyString();
}

String PlatformCAAnimation::makeKeyPath(AnimatedProperty animatedProperty, FilterOperation::Type filterOperationType, int index)
{
    switch (animatedProperty) {
    case AnimatedProperty::Translate:
    case AnimatedProperty::Scale:
    case AnimatedProperty::Rotate:
    case AnimatedProperty::Transform:
        return transformKeyPath;
    case AnimatedProperty::Opacity:
        return opacityKeyPath;
    case AnimatedProperty::BackgroundColor:
        return backgroundColorKeyPath;
    case AnimatedProperty::Filter:
        return makeString(filterKeyPathPrefix, index, ".", PlatformCAFilters::animatedFilterPropertyName(filterOperationType));
    case AnimatedProperty::WebkitBackdropFilter:
        return backdropFiltersKeyPath;
    case AnimatedProperty::Invalid:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

static bool isValidFilterKeyPath(const String& keyPath)
{
    if (!keyPath.startsWith(filterKeyPathPrefix))
        return false;

    size_t underscoreIndex = filterKeyPathPrefix.length();
    auto dotIndex = keyPath.find('.', underscoreIndex);
    if (dotIndex == notFound || dotIndex <= underscoreIndex)
        return false;

    auto indexString = keyPath.substring(underscoreIndex, dotIndex - underscoreIndex);
    auto parsedIndex = parseInteger<unsigned>(indexString);
    if (!parsedIndex)
        return false;

    auto filterOperationTypeString = keyPath.substring(dotIndex + 1);
    return PlatformCAFilters::isValidAnimatedFilterPropertyName(filterOperationTypeString);
}

bool PlatformCAAnimation::isValidKeyPath(const String& keyPath, AnimationType type)
{
    if (type == AnimationType::Group)
        return keyPath.isEmpty();

    if (keyPath == transformKeyPath
        || keyPath == opacityKeyPath
        || keyPath == backgroundColorKeyPath)
        return true;

    if (keyPath == backdropFiltersKeyPath)
        return true;

    if (isValidFilterKeyPath(keyPath))
        return true;

    return false;
}

} // namespace WebCore
