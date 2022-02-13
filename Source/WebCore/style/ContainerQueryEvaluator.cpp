/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContainerQueryEvaluator.h"

#include "CSSToLengthConversionData.h"
#include "Document.h"
#include "MediaFeatureNames.h"
#include "MediaList.h"
#include "MediaQuery.h"
#include "RenderView.h"
#include "StyleRule.h"

namespace WebCore::Style {

ContainerQueryEvaluator::ContainerQueryEvaluator(const Vector<Ref<const Element>>& containers)
    : m_containers(containers)
{
}

static std::optional<LayoutUnit> computeSize(CSSValue* value, const CSSToLengthConversionData& conversionData)
{
    if (!is<CSSPrimitiveValue>(value))
        return { };

    auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
    if (primitiveValue.isNumberOrInteger()) {
        if (primitiveValue.doubleValue())
            return { };
        return 0_lu;
    }

    if (!primitiveValue.isLength())
        return { };
    return primitiveValue.computeLength<LayoutUnit>(conversionData);
}

enum class Comparator { Lesser, Greater, LesserOrEqual, GreaterOrEqual, Equal, True };

bool ContainerQueryEvaluator::evaluate(const FilteredContainerQuery& filteredContainerQuery) const
{
    if (m_containers.isEmpty())
        return false;

    auto containerRendererForFilter = [&]() -> RenderBox* {
        for (auto& container : makeReversedRange(m_containers)) {
            auto* renderer = dynamicDowncast<RenderBox>(container->renderer());
            if (!renderer)
                return nullptr;
            if (filteredContainerQuery.nameFilter.isEmpty())
                return renderer;
            // FIXME: Support type filter.
            if (renderer->style().containerNames().contains(filteredContainerQuery.nameFilter))
                return renderer;
        }
        return nullptr;
    };

    auto* renderer = containerRendererForFilter();
    if (!renderer)
        return false;

    auto& view = renderer->view();
    CSSToLengthConversionData conversionData { &renderer->style(), &view.style(), nullptr, &view, 1 };

    auto evaluateSize = [&](const MediaQueryExpression& expression, Comparator comparator, auto&& sizeGetter)
    {
        std::optional<LayoutUnit> expressionSize;

        if (comparator != Comparator::True) {
            expressionSize = computeSize(expression.value(), conversionData);
            if (!expressionSize)
                return false;
        }

        auto size = sizeGetter(*renderer);

        switch (comparator) {
        case Comparator::Lesser:
            return size < *expressionSize;
        case Comparator::Greater:
            return size > *expressionSize;
        case Comparator::LesserOrEqual:
            return size <= *expressionSize;
        case Comparator::GreaterOrEqual:
            return size >= *expressionSize;
        case Comparator::Equal:
            return size == *expressionSize;
        case Comparator::True:
            return !!size;
        }
    };

    auto evaluateWidth = [&](const MediaQueryExpression& expression, Comparator comparator)
    {
        return evaluateSize(expression, comparator, [&](const RenderBox& renderer) {
            return renderer.width();
        });
    };

    auto evaluateHeight = [&](const MediaQueryExpression& expression, Comparator comparator)
    {
        return evaluateSize(expression, comparator, [&](const RenderBox& renderer) {
            return renderer.height();
        });
    };

    bool result = false;

    // FIXME: This is very rudimentary.
    auto& queries = filteredContainerQuery.query->queryVector();
    for (auto& query : queries) {
        for (auto& expression : query.expressions()) {
            if (expression.mediaFeature() == MediaFeatureNames::minWidth) {
                if (evaluateWidth(expression, Comparator::GreaterOrEqual))
                    result = true;
                continue;
            }
            if (expression.mediaFeature() == MediaFeatureNames::maxWidth) {
                if (evaluateWidth(expression, Comparator::LesserOrEqual))
                    result = true;
                continue;
            }
            if (expression.mediaFeature() == MediaFeatureNames::width) {
                if (evaluateWidth(expression, expression.value() ? Comparator::Equal : Comparator::True))
                    result = true;
                continue;
            }
            if (expression.mediaFeature() == MediaFeatureNames::minHeight) {
                if (evaluateHeight(expression, Comparator::GreaterOrEqual))
                    result = true;
                continue;
            }
            if (expression.mediaFeature() == MediaFeatureNames::maxHeight) {
                if (evaluateHeight(expression, Comparator::LesserOrEqual))
                    result = true;
                continue;
            }
            if (expression.mediaFeature() == MediaFeatureNames::height) {
                if (evaluateHeight(expression, expression.value() ? Comparator::Equal : Comparator::True))
                    result = true;
                continue;
            }
        }
    }

    return result;
}

}
