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

#pragma once

#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSValuePool.h"
#include "StyleRuleType.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

struct CSSParserContext;

namespace CSS {

struct PropertyParserState {
    const CSSParserContext& context;
    CSSValuePool& pool { CSSValuePool::singleton() };

    StyleRuleType currentRule { StyleRuleType::Style };
    CSSPropertyID currentProperty { CSSPropertyInvalid };
    // Set when currentProperty is CSSPropertyCustom, which every custom property shares.
    AtomString currentCustomPropertyName { };
    IsImportant important { IsImportant::No };

    // Count of CSS random() functions seen so far for the current property.
    unsigned cssRandomFunctionCount { 0 };

    // Set where there is no element to key a random draw to: a @container style() query value or an @property initial value.
    // FIXME: Which of the functions needing an element context are allowed in which context is still
    // being worked out, and each one has its own control until then.
    // https://github.com/w3c/csswg-drafts/issues/10982
    bool randomFunctionsDisallowed { false };

    // Set where there is an element to resolve against but no property being parsed, which is the case
    // for the values in a container query condition.
    // "RESOLVED: Explicitly allow tree-counting functions in container queries"
    // https://github.com/w3c/csswg-drafts/issues/10982
    bool treeCountingFunctionsAllowed { false };

    // Used by non-CSS users of the CSS parsers like `DOMMatrix` to limit <length> and <length-percentage> parsing to only absolute units.
    bool absoluteLengthUnitsOnly { false };
};

} // namespace CSS
} // namespace WebCore
