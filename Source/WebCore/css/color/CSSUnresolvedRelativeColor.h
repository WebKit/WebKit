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

#pragma once

#include "CSSColorDescriptors.h"
#include "CSSRelativeColorResolver.h"
#include "CSSRelativeColorSerialization.h"
#include "StyleColor.h"
#include "StyleRelativeColor.h"
#include <variant>
#include <wtf/Forward.h>

namespace WebCore {

namespace Style {
enum class ForVisitedLink : bool;
}

class CSSUnresolvedColor;
class Document;
class RenderStyle;

struct CSSUnresolvedColorResolutionContext;

template<typename Descriptor, unsigned Index>
using CSSUnresolvedRelativeColorComponent = GetComponentResultWithCalcAndSymbolsResult<Descriptor, Index>;

bool relativeColorOriginsEqual(const UniqueRef<CSSUnresolvedColor>&, const UniqueRef<CSSUnresolvedColor>&);

template<typename D>
struct CSSUnresolvedRelativeColor {
    using Descriptor = D;

    UniqueRef<CSSUnresolvedColor> origin;
    CSSColorParseTypeWithCalcAndSymbols<Descriptor> components;

    bool operator==(const CSSUnresolvedRelativeColor<Descriptor>& other) const
    {
        return relativeColorOriginsEqual(origin, other.origin)
            && components == other.components;
    }
};

template<typename Descriptor>
void serializationForCSS(StringBuilder& builder, const CSSUnresolvedRelativeColor<Descriptor>& unresolved)
{
    serializationForCSSRelativeColor(builder, unresolved);
}

template<typename Descriptor>
String serializationForCSS(const CSSUnresolvedRelativeColor<Descriptor>& unresolved)
{
    StringBuilder builder;
    serializationForCSS(builder, unresolved);
    return builder.toString();
}

template<typename Descriptor>
StyleColor createStyleColor(const CSSUnresolvedRelativeColor<Descriptor>& unresolved, const Document& document, RenderStyle& style, Style::ForVisitedLink forVisitedLink)
{
    return StyleColor {
        StyleRelativeColor<Descriptor> {
            .origin = unresolved.origin->createStyleColor(document, style, forVisitedLink),
            .components = unresolved.components
        }
    };
}

template<typename Descriptor>
Color createColor(const CSSUnresolvedRelativeColor<Descriptor>& unresolved, const CSSUnresolvedColorResolutionContext& context)
{
    auto origin = unresolved.origin->createColor(context);
    if (!origin.isValid())
        return { };

    return resolve(
        CSSRelativeColorResolver<Descriptor> {
            .origin = WTFMove(origin),
            .components = unresolved.components
        }
    );
}

template<typename Descriptor>
bool containsColorSchemeDependentColor(const CSSUnresolvedRelativeColor<Descriptor>& unresolved)
{
    return unresolved.origin->containsColorSchemeDependentColor();
}

template<typename Descriptor>
bool containsCurrentColor(const CSSUnresolvedRelativeColor<Descriptor>& unresolved)
{
    return unresolved.origin->containsColorSchemeDependentColor();
}

} // namespace WebCore
