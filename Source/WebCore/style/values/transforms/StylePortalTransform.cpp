/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "StylePortalTransform.h"

#include "CSSKeywordValue.h"
#include "CSSTransformListValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "StyleBuilderChecking.h"
#include "StyleInterpolationContext.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

// MARK: - Construction

static const TransformList& emptyTransformList()
{
    static NeverDestroyed<TransformList> list;
    return list.get();
}

PortalTransform PortalTransform::withAutoFit(TransformList&& beforeAuto, TransformList&& afterAuto)
{
    if (beforeAuto.isEmpty()) {
        if (afterAuto.isEmpty())
            return CSS::Keyword::Auto { };
        return PortalTransform { AfterAuto { CSS::Keyword::Auto { }, WTF::move(afterAuto) } };
    }

    if (afterAuto.isEmpty())
        return PortalTransform { BeforeAndAfterAuto { WTF::move(beforeAuto), { }, std::nullopt } };

    return PortalTransform { BeforeAndAfterAuto { WTF::move(beforeAuto), { }, WTF::move(afterAuto) } };
}

PortalTransform PortalTransform::withoutAutoFit(TransformList&& transforms)
{
    if (transforms.isEmpty())
        return CSS::Keyword::None { };

    return PortalTransform { AfterAuto { std::nullopt, WTF::move(transforms) } };
}

bool PortalTransform::hasAuto() const
{
    return WTF::switchOn(m_value,
        [](const CSS::Keyword::Auto&) { return true; },
        [](const AfterAuto& value) { return !!value.autoKeyword; },
        [](const BeforeAndAfterAuto&) { return true; },
        [](const auto&) { return false; }
    );
}

const TransformList& PortalTransform::beforeAutoList() const
{
    if (auto* value = std::get_if<BeforeAndAfterAuto>(&m_value))
        return value->before;

    return emptyTransformList();
}

const TransformList& PortalTransform::afterAutoList() const
{
    return WTF::switchOn(m_value,
        [](const AfterAuto& value) -> const TransformList& { return value.after; },
        [](const BeforeAndAfterAuto& value) -> const TransformList& { return value.after ? *value.after : emptyTransformList(); },
        [](const auto&) -> const TransformList& { return emptyTransformList(); }
    );
}

void PortalTransform::applyBeforeAuto(TransformationMatrix& matrix, const FloatSize& size, ZoomFactor zoom) const
{
    beforeAutoList().apply(matrix, size, zoom);
}

void PortalTransform::applyAfterAuto(TransformationMatrix& matrix, const FloatSize& size, ZoomFactor zoom) const
{
    afterAutoList().apply(matrix, size, zoom);
}

// MARK: - Conversion

static TransformList transformListSlice(BuilderState& state, const CSSValueContainingVector& source, size_t begin, size_t end)
{
    return TransformList { TransformList::Container::createWithSizeFromGenerator(end - begin, [&](size_t i) {
        Ref value = source[begin + i];
        return toStyleFromCSSValue<TransformFunction>(state, value);
    }) };
}

static bool isAutoKeyword(const CSSValue& value)
{
    auto* keyword = dynamicDowncast<CSSKeywordValue>(value);
    return keyword && keyword->valueID() == CSSValueAuto;
}

auto CSSValueConversion<PortalTransform>::operator()(BuilderState& state, const CSSValue& value) -> PortalTransform
{
    if (auto* keyword = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keyword->valueID()) {
        case CSSValueNone:
            return CSS::Keyword::None { };
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        default:
            break;
        }
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }

    if (auto* transformList = dynamicDowncast<CSSTransformListValue>(value))
        return PortalTransform::withoutAutoFit(transformListSlice(state, *transformList, 0, transformList->size()));

    RefPtr list = requiredDowncast<CSSValueList>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    auto count = list->size();
    for (size_t index = 0; index < count; ++index) {
        if (isAutoKeyword((*list)[index]))
            return PortalTransform::withAutoFit(transformListSlice(state, *list, 0, index), transformListSlice(state, *list, index + 1, count));
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::Auto { };
}

// MARK: - Blending

static bool hasMatchingAutoFit(const PortalTransform& from, const PortalTransform& to)
{
    return from.hasAuto() == to.hasAuto();
}

auto Blending<PortalTransform>::canBlend(const PortalTransform& from, const PortalTransform& to, CompositeOperation compositeOperation) -> bool
{
    return hasMatchingAutoFit(from, to)
        && Style::canBlend(from.beforeAutoList(), to.beforeAutoList(), compositeOperation)
        && Style::canBlend(from.afterAutoList(), to.afterAutoList(), compositeOperation);
}

auto Blending<PortalTransform>::blend(const PortalTransform& from, const PortalTransform& to, const Interpolation::Context& context) -> PortalTransform
{
    if (!hasMatchingAutoFit(from, to))
        return context.progress < 0.5 ? from : to;

    if (!from.hasAuto())
        return PortalTransform::withoutAutoFit(Style::blend(from.afterAutoList(), to.afterAutoList(), context));

    return PortalTransform::withAutoFit(Style::blend(from.beforeAutoList(), to.beforeAutoList(), context), Style::blend(from.afterAutoList(), to.afterAutoList(), context));
}

} // namespace Style
} // namespace WebCore
