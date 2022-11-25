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
#include "MediaQueryExpression.h"

#include "CSSAspectRatioValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserHelpers.h"
#include "MediaFeatureNames.h"
#include "MediaQueryParserContext.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

static inline bool isValidValueForIdentMediaFeature(const AtomString& feature, const CSSPrimitiveValue& value)
{
    auto valueID = value.valueID();

    if (feature == MediaFeatureNames::orientation)
        return valueID == CSSValuePortrait || valueID == CSSValueLandscape;
    if (feature == MediaFeatureNames::colorGamut)
        return valueID == CSSValueSRGB || valueID == CSSValueP3 || valueID == CSSValueRec2020;
    if (feature == MediaFeatureNames::anyHover || feature == MediaFeatureNames::hover)
        return valueID == CSSValueHover || valueID == CSSValueNone;
    if (feature == MediaFeatureNames::anyPointer || feature == MediaFeatureNames::pointer)
        return valueID == CSSValueFine || valueID == CSSValueCoarse || valueID == CSSValueNone;
    if (feature == MediaFeatureNames::invertedColors)
        return valueID == CSSValueInverted || valueID == CSSValueNone;
#if ENABLE(APPLICATION_MANIFEST)
    if (feature == MediaFeatureNames::displayMode)
        return valueID == CSSValueFullscreen || valueID == CSSValueStandalone || valueID == CSSValueMinimalUi || valueID == CSSValueBrowser;
#endif
#if ENABLE(DARK_MODE_CSS)
    if (feature == MediaFeatureNames::prefersColorScheme)
        return valueID == CSSValueLight || valueID == CSSValueDark;
#endif
    if (feature == MediaFeatureNames::prefersContrast)
        return valueID == CSSValueNoPreference || valueID == CSSValueMore || valueID == CSSValueLess;
    if (feature == MediaFeatureNames::prefersReducedMotion)
        return valueID == CSSValueNoPreference || valueID == CSSValueReduce;
    if (feature == MediaFeatureNames::prefersDarkInterface)
        return valueID == CSSValuePrefers || valueID == CSSValueNoPreference;
    if (feature == MediaFeatureNames::dynamicRange)
        return valueID == CSSValueHigh || valueID == CSSValueStandard;
    if (feature == MediaFeatureNames::scan)
        return valueID == CSSValueProgressive || valueID == CSSValueInterlace;
    if (feature == MediaFeatureNames::forcedColors)
        return valueID == CSSValueNone || valueID == CSSValueActive;

    return false;
}

static inline bool featureWithValidIdent(const AtomString& mediaFeature, const CSSPrimitiveValue& value, const MediaQueryParserContext& context)
{
    if (value.primitiveType() != CSSUnitType::CSS_IDENT || !isValidValueForIdentMediaFeature(mediaFeature, value))
        return false;

    return mediaFeature == MediaFeatureNames::orientation
        || mediaFeature == MediaFeatureNames::colorGamut
        || mediaFeature == MediaFeatureNames::anyHover
        || mediaFeature == MediaFeatureNames::anyPointer
        || mediaFeature == MediaFeatureNames::hover
        || mediaFeature == MediaFeatureNames::invertedColors
        || mediaFeature == MediaFeatureNames::pointer
#if ENABLE(APPLICATION_MANIFEST)
        || mediaFeature == MediaFeatureNames::displayMode
#endif
#if ENABLE(DARK_MODE_CSS)
        || mediaFeature == MediaFeatureNames::prefersColorScheme
#endif
        || mediaFeature == MediaFeatureNames::prefersContrast
        || mediaFeature == MediaFeatureNames::prefersReducedMotion
        || (mediaFeature == MediaFeatureNames::prefersDarkInterface && (context.useSystemAppearance || isUASheetBehavior(context.mode)))
        || mediaFeature == MediaFeatureNames::dynamicRange
        || mediaFeature == MediaFeatureNames::scan
        || mediaFeature == MediaFeatureNames::forcedColors;
}

static inline bool featureWithValidDensity(const String& mediaFeature, const CSSPrimitiveValue& value)
{
    if (!value.isResolution() || value.doubleValue() <= 0)
        return false;
    
    return mediaFeature == MediaFeatureNames::resolution
        || mediaFeature == MediaFeatureNames::minResolution
        || mediaFeature == MediaFeatureNames::maxResolution;
}

static inline bool featureWithValidPositiveLength(const String& mediaFeature, const CSSPrimitiveValue& value)
{
    if (!(value.isLength() || (value.isNumberOrInteger() && !value.doubleValue())) || value.doubleValue() < 0)
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
        || mediaFeature == MediaFeatureNames::minDeviceWidth
        || mediaFeature == MediaFeatureNames::maxDeviceWidth;
}

static inline bool featureExpectingPositiveInteger(const String& mediaFeature)
{
    return mediaFeature == MediaFeatureNames::color
        || mediaFeature == MediaFeatureNames::maxColor
        || mediaFeature == MediaFeatureNames::minColor
        || mediaFeature == MediaFeatureNames::colorIndex
        || mediaFeature == MediaFeatureNames::maxColorIndex
        || mediaFeature == MediaFeatureNames::minColorIndex
        || mediaFeature == MediaFeatureNames::monochrome
        || mediaFeature == MediaFeatureNames::maxMonochrome
        || mediaFeature == MediaFeatureNames::minMonochrome;
}

static inline bool featureWithPositiveInteger(const String& mediaFeature, const CSSPrimitiveValue& value)
{
    if (!value.isInteger())
        return false;
    return featureExpectingPositiveInteger(mediaFeature);
}

static inline bool featureWithPositiveNumber(const String& mediaFeature, const CSSPrimitiveValue& value)
{
    if (!value.isNumberOrInteger())
        return false;
    
    return mediaFeature == MediaFeatureNames::transform3d
        || mediaFeature == MediaFeatureNames::devicePixelRatio
        || mediaFeature == MediaFeatureNames::maxDevicePixelRatio
        || mediaFeature == MediaFeatureNames::minDevicePixelRatio
        || mediaFeature == MediaFeatureNames::transition
        || mediaFeature == MediaFeatureNames::animation
        || mediaFeature == MediaFeatureNames::transform2d;
}

static inline bool featureWithZeroOrOne(const String& mediaFeature, const CSSPrimitiveValue& value)
{
    if (!value.isNumberOrInteger() || !(value.doubleValue() == 1 || !value.doubleValue()))
        return false;
    
    return mediaFeature == MediaFeatureNames::grid;
}

static inline bool isAspectRatioFeature(const AtomString& mediaFeature)
{
    return mediaFeature == MediaFeatureNames::aspectRatio
        || mediaFeature == MediaFeatureNames::deviceAspectRatio
        || mediaFeature == MediaFeatureNames::minAspectRatio
        || mediaFeature == MediaFeatureNames::maxAspectRatio
        || mediaFeature == MediaFeatureNames::minDeviceAspectRatio
        || mediaFeature == MediaFeatureNames::maxDeviceAspectRatio;
}

static inline bool isFeatureValidWithoutValue(const AtomString& mediaFeature, const MediaQueryParserContext& context)
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
        || mediaFeature == MediaFeatureNames::pointer
        || mediaFeature == MediaFeatureNames::prefersContrast
        || mediaFeature == MediaFeatureNames::prefersReducedMotion
        || (mediaFeature == MediaFeatureNames::prefersDarkInterface && (context.useSystemAppearance || isUASheetBehavior(context.mode)))
#if ENABLE(DARK_MODE_CSS)
        || (mediaFeature == MediaFeatureNames::prefersColorScheme)
#endif
        || mediaFeature == MediaFeatureNames::devicePixelRatio
        || mediaFeature == MediaFeatureNames::resolution
#if ENABLE(APPLICATION_MANIFEST)
        || mediaFeature == MediaFeatureNames::displayMode
#endif
        || mediaFeature == MediaFeatureNames::scan
        || mediaFeature == MediaFeatureNames::forcedColors
        || mediaFeature == MediaFeatureNames::videoPlayableInline;
}

inline RefPtr<CSSPrimitiveValue> consumeFirstValue(const String& mediaFeature, CSSParserTokenRange& range)
{
    if (auto value = CSSPropertyParserHelpers::consumeNonNegativeInteger(range))
        return value;

    if (!featureExpectingPositiveInteger(mediaFeature) && !isAspectRatioFeature(AtomString { mediaFeature })) {
        if (auto value = CSSPropertyParserHelpers::consumeNumber(range, ValueRange::NonNegative))
            return value;
    }

    if (auto value = CSSPropertyParserHelpers::consumeLength(range, HTMLStandardMode, ValueRange::NonNegative))
        return value;

    if (auto value = CSSPropertyParserHelpers::consumeResolution(range))
        return value;

    if (auto value = CSSPropertyParserHelpers::consumeIdent(range))
        return value;

    return nullptr;
}

MediaQueryExpression::MediaQueryExpression(const String& feature, CSSParserTokenRange& range, MediaQueryParserContext& context)
    : m_mediaFeature(feature.convertToASCIILowercase())
    , m_isValid(false)
{
    RefPtr<CSSPrimitiveValue> firstValue = consumeFirstValue(m_mediaFeature, range);
    if (!firstValue) {
        if (isFeatureValidWithoutValue(m_mediaFeature, context)) {
            // Valid, creates a MediaQueryExp with an 'invalid' MediaQueryExpValue
            m_isValid = true;
        }
        return;
    }
    // Create value for media query expression that must have 1 or more values.
    if (isAspectRatioFeature(m_mediaFeature)) {
        if (!firstValue->isNumberOrInteger() || !firstValue->doubleValue())
            return;
        if (!CSSPropertyParserHelpers::consumeSlashIncludingWhitespace(range))
            return;
        auto denominatorValue = CSSPropertyParserHelpers::consumePositiveIntegerRaw(range);
        if (!denominatorValue)
            return;

        unsigned numerator = clampTo<unsigned>(firstValue->doubleValue());
        m_value = CSSAspectRatioValue::create(numerator, *denominatorValue);
        m_isValid = true;
        return;
    }
    if (featureWithPositiveInteger(m_mediaFeature, *firstValue) || featureWithPositiveNumber(m_mediaFeature, *firstValue)
        || featureWithZeroOrOne(m_mediaFeature, *firstValue) || featureWithValidDensity(m_mediaFeature, *firstValue)
        || featureWithValidPositiveLength(m_mediaFeature, *firstValue) || featureWithValidIdent(m_mediaFeature, *firstValue, context)) {
        m_value = firstValue;
        m_isValid = true;
        return;
    }
}

bool MediaQueryExpression::isViewportDependent() const
{
    return m_mediaFeature == MediaFeatureNames::width
        || m_mediaFeature == MediaFeatureNames::height
        || m_mediaFeature == MediaFeatureNames::minWidth
        || m_mediaFeature == MediaFeatureNames::minHeight
        || m_mediaFeature == MediaFeatureNames::maxWidth
        || m_mediaFeature == MediaFeatureNames::maxHeight
        || m_mediaFeature == MediaFeatureNames::orientation
        || m_mediaFeature == MediaFeatureNames::aspectRatio
        || m_mediaFeature == MediaFeatureNames::minAspectRatio
        || m_mediaFeature == MediaFeatureNames::maxAspectRatio;
}

String MediaQueryExpression::serialize() const
{
    if (m_serializationCache.isNull())
        m_serializationCache = makeString('(', asASCIILowercase(m_mediaFeature), m_value ? ": " : "", m_value ? m_value->cssText() : emptyString(), ')');
    return m_serializationCache;
}

TextStream& operator<<(TextStream& ts, const MediaQueryExpression& expression)
{
    ts << expression.serialize();
    return ts;
}


} // namespace
