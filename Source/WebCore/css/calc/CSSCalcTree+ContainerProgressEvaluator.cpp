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
#include "CSSCalcTree+ContainerProgressEvaluator.h"

#include "CSSCalcTree.h"
#include "ContainerQueryEvaluator.h"
#include "ContainerQueryFeatures.h"
#include "RenderBox.h"

namespace WebCore {
namespace CSSCalc {

std::optional<double> evaluateContainerProgress(const ContainerProgress& root, const Element& initialElement, const CSSToLengthConversionData& conversionData)
{
    // FIXME: This lookup loop is the same as the one used in CSSPrimitiveValue for resolving container units. Would be good to figure out a nice place to share this.

    RefPtr element = &initialElement;

    auto mode = conversionData.style()->pseudoElementType() == PseudoId::None
        ? Style::ContainerQueryEvaluator::SelectionMode::Element
        : Style::ContainerQueryEvaluator::SelectionMode::PseudoElement;

    while ((element = Style::ContainerQueryEvaluator::selectContainer({ }, root.container, *element, mode))) {
        auto* containerRenderer = dynamicDowncast<RenderBox>(element->renderer());
        if (containerRenderer && containerRenderer->hasEligibleContainmentForSizeQuery())
            return root.feature->valueInCanonicalUnits(*containerRenderer);

        // For pseudo-elements the element itself can be the container. Avoid looping forever.
        mode = Style::ContainerQueryEvaluator::SelectionMode::Element;
    }

    // "If no appropriate containers are found, container-progress() resolves its <size-feature> query against the small viewport size."
    auto view = conversionData.renderView();
    if (!view)
        return { };

    return root.feature->valueInCanonicalUnits(*view, *conversionData.style());
}

} // namespace CSSCalc
} // namespace WebCore
