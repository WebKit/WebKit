/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "CSSUnresolvedContrastColor.h"

#include "CSSContrastColorResolver.h"
#include "CSSContrastColorSerialization.h"
#include "CSSUnresolvedColor.h"
#include "CSSUnresolvedColorResolutionState.h"
#include "CSSUnresolvedStyleColorResolutionState.h"
#include "ColorSerialization.h"
#include "StyleBuilderState.h"
#include "StyleContrastColor.h"

namespace WebCore {

bool CSSUnresolvedContrastColor::operator==(const CSSUnresolvedContrastColor& other) const
{
    return color == other.color
        && max == other.max;
}

void serializationForCSS(StringBuilder& builder, const CSSUnresolvedContrastColor& contrastColor)
{
    serializationForCSSContrastColor(builder, contrastColor);
}

String serializationForCSS(const CSSUnresolvedContrastColor& unresolved)
{
    StringBuilder builder;
    serializationForCSS(builder, unresolved);
    return builder.toString();
}

StyleColor createStyleColor(const CSSUnresolvedContrastColor& unresolved, CSSUnresolvedStyleColorResolutionState& state)
{
    CSSUnresolvedStyleColorResolutionNester nester { state };

    auto color = unresolved.color->createStyleColor(state);

    if (!color.isAbsoluteColor()) {
        return StyleColor {
            StyleContrastColor {
                WTFMove(color),
                unresolved.max
            }
        };
    }

    return resolve(
        CSSContrastColorResolver {
            color.absoluteColor(),
            unresolved.max
        }
    );
}

Color createColor(const CSSUnresolvedContrastColor& unresolved, CSSUnresolvedColorResolutionState& state)
{
    return resolve(
        CSSContrastColorResolver {
            unresolved.color->createColor(state),
            unresolved.max
        }
    );
}

bool containsCurrentColor(const CSSUnresolvedContrastColor& unresolved)
{
    return unresolved.color->containsCurrentColor();
}

bool containsColorSchemeDependentColor(const CSSUnresolvedContrastColor& unresolved)
{
    return unresolved.color->containsColorSchemeDependentColor();
}

} // namespace WebCore
