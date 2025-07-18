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

#include "CSSValueConcepts.h"
#include "CSSValueKeywords.h"
#include "RectCorners.h"
#include "RectEdges.h"
#include <optional>
#include <tuple>
#include <utility>
#include <wtf/FixedVector.h>
#include <wtf/Markable.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

enum class SerializationSeparatorType : uint8_t { None, Space, Comma, Slash };

// Types that specialize TreatAsTupleLike or TreatAsRangeLike can specialize this to
// indicate how to serialize the gaps between elements.
template<typename> inline constexpr SerializationSeparatorType SerializationSeparator = SerializationSeparatorType::None;

template<SerializationSeparatorType> inline constexpr ASCIILiteral SerializationSeparatorStringForType = ""_s;
template<> inline constexpr ASCIILiteral SerializationSeparatorStringForType<SerializationSeparatorType::Space> = " "_s;
template<> inline constexpr ASCIILiteral SerializationSeparatorStringForType<SerializationSeparatorType::Comma> = ", "_s;
template<> inline constexpr ASCIILiteral SerializationSeparatorStringForType<SerializationSeparatorType::Slash> = " / "_s;

template<typename T> inline constexpr ASCIILiteral SerializationSeparatorString = SerializationSeparatorStringForType<SerializationSeparator<T>>;

// Helper to define a simple `get()` implementation for a single value `name`.
#define DEFINE_TYPE_WRAPPER_GET(t, name) \
    template<size_t> const auto& get(const t& value) { return value.name; }

// Helper to define a type by extending another type via inheritance.
#define DEFINE_TYPE_EXTENDER(wrapper, wrapped)                                \
    struct wrapper : wrapped {                                                \
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(wrapper);                                       \
        using Wrapped = wrapped;                                              \
        using Wrapped::Wrapped;                                               \
        template<size_t I> friend const auto& get(const wrapper& self)        \
        {                                                                     \
            return get<I>(static_cast<const wrapped&>(self));                 \
        }                                                                     \
        bool operator==(const wrapper&) const = default;                      \
    };

// Helper to define a type via direct wrapping of another type.
#define DEFINE_TYPE_WRAPPER(wrapper, wrapped)                                 \
    struct wrapper {                                                          \
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(wrapper);                                       \
        using Wrapped = wrapped;                                              \
        wrapped value;                                                        \
        template<typename... Args>                                            \
        wrapper(Args&&... args) requires (requires { { wrapped(args...) }; }) \
            : value(std::forward<Args>(args)...)                              \
        {                                                                     \
        }                                                                     \
        const Wrapped& operator*() const { return value; }                    \
        Wrapped& operator*() { return value; }                                \
        const Wrapped* operator->() const { return &value; }                  \
        Wrapped* operator->() { return &value; }                              \
        template<size_t> friend const auto& get(const wrapper& self)          \
        {                                                                     \
            return self.value;                                                \
        }                                                                     \
        bool operator==(const wrapper&) const = default;                      \
    };

// Helper to define a tuple-like conformance for a type with `numberOfArguments` arguments.
#define DEFINE_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    namespace std { \
        template<> class tuple_size<t> : public std::integral_constant<size_t, numberOfArguments> { }; \
        template<size_t I> class tuple_element<I, t> { \
        public: \
            using type = decltype(get<I>(std::declval<t>())); \
        }; \
    } \
    template<> inline constexpr bool WebCore::TreatAsTupleLike<t> = true;

// Helper to define a tuple-like conformance and that the type should be serialized as space separated.
#define DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    DEFINE_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    template<> inline constexpr WebCore::SerializationSeparatorType WebCore::SerializationSeparator<t> = WebCore::SerializationSeparatorType::Space;

// Helper to define a tuple-like conformance and that the type should be serialized as comma separated.
#define DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    DEFINE_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    template<> inline constexpr WebCore::SerializationSeparatorType WebCore::SerializationSeparator<t> = WebCore::SerializationSeparatorType::Comma;

// Helper to define a tuple-like conformance and that the type should be serialized as slash separated.
#define DEFINE_SLASH_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    DEFINE_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    template<> inline constexpr WebCore::SerializationSeparatorType WebCore::SerializationSeparator<t> = WebCore::SerializationSeparatorType::Slash;

// Helper to define a tuple-like conformance based on the type being extended.
#define DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_EXTENDER(t) \
    DEFINE_TUPLE_LIKE_CONFORMANCE(t, std::tuple_size_v<t::Wrapped>) \
    template<> inline constexpr WebCore::SerializationSeparatorType WebCore::SerializationSeparator<t> = WebCore::SerializationSeparator<t::Wrapped>;

// Helper to define a tuple-like conformance for a wrapper type.
#define DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(t) \
    DEFINE_TUPLE_LIKE_CONFORMANCE(t, 1)

// Helper to define a variant-like conformance.
#define DEFINE_VARIANT_LIKE_CONFORMANCE(t) \
    template<> inline constexpr auto WebCore::TreatAsVariantLike<t> = true;

// Helper to define a range-like conformance.
#define DEFINE_RANGE_LIKE_CONFORMANCE(t) \
    template<> inline constexpr auto WebCore::TreatAsRangeLike<t> = true;

// MARK: - Conforming Existing Types

// - Optional-like
template<typename T> inline constexpr auto TreatAsOptionalLike<std::optional<T>> = true;
template<typename T> inline constexpr auto TreatAsOptionalLike<WTF::Markable<T>> = true;

// - Tuple-like
template<typename... Ts> inline constexpr auto TreatAsTupleLike<std::tuple<Ts...>> = true;

// - Variant-like
template<typename... Ts> inline constexpr auto TreatAsVariantLike<Variant<Ts...>> = true;

// MARK: - Standard Leaf Types

// Helper type used to represent an arbitrary constant identifier.
struct CustomIdentifier {
    AtomString value;

    bool operator==(const CustomIdentifier&) const = default;
    bool operator==(const AtomString& other) const { return value == other; }
};
TextStream& operator<<(TextStream&, const CustomIdentifier&);

template<CSSValueID C> TextStream& operator<<(TextStream& ts, const Constant<C>&)
{
    return ts << nameLiteral(C);
}

// MARK: - Standard Aggregates

// Helper type used to represent a CSS function.
template<CSSValueID C, typename T> struct FunctionNotation {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(CustomIdentifier);

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

template<CSSValueID C, typename T> bool operator==(const UniqueRef<FunctionNotation<C, T>>& a, const UniqueRef<FunctionNotation<C, T>>& b)
{
    return arePointingToEqualData(a, b);
}

template<size_t, CSSValueID C, typename T> const auto& get(const FunctionNotation<C, T>& function)
{
    return function.parameters;
}

template<CSSValueID C, typename T> TextStream& operator<<(TextStream& ts, const FunctionNotation<C, T>& function)
{
    return ts << nameLiteral(function.name) << '(' << function.parameters << ')';
}

template<CSSValueID C, typename T> inline constexpr auto TreatAsTupleLike<FunctionNotation<C, T>> = true;

// Wraps a variable number of elements of a single type, semantically marking them as serializing as "space separated".
template<typename T, size_t inlineCapacity = 0> struct SpaceSeparatedVector {
    using Container = WTF::Vector<T, inlineCapacity>;
    using const_iterator = typename Container::const_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    using value_type = typename Container::value_type;

    SpaceSeparatedVector() = default;

    SpaceSeparatedVector(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    SpaceSeparatedVector(Container&& value)
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

    template<typename F> decltype(auto) map(F&& functor) const { return value.map(std::forward<F>(functor)); }

    bool operator==(const SpaceSeparatedVector&) const = default;

    Container value;
};

template<typename T, size_t N> inline constexpr auto TreatAsRangeLike<SpaceSeparatedVector<T, N>> = true;
template<typename T, size_t N> inline constexpr auto SerializationSeparator<SpaceSeparatedVector<T, N>> = SerializationSeparatorType::Space;

// Wraps a variable number of elements of a single type, semantically marking them as serializing as "comma separated".
template<typename T, size_t inlineCapacity = 0> struct CommaSeparatedVector {
    using Container = WTF::Vector<T, inlineCapacity>;
    using const_iterator = typename Container::const_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    using value_type = typename Container::value_type;

    CommaSeparatedVector() = default;

    CommaSeparatedVector(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    CommaSeparatedVector(Container&& value)
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

    template<typename F> decltype(auto) map(F&& functor) const { return value.map(std::forward<F>(functor)); }

    bool operator==(const CommaSeparatedVector&) const = default;

    Container value;
};

template<typename T, size_t N> inline constexpr auto TreatAsRangeLike<CommaSeparatedVector<T, N>> = true;
template<typename T, size_t N> inline constexpr auto SerializationSeparator<CommaSeparatedVector<T, N>> = SerializationSeparatorType::Comma;

// Wraps a variable (though known at construction) number of elements of a single type, semantically marking them as serializing as "space separated".
template<typename T> struct SpaceSeparatedFixedVector {
    using Container = WTF::FixedVector<T>;
    using const_iterator = typename Container::const_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    using value_type = typename Container::value_type;

    SpaceSeparatedFixedVector() = default;

    SpaceSeparatedFixedVector(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    SpaceSeparatedFixedVector(Container&& value)
        : value { WTFMove(value) }
    {
    }

    SpaceSeparatedFixedVector(T&& value)
        : value { WTFMove(value) }
    {
    }

    template<typename SizedRange, typename Mapper>
    static SpaceSeparatedFixedVector map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        return Container::map(std::forward<SizedRange>(range), std::forward<Mapper>(mapper));
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const T& operator[](size_t i) const { return value[i]; }

    const T& first() const LIFETIME_BOUND { return value.first(); }
    const T& last() const LIFETIME_BOUND { return value.last(); }

    bool contains(const auto& x) const { return value.contains(x); }
    bool containsIf(NOESCAPE const Invocable<bool(const value_type&)> auto& f) const { return value.containsIf(f); }

    template<typename F> decltype(auto) map(F&& functor) const { return value.map(std::forward<F>(functor)); }

    bool operator==(const SpaceSeparatedFixedVector&) const = default;

    Container value;
};

template<typename T> inline constexpr auto TreatAsRangeLike<SpaceSeparatedFixedVector<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<SpaceSeparatedFixedVector<T>> = SerializationSeparatorType::Space;

// Wraps a variable (though known at construction) number of elements of a single type, semantically marking them as serializing as "comma separated".
template<typename T> struct CommaSeparatedFixedVector {
    using Container = WTF::FixedVector<T>;
    using const_iterator = typename Container::const_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    using value_type = typename Container::value_type;

    CommaSeparatedFixedVector() = default;

    CommaSeparatedFixedVector(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    CommaSeparatedFixedVector(Container&& value)
        : value { WTFMove(value) }
    {
    }

    CommaSeparatedFixedVector(T&& value)
        : value { WTFMove(value) }
    {
    }

    template<typename SizedRange, typename Mapper>
    static CommaSeparatedFixedVector map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        return Container::map(std::forward<SizedRange>(range), std::forward<Mapper>(mapper));
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const T& operator[](size_t i) const { return value[i]; }

    const T& first() const LIFETIME_BOUND { return value.first(); }
    const T& last() const LIFETIME_BOUND { return value.last(); }

    bool contains(const auto& x) const { return value.contains(x); }
    bool containsIf(NOESCAPE const Invocable<bool(const value_type&)> auto& f) const { return value.containsIf(f); }

    template<typename F> decltype(auto) map(F&& functor) const { return value.map(std::forward<F>(functor)); }

    bool operator==(const CommaSeparatedFixedVector&) const = default;

    Container value;
};

template<typename T> inline constexpr auto TreatAsRangeLike<CommaSeparatedFixedVector<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<CommaSeparatedFixedVector<T>> = SerializationSeparatorType::Comma;

// Wraps a list and enforces the invariant that it is either created with a non-empty value or `CSS::Keyword::None`.
template<typename T> struct ListOrNone {
    using List = T;
    using const_iterator = typename List::const_iterator;
    using const_reverse_iterator = typename List::const_reverse_iterator;
    using value_type = typename List::value_type;

    ListOrNone(List&& list)
        : value { WTFMove(list) }
    {
        RELEASE_ASSERT(!value.isEmpty());
    }

    ListOrNone(CSS::Keyword::None)
        : value { }
    {
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    const value_type& first() const LIFETIME_BOUND { return value.first(); }
    const value_type& last() const LIFETIME_BOUND { return value.last(); }

    size_t size() const { return value.size(); }
    const value_type& operator[](size_t i) const { return value[i]; }

    bool contains(const auto& x) const { return value.contains(x); }
    bool containsIf(NOESCAPE const Invocable<bool(const value_type&)> auto& f) const { return value.containsIf(f); }

    bool operator==(const ListOrNone&) const = default;

    bool isNone() const { return value.isEmpty(); }
    bool isList() const { return !value.isEmpty(); }
    const List* tryList() const { return isList() ? &value : nullptr; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(value);
    }

private:
    // An empty list indicates the value `none`. This invariant is ensured
    // with a release assert in the constructor.
    List value;
};

template<typename T> inline constexpr auto TreatAsVariantLike<ListOrNone<T>> = true;

// Concept to constrain types to only those that derive from `ListOrNone`.
template<typename T> concept ListOrNoneDerived = WTF::IsBaseOfTemplate<ListOrNone, T>::value;

// Wraps a list and makes it so that when the list is empty, it looks to clients like it has a single "default" item in instead.
template<typename T, typename Defaulter> struct ListOrDefault {
    using List = T;
    using value_type = typename List::value_type;

    // Special value to construct the empty (e.g. list with just the default value) list.
    struct DefaultValueToken { };
    static constexpr DefaultValueToken DefaultValue { };

    // Iterator that iterates a fictitious single item list, [default value], if the underlying list is empty, or the underlying list.
    struct const_iterator {
        typename List::const_iterator it;
        bool atEndForDefault;
        const ListOrDefault<List, Defaulter>* owner;

        using iterator_category = std::forward_iterator_tag;
        using value_type = typename List::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        const value_type& operator*() const
        {
            if (owner->isDefault())
                return owner->defaulter();
            return *it;
        }

        const_iterator& operator++()
        {
            if (owner->isDefault()) {
                atEndForDefault = true;
            } else {
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
                ++it;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
            }
            return *this;
        }

        const_iterator operator++(int)
        {
            auto result = *this;
            ++*this;
            return result;
        }

        bool operator==(const const_iterator& other) const = default;
    };

    ListOrDefault(List&& list, Defaulter&& defaulter = Defaulter())
        : value { WTFMove(list) }
        , defaulter { WTFMove(defaulter) }
    {
    }

    ListOrDefault(DefaultValueToken, Defaulter&& defaulter = Defaulter())
        : value { }
        , defaulter { WTFMove(defaulter) }
    {
    }

    const_iterator begin() const { return { .it = value.begin(), .atEndForDefault = !isDefault(), .owner = this }; }
    const_iterator end() const { return { .it = value.end(), .atEndForDefault = true, .owner = this }; }

    size_t size() const { return isDefault() ? 1 : value.size(); }
    const value_type& operator[](size_t i) const { return isDefault() ? defaulter() : value[i]; }

    bool contains(const auto& x) const { return isDefault() ? (x == defaulter()) : value.contains(x); }
    bool containsIf(NOESCAPE const Invocable<bool(const value_type&)> auto& f) const { return isDefault() ? f(defaulter()) : value.containsIf(f); }

    bool isDefault() const { return value.isEmpty(); }

    bool operator==(const ListOrDefault&) const = default;

private:
    friend struct const_iterator;

    List value;
    NO_UNIQUE_ADDRESS Defaulter defaulter;
};

template<typename List, typename Defaulter> inline constexpr auto TreatAsRangeLike<ListOrDefault<List, Defaulter>> = true;
template<typename List, typename Defaulter> inline constexpr auto SerializationSeparator<ListOrDefault<List, Defaulter>> = SerializationSeparator<List>;

// Concept to constrain types to only those that derive from `ListOrDefault`.
template<typename T> concept ListOrDefaultDerived = WTF::IsBaseOfTemplate<ListOrDefault, T>::value;

// Wraps a fixed size list of elements of a single type, semantically marking them as serializing as "space separated".
template<typename T, size_t N> struct SpaceSeparatedArray {
    using Array = std::array<T, N>;
    using value_type = T;

    template<typename... Ts>
        requires (sizeof...(Ts) == N && WTF::all<std::convertible_to<Ts, T>...>)
    constexpr SpaceSeparatedArray(Ts... values)
        : value { std::forward<Ts>(values)... }
    {
    }

    constexpr SpaceSeparatedArray(std::array<T, N>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const SpaceSeparatedArray<T, N>&) const = default;

    std::array<T, N> value;
};

template<typename T, typename... Ts>
    requires (WTF::all<std::convertible_to<Ts, T>...>)
SpaceSeparatedArray(T, Ts...) -> SpaceSeparatedArray<T, 1 + sizeof...(Ts)>;

template<size_t I, typename T, size_t N> decltype(auto) get(const SpaceSeparatedArray<T, N>& array)
{
    return std::get<I>(array.value);
}

template<typename T, size_t N> inline constexpr auto TreatAsTupleLike<SpaceSeparatedArray<T, N>> = true;
template<typename T, size_t N> inline constexpr auto SerializationSeparator<SpaceSeparatedArray<T, N>> = SerializationSeparatorType::Space;

// Convenience for representing a two element array.
template<typename T> using SpaceSeparatedPair = SpaceSeparatedArray<T, 2>;

// Wraps a fixed size list of elements of a single type, semantically marking them as serializing as "comma separated".
template<typename T, size_t N> struct CommaSeparatedArray {
    using Array = std::array<T, N>;
    using value_type = T;

    template<typename... Ts>
        requires (sizeof...(Ts) == N && WTF::all<std::convertible_to<Ts, T>...>)
    constexpr CommaSeparatedArray(Ts... values)
        : value { std::forward<Ts>(values)... }
    {
    }

    constexpr CommaSeparatedArray(std::array<T, N>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const CommaSeparatedArray<T, N>&) const = default;

    std::array<T, N> value;
};


template<typename T, typename... Ts>
    requires (WTF::all<std::convertible_to<Ts, T>...>)
CommaSeparatedArray(T, Ts...) -> CommaSeparatedArray<T, 1 + sizeof...(Ts)>;

template<size_t I, typename T, size_t N> decltype(auto) get(const CommaSeparatedArray<T, N>& array)
{
    return std::get<I>(array.value);
}

template<typename T, size_t N> inline constexpr auto TreatAsTupleLike<CommaSeparatedArray<T, N>> = true;
template<typename T, size_t N> inline constexpr auto SerializationSeparator<CommaSeparatedArray<T, N>> = SerializationSeparatorType::Comma;

// Convenience for representing a two element array.
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

template<typename... Ts> inline constexpr auto TreatAsTupleLike<SpaceSeparatedTuple<Ts...>> = true;
template<typename... Ts> inline constexpr auto SerializationSeparator<SpaceSeparatedTuple<Ts...>> = SerializationSeparatorType::Space;

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

template<typename... Ts> inline constexpr auto TreatAsTupleLike<CommaSeparatedTuple<Ts...>> = true;
template<typename... Ts> inline constexpr auto SerializationSeparator<CommaSeparatedTuple<Ts...>> = SerializationSeparatorType::Comma;

// Wraps a pair of elements of a single type representing a point, semantically marking them as serializing as "space separated".
template<typename T> struct SpaceSeparatedPoint {
    using Array = SpaceSeparatedPair<T>;
    using value_type = T;

    constexpr SpaceSeparatedPoint(T p1, T p2)
        : value { WTFMove(p1), WTFMove(p2) }
    {
    }

    constexpr SpaceSeparatedPoint(SpaceSeparatedPair<T>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const SpaceSeparatedPoint<T>&) const = default;

    const T& x() const { return get<0>(value); }
    const T& y() const { return get<1>(value); }

    SpaceSeparatedPair<T> value;
};

template<size_t I, typename T> decltype(auto) get(const SpaceSeparatedPoint<T>& point)
{
    return get<I>(point.value);
}

template<typename T> inline constexpr auto TreatAsTupleLike<SpaceSeparatedPoint<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<SpaceSeparatedPoint<T>> = SerializationSeparatorType::Space;

// Wraps a pair of elements of a single type representing a size, semantically marking them as serializing as "space separated".
template<typename T> struct SpaceSeparatedSize {
    using Array = SpaceSeparatedPair<T>;
    using value_type = T;

    constexpr SpaceSeparatedSize(T p1, T p2)
        : value { WTFMove(p1), WTFMove(p2) }
    {
    }

    constexpr SpaceSeparatedSize(SpaceSeparatedPair<T>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const SpaceSeparatedSize<T>&) const = default;

    const T& width() const { return get<0>(value); }
    const T& height() const { return get<1>(value); }

    SpaceSeparatedPair<T> value;
};

template<size_t I, typename T> decltype(auto) get(const SpaceSeparatedSize<T>& size)
{
    return get<I>(size.value);
}

template<typename T> inline constexpr auto TreatAsTupleLike<SpaceSeparatedSize<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<SpaceSeparatedSize<T>> = SerializationSeparatorType::Space;

// Wraps a pair of elements of a single type representing a size, semantically marking them as serializing as "space separated" and "minimally serializing".
template<typename T> struct MinimallySerializingSpaceSeparatedSize {
    using Array = SpaceSeparatedPair<T>;
    using value_type = T;

    constexpr MinimallySerializingSpaceSeparatedSize(T p1, T p2)
        : value { WTFMove(p1), WTFMove(p2) }
    {
    }

    constexpr MinimallySerializingSpaceSeparatedSize(SpaceSeparatedPair<T>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const MinimallySerializingSpaceSeparatedSize<T>&) const = default;

    const T& width() const { return get<0>(value); }
    const T& height() const { return get<1>(value); }

    SpaceSeparatedPair<T> value;
};

template<size_t I, typename T> decltype(auto) get(const MinimallySerializingSpaceSeparatedSize<T>& size)
{
    return get<I>(size.value);
}

template<typename T> inline constexpr auto TreatAsTupleLike<MinimallySerializingSpaceSeparatedSize<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<MinimallySerializingSpaceSeparatedSize<T>> = SerializationSeparatorType::Space;

// Wraps a quad of elements of a single type representing the edges of a rect, semantically marking them as serializing as "space separated".
template<typename T> struct SpaceSeparatedRectEdges : RectEdges<T> {
    using value_type = T;

    constexpr SpaceSeparatedRectEdges(T repeat)
        : RectEdges<T> { repeat, repeat, repeat, repeat }
    {
    }

    constexpr SpaceSeparatedRectEdges(T top, T right, T bottom, T left)
        : RectEdges<T> { WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left) }
    {
    }

    constexpr SpaceSeparatedRectEdges(RectEdges<T>&& rectEdges)
        : RectEdges<T> { WTFMove(rectEdges) }
    {
    }

    constexpr bool operator==(const SpaceSeparatedRectEdges<T>&) const = default;
};

template<size_t I, typename T> const auto& get(const SpaceSeparatedRectEdges<T>& rectEdges)
{
    if constexpr (!I)
        return rectEdges.top();
    else if constexpr (I == 1)
        return rectEdges.right();
    else if constexpr (I == 2)
        return rectEdges.bottom();
    else if constexpr (I == 3)
        return rectEdges.left();
}

template<typename T> inline constexpr auto TreatAsTupleLike<SpaceSeparatedRectEdges<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<SpaceSeparatedRectEdges<T>> = SerializationSeparatorType::Space;

// Wraps a quad of elements of a single type representing the edges of a rect, semantically marking them as serializing as "comma separated".
template<typename T> struct CommaSeparatedRectEdges : RectEdges<T> {
    using value_type = T;

    constexpr CommaSeparatedRectEdges(T repeat)
        : RectEdges<T> { repeat, repeat, repeat, repeat }
    {
    }

    constexpr CommaSeparatedRectEdges(T top, T right, T bottom, T left)
        : RectEdges<T> { WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left) }
    {
    }

    constexpr CommaSeparatedRectEdges(RectEdges<T>&& rectEdges)
        : RectEdges<T> { WTFMove(rectEdges) }
    {
    }

    constexpr bool operator==(const CommaSeparatedRectEdges<T>&) const = default;
};

template<size_t I, typename T> const auto& get(const CommaSeparatedRectEdges<T>& rectEdges)
{
    if constexpr (!I)
        return rectEdges.top();
    else if constexpr (I == 1)
        return rectEdges.right();
    else if constexpr (I == 2)
        return rectEdges.bottom();
    else if constexpr (I == 3)
        return rectEdges.left();
}

template<typename T> inline constexpr auto TreatAsTupleLike<CommaSeparatedRectEdges<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<CommaSeparatedRectEdges<T>> = SerializationSeparatorType::Comma;


// A set of 4 values parsed and interpreted in the same manner as defined for the margin shorthand.
//
// <minimally-serializing-rect-edges> = <type>{1,4}
//
// - if only 1 value, `a`, is provided, set top, bottom, right & left to `a`.
// - if only 2 values, `a` and `b` are provided, set top & bottom to `a`, right & left to `b`.
// - if only 3 values, `a`, `b`, and `c` are provided, set top to `a`, right to `b`, bottom to `c`, & left to `b`.
//
// As the name implies, the benefit of using this over `SpaceSeparatedRectEdges` directly
// is that this will serialize in its minimal form, checking for element equality and only
// serializing what is necessary.
template<typename T> struct MinimallySerializingSpaceSeparatedRectEdges : RectEdges<T> {
    using value_type = T;

    constexpr MinimallySerializingSpaceSeparatedRectEdges(T value)
        : RectEdges<T> { value, value, value, value }
    {
    }

    constexpr MinimallySerializingSpaceSeparatedRectEdges(T top, T right, T bottom, T left)
        : RectEdges<T> { WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left) }
    {
    }

    constexpr MinimallySerializingSpaceSeparatedRectEdges(RectEdges<T>&& rectEdges)
        : RectEdges<T> { WTFMove(rectEdges) }
    {
    }

    constexpr bool operator==(const MinimallySerializingSpaceSeparatedRectEdges<T>&) const = default;
};

template<size_t I, typename T> decltype(auto) get(const MinimallySerializingSpaceSeparatedRectEdges<T>& value)
{
    if constexpr (!I)
        return value.top();
    else if constexpr (I == 1)
        return value.right();
    else if constexpr (I == 2)
        return value.bottom();
    else if constexpr (I == 3)
        return value.left();
}

template<typename T> inline constexpr auto TreatAsTupleLike<MinimallySerializingSpaceSeparatedRectEdges<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<MinimallySerializingSpaceSeparatedRectEdges<T>> = SerializationSeparatorType::Space;

template<typename T> struct MinimallySerializingSpaceSeparatedRectCorners : RectCorners<T> {
    using value_type = T;

    constexpr MinimallySerializingSpaceSeparatedRectCorners(T value)
        : RectCorners<T> { value, value, value, value }
    {
    }

    constexpr MinimallySerializingSpaceSeparatedRectCorners(T topLeft, T topRight, T bottomLeft, T bottomRight)
        : RectCorners<T> { WTFMove(topLeft), WTFMove(topRight), WTFMove(bottomLeft), WTFMove(bottomRight) }
    {
    }

    constexpr MinimallySerializingSpaceSeparatedRectCorners(RectCorners<T>&& rectCorners)
        : RectCorners<T> { WTFMove(rectCorners) }
    {
    }

    constexpr bool operator==(const MinimallySerializingSpaceSeparatedRectCorners<T>&) const = default;
};

template<size_t I, typename T> decltype(auto) get(const MinimallySerializingSpaceSeparatedRectCorners<T>& value)
{
    if constexpr (!I)
        return value.topLeft();
    else if constexpr (I == 1)
        return value.topRight();
    else if constexpr (I == 2)
        return value.bottomLeft();
    else if constexpr (I == 3)
        return value.bottomRight();
}

template<typename T> inline constexpr auto TreatAsTupleLike<MinimallySerializingSpaceSeparatedRectCorners<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<MinimallySerializingSpaceSeparatedRectCorners<T>> = SerializationSeparatorType::Space;

// MARK: - Logging

template<typename T> void logForCSSOnTupleLike(TextStream& ts, const T& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    auto caller = WTF::makeVisitor(
        [&]<typename U>(const std::optional<U>& element) {
            if (!element)
                return;
            ts << std::exchange(swappedSeparator, separator);
            ts << *element;
        },
        [&]<typename U>(const Markable<U>& element) {
            if (!element)
                return;
            ts << std::exchange(swappedSeparator, separator);
            ts << *element;
        },
        [&](const auto& element) {
            ts << std::exchange(swappedSeparator, separator);
            ts << element;
        }
    );

    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);
}

template<typename T> void logForCSSOnRangeLike(TextStream& ts, const T& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    for (const auto& element : value) {
        ts << std::exchange(swappedSeparator, separator);
        ts << element;
    }
}

template<typename T> void logForCSSOnVariantLike(TextStream& ts, const T& value)
{
    WTF::switchOn(value, [&](const auto& value) { ts << value; });
}

template<typename T, size_t inlineCapacity> TextStream& operator<<(TextStream& ts, const SpaceSeparatedVector<T, inlineCapacity>& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<SpaceSeparatedVector<T, inlineCapacity>>);
    return ts;
}

template<typename T, size_t inlineCapacity> TextStream& operator<<(TextStream& ts, const CommaSeparatedVector<T, inlineCapacity>& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<CommaSeparatedVector<T, inlineCapacity>>);
    return ts;
}

template<typename T> TextStream& operator<<(TextStream& ts, const SpaceSeparatedFixedVector<T>& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<SpaceSeparatedFixedVector<T>>);
    return ts;
}

template<typename T> TextStream& operator<<(TextStream& ts, const CommaSeparatedFixedVector<T>& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<CommaSeparatedFixedVector<T>>);
    return ts;
}

template<typename... Ts> TextStream& operator<<(TextStream& ts, const SpaceSeparatedTuple<Ts...>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<SpaceSeparatedTuple<Ts...>>);
    return ts;
}

template<typename... Ts> TextStream& operator<<(TextStream& ts, const CommaSeparatedTuple<Ts...>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<CommaSeparatedTuple<Ts...>>);
    return ts;
}

} // namespace WebCore

namespace std {

template<WebCore::CSSValueID C, typename T> class tuple_size<WebCore::FunctionNotation<C, T>> : public std::integral_constant<size_t, 1> { };
template<size_t I, WebCore::CSSValueID C, typename T> class tuple_element<I, WebCore::FunctionNotation<C, T>> {
public:
    using type = T;
};

template<typename T, size_t N> class tuple_size<WebCore::SpaceSeparatedArray<T, N>> : public std::integral_constant<size_t, N> { };
template<size_t I, typename T, size_t N> class tuple_element<I, WebCore::SpaceSeparatedArray<T, N>> {
public:
    using type = T;
};

template<typename T, size_t N> class tuple_size<WebCore::CommaSeparatedArray<T, N>> : public std::integral_constant<size_t, N> { };
template<size_t I, typename T, size_t N> class tuple_element<I, WebCore::CommaSeparatedArray<T, N>> {
public:
    using type = T;
};

template<typename... Ts> class tuple_size<WebCore::SpaceSeparatedTuple<Ts...>> : public std::integral_constant<size_t, sizeof...(Ts)> { };
template<size_t I, typename... Ts> class tuple_element<I, WebCore::SpaceSeparatedTuple<Ts...>> {
public:
    using type = tuple_element_t<I, tuple<Ts...>>;
};

template<typename... Ts> class tuple_size<WebCore::CommaSeparatedTuple<Ts...>> : public std::integral_constant<size_t, sizeof...(Ts)> { };
template<size_t I, typename... Ts> class tuple_element<I, WebCore::CommaSeparatedTuple<Ts...>> {
public:
    using type = tuple_element_t<I, tuple<Ts...>>;
};

template<typename T> class tuple_size<WebCore::SpaceSeparatedPoint<T>> : public std::integral_constant<size_t, 2> { };
template<size_t I, typename T> class tuple_element<I, WebCore::SpaceSeparatedPoint<T>> {
public:
    using type = T;
};

template<typename T> class tuple_size<WebCore::SpaceSeparatedSize<T>> : public std::integral_constant<size_t, 2> { };
template<size_t I, typename T> class tuple_element<I, WebCore::SpaceSeparatedSize<T>> {
public:
    using type = T;
};

template<typename T> class tuple_size<WebCore::MinimallySerializingSpaceSeparatedSize<T>> : public std::integral_constant<size_t, 2> { };
template<size_t I, typename T> class tuple_element<I, WebCore::MinimallySerializingSpaceSeparatedSize<T>> {
public:
    using type = T;
};

template<typename T> class tuple_size<WebCore::SpaceSeparatedRectEdges<T>> : public std::integral_constant<size_t, 4> { };
template<size_t I, typename T> class tuple_element<I, WebCore::SpaceSeparatedRectEdges<T>> {
public:
    using type = T;
};

template<typename T> class tuple_size<WebCore::CommaSeparatedRectEdges<T>> : public std::integral_constant<size_t, 4> { };
template<size_t I, typename T> class tuple_element<I, WebCore::CommaSeparatedRectEdges<T>> {
public:
    using type = T;
};

template<typename T> class tuple_size<WebCore::MinimallySerializingSpaceSeparatedRectEdges<T>> : public std::integral_constant<size_t, 4> { };
template<size_t I, typename T> class tuple_element<I, WebCore::MinimallySerializingSpaceSeparatedRectEdges<T>> {
public:
    using type = T;
};

template<typename T> class tuple_size<WebCore::MinimallySerializingSpaceSeparatedRectCorners<T>> : public std::integral_constant<size_t, 4> { };
template<size_t I, typename T> class tuple_element<I, WebCore::MinimallySerializingSpaceSeparatedRectCorners<T>> {
public:
    using type = T;
};

} // namespace std

namespace WTF {

template<typename T, size_t inlineCapacity>
struct supports_text_stream_insertion<WebCore::SpaceSeparatedVector<T, inlineCapacity>> : supports_text_stream_insertion<T> { };

template<typename T, size_t inlineCapacity>
struct supports_text_stream_insertion<WebCore::CommaSeparatedVector<T, inlineCapacity>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::SpaceSeparatedFixedVector<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::CommaSeparatedFixedVector<T>> : supports_text_stream_insertion<T> { };

template<>
struct MarkableTraits<WebCore::CustomIdentifier> {
    static bool isEmptyValue(const WebCore::CustomIdentifier& value) { return value.value.isNull(); }
    static WebCore::CustomIdentifier emptyValue() { return WebCore::CustomIdentifier { nullAtom() }; }
};

} // namespace WTF
