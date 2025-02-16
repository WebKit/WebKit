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

#pragma once

#include "CSSValueAggregates.h"
#include "ComputedStyleDependencies.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class CSSValue;

namespace CSS {

// MARK: - Serialization

// All leaf types must implement the following conversions:
//
//    template<> struct WebCore::CSS::Serialize<CSSType> {
//        void operator()(StringBuilder&, const SerializationContext&, const SerializationContext&, const CSSType&);
//    };

struct SerializationContext;

template<typename CSSType> struct Serialize;

// Serialization Invokers
template<typename CSSType> void serializationForCSS(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
{
    Serialize<CSSType>{}(builder, context, value);
}

template<typename CSSType> [[nodiscard]] String serializationForCSS(const SerializationContext& context, const CSSType& value)
{
    StringBuilder builder;
    serializationForCSS(builder, context, value);
    return builder.toString();
}

template<typename CSSType> void serializationForCSSOnOptionalLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
{
    if (!value)
        return;
    serializationForCSS(builder, context, *value);
}

template<typename CSSType> void serializationForCSSOnTupleLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    auto caller = WTF::makeVisitor(
        [&]<typename T>(const std::optional<T>& element) {
            if (!element)
                return;
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, *element);
        },
        [&]<typename T>(const Markable<T>& element) {
            if (!element)
                return;
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, *element);
        },
        [&](const auto& element) {
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, element);
        }
    );

    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);
}

template<typename CSSType> void serializationForCSSOnRangeLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    for (const auto& element : value) {
        builder.append(std::exchange(swappedSeparator, separator));
        serializationForCSS(builder, context, element);
    }
}

template<typename CSSType> void serializationForCSSOnVariantLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
{
    WTF::switchOn(value, [&](const auto& alternative) { serializationForCSS(builder, context, alternative); });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike CSSType> struct Serialize<CSSType> {
    void operator()(StringBuilder&, const SerializationContext&, const CSSType&)
    {
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike CSSType> struct Serialize<CSSType> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
    {
        serializationForCSSOnOptionalLike(builder, context, value);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike CSSType> struct Serialize<CSSType> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
    {
        serializationForCSSOnTupleLike(builder, context, value, SerializationSeparator<CSSType>);
    }
};

// Constrained for `TreatAsRangeLike`.
template<RangeLike CSSType> struct Serialize<CSSType> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
    {
        serializationForCSSOnRangeLike(builder, context, value, SerializationSeparator<CSSType>);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct Serialize<CSSType> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
    {
        serializationForCSSOnVariantLike(builder, context, value);
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct Serialize<Constant<C>> {
    void operator()(StringBuilder& builder, const SerializationContext&, const Constant<C>& value)
    {
        builder.append(nameLiteralForSerialization(value.value));
    }
};

// Specialization for `CustomIdentifier`.
template<> struct Serialize<CustomIdentifier> {
    void operator()(StringBuilder&, const SerializationContext&, const CustomIdentifier&);
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename CSSType> struct Serialize<FunctionNotation<Name, CSSType>> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const FunctionNotation<Name, CSSType>& value)
    {
        builder.append(nameLiteralForSerialization(value.name), '(');
        serializationForCSS(builder, context, value.parameters);
        builder.append(')');
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedRectEdges`.
template<typename CSSType> struct Serialize<MinimallySerializingSpaceSeparatedRectEdges<CSSType>> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const MinimallySerializingSpaceSeparatedRectEdges<CSSType>& value)
    {
        constexpr auto separator = SerializationSeparator<MinimallySerializingSpaceSeparatedRectEdges<CSSType>>;

        if (value.left() != value.right()) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { value.top(), value.right(), value.bottom(), value.left() }, separator);
            return;
        }
        if (value.bottom() != value.top()) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { value.top(), value.right(), value.bottom() }, separator);
            return;
        }
        if (value.right() != value.top()) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { value.top(), value.right() }, separator);
            return;
        }
        serializationForCSS(builder, context, value.top());
    }
};

// MARK: - Computed Style Dependencies

// What properties does this value rely on (eg, font-size for em units)?

// All non-tuple-like leaf types must implement the following conversions:
//
//    template<> struct WebCore::CSS::ComputedStyleDependenciesCollector<CSSType> {
//        void operator()(ComputedStyleDependencies&, const CSSType&);
//    };

template<typename CSSType> struct ComputedStyleDependenciesCollector;

// ComputedStyleDependencies Invoker
template<typename CSSType> void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies, const CSSType& value)
{
    ComputedStyleDependenciesCollector<CSSType>{}(dependencies, value);
}

template<typename CSSType> [[nodiscard]] ComputedStyleDependencies collectComputedStyleDependencies(const CSSType& value)
{
    ComputedStyleDependencies dependencies;
    collectComputedStyleDependencies(dependencies, value);
    return dependencies;
}

template<typename CSSType> auto collectComputedStyleDependenciesOnOptionalLike(ComputedStyleDependencies& dependencies, const CSSType& value)
{
    if (!value)
        return;
    collectComputedStyleDependencies(dependencies, *value);
}

template<typename CSSType> auto collectComputedStyleDependenciesOnTupleLike(ComputedStyleDependencies& dependencies, const CSSType& value)
{
    WTF::apply([&](const auto& ...x) { (..., collectComputedStyleDependencies(dependencies, x)); }, value);
}

template<typename CSSType> auto collectComputedStyleDependenciesOnRangeLike(ComputedStyleDependencies& dependencies, const CSSType& value)
{
    for (const auto& element : value)
        collectComputedStyleDependencies(dependencies, element);
}

template<typename CSSType> auto collectComputedStyleDependenciesOnVariantLike(ComputedStyleDependencies& dependencies, const CSSType& value)
{
    WTF::switchOn(value, [&](const auto& alternative) { collectComputedStyleDependencies(dependencies, alternative); });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike CSSType> struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies&, const CSSType&)
    {
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike CSSType> struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies& dependencies, const CSSType& value)
    {
        collectComputedStyleDependenciesOnOptionalLike(dependencies, value);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike CSSType> struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies& dependencies, const CSSType& value)
    {
        collectComputedStyleDependenciesOnTupleLike(dependencies, value);
    }
};

// Constrained for `TreatAsRangeLike`.
template<RangeLike CSSType> struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies& dependencies, const CSSType& value)
    {
        collectComputedStyleDependenciesOnRangeLike(dependencies, value);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies& dependencies, const CSSType& value)
    {
        collectComputedStyleDependenciesOnVariantLike(dependencies, value);
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct ComputedStyleDependenciesCollector<Constant<C>> {
    constexpr void operator()(ComputedStyleDependencies&, const Constant<C>&)
    {
        // Nothing to do.
    }
};

// Specialization for `CustomIdentifier`.
template<> struct ComputedStyleDependenciesCollector<CustomIdentifier> {
    constexpr void operator()(ComputedStyleDependencies&, const CustomIdentifier&)
    {
        // Nothing to do.
    }
};

// MARK: - CSSValue Visitation

// All non-tuple-like leaf types must implement the following conversions:
//
//    template<> struct WebCore::CSS::CSSValueChildrenVisitor<CSSType> {
//        IterationStatus operator()(const Function<IterationStatus(CSSValue&)>&, const CSSType&);
//    };

template<typename CSSType> struct CSSValueChildrenVisitor;

// CSSValueVisitor Invoker
template<typename CSSType> IterationStatus visitCSSValueChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    return CSSValueChildrenVisitor<CSSType>{}(func, value);
}

template<typename CSSType> IterationStatus visitCSSValueChildrenOnOptionalLike(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    return value ? visitCSSValueChildren(func, *value) : IterationStatus::Continue;
}

template<typename CSSType> IterationStatus visitCSSValueChildrenOnTupleLike(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    // Process a single element of the tuple-like, updating result, and return true if result == IterationStatus::Done to
    // short circuit the fold in the apply lambda.
    auto process = [&](const auto& x, IterationStatus& result) -> bool {
        result = visitCSSValueChildren(func, x);
        return result == IterationStatus::Done;
    };

    return WTF::apply([&](const auto& ...x) {
        auto result = IterationStatus::Continue;
        (process(x, result) || ...);
        return result;
    }, value);
}

template<typename CSSType> IterationStatus visitCSSValueChildrenOnRangeLike(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    for (const auto& element : value) {
        if (visitCSSValueChildren(func, element) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

template<typename CSSType> IterationStatus visitCSSValueChildrenOnVariantLike(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    return WTF::switchOn(value, [&](const auto& alternative) { return visitCSSValueChildren(func, alternative); });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike CSSType> struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const CSSType&)
    {
        return IterationStatus::Continue;
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike CSSType> struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
    {
        return visitCSSValueChildrenOnOptionalLike(func, value);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike CSSType> struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
    {
        return visitCSSValueChildrenOnTupleLike(func, value);
    }
};

// Constrained for `TreatAsRangeLike`.
template<RangeLike CSSType> struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
    {
        return visitCSSValueChildrenOnRangeLike(func, value);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
    {
        return visitCSSValueChildrenOnVariantLike(func, value);
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct CSSValueChildrenVisitor<Constant<C>> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const Constant<C>&)
    {
        return IterationStatus::Continue;
    }
};

// Specialization for `CustomIdentifier`.
template<> struct CSSValueChildrenVisitor<CustomIdentifier> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const CustomIdentifier&)
    {
        return IterationStatus::Continue;
    }
};

} // namespace CSS
} // namespace WebCore
