/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSUnresolvedLightDark.h"

#include "CSSUnresolvedColor.h"
#include "CSSUnresolvedColorResolutionContext.h"
#include "Document.h"
#include "StyleBuilderState.h"

namespace WebCore {

void serializationForCSS(StringBuilder& builder, const CSSUnresolvedLightDark& lightDark)
{
    builder.append("light-dark("_s);
    lightDark.lightColor->serializationForCSS(builder);
    builder.append(", "_s);
    lightDark.darkColor->serializationForCSS(builder);
    builder.append(')');
}

String serializationForCSS(const CSSUnresolvedLightDark& unresolved)
{
    StringBuilder builder;
    serializationForCSS(builder, unresolved);
    return builder.toString();
}

bool CSSUnresolvedLightDark::operator==(const CSSUnresolvedLightDark& other) const
{
    return lightColor == other.lightColor
        && darkColor == other.darkColor;
}

StyleColor createStyleColor(const CSSUnresolvedLightDark& unresolved, const Document& document, RenderStyle& style, Style::ForVisitedLink forVisitedLink)
{
    if (document.useDarkAppearance(&style))
        return unresolved.darkColor->createStyleColor(document, style, forVisitedLink);
    return unresolved.lightColor->createStyleColor(document, style, forVisitedLink);
}

Color createColor(const CSSUnresolvedLightDark& unresolved, const CSSUnresolvedColorResolutionContext& context)
{
    if (!context.appearance)
        return { };

    switch (*context.appearance) {
    case CSSUnresolvedLightDarkAppearance::Light:
        return unresolved.lightColor->createColor(context);
    case CSSUnresolvedLightDarkAppearance::Dark:
        return unresolved.darkColor->createColor(context);
    }

    ASSERT_NOT_REACHED();
    return { };
}

bool containsCurrentColor(const CSSUnresolvedLightDark& unresolved)
{
    return unresolved.lightColor->containsCurrentColor()
        || unresolved.darkColor->containsCurrentColor();
}

} // namespace WebCore
