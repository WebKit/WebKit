/*
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
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
#include "MediaQueryExp.h"

#include "CSSAspectRatioValue.h"
#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "MediaFeatureNames.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline bool isFeatureValidWithIdentifier(const AtomicString& mediaFeature, const CSSParserValue& value)
{
    if (!value.id)
        return false;

    return mediaFeature == MediaFeatureNames::orientation
#if ENABLE(VIEW_MODE_CSS_MEDIA)
        || mediaFeature == MediaFeatureNames::viewMode
#endif
        || mediaFeature == MediaFeatureNames::colorGamut
        || mediaFeature == MediaFeatureNames::anyHover
        || mediaFeature == MediaFeatureNames::anyPointer
        || mediaFeature == MediaFeatureNames::hover
        || mediaFeature == MediaFeatureNames::invertedColors
        || mediaFeature == MediaFeatureNames::pointer;
}

static inline bool isFeatureValidWithNonNegativeLengthOrNumber(const AtomicString& mediaFeature, const CSSParserValue& value)
{
    if (!(CSSPrimitiveValue::isLength(value.unit) || value.unit == CSSPrimitiveValue::CSS_NUMBER) || value.fValue < 0)
        return false;

    return mediaFeature == MediaFeatureNames::height
        || mediaFeature == MediaFeatureNames::maxHeight
        || mediaFeature == MediaFeatureNames::minHeight
        || mediaFeature == MediaFeatureNames::width
        || mediaFeature == MediaFeatureNames::maxWidth
        || mediaFeature == MediaFeatureNames::minWidth
        || mediaFeature == MediaFeatureNames::deviceHeight
        || mediaFeature == MediaFeatureNames::maxDeviceHeight
        || mediaFeature == MediaFeatureNames::minDeviceHeight
        || mediaFeature == MediaFeatureNames::deviceWidth
        || mediaFeature == MediaFeatureNames::maxDeviceWidth
        || mediaFeature == MediaFeatureNames::minDeviceWidth;
}

static inline bool isFeatureValidWithDensity(const AtomicString& mediaFeature, const CSSParserValue& value)
{
    if (!CSSPrimitiveValue::isResolution(value.unit) || value.fValue <= 0)
        return false;

    return mediaFeature == MediaFeatureNames::resolution
        || mediaFeature == MediaFeatureNames::maxResolution
        || mediaFeature == MediaFeatureNames::minResolution;
}

static inline bool isFeatureValidWithNonNegativeInteger(const AtomicString& mediaFeature, const CSSParserValue& value)
{
    if (!value.isInt || value.fValue < 0)
        return false;

    return mediaFeature == MediaFeatureNames::color
        || mediaFeature == MediaFeatureNames::maxColor
        || mediaFeature == MediaFeatureNames::minColor
        || mediaFeature == MediaFeatureNames::colorIndex
        || mediaFeature == MediaFeatureNames::maxColorIndex
        || mediaFeature == MediaFeatureNames::minColorIndex
        || mediaFeature == MediaFeatureNames::minMonochrome
        || mediaFeature == MediaFeatureNames::maxMonochrome;
}

static inline bool isFeatureValidWithNonNegativeNumber(const AtomicString& mediaFeature, const CSSParserValue& value)
{
    if (value.unit != CSSPrimitiveValue::CSS_NUMBER || value.fValue < 0)
        return false;

    return mediaFeature == MediaFeatureNames::transform2d
        || mediaFeature == MediaFeatureNames::transform3d
        || mediaFeature == MediaFeatureNames::transition
        || mediaFeature == MediaFeatureNames::animation
        || mediaFeature == MediaFeatureNames::devicePixelRatio
        || mediaFeature == MediaFeatureNames::maxDevicePixelRatio
        || mediaFeature == MediaFeatureNames::minDevicePixelRatio;
}

static inline bool isFeatureValidWithZeroOrOne(const AtomicString& mediaFeature, const CSSParserValue& value)
{
    if (!value.isInt || !(value.fValue == 1 || !value.fValue))
        return false;

    return mediaFeature == MediaFeatureNames::grid;
}

static inline bool isAspectRatioFeature(const AtomicString& mediaFeature)
{
    return mediaFeature == MediaFeatureNames::aspectRatio
        || mediaFeature == MediaFeatureNames::deviceAspectRatio
        || mediaFeature == MediaFeatureNames::minAspectRatio
        || mediaFeature == MediaFeatureNames::maxAspectRatio
        || mediaFeature == MediaFeatureNames::minDeviceAspectRatio
        || mediaFeature == MediaFeatureNames::maxDeviceAspectRatio;
}

static inline bool isFeatureValidWithoutValue(const AtomicString& mediaFeature)
{
    // Media features that are prefixed by min/max cannot be used without a value.
    return mediaFeature == MediaFeatureNames::anyHover
        || mediaFeature == MediaFeatureNames::anyPointer
        || mediaFeature == MediaFeatureNames::monochrome
        || mediaFeature == MediaFeatureNames::color
        || mediaFeature == MediaFeatureNames::colorIndex
        || mediaFeature == MediaFeatureNames::grid
        || mediaFeature == MediaFeatureNames::height
        || mediaFeature == MediaFeatureNames::width
        || mediaFeature == MediaFeatureNames::deviceHeight
        || mediaFeature == MediaFeatureNames::deviceWidth
        || mediaFeature == MediaFeatureNames::orientation
        || mediaFeature == MediaFeatureNames::aspectRatio
        || mediaFeature == MediaFeatureNames::deviceAspectRatio
        || mediaFeature == MediaFeatureNames::hover
        || mediaFeature == MediaFeatureNames::transform2d
        || mediaFeature == MediaFeatureNames::transform3d
        || mediaFeature == MediaFeatureNames::transition
        || mediaFeature == MediaFeatureNames::animation
        || mediaFeature == MediaFeatureNames::invertedColors
#if ENABLE(VIEW_MODE_CSS_MEDIA)
        || mediaFeature == MediaFeatureNames::viewMode
#endif
        || mediaFeature == MediaFeatureNames::pointer
        || mediaFeature == MediaFeatureNames::devicePixelRatio
        || mediaFeature == MediaFeatureNames::resolution
        || mediaFeature == MediaFeatureNames::videoPlayableInline;
}

static inline bool isFeatureValidWithNumberWithUnit(const AtomicString& mediaFeature, const CSSParserValue& value)
{
    return isFeatureValidWithDensity(mediaFeature, value) || isFeatureValidWithNonNegativeLengthOrNumber(mediaFeature, value);
}

static inline bool isFeatureValidWithNumber(const AtomicString& mediaFeature, const CSSParserValue& value)
{
    return isFeatureValidWithNonNegativeInteger(mediaFeature, value) || isFeatureValidWithNonNegativeNumber(mediaFeature, value) || isFeatureValidWithZeroOrOne(mediaFeature, value);
}

static inline bool isSlash(CSSParserValue& value)
{
    return value.unit == CSSParserValue::Operator && value.iValue == '/';
}

static inline bool isPositiveIntegerValue(CSSParserValue& value)
{
    return value.unit == CSSPrimitiveValue::CSS_NUMBER && value.fValue > 0 && value.isInt;
}

MediaQueryExpression::MediaQueryExpression(const AtomicString& mediaFeature, CSSParserValueList* valueList)
    : m_mediaFeature(mediaFeature)
{
    if (!valueList) {
        if (isFeatureValidWithoutValue(mediaFeature))
            m_isValid = true;
    } else if (valueList->size() == 1) {
        auto& value = *valueList->valueAt(0);
        if (isFeatureValidWithIdentifier(mediaFeature, value)) {
            m_value = CSSPrimitiveValue::createIdentifier(value.id);
            m_isValid = true;
        } else if (isFeatureValidWithNumberWithUnit(mediaFeature, value) || isFeatureValidWithNonNegativeLengthOrNumber(mediaFeature, value)) {
            m_value = CSSPrimitiveValue::create(value.fValue, (CSSPrimitiveValue::UnitTypes) value.unit);
            m_isValid = true;
        } else if (isFeatureValidWithNumber(mediaFeature, value)) {
            // FIXME: Can we merge this with the case above?
            m_value = CSSPrimitiveValue::create(value.fValue, CSSPrimitiveValue::CSS_NUMBER);
            m_isValid = true;
        }
    } else if (valueList->size() == 3 && isAspectRatioFeature(mediaFeature)) {
        auto& numerator = *valueList->valueAt(0);
        auto& slash = *valueList->valueAt(1);
        auto& denominator = *valueList->valueAt(2);
        if (isPositiveIntegerValue(numerator) && isSlash(slash) && isPositiveIntegerValue(denominator)) {
            m_value = CSSAspectRatioValue::create(numerator.fValue, denominator.fValue);
            m_isValid = true;
        }
    }
}

String MediaQueryExpression::serialize() const
{
    if (!m_serializationCache.isNull())
        return m_serializationCache;

    StringBuilder result;
    result.append('(');
    result.append(m_mediaFeature.convertToASCIILowercase());
    if (m_value) {
        result.appendLiteral(": ");
        result.append(m_value->cssText());
    }
    result.append(')');

    m_serializationCache = result.toString();
    return m_serializationCache;
}

} // namespace
