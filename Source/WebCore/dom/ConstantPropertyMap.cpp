/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "ConstantPropertyMap.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParserTokenRange.h"
#include "CSSVariableData.h"
#include "Document.h"
#include "Page.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

ConstantPropertyMap::ConstantPropertyMap(Document& document)
    : m_document(document)
{
}

const ConstantPropertyMap::Values& ConstantPropertyMap::values() const
{
    if (!m_values)
        const_cast<ConstantPropertyMap&>(*this).buildValues();
    return *m_values;
}

const AtomString& ConstantPropertyMap::nameForProperty(ConstantProperty property) const
{
    static NeverDestroyed<AtomString> safeAreaInsetTopName("safe-area-inset-top", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> safeAreaInsetRightName("safe-area-inset-right", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> safeAreaInsetBottomName("safe-area-inset-bottom", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> safeAreaInsetLeftName("safe-area-inset-left", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> fullscreenInsetTopName("fullscreen-inset-top", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> fullscreenInsetLeftName("fullscreen-inset-left", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> fullscreenInsetBottomName("fullscreen-inset-bottom", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> fullscreenInsetRightName("fullscreen-inset-right", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> fullscreenAutoHideDurationName("fullscreen-auto-hide-duration", AtomString::ConstructFromLiteral);

    switch (property) {
    case ConstantProperty::SafeAreaInsetTop:
        return safeAreaInsetTopName;
    case ConstantProperty::SafeAreaInsetRight:
        return safeAreaInsetRightName;
    case ConstantProperty::SafeAreaInsetBottom:
        return safeAreaInsetBottomName;
    case ConstantProperty::SafeAreaInsetLeft:
        return safeAreaInsetLeftName;
    case ConstantProperty::FullscreenInsetTop:
        return fullscreenInsetTopName;
    case ConstantProperty::FullscreenInsetLeft:
        return fullscreenInsetLeftName;
    case ConstantProperty::FullscreenInsetBottom:
        return fullscreenInsetBottomName;
    case ConstantProperty::FullscreenInsetRight:
        return fullscreenInsetRightName;
    case ConstantProperty::FullscreenAutoHideDuration:
        return fullscreenAutoHideDurationName;
    }

    return nullAtom();
}

void ConstantPropertyMap::setValueForProperty(ConstantProperty property, Ref<CSSVariableData>&& data)
{
    if (!m_values)
        buildValues();

    auto& name = nameForProperty(property);
    m_values->set(name, CSSCustomPropertyValue::createSyntaxAll(name, WTFMove(data)));
}

void ConstantPropertyMap::buildValues()
{
    m_values = Values { };

    updateConstantsForSafeAreaInsets();
    updateConstantsForFullscreen();
}

static Ref<CSSVariableData> variableDataForPositivePixelLength(float lengthInPx)
{
    ASSERT(lengthInPx >= 0);

    CSSParserToken token(NumberToken, lengthInPx, NumberValueType, NoSign);
    token.convertToDimensionWithUnit("px");

    Vector<CSSParserToken> tokens { token };
    CSSParserTokenRange tokenRange(tokens);
    return CSSVariableData::create(tokenRange);
}

static Ref<CSSVariableData> variableDataForPositiveDuration(Seconds durationInSeconds)
{
    ASSERT(durationInSeconds >= 0_s);

    CSSParserToken token(NumberToken, durationInSeconds.value(), NumberValueType, NoSign);
    token.convertToDimensionWithUnit("s");

    Vector<CSSParserToken> tokens { token };
    CSSParserTokenRange tokenRange(tokens);
    return CSSVariableData::create(tokenRange);
}

void ConstantPropertyMap::updateConstantsForSafeAreaInsets()
{
    FloatBoxExtent unobscuredSafeAreaInsets = m_document.page() ? m_document.page()->unobscuredSafeAreaInsets() : FloatBoxExtent();
    setValueForProperty(ConstantProperty::SafeAreaInsetTop, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.top()));
    setValueForProperty(ConstantProperty::SafeAreaInsetRight, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.right()));
    setValueForProperty(ConstantProperty::SafeAreaInsetBottom, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.bottom()));
    setValueForProperty(ConstantProperty::SafeAreaInsetLeft, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.left()));
}

void ConstantPropertyMap::didChangeSafeAreaInsets()
{
    updateConstantsForSafeAreaInsets();
    m_document.invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

void ConstantPropertyMap::updateConstantsForFullscreen()
{
    FloatBoxExtent fullscreenInsets = m_document.page() ? m_document.page()->fullscreenInsets() : FloatBoxExtent();
    setValueForProperty(ConstantProperty::FullscreenInsetTop, variableDataForPositivePixelLength(fullscreenInsets.top()));
    setValueForProperty(ConstantProperty::FullscreenInsetRight, variableDataForPositivePixelLength(fullscreenInsets.right()));
    setValueForProperty(ConstantProperty::FullscreenInsetBottom, variableDataForPositivePixelLength(fullscreenInsets.bottom()));
    setValueForProperty(ConstantProperty::FullscreenInsetLeft, variableDataForPositivePixelLength(fullscreenInsets.left()));

    Seconds fullscreenAutoHideDuration = m_document.page() ? m_document.page()->fullscreenAutoHideDuration() : 0_s;
    setValueForProperty(ConstantProperty::FullscreenAutoHideDuration, variableDataForPositiveDuration(fullscreenAutoHideDuration));
}

void ConstantPropertyMap::didChangeFullscreenInsets()
{
    updateConstantsForFullscreen();
    m_document.invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

void ConstantPropertyMap::setFullscreenAutoHideDuration(Seconds duration)
{
    setValueForProperty(ConstantProperty::FullscreenAutoHideDuration, variableDataForPositiveDuration(duration));
    m_document.invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

}
