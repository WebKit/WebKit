/*
 * Copyright (C) 2022-2023 Apple Inc.  All rights reserved.
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
#include "ColorFromPrimitiveValue.h"

#include "CSSPrimitiveValue.h"
#include "CSSUnresolvedColor.h"
#include "CSSUnresolvedColorResolutionContext.h"
#include "Document.h"
#include "RenderStyle.h"
#include "RenderTheme.h"
#include "StyleBuilderState.h"
#include "StyleColor.h"

namespace WebCore {

namespace Style {

StyleColor styleColorFromValueID(const Document& document, RenderStyle& style, CSSValueID valueID, ForVisitedLink forVisitedLink)
{
    switch (valueID) {
    case CSSValueInternalDocumentTextColor:
        return { document.textColor() };
    case CSSValueWebkitLink:
        return { forVisitedLink == ForVisitedLink::Yes ? document.visitedLinkColor(style) : document.linkColor(style) };
    case CSSValueWebkitActivelink:
        return { document.activeLinkColor(style) };
    case CSSValueWebkitFocusRingColor:
        return { RenderTheme::singleton().focusRingColor(document.styleColorOptions(&style)) };
    case CSSValueCurrentcolor:
        return StyleColor::currentColor();
    default:
        return { StyleColor::colorFromKeyword(valueID, document.styleColorOptions(&style)) };
    }
}

Color colorFromValueID(CSSValueID valueID, const CSSUnresolvedColorResolutionContext& context)
{
    switch (valueID) {
    case CSSValueInternalDocumentTextColor:
        return context.internalDocumentTextColor();
    case CSSValueWebkitLink:
        return context.forVisitedLink == Style::ForVisitedLink::Yes ? context.webkitLinkVisited() : context.webkitLink();
    case CSSValueWebkitActivelink:
        return context.webkitActiveLink();
    case CSSValueWebkitFocusRingColor:
        return context.webkitFocusRingColor();
    case CSSValueCurrentcolor:
        return context.currentColor();
    default:
        return StyleColor::colorFromKeyword(valueID, context.keywordOptions);
    }
}

StyleColor styleColorFromPrimitiveValue(const Document& document, RenderStyle& style, const CSSPrimitiveValue& value, ForVisitedLink forVisitedLink)
{
    if (value.isColor())
        return value.color();
    if (value.isUnresolvedColor())
        return value.unresolvedColor().createStyleColor(document, style, forVisitedLink);
    return styleColorFromValueID(document, style, value.valueID(), forVisitedLink);
}

Color colorFromPrimitiveValue(const CSSPrimitiveValue& value, const CSSUnresolvedColorResolutionContext& context)
{
    if (value.isColor())
        return value.color();
    if (value.isUnresolvedColor())
        return value.unresolvedColor().createColor(context);
    return colorFromValueID(value.valueID(), context);
}

} // namespace Style
} // namespace WebCore
