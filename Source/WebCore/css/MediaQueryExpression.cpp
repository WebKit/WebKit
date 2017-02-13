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
#include "CSSParser.h"
#include "CSSParserIdioms.h"
#include "CSSParserToken.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "MediaFeatureNames.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline bool featureWithValidIdent(const AtomicString& mediaFeature)
{
    return mediaFeature == MediaFeatureNames::orientation
#if ENABLE(VIEW_MODE_CSS_MEDIA)
    || mediaFeature == MediaFeatureNames::viewMode
#endif
    || mediaFeature == MediaFeatureNames::colorGamut
    || mediaFeature == MediaFeatureNames::anyHover
    || mediaFeature == MediaFeatureNames::anyPointer
    || mediaFeature == MediaFeatureNames::hover
    || mediaFeature == MediaFeatureNames::invertedColors
    || mediaFeature == MediaFeatureNames::pointer
    || mediaFeature == MediaFeatureNames::prefersReducedMotion;
}

static inline bool featureWithValidDensity(const String& mediaFeature, const CSSParserToken& token)
{
    if (!CSSPrimitiveValue::isResolution(static_cast<CSSPrimitiveValue::UnitType>(token.unitType())) || token.numericValue() <= 0)
        return false;
    
    return mediaFeature == MediaFeatureNames::resolution
    || mediaFeature == MediaFeatureNames::minResolution
    || mediaFeature == MediaFeatureNames::maxResolution;
}

static inline bool featureWithValidPositiveLength(const String& mediaFeature, const CSSParserToken& token)
{
    if (!(CSSPrimitiveValue::isLength(token.unitType()) || (token.type() == NumberToken && !token.numericValue())) || token.numericValue() < 0)
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

static inline bool featureWithPositiveInteger(const String& mediaFeature, const CSSParserToken& token)
{
    if (token.numericValueType() != IntegerValueType || token.numericValue() < 0)
        return false;
    
    return mediaFeature == MediaFeatureNames::color
    || mediaFeature == MediaFeatureNames:: maxColor
    || mediaFeature == MediaFeatureNames:: minColor
    || mediaFeature == MediaFeatureNames::colorIndex
    || mediaFeature == MediaFeatureNames::maxColorIndex
    || mediaFeature == MediaFeatureNames::minColorIndex
    || mediaFeature == MediaFeatureNames::monochrome
    || mediaFeature == MediaFeatureNames::maxMonochrome
    || mediaFeature == MediaFeatureNames::minMonochrome;
}

static inline bool featureWithPositiveNumber(const String& mediaFeature, const CSSParserToken& token)
{
    if (token.type() != NumberToken || token.numericValue() < 0)
        return false;
    
    return mediaFeature == MediaFeatureNames::transform3d
    || mediaFeature == MediaFeatureNames::devicePixelRatio
    || mediaFeature == MediaFeatureNames::maxDevicePixelRatio
    || mediaFeature == MediaFeatureNames::minDevicePixelRatio
    || mediaFeature == MediaFeatureNames::transition
    || mediaFeature == MediaFeatureNames::animation
    || mediaFeature == MediaFeatureNames::transform2d;
}

static inline bool featureWithZeroOrOne(const String& mediaFeature, const CSSParserToken& token)
{
    if (token.numericValueType() != IntegerValueType || !(token.numericValue() == 1 || !token.numericValue()))
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
        || mediaFeature == MediaFeatureNames::prefersReducedMotion
        || mediaFeature == MediaFeatureNames::devicePixelRatio
        || mediaFeature == MediaFeatureNames::resolution
        || mediaFeature == MediaFeatureNames::videoPlayableInline;
}

MediaQueryExpression::MediaQueryExpression(const String& feature, const Vector<CSSParserToken, 4>& tokenList)
    : m_mediaFeature(feature.convertToASCIILowercase())
    , m_isValid(false)
{
    // Create value for media query expression that must have 1 or more values.
    if (!tokenList.size() && isFeatureValidWithoutValue(m_mediaFeature)) {
        // Valid, creates a MediaQueryExp with an 'invalid' MediaQueryExpValue
        m_isValid = true;
    } else if (tokenList.size() == 1) {
        CSSParserToken token = tokenList.first();
        if (token.type() == IdentToken) {
            CSSValueID ident = token.id();
            if (!featureWithValidIdent(m_mediaFeature))
                return;
            m_value = CSSPrimitiveValue::createIdentifier(ident);
            m_isValid = true;
        } else if (token.type() == NumberToken || token.type() == PercentageToken || token.type() == DimensionToken) {
            // Check for numeric token types since it is only safe for these types to call numericValue.
            if (featureWithValidDensity(m_mediaFeature, token)
                || featureWithValidPositiveLength(m_mediaFeature, token)) {
                // Media features that must have non-negative <density>, ie. dppx, dpi or dpcm,
                // or Media features that must have non-negative <length> or number value.
                m_value = CSSPrimitiveValue::create(token.numericValue(), (CSSPrimitiveValue::UnitType) token.unitType());
                m_isValid = true;
            } else if (featureWithPositiveInteger(m_mediaFeature, token)
                || featureWithPositiveNumber(m_mediaFeature, token)
                || featureWithZeroOrOne(m_mediaFeature, token)) {
                // Media features that must have non-negative integer value,
                // or media features that must have non-negative number value,
                // or media features that must have (0|1) value.
                m_value = CSSPrimitiveValue::create(token.numericValue(), CSSPrimitiveValue::UnitType::CSS_NUMBER);
                m_isValid = true;
            }
        }
    } else if (tokenList.size() == 3 && isAspectRatioFeature(m_mediaFeature)) {
        // FIXME: <ratio> is supposed to allow whitespace around the '/'
        // Applicable to device-aspect-ratio and aspect-ratio.
        const CSSParserToken& numerator = tokenList[0];
        const CSSParserToken& delimiter = tokenList[1];
        const CSSParserToken& denominator = tokenList[2];
        if (delimiter.type() != DelimiterToken || delimiter.delimiter() != '/')
            return;
        if (numerator.type() != NumberToken || numerator.numericValue() <= 0 || numerator.numericValueType() != IntegerValueType)
            return;
        if (denominator.type() != NumberToken || denominator.numericValue() <= 0 || denominator.numericValueType() != IntegerValueType)
            return;
        
        m_value = CSSAspectRatioValue::create(numerator.numericValue(), denominator.numericValue());
        m_isValid = true;
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
