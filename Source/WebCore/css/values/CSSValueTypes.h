/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/CSSValue.h>
#include <WebCore/CSSValueAggregates.h>
#include <WebCore/ComputedStyleDependencies.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class CSSValuePool;
using CSSValueListBuilder = Vector<Ref<CSSValue>, 4>;

namespace CSS {

// MARK: - Serialization

// All leaf types must implement the following conversions:
//
//    template<> struct WebCore::CSS::Serialize<CSSType> {
//        void operator()(StringBuilder&, const SerializationContext&, const CSSType&);
//    };

struct SerializationContext;

template<typename CSSType> struct Serialize;

struct SerializeInvoker {
   template<typename CSSType, typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value, Rest&&... rest) const
   {
        Serialize<CSSType>{}(builder, context, value, std::forward<Rest>(rest)...);
   }

    template<typename CSSType, typename... Rest> [[nodiscard]] String operator()(const SerializationContext& context, const CSSType& value, Rest&&... rest) const
    {
        StringBuilder builder;
        this->operator()(builder, context, value, std::forward<Rest>(rest)...);
        return builder.toString();
    }
};
inline constexpr SerializeInvoker serializationForCSS{};

void serializationForCSSCustomIdentifier(StringBuilder&, const SerializationContext&, const CustomIdentifier&);
void serializationForCSSPropertyIdentifier(StringBuilder&, const SerializationContext&, const PropertyIdentifier&);
void serializationForCSSString(StringBuilder&, const SerializationContext&, const WTF::AtomString&);
void serializationForCSSString(StringBuilder&, const SerializationContext&, const WTF::String&);

template<typename CSSType, typename... Rest> void serializationForCSSOnOptionalLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value, Rest&&... rest)
{
    if (!value)
        return;
    serializationForCSS(builder, context, *value, std::forward<Rest>(rest)...);
}

template<typename CSSType, typename... Rest> void serializationForCSSOnTupleLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value, ASCIILiteral separator, Rest&&... rest)
{
    auto swappedSeparator = ""_s;
    auto caller = WTF::makeVisitor(
        [&]<OptionalLike T>(const T& element) {
            if (!element)
                return;
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, *element, rest...);
        },
        [&](const auto& element) {
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, element, rest...);
        }
    );

    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);
}

template<typename CSSType, typename... Rest> void serializationForCSSOnTupleLikeCoalescing(StringBuilder& builder, const SerializationContext& context, const CSSType& value, ASCIILiteral separator, Rest&&... rest)
{
    if constexpr (std::tuple_size_v<CSSType> == 2) {
        if (get<0>(value) != get<1>(value)) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { get<0>(value), get<1>(value) }, separator, std::forward<Rest>(rest)...);
            return;
        }
        serializationForCSS(builder, context, get<0>(value), std::forward<Rest>(rest)...);
    } else if constexpr (std::tuple_size_v<CSSType> == 4) {
        if (get<3>(value) != get<1>(value)) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { get<0>(value), get<1>(value), get<2>(value), get<3>(value) }, separator, std::forward<Rest>(rest)...);
            return;
        }
        if (get<2>(value) != get<0>(value)) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { get<0>(value), get<1>(value), get<2>(value) }, separator, std::forward<Rest>(rest)...);
            return;
        }
        if (get<1>(value) != get<0>(value)) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { get<0>(value), get<1>(value) }, separator, std::forward<Rest>(rest)...);
            return;
        }
        serializationForCSS(builder, context, get<0>(value), std::forward<Rest>(rest)...);
    }
}

template<typename CSSType, typename... Rest> void serializationForCSSOnRangeLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value, ASCIILiteral separator, Rest&&... rest)
{
    auto swappedSeparator = ""_s;
    for (const auto& element : value) {
        builder.append(std::exchange(swappedSeparator, separator));
        serializationForCSS(builder, context, element, rest...);
    }
}

template<typename CSSType, typename... Rest> void serializationForCSSOnVariantLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value, Rest&&... rest)
{
    WTF::switchOn(value, [&](const auto& alternative) { serializationForCSS(builder, context, alternative, std::forward<Rest>(rest)...); });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike CSSType> struct Serialize<CSSType> {
    template<typename... Rest> void operator()(StringBuilder&, const SerializationContext&, const CSSType&, Rest&&...)
    {
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike CSSType> struct Serialize<CSSType> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value, Rest&&... rest)
    {
        serializationForCSSOnOptionalLike(builder, context, value, std::forward<Rest>(rest)...);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike CSSType> struct Serialize<CSSType> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value, Rest&&... rest)
    {
        if constexpr (SerializationCoalescing<CSSType> == SerializationCoalescingType::Minimal)
            serializationForCSSOnTupleLikeCoalescing(builder, context, value, SerializationSeparatorString<CSSType>, std::forward<Rest>(rest)...);
        else
            serializationForCSSOnTupleLike(builder, context, value, SerializationSeparatorString<CSSType>, std::forward<Rest>(rest)...);
    }
};

// Constrained for `TreatAsRangeLike`.
template<RangeLike CSSType> struct Serialize<CSSType> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value, Rest&&... rest)
    {
        serializationForCSSOnRangeLike(builder, context, value, SerializationSeparatorString<CSSType>, std::forward<Rest>(rest)...);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct Serialize<CSSType> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value, Rest&&... rest)
    {
        serializationForCSSOnVariantLike(builder, context, value, std::forward<Rest>(rest)...);
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct Serialize<Constant<C>> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext&, const Constant<C>& value, Rest&&...)
    {
        builder.append(nameLiteralForSerialization(value.value));
    }
};

// Specialization for `CustomIdentifier`.
template<> struct Serialize<CustomIdentifier> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const CustomIdentifier& value, Rest&&...)
    {
        serializationForCSSCustomIdentifier(builder, context, value);
    }
};

// Specialization for `PropertyIdentifier`.
template<> struct Serialize<PropertyIdentifier> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const PropertyIdentifier& value, Rest&&...)
    {
        serializationForCSSPropertyIdentifier(builder, context, value);
    }
};

// Specialization for `WTF::AtomString`.
template<> struct Serialize<WTF::AtomString> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const WTF::AtomString& value, Rest&&...)
    {
        serializationForCSSString(builder, context, value);
    }
};

// Specialization for `WTF::String`.
template<> struct Serialize<WTF::String> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const WTF::String& value, Rest&&...)
    {
        serializationForCSSString(builder, context, value);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename CSSType> struct Serialize<FunctionNotation<Name, CSSType>> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const FunctionNotation<Name, CSSType>& value, Rest&&... rest)
    {
        builder.append(nameLiteralForSerialization(value.name), '(');
        serializationForCSS(builder, context, value.parameters, std::forward<Rest>(rest)...);
        builder.append(')');
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

struct ComputedStyleDependenciesCollectorInvoker {
    // ComputedStyleDependencies Invoker
    template<typename CSSType> void operator()(ComputedStyleDependencies& dependencies, const CSSType& value) const
    {
        ComputedStyleDependenciesCollector<CSSType>{}(dependencies, value);
    }

    template<typename CSSType> [[nodiscard]] ComputedStyleDependencies operator()(const CSSType& value) const
    {
        ComputedStyleDependencies dependencies;
        this->operator()(dependencies, value);
        return dependencies;
    }
};
inline constexpr ComputedStyleDependenciesCollectorInvoker collectComputedStyleDependencies{};

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

// Specialization for `PropertyIdentifier`.
template<> struct ComputedStyleDependenciesCollector<PropertyIdentifier> {
    constexpr void operator()(ComputedStyleDependencies&, const PropertyIdentifier&)
    {
        // Nothing to do.
    }
};

// Specialization for `WTF::AtomString`.
template<> struct ComputedStyleDependenciesCollector<WTF::AtomString> {
    constexpr void operator()(ComputedStyleDependencies&, const WTF::AtomString&)
    {
        // Nothing to do.
    }
};

// Specialization for `WTF::String`.
template<> struct ComputedStyleDependenciesCollector<WTF::String> {
    constexpr void operator()(ComputedStyleDependencies&, const WTF::String&)
    {
        // Nothing to do.
    }
};

// Specialization for `WTF::URL`.
template<> struct ComputedStyleDependenciesCollector<WTF::URL> {
    constexpr void operator()(ComputedStyleDependencies&, const WTF::URL&)
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

struct CSSValueChildrenVisitorInvoker {
    template<typename CSSType> IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value) const
    {
        return CSSValueChildrenVisitor<CSSType>{}(func, value);
    }
};
inline constexpr CSSValueChildrenVisitorInvoker visitCSSValueChildren{};

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

// Specialization for `PropertyIdentifier`.
template<> struct CSSValueChildrenVisitor<PropertyIdentifier> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const PropertyIdentifier&)
    {
        return IterationStatus::Continue;
    }
};

// Specialization for `WTF::AtomString`.
template<> struct CSSValueChildrenVisitor<WTF::AtomString> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const WTF::AtomString&)
    {
        return IterationStatus::Continue;
    }
};

// Specialization for `WTF::String`.
template<> struct CSSValueChildrenVisitor<WTF::String> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const WTF::String&)
    {
        return IterationStatus::Continue;
    }
};

// Specialization for `WTF::URL`.
template<> struct CSSValueChildrenVisitor<WTF::URL> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const WTF::URL&)
    {
        return IterationStatus::Continue;
    }
};

// MARK: - CSSValue Creation

template<typename CSSType> struct CSSValueCreation;

struct CSSValueCreationInvoker {
    template<typename CSSType, typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const CSSType& value, Rest&&... rest) const
    {
        return CSSValueCreation<CSSType>{}(pool, value, std::forward<Rest>(rest)...);
    }
};
inline constexpr CSSValueCreationInvoker createCSSValue{};

Ref<CSSValue> makePrimitiveCSSValue(CSSValueID);
Ref<CSSValue> makePrimitiveCSSValue(const CustomIdentifier&);
Ref<CSSValue> makePrimitiveCSSValue(const PropertyIdentifier&);
Ref<CSSValue> makePrimitiveCSSValue(const WTF::AtomString&);
Ref<CSSValue> makePrimitiveCSSValue(const WTF::String&);
Ref<CSSValue> makeFunctionCSSValue(CSSValueID, Ref<CSSValue>&&);

template<SerializationSeparatorType> Ref<CSSValue> makeCoalescingPairCSSValue(Ref<CSSValue>&&, Ref<CSSValue>&&);
template<> Ref<CSSValue> makeCoalescingPairCSSValue<SerializationSeparatorType::Space>(Ref<CSSValue>&&, Ref<CSSValue>&&);

template<SerializationSeparatorType> Ref<CSSValue> makeCoalescingQuadCSSValue(Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&);
template<> Ref<CSSValue> makeCoalescingQuadCSSValue<SerializationSeparatorType::Space>(Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&);

template<SerializationSeparatorType> Ref<CSSValue> makeListCSSValue(CSSValueListBuilder&&);
template<> Ref<CSSValue> makeListCSSValue<SerializationSeparatorType::Space>(CSSValueListBuilder&&);
template<> Ref<CSSValue> makeListCSSValue<SerializationSeparatorType::Comma>(CSSValueListBuilder&&);
template<> Ref<CSSValue> makeListCSSValue<SerializationSeparatorType::Slash>(CSSValueListBuilder&&);

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct CSSValueCreation<CSSType> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const CSSType& value, Rest&&... rest)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return createCSSValue(pool, alternative, std::forward<Rest>(rest)...); });
    }
};

// Constrained for `TreatAsTupleLike`
template<TupleLike CSSType> struct CSSValueCreation<CSSType> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const CSSType& value, Rest&&... rest)
    {
        if constexpr (std::tuple_size_v<CSSType> == 1 && SerializationSeparator<CSSType> == SerializationSeparatorType::None) {
            return createCSSValue(pool, get<0>(value), std::forward<Rest>(rest)...);
        } else if constexpr (std::tuple_size_v<CSSType> == 2 && SerializationCoalescing<CSSType> == SerializationCoalescingType::Minimal) {
            return makeCoalescingPairCSSValue<SerializationSeparator<CSSType>>(createCSSValue(pool, get<0>(value), rest...), createCSSValue(pool, get<1>(value), rest...));
        } else if constexpr (std::tuple_size_v<CSSType> == 4 && SerializationCoalescing<CSSType> == SerializationCoalescingType::Minimal) {
            return makeCoalescingQuadCSSValue<SerializationSeparator<CSSType>>(createCSSValue(pool, get<0>(value), rest...), createCSSValue(pool, get<1>(value), rest...), createCSSValue(pool, get<2>(value), rest...), createCSSValue(pool, get<3>(value), rest...));
        } else {
            CSSValueListBuilder list;

            auto caller = WTF::makeVisitor(
                [&]<OptionalLike T>(const T& element) {
                    if (!element)
                        return;
                    list.append(createCSSValue(pool, *element, rest...));
                },
                [&](const auto& element) {
                    list.append(createCSSValue(pool, element, rest...));
                }
            );
            WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);

            return makeListCSSValue<SerializationSeparator<CSSType>>(WTFMove(list));
        }
    }
};

// Constrained for `TreatAsRangeLike`
template<RangeLike CSSType> struct CSSValueCreation<CSSType> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const CSSType& value, Rest&&... rest)
    {
        CSSValueListBuilder list;
        for (const auto& element : value)
            list.append(createCSSValue(pool, element, rest...));

        return makeListCSSValue<SerializationSeparator<CSSType>>(WTFMove(list));
    }
};

// Specialization for `Constant`.
template<CSSValueID Id> struct CSSValueCreation<Constant<Id>> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool&, const Constant<Id>&, Rest&&...)
    {
        return makePrimitiveCSSValue(Id);
    }
};

// Specialization for `CustomIdentifier`.
template<> struct CSSValueCreation<CustomIdentifier> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool&, const CustomIdentifier& customIdentifier, Rest&&...)
    {
        return makePrimitiveCSSValue(customIdentifier);
    }
};

// Specialization for `PropertyIdentifier`.
template<> struct CSSValueCreation<PropertyIdentifier> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool&, const PropertyIdentifier& propertyIdentifier, Rest&&...)
    {
        return makePrimitiveCSSValue(propertyIdentifier);
    }
};

// Specialization for `WTF::AtomString`.
template<> struct CSSValueCreation<WTF::AtomString> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool&, const WTF::AtomString& string, Rest&&...)
    {
        return makePrimitiveCSSValue(string);
    }
};

// Specialization for `WTF::String`.
template<> struct CSSValueCreation<WTF::String> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool&, const WTF::String& string, Rest&&...)
    {
        return makePrimitiveCSSValue(string);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename CSSType> struct CSSValueCreation<FunctionNotation<Name, CSSType>> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const FunctionNotation<Name, CSSType>& value, Rest&&... rest)
    {
        return makeFunctionCSSValue(value.name, createCSSValue(pool, value.parameters, std::forward<Rest>(rest)...));
    }
};

} // namespace CSS
} // namespace WebCore
