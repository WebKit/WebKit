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
#include "CSSContrastColor.h"

#include "CSSColor.h"
#include "CSSContrastColorResolver.h"
#include "CSSContrastColorSerialization.h"
#include "CSSPlatformColorResolutionState.h"
#include "ColorSerialization.h"

namespace WebCore {
namespace CSS {

bool ContrastColor::operator==(const ContrastColor&) const = default;

WebCore::Color createColor(const ContrastColor& unresolved, PlatformColorResolutionState& state)
{
    PlatformColorResolutionStateNester nester { state };

    return resolve(
        ContrastColorResolver {
            createColor(unresolved.color, state),
            unresolved.max
        }
    );
}

bool containsCurrentColor(const ContrastColor& unresolved)
{
    return containsCurrentColor(unresolved.color);
}

bool containsColorSchemeDependentColor(const ContrastColor& unresolved)
{
    return containsColorSchemeDependentColor(unresolved.color);
}

void Serialize<ContrastColor>::operator()(StringBuilder& builder, const ContrastColor& value)
{
    serializationForCSSContrastColor(builder, value);
}

void ComputedStyleDependenciesCollector<ContrastColor>::operator()(ComputedStyleDependencies& dependencies, const ContrastColor& value)
{
    collectComputedStyleDependencies(dependencies, value.color);
}

IterationStatus CSSValueChildrenVisitor<ContrastColor>::operator()(const Function<IterationStatus(CSSValue&)>& func, const ContrastColor& value)
{
    return visitCSSValueChildren(func, value.color);
}

} // namespace CSS
} // namespace WebCore
