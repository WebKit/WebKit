/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSUnresolvedColorKeyword.h"

#include "CSSUnresolvedColorResolutionState.h"
#include "CSSUnresolvedStyleColorResolutionState.h"
#include "CSSValueKeywords.h"
#include "ColorFromPrimitiveValue.h"
#include "StyleBuilderState.h"

namespace WebCore {

void serializationForCSS(StringBuilder& builder, const CSSUnresolvedColorKeyword& unresolved)
{
    builder.append(nameLiteralForSerialization(unresolved.valueID));
}

String serializationForCSS(const CSSUnresolvedColorKeyword& unresolved)
{
    return nameStringForSerialization(unresolved.valueID);
}

StyleColor createStyleColor(const CSSUnresolvedColorKeyword& unresolved, CSSUnresolvedStyleColorResolutionState& state)
{
    return Style::colorFromValueID(state.document, state.style, unresolved.valueID, state.forVisitedLink);
}

Color createColor(const CSSUnresolvedColorKeyword& unresolved, CSSUnresolvedColorResolutionState& state)
{
    switch (unresolved.valueID) {
    case CSSValueInternalDocumentTextColor:
        return state.internalDocumentTextColor();
    case CSSValueWebkitLink:
        return state.forVisitedLink == Style::ForVisitedLink::Yes ? state.webkitLinkVisited() : state.webkitLink();
    case CSSValueWebkitActivelink:
        return state.webkitActiveLink();
    case CSSValueWebkitFocusRingColor:
        return state.webkitFocusRingColor();
    case CSSValueCurrentcolor:
        return state.currentColor();
    default:
        return StyleColor::colorFromKeyword(unresolved.valueID, state.keywordOptions);
    }
}

bool containsCurrentColor(const CSSUnresolvedColorKeyword& unresolved)
{
    return StyleColor::isCurrentColorKeyword(unresolved.valueID);
}

bool containsColorSchemeDependentColor(const CSSUnresolvedColorKeyword& unresolved)
{
    return StyleColor::isSystemColorKeyword(unresolved.valueID);
}

} // namespace WebCore
