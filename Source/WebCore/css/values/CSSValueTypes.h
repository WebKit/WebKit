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

#include "CSSValueKeywords.h"
#include "ComputedStyleDependencies.h"
#include "RectEdges.h"
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
#include <wtf/Markable.h>
#include <wtf/Hasher.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

class CSSValue;
struct ComputedStyleDependencies;

namespace CSS {

// Types can specialize this and set the value to true to be treated as "tuple-like"
// for CSS value type algorithms.
// NOTE: This gets automatically specialized when using the CSS_TUPLE_LIKE_CONFORMANCE macro.
template<class> inline constexpr bool TreatAsTupleLike = false;

// Types can specialize this and set the value to true to be treated as a "type wrapper"
// for CSS value type algorithms. "Type wrappers" must be simple structs with exactly one
// member variable with the name "value".
// NOTE: This gets automatically specialized when using the DEFINE_CSS_TYPE_WRAPPER macro.
template<class> inline constexpr bool TreatAsTypeWrapper = false;

#define DEFINE_CSS_TYPE_WRAPPER(t) \
    template<> inline constexpr bool TreatAsTypeWrapper<t> = true;

// Helper type used to represent a known constant identifier.
template<CSSValueID C> struct Constant {
    static constexpr auto value = C;

    constexpr bool operator==(const Constant<C>&) const = default;
};

// Helper type used to represent an arbitrary constant identifier.
struct CustomIdentifier {
    AtomString value;

    bool operator==(const CustomIdentifier&) const = default;
    bool operator==(const AtomString& other) const { return value == other; }
};

// Helper type used to represent a CSS function.
template<CSSValueID C, typename T> struct FunctionNotation {
    static constexpr auto name = C;
    T parameters;

    // Forward * and -> to the parameters for convenience.
    const T& operator*() const { return parameters; }
    T& operator*() { return parameters; }
    const T* operator->() const { return &parameters; }
    T* operator->() { return &parameters; }
    operator const T&() const { return parameters; }
    operator T&() { return parameters; }

    bool operator==(const FunctionNotation<C, T>&) const = default;
};

// Wraps a variable number of elements of a single type, semantically marking them as serializing as "space separated".
template<typename T, size_t inlineCapacity = 0> struct SpaceSeparatedVector {
    using Vector = WTF::Vector<T, inlineCapacity>;
    using const_iterator = typename Vector::const_iterator;
    using const_reverse_iterator = typename Vector::const_reverse_iterator;
    using value_type = typename Vector::value_type;

    SpaceSeparatedVector(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    SpaceSeparatedVector(WTF::Vector<T, inlineCapacity>&& value)
        : value { WTFMove(value) }
    {
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const T& operator[](size_t i) const { return value[i]; }

    bool operator==(const SpaceSeparatedVector<T, inlineCapacity>&) const = default;

    WTF::Vector<T, inlineCapacity> value;
};

// Wraps a variable number of elements of a single type, semantically marking them as serializing as "comma separated".
template<typename T, size_t inlineCapacity = 0> struct CommaSeparatedVector {
    using Vector = WTF::Vector<T, inlineCapacity>;
    using const_iterator = typename Vector::const_iterator;
    using const_reverse_iterator = typename Vector::const_reverse_iterator;
    using value_type = typename Vector::value_type;

    CommaSeparatedVector(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    CommaSeparatedVector(WTF::Vector<T, inlineCapacity>&& value)
        : value { WTFMove(value) }
    {
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const T& operator[](size_t i) const { return value[i]; }

    bool operator==(const CommaSeparatedVector<T, inlineCapacity>&) const = default;

    WTF::Vector<T, inlineCapacity> value;
};

// Wraps a fixed size list of elements of a single type, semantically marking them as serializing as "space separated".
template<typename T, size_t N> struct SpaceSeparatedArray {
    using Array = std::array<T, N>;
    using const_iterator = typename Array::const_iterator;
    using const_reverse_iterator = typename Array::const_reverse_iterator;
    using value_type = T;

    template<typename...Ts> requires (sizeof...(Ts) == N && WTF::all<std::convertible_to<Ts, T>...>)
    constexpr SpaceSeparatedArray(Ts... values)
        : value { std::forward<Ts>(values)... }
    {
    }

    constexpr SpaceSeparatedArray(std::array<T, N>&& array)
        : value { WTFMove(array) }
    {
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const T& operator[](size_t i) const { return value[i]; }

    constexpr bool operator==(const SpaceSeparatedArray<T, N>&) const = default;

    std::array<T, N> value;
};

template<class T, class... Ts>
    requires (WTF::all<std::convertible_to<Ts, T>...>)
SpaceSeparatedArray(T, Ts...) -> SpaceSeparatedArray<T, 1 + sizeof...(Ts)>;

template<size_t I, typename T, size_t N> decltype(auto) get(const SpaceSeparatedArray<T, N>& array)
{
    return std::get<I>(array.value);
}

template<typename T> using SpaceSeparatedPair = SpaceSeparatedArray<T, 2>;

// Wraps a fixed size list of elements of a single type, semantically marking them as serializing as "comma separated".
template<typename T, size_t N> struct CommaSeparatedArray {
    using Array = std::array<T, N>;
    using const_iterator = typename Array::const_iterator;
    using const_reverse_iterator = typename Array::const_reverse_iterator;
    using value_type = T;

    template<typename...Ts> requires (sizeof...(Ts) == N && WTF::all<std::convertible_to<Ts, T>...>)
    constexpr CommaSeparatedArray(Ts... values)
        : value { std::forward<Ts>(values)... }
    {
    }

    constexpr CommaSeparatedArray(std::array<T, N>&& array)
        : value { WTFMove(array) }
    {
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const T& operator[](size_t i) const { return value[i]; }

    constexpr bool operator==(const CommaSeparatedArray<T, N>&) const = default;

    std::array<T, N> value;
};


template<class T, class... Ts>
    requires (WTF::all<std::convertible_to<Ts, T>...>)
CommaSeparatedArray(T, Ts...) -> CommaSeparatedArray<T, 1 + sizeof...(Ts)>;

template<size_t I, typename T, size_t N> decltype(auto) get(const CommaSeparatedArray<T, N>& array)
{
    return std::get<I>(array.value);
}

template<typename T> using CommaSeparatedPair = CommaSeparatedArray<T, 2>;

// Wraps a variadic list of types, semantically marking them as serializing as "space separated".
template<typename... Ts> struct SpaceSeparatedTuple {
    using Tuple = std::tuple<Ts...>;

    constexpr SpaceSeparatedTuple(Ts&&... values)
        : value { std::make_tuple(std::forward<Ts>(values)...) }
    {
    }

    constexpr SpaceSeparatedTuple(const Ts&... values)
        : value { std::make_tuple(values...) }
    {
    }

    constexpr SpaceSeparatedTuple(std::tuple<Ts...>&& tuple)
        : value { WTFMove(tuple) }
    {
    }

    constexpr bool operator==(const SpaceSeparatedTuple<Ts...>&) const = default;

    std::tuple<Ts...> value;
};

template<size_t I, typename... Ts> decltype(auto) get(const SpaceSeparatedTuple<Ts...>& tuple)
{
    return std::get<I>(tuple.value);
}

// Wraps a variadic list of types, semantically marking them as serializing as "comma separated".
template<typename... Ts> struct CommaSeparatedTuple {
    using Tuple = std::tuple<Ts...>;

    constexpr CommaSeparatedTuple(Ts&&... values)
        : value { std::make_tuple(std::forward<Ts>(values)...) }
    {
    }

    constexpr CommaSeparatedTuple(const Ts&... values)
        : value { std::make_tuple(values...) }
    {
    }

    constexpr CommaSeparatedTuple(std::tuple<Ts...>&& tuple)
        : value { WTFMove(tuple) }
    {
    }

    constexpr bool operator==(const CommaSeparatedTuple<Ts...>&) const = default;

    std::tuple<Ts...> value;
};

template<size_t I, typename... Ts> decltype(auto) get(const CommaSeparatedTuple<Ts...>& tuple)
{
    return std::get<I>(tuple.value);
}

// Wraps a pair of elements of a single type, semantically marking them as serializing as "point".
template<typename T> struct Point {
    using Array = SpaceSeparatedPair<T>;
    using value_type = T;

    constexpr Point(T p1, T p2)
        : value { WTFMove(p1), WTFMove(p2) }
    {
    }

    constexpr Point(SpaceSeparatedPair<T>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const Point<T>&) const = default;

    const T& x() const { return get<0>(value); }
    const T& y() const { return get<1>(value); }

    SpaceSeparatedPair<T> value;
};

template<size_t I, typename T> decltype(auto) get(const Point<T>& point)
{
    return get<I>(point.value);
}

// Wraps a pair of elements of a single type, semantically marking them as serializing as "size".
template<typename T> struct Size {
    using Array = SpaceSeparatedPair<T>;
    using value_type = T;

    constexpr Size(T p1, T p2)
        : value { WTFMove(p1), WTFMove(p2) }
    {
    }

    constexpr Size(SpaceSeparatedPair<T>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const Size<T>&) const = default;

    const T& width() const { return get<0>(value); }
    const T& height() const { return get<1>(value); }

    SpaceSeparatedPair<T> value;
};

template<size_t I, typename T> decltype(auto) get(const Size<T>& size)
{
    return get<I>(size.value);
}

// MARK: - Hashing

// All leaf types must implement the following conversions:
//
//    template<> struct WebCore::CSS::Hash<CSSType> {
//        void operator()(Hasher&, const CSSType&);
//    };

template<typename CSSType> struct Hash;

// Serialization Invokers
template<typename CSSType> void addHash(Hasher& hasher, const CSSType& value)
{
    Hash<CSSType>{}(hasher, value);
}

template<typename CSSType> unsigned hash(const CSSType& value)
{
    Hasher hasher;
    addHash(hasher, value);
    return hasher.hash();
}

template<typename CSSType> auto hashTupleLike(Hasher& hasher, const CSSType& value)
{
    WTF::apply([&](const auto& ...x) { (..., addHash(hasher, x)); }, value);
}

template<typename CSSType> auto hashRangeLike(Hasher& hasher, const CSSType& value)
{
    for (const auto& element : value)
        addHash(hasher, element);
}

// Constrained for `TreatAsTupleLike`.
template<typename CSSType> requires (TreatAsTupleLike<CSSType>) struct Hash<CSSType> {
    void operator()(Hasher& hasher, const CSSType& value)
    {
        hashTupleLike(hasher, value);
    }
};

// Constrained for `TreatAsTypeWrapper`.
template<typename CSSType> requires (TreatAsTypeWrapper<CSSType>) struct Hash<CSSType> {
    void operator()(Hasher& hasher, const CSSType& value)
    {
        addHash(hasher, value.value);
    }
};

// Specialization for `std::optional`.
template<typename CSSType> struct Hash<std::optional<CSSType>> {
    void operator()(Hasher& hasher, const std::optional<CSSType>& value)
    {
        if (!value)
            return;
        addHash(hasher, *value);
    }
};

// Specialization for `Markable`.
template<typename CSSType> struct Hash<Markable<CSSType>> {
    void operator()(Hasher& hasher, const Markable<CSSType>& value)
    {
        if (!value)
            return;
        addHash(hasher, *value);
    }
};

// Specialization for `std::variant`.
template<typename... CSSTypes> struct Hash<std::variant<CSSTypes...>> {
    void operator()(Hasher& hasher, const std::variant<CSSTypes...>& value)
    {
        WTF::switchOn(value, [&](const auto& alternative) { addHash(hasher, alternative); });
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct Hash<Constant<C>> {
    constexpr void operator()(Hasher& hasher, const Constant<C>& value)
    {
        WTF::add(hasher, value.value);
    }
};

// Specialization for `CustomIdentifier`.
template<> struct Hash<CustomIdentifier> {
    constexpr void operator()(Hasher& hasher, const CustomIdentifier& value)
    {
        WTF::add(hasher, value.value.existingHash());
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename CSSType> struct Hash<FunctionNotation<Name, CSSType>> {
    constexpr void operator()(Hasher& hasher, const FunctionNotation<Name, CSSType>& value)
    {
        addHash(hasher, value.parameters);
    }
};

// Specialization for `SpaceSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct Hash<SpaceSeparatedVector<CSSType, inlineCapacity>> {
    void operator()(Hasher& hasher, const SpaceSeparatedVector<CSSType, inlineCapacity>& value)
    {
        hashRangeLike(hasher, value.value);
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct Hash<CommaSeparatedVector<CSSType, inlineCapacity>> {
    void operator()(Hasher& hasher, const CommaSeparatedVector<CSSType, inlineCapacity>& value)
    {
        hashRangeLike(hasher, value.value);
    }
};

// Specialization for `SpaceSeparatedArray`.
template<typename CSSType, size_t N> struct Hash<SpaceSeparatedArray<CSSType, N>> {
    void operator()(Hasher& hasher, const SpaceSeparatedArray<CSSType, N>& value)
    {
        hashTupleLike(hasher, value.value);
    }
};

// Specialization for `CommaSeparatedArray`.
template<typename CSSType, size_t N> struct Hash<CommaSeparatedArray<CSSType, N>> {
    void operator()(Hasher& hasher, const CommaSeparatedArray<CSSType, N>& value)
    {
        hashTupleLike(hasher, value.value);
    }
};

// Specialization for `SpaceSeparatedTuple`.
template<typename... CSSTypes> struct Hash<SpaceSeparatedTuple<CSSTypes...>> {
    void operator()(Hasher& hasher, const SpaceSeparatedTuple<CSSTypes...>& value)
    {
        hashTupleLike(hasher, value.value);
    }
};

// Specialization for `CommaSeparatedTuple`.
template<typename... CSSTypes> struct Hash<CommaSeparatedTuple<CSSTypes...>> {
    void operator()(Hasher& hasher, const CommaSeparatedTuple<CSSTypes...>& value)
    {
        hashTupleLike(hasher, value.value);
    }
};

// Specialization for `Point`.
template<typename CSSType> struct Hash<Point<CSSType>> {
    void operator()(Hasher& hasher, const Point<CSSType>& value)
    {
        addHash(hasher, value.value);
    }
};

// Specialization for `Size`.
template<typename CSSType> struct Hash<Size<CSSType>> {
    void operator()(Hasher& hasher, const Size<CSSType>& value)
    {
        addHash(hasher, value.value);
    }
};

// Specialization for `RectEdges`.
template<typename CSSType> struct Hash<RectEdges<CSSType>> {
    void operator()(Hasher& hasher, const RectEdges<CSSType>& value)
    {
        addHash(hasher, value.top());
        addHash(hasher, value.right());
        addHash(hasher, value.bottom());
        addHash(hasher, value.left());
    }
};

// MARK: - Serialization

// All leaf types must implement the following conversions:
//
//    template<> struct WebCore::CSS::Serialize<CSSType> {
//        void operator()(StringBuilder&, const CSSType&);
//    };

template<typename CSSType> struct Serialize;

// Serialization Invokers
template<typename CSSType> void serializationForCSS(StringBuilder& builder, const CSSType& value)
{
    Serialize<CSSType>{}(builder, value);
}

template<typename CSSType> String serializationForCSS(const CSSType& value)
{
    StringBuilder builder;
    serializationForCSS(builder, value);
    return builder.toString();
}

// Constrained for `TreatAsTypeWrapper`.
template<typename CSSType> requires (TreatAsTypeWrapper<CSSType>) struct Serialize<CSSType> {
    void operator()(StringBuilder& builder, const CSSType& value)
    {
        serializationForCSS(builder, value.value);
    }
};

// Specialization for `std::optional`.
template<typename CSSType> struct Serialize<std::optional<CSSType>> {
    void operator()(StringBuilder& builder, const std::optional<CSSType>& value)
    {
        if (!value)
            return;
        serializationForCSS(builder, *value);
    }
};

// Specialization for `Markable`.
template<typename CSSType> struct Serialize<Markable<CSSType>> {
    void operator()(StringBuilder& builder, const Markable<CSSType>& value)
    {
        if (!value)
            return;
        serializationForCSS(builder, *value);
    }
};

// Specialization for `std::variant`.
template<typename... CSSTypes> struct Serialize<std::variant<CSSTypes...>> {
    void operator()(StringBuilder& builder, const std::variant<CSSTypes...>& value)
    {
        WTF::switchOn(value, [&](auto& alternative) { serializationForCSS(builder, alternative); });
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct Serialize<Constant<C>> {
    void operator()(StringBuilder& builder, const Constant<C>& value)
    {
        builder.append(nameLiteralForSerialization(value.value));
    }
};

// Specialization for `CustomIdentifier`.
template<> struct Serialize<CustomIdentifier> {
    void operator()(StringBuilder&, const CustomIdentifier&);
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename CSSType> struct Serialize<FunctionNotation<Name, CSSType>> {
    void operator()(StringBuilder& builder, const FunctionNotation<Name, CSSType>& value)
    {
        builder.append(nameLiteralForSerialization(value.name), '(');
        serializationForCSS(builder, value.parameters);
        builder.append(')');
    }
};

// Specialization for `SpaceSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct Serialize<SpaceSeparatedVector<CSSType, inlineCapacity>> {
    void operator()(StringBuilder& builder, const SpaceSeparatedVector<CSSType, inlineCapacity>& value)
    {
        builder.append(interleave(value.value, [](auto& builder, const auto& element) { serializationForCSS(builder, element); }, ' '));
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct Serialize<CommaSeparatedVector<CSSType, inlineCapacity>> {
    void operator()(StringBuilder& builder, const CommaSeparatedVector<CSSType, inlineCapacity>& value)
    {
        builder.append(interleave(value.value, [](auto& builder, const auto& element) { serializationForCSS(builder, element); }, ", "_s));
    }
};

// Specialization for `SpaceSeparatedArray`.
template<typename CSSType, size_t N> struct Serialize<SpaceSeparatedArray<CSSType, N>> {
    void operator()(StringBuilder& builder, const SpaceSeparatedArray<CSSType, N>& value)
    {
        builder.append(interleave(value.value, [](auto& builder, auto& element) { serializationForCSS(builder, element); }, ' '));
    }
};

// Specialization for `CommaSeparatedArray`.
template<typename CSSType, size_t N> struct Serialize<CommaSeparatedArray<CSSType, N>> {
    void operator()(StringBuilder& builder, const CommaSeparatedArray<CSSType, N>& value)
    {
        builder.append(interleave(value.value, [](auto& builder, auto& element) { serializationForCSS(builder, element); }, ", "_s));
    }
};

// Specialization for `SpaceSeparatedTuple`.
template<typename... CSSTypes> struct Serialize<SpaceSeparatedTuple<CSSTypes...>> {
    void operator()(StringBuilder& builder, const SpaceSeparatedTuple<CSSTypes...>& value)
    {
        auto separator = ""_s;
        auto caller = WTF::makeVisitor(
            [&]<typename T>(const std::optional<T>& css) {
                if (!css)
                    return;
                builder.append(std::exchange(separator, " "_s));
                serializationForCSS(builder, *css);
            },
            [&](const auto& css) {
                builder.append(std::exchange(separator, " "_s));
                serializationForCSS(builder, css);
            }
        );

        WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value.value);
    }
};

// Specialization for `CommaSeparatedTuple`.
template<typename... CSSTypes> struct Serialize<CommaSeparatedTuple<CSSTypes...>> {
    void operator()(StringBuilder& builder, const CommaSeparatedTuple<CSSTypes...>& value)
    {
        auto separator = ""_s;
        auto caller = WTF::makeVisitor(
            [&]<typename T>(const std::optional<T>& css) {
                if (!css)
                    return;
                builder.append(std::exchange(separator, ", "_s));
                serializationForCSS(builder, *css);
            },
            [&](const auto& css) {
                builder.append(std::exchange(separator, ", "_s));
                serializationForCSS(builder, css);
            }
        );

        WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value.value);
    }
};

// Specialization for `Point`.
template<typename CSSType> struct Serialize<Point<CSSType>> {
    void operator()(StringBuilder& builder, const Point<CSSType>& value)
    {
        serializationForCSS(builder, value.value);
    }
};

// Specialization for `Size`.
template<typename CSSType> struct Serialize<Size<CSSType>> {
    void operator()(StringBuilder& builder, const Size<CSSType>& value)
    {
        serializationForCSS(builder, value.value);
    }
};

// Specialization for `RectEdges`.
template<typename CSSType> struct Serialize<RectEdges<CSSType>> {
    void operator()(StringBuilder& builder, const RectEdges<CSSType>& value)
    {
        serializationForCSS(builder, value.top());
        builder.append(' ');
        serializationForCSS(builder, value.right());
        builder.append(' ');
        serializationForCSS(builder, value.bottom());
        builder.append(' ');
        serializationForCSS(builder, value.left());
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

template<typename CSSType> ComputedStyleDependencies collectComputedStyleDependencies(const CSSType& value)
{
    ComputedStyleDependencies dependencies;
    collectComputedStyleDependencies(dependencies, value);
    return dependencies;
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

// Constrained for `TreatAsTupleLike`.
template<typename CSSType> requires (TreatAsTupleLike<CSSType>) struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies& dependencies, const CSSType& value)
    {
        collectComputedStyleDependenciesOnTupleLike(dependencies, value);
    }
};

// Constrained for `TreatAsTypeWrapper`.
template<typename CSSType> requires (TreatAsTypeWrapper<CSSType>) struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies& dependencies, const CSSType& value)
    {
        collectComputedStyleDependencies(dependencies, value.value);
    }
};

// Specialization for `std::optional`.
template<typename CSSType> struct ComputedStyleDependenciesCollector<std::optional<CSSType>> {
    void operator()(ComputedStyleDependencies& dependencies, const std::optional<CSSType>& value)
    {
        if (!value)
            return;
        collectComputedStyleDependencies(dependencies, *value);
    }
};

// Specialization for `Markable`.
template<typename CSSType> struct ComputedStyleDependenciesCollector<Markable<CSSType>> {
    void operator()(ComputedStyleDependencies& dependencies, const Markable<CSSType>& value)
    {
        if (!value)
            return;
        collectComputedStyleDependencies(dependencies, *value);
    }
};

// Specialization for `std::variant`.
template<typename... CSSTypes> struct ComputedStyleDependenciesCollector<std::variant<CSSTypes...>> {
    void operator()(ComputedStyleDependencies& dependencies, const std::variant<CSSTypes...>& value)
    {
        WTF::switchOn(value, [&](const auto& alternative) { collectComputedStyleDependencies(dependencies, alternative); });
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

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename CSSType> struct ComputedStyleDependenciesCollector<FunctionNotation<Name, CSSType>> {
    constexpr void operator()(ComputedStyleDependencies& dependencies, const FunctionNotation<Name, CSSType>& value)
    {
        collectComputedStyleDependencies(dependencies, value.parameters);
    }
};

// Specialization for `SpaceSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct ComputedStyleDependenciesCollector<SpaceSeparatedVector<CSSType, inlineCapacity>> {
    void operator()(ComputedStyleDependencies& dependencies, const SpaceSeparatedVector<CSSType, inlineCapacity>& value)
    {
        collectComputedStyleDependenciesOnRangeLike(dependencies, value.value);
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct ComputedStyleDependenciesCollector<CommaSeparatedVector<CSSType, inlineCapacity>> {
    void operator()(ComputedStyleDependencies& dependencies, const CommaSeparatedVector<CSSType, inlineCapacity>& value)
    {
        collectComputedStyleDependenciesOnRangeLike(dependencies, value.value);
    }
};

// Specialization for `SpaceSeparatedArray`.
template<typename CSSType, size_t N> struct ComputedStyleDependenciesCollector<SpaceSeparatedArray<CSSType, N>> {
    void operator()(ComputedStyleDependencies& dependencies, const SpaceSeparatedArray<CSSType, N>& value)
    {
        collectComputedStyleDependenciesOnTupleLike(dependencies, value.value);
    }
};

// Specialization for `CommaSeparatedArray`.
template<typename CSSType, size_t N> struct ComputedStyleDependenciesCollector<CommaSeparatedArray<CSSType, N>> {
    void operator()(ComputedStyleDependencies& dependencies, const CommaSeparatedArray<CSSType, N>& value)
    {
        collectComputedStyleDependenciesOnTupleLike(dependencies, value.value);
    }
};

// Specialization for `SpaceSeparatedTuple`.
template<typename... CSSTypes> struct ComputedStyleDependenciesCollector<SpaceSeparatedTuple<CSSTypes...>> {
    void operator()(ComputedStyleDependencies& dependencies, const SpaceSeparatedTuple<CSSTypes...>& value)
    {
        collectComputedStyleDependenciesOnTupleLike(dependencies, value.value);
    }
};

// Specialization for `CommaSeparatedTuple`.
template<typename... CSSTypes> struct ComputedStyleDependenciesCollector<CommaSeparatedTuple<CSSTypes...>> {
    void operator()(ComputedStyleDependencies& dependencies, const CommaSeparatedTuple<CSSTypes...>& value)
    {
        collectComputedStyleDependenciesOnTupleLike(dependencies, value.value);
    }
};

// Specialization for `Point`.
template<typename CSSType> struct ComputedStyleDependenciesCollector<Point<CSSType>> {
    void operator()(ComputedStyleDependencies& dependencies, const Point<CSSType>& value)
    {
        collectComputedStyleDependencies(dependencies, value.value);
    }
};

// Specialization for `Size`.
template<typename CSSType> struct ComputedStyleDependenciesCollector<Size<CSSType>> {
    void operator()(ComputedStyleDependencies& dependencies, const Size<CSSType>& value)
    {
        collectComputedStyleDependencies(dependencies, value.value);
    }
};

// Specialization for `RectEdges`.
template<typename CSSType> struct ComputedStyleDependenciesCollector<RectEdges<CSSType>> {
    void operator()(ComputedStyleDependencies& dependencies, const RectEdges<CSSType>& value)
    {
        collectComputedStyleDependencies(dependencies, value.top());
        collectComputedStyleDependencies(dependencies, value.right());
        collectComputedStyleDependencies(dependencies, value.bottom());
        collectComputedStyleDependencies(dependencies, value.left());
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
template<typename CSSType> IterationStatus visitCSSValueChildren(const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    return CSSValueChildrenVisitor<CSSType>{}(func, value);
}

template<typename CSSType> IterationStatus visitCSSValueChildrenOnTupleLike(const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
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

template<typename CSSType> IterationStatus visitCSSValueChildrenOnRangeLike(const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    for (const auto& element : value) {
        if (visitCSSValueChildren(func, element) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

// Constrained for `TreatAsTupleLike`.
template<typename CSSType> requires (TreatAsTupleLike<CSSType>) struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
    {
        return visitCSSValueChildrenOnTupleLike(func, value);
    }
};

// Constrained for `TreatAsTypeWrapper`.
template<typename CSSType> requires (TreatAsTypeWrapper<CSSType>) struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
    {
        return visitCSSValueChildren(func, value.value);
    }
};

// Specialization for `std::optional`.
template<typename CSSType> struct CSSValueChildrenVisitor<std::optional<CSSType>> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const std::optional<CSSType>& value)
    {
        return value ? visitCSSValueChildren(func, *value) : IterationStatus::Continue;
    }
};

// Specialization for `Markable`.
template<typename CSSType> struct CSSValueChildrenVisitor<Markable<CSSType>> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const Markable<CSSType>& value)
    {
        return value ? visitCSSValueChildren(func, *value) : IterationStatus::Continue;
    }
};

// Specialization for `std::variant`.
template<typename... CSSTypes> struct CSSValueChildrenVisitor<std::variant<CSSTypes...>> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const std::variant<CSSTypes...>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return visitCSSValueChildren(func, alternative); });
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct CSSValueChildrenVisitor<Constant<C>> {
    constexpr IterationStatus operator()(const Function<IterationStatus(CSSValue&)>&, const Constant<C>&)
    {
        return IterationStatus::Continue;
    }
};

// Specialization for `CustomIdentifier`.
template<> struct CSSValueChildrenVisitor<CustomIdentifier> {
    constexpr IterationStatus operator()(const Function<IterationStatus(CSSValue&)>&, const CustomIdentifier&)
    {
        return IterationStatus::Continue;
    }
};

// Specialization for `Function`.
template<CSSValueID Name, typename CSSType> struct CSSValueChildrenVisitor<FunctionNotation<Name, CSSType>> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const FunctionNotation<Name, CSSType>& value)
    {
        return visitCSSValueChildren(func, value.parameters);
    }
};

// Specialization for `SpaceSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct CSSValueChildrenVisitor<SpaceSeparatedVector<CSSType, inlineCapacity>> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const SpaceSeparatedVector<CSSType, inlineCapacity>& value)
    {
        return visitCSSValueChildrenOnRangeLike(func, value.value);
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct CSSValueChildrenVisitor<CommaSeparatedVector<CSSType, inlineCapacity>> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const CommaSeparatedVector<CSSType, inlineCapacity>& value)
    {
        return visitCSSValueChildrenOnRangeLike(func, value.value);
    }
};

// Specialization for `SpaceSeparatedArray`.
template<typename CSSType, size_t N> struct CSSValueChildrenVisitor<SpaceSeparatedArray<CSSType, N>> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const SpaceSeparatedArray<CSSType, N>& value)
    {
        return visitCSSValueChildrenOnTupleLike(func, value.value);
    }
};

// Specialization for `CommaSeparatedArray`.
template<typename CSSType, size_t N> struct CSSValueChildrenVisitor<CommaSeparatedArray<CSSType, N>> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const CommaSeparatedArray<CSSType, N>& value)
    {
        return visitCSSValueChildrenOnTupleLike(func, value.value);
    }
};

// Specialization for `SpaceSeparatedTuple`.
template<typename... CSSTypes> struct CSSValueChildrenVisitor<SpaceSeparatedTuple<CSSTypes...>> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const SpaceSeparatedTuple<CSSTypes...>& value)
    {
        return visitCSSValueChildrenOnTupleLike(func, value.value);
    }
};

// Specialization for `CommaSeparatedTuple`.
template<typename... CSSTypes> struct CSSValueChildrenVisitor<CommaSeparatedTuple<CSSTypes...>> {
    IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const CommaSeparatedTuple<CSSTypes...>& value)
    {
        return visitCSSValueChildrenOnTupleLike(func, value.value);
    }
};

// Specialization for `Point`.
template<typename CSSType> struct CSSValueChildrenVisitor<Point<CSSType>> {
    constexpr IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const Point<CSSType>& value)
    {
        return visitCSSValueChildren(func, value.value);
    }
};

// Specialization for `Size`.
template<typename CSSType> struct CSSValueChildrenVisitor<Size<CSSType>> {
    constexpr IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const Size<CSSType>& value)
    {
        return visitCSSValueChildren(func, value.value);
    }
};

// Specialization for `RectEdges`.
template<typename CSSType> struct CSSValueChildrenVisitor<RectEdges<CSSType>> {
    constexpr IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const RectEdges<CSSType>& value)
    {
        if (visitCSSValueChildren(func, value.top()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (visitCSSValueChildren(func, value.right()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (visitCSSValueChildren(func, value.bottom()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (visitCSSValueChildren(func, value.left()) == IterationStatus::Done)
            return IterationStatus::Done;
        return IterationStatus::Continue;
    }
};

// MARK: - Text Stream

// Overload for `CustomIdentifier`.
WTF::TextStream& operator<<(WTF::TextStream&, const CustomIdentifier&);

// Overload for `FunctionNotation`.
template<WebCore::CSSValueID Name, typename T> TextStream& operator<<(TextStream& ts, const FunctionNotation<Name, T>& value)
{
    return ts << nameLiteral(value.name) << "(" << value.parameters << ")";
}

} // namespace CSS
} // namespace WebCore

namespace std {

template<typename T, size_t N> class tuple_size<WebCore::CSS::SpaceSeparatedArray<T, N>> : public std::integral_constant<size_t, N> { };
template<size_t I, typename T, size_t N> class tuple_element<I, WebCore::CSS::SpaceSeparatedArray<T, N>> {
public:
    using type = T;
};

template<typename T, size_t N> class tuple_size<WebCore::CSS::CommaSeparatedArray<T, N>> : public std::integral_constant<size_t, N> { };
template<size_t I, typename T, size_t N> class tuple_element<I, WebCore::CSS::CommaSeparatedArray<T, N>> {
public:
    using type = T;
};

template<typename... Ts> class tuple_size<WebCore::CSS::SpaceSeparatedTuple<Ts...>> : public std::integral_constant<size_t, sizeof...(Ts)> { };
template<size_t I, typename... Ts> class tuple_element<I, WebCore::CSS::SpaceSeparatedTuple<Ts...>> {
public:
    using type = tuple_element_t<I, tuple<Ts...>>;
};

template<typename... Ts> class tuple_size<WebCore::CSS::CommaSeparatedTuple<Ts...>> : public std::integral_constant<size_t, sizeof...(Ts)> { };
template<size_t I, typename... Ts> class tuple_element<I, WebCore::CSS::CommaSeparatedTuple<Ts...>> {
public:
    using type = tuple_element_t<I, tuple<Ts...>>;
};

template<typename T> class tuple_size<WebCore::CSS::Point<T>> : public std::integral_constant<size_t, 2> { };
template<size_t I, typename T> class tuple_element<I, WebCore::CSS::Point<T>> {
public:
    using type = T;
};

template<typename T> class tuple_size<WebCore::CSS::Size<T>> : public std::integral_constant<size_t, 2> { };
template<size_t I, typename T> class tuple_element<I, WebCore::CSS::Size<T>> {
public:
    using type = T;
};

#define CSS_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    namespace std { \
        template<> class tuple_size<WebCore::CSS::t> : public std::integral_constant<size_t, numberOfArguments> { }; \
        template<size_t I> class tuple_element<I, WebCore::CSS::t> { \
        public: \
            using type = decltype(WebCore::CSS::get<I>(std::declval<WebCore::CSS::t>())); \
        }; \
    } \
    template<> inline constexpr bool WebCore::CSS::TreatAsTupleLike<WebCore::CSS::t> = true; \
\

} // namespace std
