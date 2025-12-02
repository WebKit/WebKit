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

#include <WebCore/CSSPropertyNames.h>
#include <WebCore/CSSValueConcepts.h>
#include <WebCore/CSSValueKeywords.h>
#include <WebCore/RectCorners.h>
#include <WebCore/RectEdges.h>
#include <optional>
#include <tuple>
#include <utility>
#include <wtf/EnumSet.h>
#include <wtf/FixedVector.h>
#include <wtf/ListHashSet.h>
#include <wtf/Markable.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

enum class SerializationSeparatorType : uint8_t { None, Space, Comma, Slash };
enum class SerializationCoalescingType : uint8_t { None, Minimal };

// Types that specialize TreatAsTupleLike or TreatAsRangeLike can specialize this to
// indicate how to serialize the gaps between elements.
template<typename> inline constexpr SerializationSeparatorType SerializationSeparator = SerializationSeparatorType::None;

template<SerializationSeparatorType> inline constexpr ASCIILiteral SerializationSeparatorStringForType = ""_s;
template<> inline constexpr ASCIILiteral SerializationSeparatorStringForType<SerializationSeparatorType::Space> = " "_s;
template<> inline constexpr ASCIILiteral SerializationSeparatorStringForType<SerializationSeparatorType::Comma> = ", "_s;
template<> inline constexpr ASCIILiteral SerializationSeparatorStringForType<SerializationSeparatorType::Slash> = " / "_s;

template<typename T> inline constexpr ASCIILiteral SerializationSeparatorString = SerializationSeparatorStringForType<SerializationSeparator<T>>;

// Types that specialize TreatAsTupleLike and have size 2 or 4 can specialize this to
// indicate how to serialize identical elements.
template<typename> inline constexpr SerializationCoalescingType SerializationCoalescing = SerializationCoalescingType::None;

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

// Helper to define a tuple-like conformance and that the type should be serialized as coalescing and space separated.
#define DEFINE_COALESCING_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    static_assert(numberOfArguments == 2 || numberOfArguments == 4); \
    DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    template<> inline constexpr WebCore::SerializationCoalescingType WebCore::SerializationCoalescing<t> = WebCore::SerializationCoalescingType::Minimal;

// Helper to define a tuple-like conformance and that the type should be serialized as comma separated.
#define DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    DEFINE_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    template<> inline constexpr WebCore::SerializationSeparatorType WebCore::SerializationSeparator<t> = WebCore::SerializationSeparatorType::Comma;

// Helper to define a tuple-like conformance and that the type should be serialized as coalescing and comma separated.
#define DEFINE_COALESCING_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    static_assert(numberOfArguments == 2 || numberOfArguments == 4); \
    DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    template<> inline constexpr WebCore::SerializationCoalescingType WebCore::SerializationCoalescing<t> = WebCore::SerializationCoalescingType::Minimal;

// Helper to define a tuple-like conformance and that the type should be serialized as slash separated.
#define DEFINE_SLASH_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    DEFINE_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    template<> inline constexpr WebCore::SerializationSeparatorType WebCore::SerializationSeparator<t> = WebCore::SerializationSeparatorType::Slash;

// Helper to define a tuple-like conformance and that the type should be serialized as coalescing and slash separated.
#define DEFINE_COALESCING_SLASH_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    static_assert(numberOfArguments == 2 || numberOfArguments == 4); \
    DEFINE_SLASH_SEPARATED_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    template<> inline constexpr WebCore::SerializationCoalescingType WebCore::SerializationCoalescing<t> = WebCore::SerializationCoalescingType::Minimal;

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

// Helper to define a range-like conformance and that the type should be serialized as space separated.
#define DEFINE_SPACE_SEPARATED_RANGE_LIKE_CONFORMANCE(t) \
    DEFINE_RANGE_LIKE_CONFORMANCE(t) \
    template<> inline constexpr WebCore::SerializationSeparatorType WebCore::SerializationSeparator<t> = WebCore::SerializationSeparatorType::Space;

// Helper to define a range-like conformance and that the type should be serialized as comma separated.
#define DEFINE_COMMA_SEPARATED_RANGE_LIKE_CONFORMANCE(t) \
    DEFINE_RANGE_LIKE_CONFORMANCE(t) \
    template<> inline constexpr WebCore::SerializationSeparatorType WebCore::SerializationSeparator<t> = WebCore::SerializationSeparatorType::Comma;

// Helper to define a range-like conformance and that the type should be serialized as slash separated.
#define DEFINE_SLASH_SEPARATED_RANGE_LIKE_CONFORMANCE(t) \
    DEFINE_RANGE_LIKE_CONFORMANCE(t) \
    template<> inline constexpr WebCore::SerializationSeparatorType WebCore::SerializationSeparator<t> = WebCore::SerializationSeparatorType::Slash;

// Helper to define an empty-like conformance for a type.
#define DEFINE_EMPTY_LIKE_CONFORMANCE(t) \
    template<> inline constexpr auto WebCore::TreatAsEmptyLike<t> = true;

// MARK: - Conforming Existing Types

// - Optional-like
template<typename T> inline constexpr auto TreatAsOptionalLike<std::optional<T>> = true;
template<typename T, typename MarkableTraits> inline constexpr auto TreatAsOptionalLike<WTF::Markable<T, MarkableTraits>> = true;

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

void add(Hasher&, const CustomIdentifier&);

// Helper type used to represent an arbitrary property identifier.
struct PropertyIdentifier {
    CSSPropertyID value;

    bool operator==(const PropertyIdentifier&) const = default;
};
TextStream& operator<<(TextStream&, const PropertyIdentifier&);

void add(Hasher&, const PropertyIdentifier&);

template<CSSValueID C> TextStream& operator<<(TextStream& ts, const Constant<C>&)
{
    return ts << nameLiteral(C);
}

// MARK: - Standard Aggregates

// Helper type used to represent a CSS function.
template<CSSValueID C, typename T> struct FunctionNotation {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(FunctionNotation);

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

// Deduction guide for getter/setters that return values and take r-value references.
template<typename Keyword, typename T>
FunctionNotation(Keyword, T) -> FunctionNotation<Keyword::value, T>;

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

// Wraps an enum set, semantically marking it as serializing as "space separated".
template<typename T>
    requires (std::is_enum_v<T>)
struct SpaceSeparatedEnumSet {
    using Container = EnumSet<T>;
    using StorageType = typename Container::StorageType;
    using const_iterator = typename Container::iterator;
    using value_type = T;

    constexpr SpaceSeparatedEnumSet() = default;

    constexpr SpaceSeparatedEnumSet(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    constexpr SpaceSeparatedEnumSet(Container&& value)
        : value { WTFMove(value) }
    {
    }

    template<typename SizedRange, typename Mapper>
    static SpaceSeparatedEnumSet map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        Container result;
        for (auto&& value : range)
            result.add(mapper(value));
        return result;
    }

    static constexpr SpaceSeparatedEnumSet fromRaw(StorageType rawValue)
    {
        return Container::fromRaw(rawValue);
    }

    constexpr StorageType toRaw() const { return value.toRaw(); }

    constexpr const_iterator begin() const { return value.begin(); }
    constexpr const_iterator end() const { return value.end(); }

    constexpr bool isEmpty() const { return value.isEmpty(); }
    constexpr size_t size() const { return value.size(); }

    constexpr bool contains(T e) const
    {
        return value.contains(e);
    }

    constexpr bool containsAny(Container other) const
    {
        return value.containsAny(other);
    }

    constexpr bool containsAll(Container other) const
    {
        return value.containsAll(other);
    }

    constexpr bool containsOnly(Container other) const
    {
        return value.containsOnly(other);
    }

    constexpr friend SpaceSeparatedEnumSet operator&(SpaceSeparatedEnumSet lhs, SpaceSeparatedEnumSet rhs)
    {
        return lhs.value & rhs.value;
    }

    constexpr friend SpaceSeparatedEnumSet operator-(SpaceSeparatedEnumSet lhs, SpaceSeparatedEnumSet rhs)
    {
        return lhs.value - rhs.value;
    }

    constexpr friend SpaceSeparatedEnumSet operator^(SpaceSeparatedEnumSet lhs, SpaceSeparatedEnumSet rhs)
    {
        return lhs.value ^ rhs.value;
    }

    constexpr bool operator==(const SpaceSeparatedEnumSet&) const = default;

    Container value;
};
template<typename T> inline constexpr auto TreatAsRangeLike<SpaceSeparatedEnumSet<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<SpaceSeparatedEnumSet<T>> = SerializationSeparatorType::Space;

// Wraps an enum set, semantically marking it as serializing as "comma separated".
template<typename T>
    requires (std::is_enum_v<T>)
struct CommaSeparatedEnumSet {
    using Container = EnumSet<T>;
    using StorageType = typename Container::StorageType;
    using const_iterator = typename Container::iterator;
    using value_type = T;

    constexpr CommaSeparatedEnumSet() = default;

    constexpr CommaSeparatedEnumSet(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    constexpr CommaSeparatedEnumSet(Container&& value)
        : value { WTFMove(value) }
    {
    }

    template<typename SizedRange, typename Mapper>
    static CommaSeparatedEnumSet map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        Container result;
        for (auto&& value : range)
            result.add(mapper(value));
        return result;
    }

    static constexpr CommaSeparatedEnumSet fromRaw(StorageType rawValue)
    {
        return Container::fromRaw(rawValue);
    }

    constexpr StorageType toRaw() const { return value.toRaw(); }

    constexpr const_iterator begin() const { return value.begin(); }
    constexpr const_iterator end() const { return value.end(); }

    constexpr bool isEmpty() const { return value.isEmpty(); }
    constexpr size_t size() const { return value.size(); }

    constexpr bool contains(T e) const
    {
        return value.contains(e);
    }

    constexpr bool containsAny(Container other) const
    {
        return value.containsAny(other);
    }

    constexpr bool containsAll(Container other) const
    {
        return value.containsAll(other);
    }

    constexpr bool containsOnly(Container other) const
    {
        return value.containsOnly(other);
    }

    constexpr friend CommaSeparatedEnumSet operator&(CommaSeparatedEnumSet lhs, CommaSeparatedEnumSet rhs)
    {
        return lhs.value & rhs.value;
    }

    constexpr friend CommaSeparatedEnumSet operator-(CommaSeparatedEnumSet lhs, CommaSeparatedEnumSet rhs)
    {
        return lhs.value - rhs.value;
    }

    constexpr friend CommaSeparatedEnumSet operator^(CommaSeparatedEnumSet lhs, CommaSeparatedEnumSet rhs)
    {
        return lhs.value ^ rhs.value;
    }

    bool operator==(const CommaSeparatedEnumSet&) const = default;

    Container value;
};
template<typename T> inline constexpr auto TreatAsRangeLike<CommaSeparatedEnumSet<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<CommaSeparatedEnumSet<T>> = SerializationSeparatorType::Comma;

// Wraps a ListHashSet, semantically marking it as serializing as "space separated".
template<typename T>
struct SpaceSeparatedListHashSet {
    using Container = ListHashSet<T>;
    using const_iterator = typename Container::const_iterator;
    using value_type = T;

    constexpr SpaceSeparatedListHashSet() = default;

    constexpr SpaceSeparatedListHashSet(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    constexpr SpaceSeparatedListHashSet(Container&& value)
        : value { WTFMove(value) }
    {
    }

    template<typename SizedRange, typename Mapper>
    static SpaceSeparatedListHashSet map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        Container result;
        for (auto&& value : range)
            result.add(mapper(value));
        return result;
    }

    constexpr const_iterator begin() const { return value.begin(); }
    constexpr const_iterator end() const { return value.end(); }

    constexpr bool isEmpty() const { return value.isEmpty(); }
    constexpr size_t size() const { return value.size(); }

    constexpr bool contains(const T& item) const { return value.contains(item); }

    constexpr bool operator==(const SpaceSeparatedListHashSet&) const = default;

    Container value;
};
template<typename T> inline constexpr auto TreatAsRangeLike<SpaceSeparatedListHashSet<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<SpaceSeparatedListHashSet<T>> = SerializationSeparatorType::Space;

// Wraps a ListHashSet, semantically marking it as serializing as "comma separated".
template<typename T>
struct CommaSeparatedListHashSet {
    using Container = ListHashSet<T>;
    using const_iterator = typename Container::const_iterator;
    using value_type = T;

    constexpr CommaSeparatedListHashSet() = default;

    constexpr CommaSeparatedListHashSet(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    constexpr CommaSeparatedListHashSet(Container&& value)
        : value { WTFMove(value) }
    {
    }

    template<typename SizedRange, typename Mapper>
    static CommaSeparatedListHashSet map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        Container result;
        for (auto&& value : range)
            result.add(mapper(value));
        return result;
    }

    constexpr const_iterator begin() const { return value.begin(); }
    constexpr const_iterator end() const { return value.end(); }

    constexpr bool isEmpty() const { return value.isEmpty(); }
    constexpr size_t size() const { return value.size(); }

    constexpr bool contains(const T& item) const { return value.contains(item); }

    constexpr bool operator==(const CommaSeparatedListHashSet&) const = default;

    Container value;
};
template<typename T> inline constexpr auto TreatAsRangeLike<CommaSeparatedListHashSet<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<CommaSeparatedListHashSet<T>> = SerializationSeparatorType::Comma;

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

    template<typename SizedRange, typename Mapper>
    static SpaceSeparatedVector map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        return WTF::map<inlineCapacity>(std::forward<SizedRange>(range), std::forward<Mapper>(mapper));
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const T& operator[](size_t i) const { return value[i]; }

    bool contains(const auto& x) const { return value.contains(x); }
    bool containsIf(NOESCAPE const Invocable<bool(const value_type&)> auto& f) const { return value.containsIf(f); }

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

    template<typename SizedRange, typename Mapper>
    static CommaSeparatedVector map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        return WTF::map<inlineCapacity>(std::forward<SizedRange>(range), std::forward<Mapper>(mapper));
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const T& operator[](size_t i) const { return value[i]; }

    bool contains(const auto& x) const { return value.contains(x); }
    bool containsIf(NOESCAPE const Invocable<bool(const value_type&)> auto& f) const { return value.containsIf(f); }

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

    template<std::invocable<size_t> Generator>
    static SpaceSeparatedFixedVector createWithSizeFromGenerator(size_t size, NOESCAPE Generator&& generator)
    {
        return Container::createWithSizeFromGenerator(size, std::forward<Generator>(generator));
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

    template<std::invocable<size_t> Generator>
    static CommaSeparatedFixedVector createWithSizeFromGenerator(size_t size, NOESCAPE Generator&& generator)
    {
        return Container::createWithSizeFromGenerator(size, std::forward<Generator>(generator));
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

// Wraps a variable (though known at construction) number of elements of a single type in a reference counted container, semantically marking them as serializing as "space separated".
template<typename T> struct SpaceSeparatedRefCountedFixedVector {
    using Container = WTF::RefCountedFixedVector<T>;
    using const_iterator = typename Container::const_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    using value_type = typename Container::value_type;

    SpaceSeparatedRefCountedFixedVector(T&& value)
        : value { Container::create(WTFMove(value)) }
    {
    }

    SpaceSeparatedRefCountedFixedVector(std::initializer_list<T> initializerList)
        : value { Container::create(initializerList) }
    {
    }

    SpaceSeparatedRefCountedFixedVector(Ref<Container>&& value)
        : value { WTFMove(value) }
    {
    }

    template<typename SizedRange, typename Mapper>
    static SpaceSeparatedRefCountedFixedVector map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        auto size = range.size();
        return Container::map(size, std::forward<SizedRange>(range), std::forward<Mapper>(mapper));
    }

    template<std::invocable<size_t> Generator>
    static SpaceSeparatedRefCountedFixedVector createWithSizeFromGenerator(size_t size, NOESCAPE Generator&& generator)
    {
        return Container::createWithSizeFromGenerator(size, std::forward<Generator>(generator));
    }

    const_iterator begin() const { return value->begin(); }
    const_iterator end() const { return value->end(); }
    const_reverse_iterator rbegin() const { return value->rbegin(); }
    const_reverse_iterator rend() const { return value->rend(); }

    bool isEmpty() const { return value->isEmpty(); }
    size_t size() const { return value->size(); }
    const T& operator[](size_t i) const { return value.get()[i]; }

    const T& first() const LIFETIME_BOUND { return value->first(); }
    const T& last() const LIFETIME_BOUND { return value->last(); }

    bool operator==(const SpaceSeparatedRefCountedFixedVector& other) const
    {
        return arePointingToEqualData(value, other.value);
    }

    Ref<Container> value;
};

template<typename T> inline constexpr auto TreatAsRangeLike<SpaceSeparatedRefCountedFixedVector<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<SpaceSeparatedRefCountedFixedVector<T>> = SerializationSeparatorType::Space;

// Wraps a variable (though known at construction) number of elements of a single type in a reference counted container, semantically marking them as serializing as "comma separated".
template<typename T> struct CommaSeparatedRefCountedFixedVector {
    using Container = WTF::RefCountedFixedVector<T>;
    using const_iterator = typename Container::const_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    using value_type = typename Container::value_type;

    CommaSeparatedRefCountedFixedVector(T&& value)
        : value { Container::create(WTFMove(value)) }
    {
    }

    CommaSeparatedRefCountedFixedVector(std::initializer_list<T> initializerList)
        : value { Container::create(initializerList) }
    {
    }

    CommaSeparatedRefCountedFixedVector(Ref<Container>&& value)
        : value { WTFMove(value) }
    {
    }

    template<typename SizedRange, typename Mapper>
    static CommaSeparatedRefCountedFixedVector map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        auto size = range.size();
        return Container::map(size, std::forward<SizedRange>(range), std::forward<Mapper>(mapper));
    }

    template<std::invocable<size_t> Generator>
    static CommaSeparatedRefCountedFixedVector createWithSizeFromGenerator(size_t size, NOESCAPE Generator&& generator)
    {
        return Container::createWithSizeFromGenerator(size, std::forward<Generator>(generator));
    }

    const_iterator begin() const { return value->begin(); }
    const_iterator end() const { return value->end(); }
    const_reverse_iterator rbegin() const { return value->rbegin(); }
    const_reverse_iterator rend() const { return value->rend(); }

    bool isEmpty() const { return value->isEmpty(); }
    size_t size() const { return value->size(); }
    const T& operator[](size_t i) const { return value.get()[i]; }

    const T& first() const LIFETIME_BOUND { return value->first(); }
    const T& last() const LIFETIME_BOUND { return value->last(); }

    bool operator==(const CommaSeparatedRefCountedFixedVector& other) const
    {
        return arePointingToEqualData(value, other.value);
    }

    Ref<Container> value;
};

template<typename T> inline constexpr auto TreatAsRangeLike<CommaSeparatedRefCountedFixedVector<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<CommaSeparatedRefCountedFixedVector<T>> = SerializationSeparatorType::Comma;

// Wraps a `markable` type and enforces the invariant that it is either created with a non-empty value or the provided keyword.
template<typename T, typename K, typename Traits = MarkableTraits<T>> struct ValueOrKeyword {
    using Base = ValueOrKeyword<T, K, Traits>;
    using Value = T;
    using Keyword = K;

    constexpr ValueOrKeyword(Keyword)
    {
    }

    constexpr ValueOrKeyword(Value&& value)
        : m_value { WTFMove(value) }
    {
    }

    constexpr bool isKeyword() const { return !m_value; }
    constexpr bool isValue() const { return !!m_value; }
    constexpr std::optional<Value> tryValue() const { return m_value; }

    template<typename U> bool holdsAlternative() const
    {
             if constexpr (std::same_as<U, Keyword>) return isKeyword();
        else if constexpr (std::same_as<U, Value>)   return isValue();
    }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isKeyword())
            return visitor(Keyword { });
        return visitor(*m_value);
    }

    constexpr bool operator==(const ValueOrKeyword&) const = default;

protected:
    constexpr ValueOrKeyword(std::optional<Value>&& value)
        : m_value { WTFMove(value) }
    {
    }

    Markable<Value, Traits> m_value { };
};

template<typename T, typename K, typename Traits> inline constexpr auto TreatAsVariantLike<ValueOrKeyword<T, K, Traits>> = true;

// Concept to constrain types to only those that derive from `ValueOrKeyword`.
template<typename T> concept ValueOrKeywordDerived = WTF::IsBaseOfTemplate<ValueOrKeyword, T>::value;

// Wraps a list and enforces the invariant that it is either created with a non-empty value or `CSS::Keyword::None`.
template<typename T> struct ListOrNone {
    using List = T;
    using const_iterator = typename List::const_iterator;
    using const_reverse_iterator = typename List::const_reverse_iterator;
    using value_type = typename List::value_type;

    ListOrNone(List&& list)
        : m_value { WTFMove(list) }
    {
        RELEASE_ASSERT(!m_value.isEmpty());
    }

    ListOrNone(CSS::Keyword::None)
        : m_value { }
    {
    }

    const_iterator begin() const LIFETIME_BOUND { return m_value.begin(); }
    const_iterator end() const LIFETIME_BOUND { return m_value.end(); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return m_value.rbegin(); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return m_value.rend(); }

    const value_type& first() const LIFETIME_BOUND { return m_value.first(); }
    const value_type& last() const LIFETIME_BOUND { return m_value.last(); }

    size_t size() const { return m_value.size(); }
    const value_type& operator[](size_t i) const LIFETIME_BOUND { return m_value[i]; }

    bool contains(const auto& x) const { return m_value.contains(x); }
    bool containsIf(NOESCAPE const Invocable<bool(const value_type&)> auto& f) const { return m_value.containsIf(f); }

    bool operator==(const ListOrNone&) const = default;

    bool isNone() const { return m_value.isEmpty(); }
    bool isList() const { return !m_value.isEmpty(); }
    const List* tryList() const LIFETIME_BOUND { return isList() ? &m_value : nullptr; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(m_value);
    }

protected:
    // An empty list indicates the value `none`. This invariant is ensured
    // with a release assert in the constructor.
    List m_value;
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

// Helper to define a range-like conformance for a type that derives from `ListOrDefault`.
#define DEFINE_RANGE_LIKE_CONFORMANCE_FOR_LIST_OR_DEFAULT_DERIVED_TYPE(t) \
    DEFINE_RANGE_LIKE_CONFORMANCE(t) \
    template<> inline constexpr auto WebCore::SerializationSeparator<t> = WebCore::SerializationSeparator<typename t::List>;

// Wraps a list and behaves as `optional-like`, using the `list.isEmpty()` as the nullopt state.
template<typename T> struct ListOrNullopt {
    using List = T;
    using const_iterator = typename List::const_iterator;
    using const_reverse_iterator = typename List::const_reverse_iterator;
    using value_type = typename List::value_type;

    ListOrNullopt(List&& list)
        : m_value { WTFMove(list) }
    {
        RELEASE_ASSERT(!m_value.isEmpty());
    }

    ListOrNullopt(std::nullopt_t)
        : m_value { }
    {
    }

    ListOrNullopt()
        : m_value { }
    {
    }

    const_iterator begin() const LIFETIME_BOUND { return m_value.begin(); }
    const_iterator end() const LIFETIME_BOUND { return m_value.end(); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return m_value.rbegin(); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return m_value.rend(); }

    const value_type& first() const LIFETIME_BOUND { return m_value.first(); }
    const value_type& last() const LIFETIME_BOUND { return m_value.last(); }

    size_t size() const { return m_value.size(); }
    const value_type& operator[](size_t i) const LIFETIME_BOUND { return m_value[i]; }

    bool contains(const auto& x) const { return m_value.contains(x); }
    bool containsIf(NOESCAPE const Invocable<bool(const value_type&)> auto& f) const { return m_value.containsIf(f); }

    bool operator==(const ListOrNullopt&) const = default;

    bool isNullopt() const { return m_value.isEmpty(); }
    bool isList() const { return !m_value.isEmpty(); }
    const List* tryList() const LIFETIME_BOUND { return isList() ? &m_value : nullptr; }

    // Returns the underlying list regardless of whether or not it is empty.
    const List& list() const LIFETIME_BOUND { return m_value; }

    explicit operator bool() const { return !m_value.isEmpty(); }

    const List* operator->() const LIFETIME_BOUND { return &m_value; }
    const List& operator*() const LIFETIME_BOUND { return m_value; }

protected:
    List m_value;
};

template<typename List> inline constexpr auto TreatAsOptionalLike<ListOrNullopt<List>> = true;

// Wraps an `EnumSet` and enforces the invariant that it is either created with a non-empty value or specified keyword.
// Required to be subclassed, passing the derived type as the first template parameter.
template<typename Derived, typename T, typename K> struct EnumSetOrKeywordBase {
    using Base = EnumSetOrKeywordBase<Derived, T, K>;
    using EnumSet = T;
    using Keyword = K;
    using value_type = typename EnumSet::value_type;

    constexpr EnumSetOrKeywordBase(EnumSet&& list)
        : m_value { WTFMove(list) }
    {
        RELEASE_ASSERT(!m_value.isEmpty());
    }

    constexpr EnumSetOrKeywordBase(Keyword)
        : m_value { }
    {
    }

    constexpr EnumSetOrKeywordBase(value_type value)
        : EnumSetOrKeywordBase { EnumSet { value } }
    {
    }

    constexpr EnumSetOrKeywordBase(std::initializer_list<value_type> initializerList)
        : EnumSetOrKeywordBase { EnumSet { initializerList } }
    {
    }

    static constexpr Derived fromRaw(typename EnumSet::StorageType rawValue)
    {
        if (!rawValue)
            return Keyword { };
        return EnumSet::fromRaw(rawValue);
    }
    constexpr typename EnumSet::StorageType toRaw() const { return m_value.toRaw(); }

    constexpr bool contains(value_type e) const { return m_value.contains(e); }
    constexpr bool containsAny(EnumSet other) const { return m_value.containsAny(other.value); }
    constexpr bool containsAll(EnumSet other) const { return m_value.containsAll(other.value); }
    constexpr bool containsOnly(EnumSet other) const { return m_value.containsOnly(other.value); }

    constexpr decltype(auto) begin() const LIFETIME_BOUND { return m_value.begin(); }
    constexpr decltype(auto) end() const LIFETIME_BOUND { return m_value.end(); }

    constexpr size_t size() const { return m_value.size(); }

    constexpr bool contains(const auto& x) const { return m_value.contains(x); }

    constexpr bool operator==(const EnumSetOrKeywordBase&) const = default;

    constexpr bool isKeyword() const { return m_value.isEmpty(); }
    constexpr bool isEnumSet() const { return !m_value.isEmpty(); }
    constexpr const EnumSet* tryEnumSet() const { return isEnumSet() ? &m_value : nullptr; }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isKeyword())
            return visitor(Keyword { });
        return visitor(m_value);
    }

protected:
    // An empty enum set indicates the value is `Keyword`. This invariant is ensured
    // with a release assert in the constructor.
    EnumSet m_value;
};

// Concept to constrain types to only those that derive from `EnumSetOrKeywordBase`.
template<typename T> concept EnumSetOrKeywordBaseDerived = WTF::IsBaseOfTemplate<EnumSetOrKeywordBase, T>::value;

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

// Wraps a pair of elements of a single type, semantically marking them as serializing as "space separated" and "minimally serializing".
template<typename T> struct MinimallySerializingSpaceSeparatedPair {
    using Array = SpaceSeparatedPair<T>;
    using value_type = T;

    constexpr MinimallySerializingSpaceSeparatedPair(T p1, T p2)
        : value { WTFMove(p1), WTFMove(p2) }
    {
    }

    constexpr MinimallySerializingSpaceSeparatedPair(SpaceSeparatedPair<T>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const MinimallySerializingSpaceSeparatedPair<T>&) const = default;

    constexpr const T& first() const { return get<0>(value); }
    constexpr const T& second() const { return get<1>(value); }

    SpaceSeparatedPair<T> value;
};

template<size_t I, typename T> decltype(auto) get(const MinimallySerializingSpaceSeparatedPair<T>& size)
{
    return get<I>(size.value);
}

template<typename T> inline constexpr auto TreatAsTupleLike<MinimallySerializingSpaceSeparatedPair<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<MinimallySerializingSpaceSeparatedPair<T>> = SerializationSeparatorType::Space;
template<typename T> inline constexpr auto SerializationCoalescing<MinimallySerializingSpaceSeparatedPair<T>> = SerializationCoalescingType::Minimal;

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

// Wraps a pair of elements of a single type representing a point, semantically marking them as serializing as "space separated" and "minimally serializing".
template<typename T> struct MinimallySerializingSpaceSeparatedPoint {
    using Array = SpaceSeparatedPair<T>;
    using value_type = T;

    constexpr MinimallySerializingSpaceSeparatedPoint(T p1)
        : value { p1, p1 }
    {
    }

    constexpr MinimallySerializingSpaceSeparatedPoint(T p1, T p2)
        : value { WTFMove(p1), WTFMove(p2) }
    {
    }

    constexpr MinimallySerializingSpaceSeparatedPoint(SpaceSeparatedPair<T>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const MinimallySerializingSpaceSeparatedPoint<T>&) const = default;

    const T& x() const { return get<0>(value); }
    const T& y() const { return get<1>(value); }

    SpaceSeparatedPair<T> value;
};

template<size_t I, typename T> decltype(auto) get(const MinimallySerializingSpaceSeparatedPoint<T>& point)
{
    return get<I>(point.value);
}

template<typename T> inline constexpr auto TreatAsTupleLike<MinimallySerializingSpaceSeparatedPoint<T>> = true;
template<typename T> inline constexpr auto SerializationSeparator<MinimallySerializingSpaceSeparatedPoint<T>> = SerializationSeparatorType::Space;
template<typename T> inline constexpr auto SerializationCoalescing<MinimallySerializingSpaceSeparatedPoint<T>> = SerializationCoalescingType::Minimal;

// Wraps a pair of elements of a single type representing a size, semantically marking them as serializing as "space separated" and "minimally serializing".
template<typename T> struct MinimallySerializingSpaceSeparatedSize {
    using Array = SpaceSeparatedPair<T>;
    using value_type = T;

    constexpr MinimallySerializingSpaceSeparatedSize(T p1)
        : value { p1, p1 }
    {
    }

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
template<typename T> inline constexpr auto SerializationCoalescing<MinimallySerializingSpaceSeparatedSize<T>> = SerializationCoalescingType::Minimal;

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
template<typename T> inline constexpr auto SerializationCoalescing<MinimallySerializingSpaceSeparatedRectEdges<T>> = SerializationCoalescingType::Minimal;

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
template<typename T> inline constexpr auto SerializationCoalescing<MinimallySerializingSpaceSeparatedRectCorners<T>> = SerializationCoalescingType::Minimal;

// MARK: - Logging

template<typename T> void logForCSSOnTupleLike(TextStream& ts, const T& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    auto caller = WTF::makeVisitor(
        [&]<OptionalLike U>(const U& element) {
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

template<typename T> TextStream& operator<<(TextStream& ts, const SpaceSeparatedEnumSet<T>& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<SpaceSeparatedEnumSet<T>>);
    return ts;
}

template<typename T> TextStream& operator<<(TextStream& ts, const CommaSeparatedEnumSet<T>& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<CommaSeparatedEnumSet<T>>);
    return ts;
}

template<typename T> TextStream& operator<<(TextStream& ts, const SpaceSeparatedListHashSet<T>& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<SpaceSeparatedListHashSet<T>>);
    return ts;
}

template<typename T> TextStream& operator<<(TextStream& ts, const CommaSeparatedListHashSet<T>& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<CommaSeparatedListHashSet<T>>);
    return ts;
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

template<typename T> TextStream& operator<<(TextStream& ts, const SpaceSeparatedRefCountedFixedVector<T>& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<SpaceSeparatedRefCountedFixedVector<T>>);
    return ts;
}

template<typename T> TextStream& operator<<(TextStream& ts, const CommaSeparatedRefCountedFixedVector<T>& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<CommaSeparatedRefCountedFixedVector<T>>);
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

template<typename T, size_t N> TextStream& operator<<(TextStream& ts, const SpaceSeparatedArray<T, N>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<SpaceSeparatedArray<T, N>>);
    return ts;
}

template<typename T, size_t N> TextStream& operator<<(TextStream& ts, const CommaSeparatedArray<T, N>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<CommaSeparatedArray<T, N>>);
    return ts;
}

template<typename T> TextStream& operator<<(TextStream& ts, const MinimallySerializingSpaceSeparatedPair<T>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<MinimallySerializingSpaceSeparatedPair<T>>);
    return ts;
}

template<typename T> WTF::TextStream& operator<<(WTF::TextStream& ts, const SpaceSeparatedPoint<T>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<SpaceSeparatedPoint<T>>);
    return ts;
}

template<typename T> WTF::TextStream& operator<<(WTF::TextStream& ts, const SpaceSeparatedSize<T>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<SpaceSeparatedSize<T>>);
    return ts;
}


template<typename T> TextStream& operator<<(TextStream& ts, const MinimallySerializingSpaceSeparatedPoint<T>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<MinimallySerializingSpaceSeparatedPoint<T>>);
    return ts;
}

template<typename T> TextStream& operator<<(TextStream& ts, const MinimallySerializingSpaceSeparatedSize<T>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<MinimallySerializingSpaceSeparatedSize<T>>);
    return ts;
}

template<typename T> TextStream& operator<<(TextStream& ts, const MinimallySerializingSpaceSeparatedRectEdges<T>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<MinimallySerializingSpaceSeparatedRectEdges<T>>);
    return ts;
}

template<typename T> TextStream& operator<<(TextStream& ts, const MinimallySerializingSpaceSeparatedRectCorners<T>& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<MinimallySerializingSpaceSeparatedRectCorners<T>>);
    return ts;
}

// Helper for using `Markable<>` with `RangeLike` types where an empty range can represent `Markable::emptyValue`.
template<typename RangeLike>
struct NonEmptyRangeLikeMarkableTraits {
    static bool isEmptyValue(const RangeLike& value) { return value.isEmpty(); }
    static RangeLike emptyValue() { return RangeLike { }; }
};

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

template<typename T> class tuple_size<WebCore::MinimallySerializingSpaceSeparatedPair<T>> : public std::integral_constant<size_t, 2> { };
template<size_t I, typename T> class tuple_element<I, WebCore::MinimallySerializingSpaceSeparatedPair<T>> {
public:
    using type = T;
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

template<typename T> class tuple_size<WebCore::MinimallySerializingSpaceSeparatedPoint<T>> : public std::integral_constant<size_t, 2> { };
template<size_t I, typename T> class tuple_element<I, WebCore::MinimallySerializingSpaceSeparatedPoint<T>> {
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

template<typename T>
struct supports_text_stream_insertion<WebCore::SpaceSeparatedEnumSet<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::CommaSeparatedEnumSet<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::SpaceSeparatedListHashSet<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::CommaSeparatedListHashSet<T>> : supports_text_stream_insertion<T> { };

template<typename T, size_t inlineCapacity>
struct supports_text_stream_insertion<WebCore::SpaceSeparatedVector<T, inlineCapacity>> : supports_text_stream_insertion<T> { };

template<typename T, size_t inlineCapacity>
struct supports_text_stream_insertion<WebCore::CommaSeparatedVector<T, inlineCapacity>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::SpaceSeparatedFixedVector<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::CommaSeparatedFixedVector<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::SpaceSeparatedRefCountedFixedVector<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::CommaSeparatedRefCountedFixedVector<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::MinimallySerializingSpaceSeparatedPair<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::SpaceSeparatedPoint<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::SpaceSeparatedSize<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::MinimallySerializingSpaceSeparatedPoint<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::MinimallySerializingSpaceSeparatedSize<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::MinimallySerializingSpaceSeparatedRectEdges<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WebCore::MinimallySerializingSpaceSeparatedRectCorners<T>> : supports_text_stream_insertion<T> { };

template<>
struct MarkableTraits<WebCore::CustomIdentifier> {
    static bool isEmptyValue(const WebCore::CustomIdentifier& value) { return value.value.isNull(); }
    static WebCore::CustomIdentifier emptyValue() { return WebCore::CustomIdentifier { nullAtom() }; }
};

} // namespace WTF
