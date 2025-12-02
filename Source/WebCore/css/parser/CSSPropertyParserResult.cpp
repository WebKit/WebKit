/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPropertyParserResult.h"

#include "CSSParserContext.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserState.h"
#include "StylePropertyShorthand.h"
#include <wtf/Vector.h>

namespace WebCore {
namespace CSS {

void PropertyParserResult::addProperty(CSSProperty&& property)
{
    parsedProperties.append(WTFMove(property));
}

void PropertyParserResult::addProperty([[maybe_unused]] CSS::PropertyParserState& state, CSSPropertyID property, CSSPropertyID currentShorthand, RefPtr<CSSValue>&& value, IsImportant important, IsImplicit implicit)
{
    int shorthandIndex = 0;
    bool setFromShorthand = false;

    if (currentShorthand) {
        auto shorthands = matchingShorthandsForLonghand(property);
        setFromShorthand = true;
        if (shorthands.size() > 1)
            shorthandIndex = indexOfShorthandForLonghand(currentShorthand, shorthands);
    }

    // Allow anything to be set from a shorthand (e.g. the CSS all property always sets everything,
    // regardless of whether the longhands are enabled), and allow internal properties as we use
    // them to handle certain DOM-exposed values (e.g. -webkit-font-size-delta from
    // execCommand('FontSizeDelta')).
    ASSERT(isExposed(property, &state.context.propertySettings) || setFromShorthand || isInternal(property));

    if (value && !value->isImplicitInitialValue())
        addProperty(CSSProperty(property, value.releaseNonNull(), important, setFromShorthand, shorthandIndex, implicit));
    else {
        ASSERT(setFromShorthand);
        addProperty(CSSProperty(property, Ref { CSSPrimitiveValue::implicitInitialValue() }, important, setFromShorthand, shorthandIndex, IsImplicit::Yes));
    }
}

void PropertyParserResult::addPropertyForCurrentShorthand(CSS::PropertyParserState& state, CSSPropertyID longhand, RefPtr<CSSValue>&& value, IsImplicit implicit)
{
    addProperty(state, longhand, state.currentProperty, WTFMove(value), state.important, implicit);
}

void PropertyParserResult::addPropertyForAllLonghandsOfShorthand(CSS::PropertyParserState& state, CSSPropertyID shorthand, RefPtr<CSSValue>&& value, IsImportant important, IsImplicit implicit)
{
    for (auto longhand : shorthandForProperty(shorthand))
        addProperty(state, longhand, shorthand, value.copyRef(), important, implicit);
}

void PropertyParserResult::addPropertyForAllLonghandsOfCurrentShorthand(CSS::PropertyParserState& state, RefPtr<CSSValue>&& value, IsImplicit implicit)
{
    addPropertyForAllLonghandsOfShorthand(state, state.currentProperty, WTFMove(value), state.important, implicit);
}

} // namespace CSS
} // namespace WebCore
