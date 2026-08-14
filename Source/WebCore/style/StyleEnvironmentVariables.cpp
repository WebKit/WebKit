/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "StyleEnvironmentVariables.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParserTokenRange.h"
#include "CSSVariableData.h"
#include "DocumentPage.h"
#include "StyleCustomProperty.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(EnvironmentVariables);

EnvironmentVariables::EnvironmentVariables(Document& document)
    : m_document(document)
{
}

const EnvironmentVariables::Values& EnvironmentVariables::values() const
{
    if (!m_values)
        const_cast<EnvironmentVariables&>(*this).buildValues();
    return *m_values;
}

const AtomString& EnvironmentVariables::nameForVariable(UADefinedVariable variable) const
{
    static MainThreadNeverDestroyed<const AtomString> safeAreaInsetTopName("safe-area-inset-top"_s);
    static MainThreadNeverDestroyed<const AtomString> safeAreaInsetRightName("safe-area-inset-right"_s);
    static MainThreadNeverDestroyed<const AtomString> safeAreaInsetBottomName("safe-area-inset-bottom"_s);
    static MainThreadNeverDestroyed<const AtomString> safeAreaInsetLeftName("safe-area-inset-left"_s);
    static MainThreadNeverDestroyed<const AtomString> fullscreenInsetTopName("fullscreen-inset-top"_s);
    static MainThreadNeverDestroyed<const AtomString> fullscreenInsetLeftName("fullscreen-inset-left"_s);
    static MainThreadNeverDestroyed<const AtomString> fullscreenInsetBottomName("fullscreen-inset-bottom"_s);
    static MainThreadNeverDestroyed<const AtomString> fullscreenInsetRightName("fullscreen-inset-right"_s);
    static MainThreadNeverDestroyed<const AtomString> fullscreenAutoHideDurationName("fullscreen-auto-hide-duration"_s);

    switch (variable) {
    case UADefinedVariable::SafeAreaInsetTop:
        return safeAreaInsetTopName;
    case UADefinedVariable::SafeAreaInsetRight:
        return safeAreaInsetRightName;
    case UADefinedVariable::SafeAreaInsetBottom:
        return safeAreaInsetBottomName;
    case UADefinedVariable::SafeAreaInsetLeft:
        return safeAreaInsetLeftName;
    case UADefinedVariable::FullscreenInsetTop:
        return fullscreenInsetTopName;
    case UADefinedVariable::FullscreenInsetLeft:
        return fullscreenInsetLeftName;
    case UADefinedVariable::FullscreenInsetBottom:
        return fullscreenInsetBottomName;
    case UADefinedVariable::FullscreenInsetRight:
        return fullscreenInsetRightName;
    case UADefinedVariable::FullscreenAutoHideDuration:
        return fullscreenAutoHideDurationName;
    }

    return nullAtom();
}

void EnvironmentVariables::setValueForVariable(UADefinedVariable variable, Ref<CSSVariableData>&& data)
{
    if (!m_values)
        buildValues();

    auto& name = nameForVariable(variable);
    m_values->set(name, CustomProperty::createForVariableData(name, WTF::move(data)));
}

void EnvironmentVariables::buildValues()
{
    m_values = Values { };

    updateSafeAreaInsetVariables();
    updateFullscreenVariables();
}

static Ref<CSSVariableData> variableDataForPositivePixelLength(float lengthInPx)
{
    ASSERT(lengthInPx >= 0);

    CSSParserToken token(lengthInPx, NumberValueType, NoSign, { });
    token.convertToDimensionWithUnit(CSSUnitType::Px);

    Vector<CSSParserToken> tokens { token };
    CSSParserTokenRange tokenRange(tokens);
    return CSSVariableData::create(tokenRange);
}

static Ref<CSSVariableData> variableDataForPositiveDuration(Seconds durationInSeconds)
{
    ASSERT(durationInSeconds >= 0_s);

    CSSParserToken token(durationInSeconds.value(), NumberValueType, NoSign, { });
    token.convertToDimensionWithUnit(CSSUnitType::S);

    Vector<CSSParserToken> tokens { token };
    CSSParserTokenRange tokenRange(tokens);
    return CSSVariableData::create(tokenRange);
}

void EnvironmentVariables::updateSafeAreaInsetVariables()
{
    RefPtr page = m_document->page();
    FloatBoxExtent unobscuredSafeAreaInsets = page ? page->unobscuredSafeAreaInsets() : FloatBoxExtent();
    setValueForVariable(UADefinedVariable::SafeAreaInsetTop, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.top()));
    setValueForVariable(UADefinedVariable::SafeAreaInsetRight, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.right()));
    setValueForVariable(UADefinedVariable::SafeAreaInsetBottom, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.bottom()));
    setValueForVariable(UADefinedVariable::SafeAreaInsetLeft, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.left()));
}

void EnvironmentVariables::didChangeSafeAreaInsets()
{
    updateSafeAreaInsetVariables();
    protect(m_document)->invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

void EnvironmentVariables::updateFullscreenVariables()
{
    RefPtr page = m_document->page();
    FloatBoxExtent fullscreenInsets = page ? page->fullscreenInsets() : FloatBoxExtent();
    setValueForVariable(UADefinedVariable::FullscreenInsetTop, variableDataForPositivePixelLength(fullscreenInsets.top()));
    setValueForVariable(UADefinedVariable::FullscreenInsetRight, variableDataForPositivePixelLength(fullscreenInsets.right()));
    setValueForVariable(UADefinedVariable::FullscreenInsetBottom, variableDataForPositivePixelLength(fullscreenInsets.bottom()));
    setValueForVariable(UADefinedVariable::FullscreenInsetLeft, variableDataForPositivePixelLength(fullscreenInsets.left()));

    Seconds fullscreenAutoHideDuration = page ? page->fullscreenAutoHideDuration() : 0_s;
    setValueForVariable(UADefinedVariable::FullscreenAutoHideDuration, variableDataForPositiveDuration(fullscreenAutoHideDuration));
}

void EnvironmentVariables::didChangeFullscreenInsets()
{
    updateFullscreenVariables();
    protect(m_document)->invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

void EnvironmentVariables::setFullscreenAutoHideDuration(Seconds duration)
{
    setValueForVariable(UADefinedVariable::FullscreenAutoHideDuration, variableDataForPositiveDuration(duration));
    protect(m_document)->invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

}
}
