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

#include "CalculationOperator.h"
#include <optional>
#include <tuple>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Calculation {

// `Calculation::Tree` is a reduced representation of `CSSCalc::Tree` used in cases where everything else (e.g all non-canonical dimensions) has been resolved except percentages, which can't be resolved until they are used as they need some value to resolve against. Currently, these are only used by the `Length` type to represent <length-percentage> values, but are implemented generically so can be used for any <*-percentage> type if the need ever arises.

enum class Category : uint8_t;

// Container.
struct Tree;

// Math Operators.
struct Sum;
struct Product;
struct Negate;
struct Invert;

// Math Functions.
struct Min;
struct Max;
struct Clamp;
struct RoundNearest;
struct RoundUp;
struct RoundDown;
struct RoundToZero;
struct Mod;
struct Rem;
struct Sin;
struct Cos;
struct Tan;
struct Exp;
struct Log;
struct Asin;
struct Acos;
struct Atan;
struct Atan2;
struct Pow;
struct Sqrt;
struct Hypot;
struct Abs;
struct Sign;
struct Progress;
struct Random;

// Non-standard
struct Blend;

template<typename Op>
concept Leaf = requires(Op) {
    Op::isLeaf == true;
};

template<typename Op>
concept Numeric = requires(Op) {
    Op::isNumeric == true;
};

// Leaf Values
struct Number {
    static constexpr bool isLeaf = true;
    static constexpr bool isNumeric = true;

    double value;

    bool operator==(const Number&) const = default;
};

struct Percentage {
    static constexpr bool isLeaf = true;
    static constexpr bool isNumeric = true;

    double value;

    bool operator==(const Percentage&) const = default;
};

struct Dimension {
    static constexpr bool isLeaf = true;
    static constexpr bool isNumeric = true;

    double value;

    bool operator==(const Dimension&) const = default;
};

struct None {
    constexpr bool operator==(const None&) const = default;
};

template<typename Op> struct IndirectNode {
    UniqueRef<Op> op;

    // Forward * and -> to the operation for convenience.
    const Op& operator*() const { return op.get(); }
    Op& operator*() { return op.get(); }
    const Op* operator->() const { return op.ptr(); }
    Op* operator->() { return op.ptr(); }
    operator const Op&() const { return op.get(); }
    operator Op&() { return op.get(); }

    bool operator==(const IndirectNode<Op>& other) const { return op.get() == other.op.get(); }
};

using Node = Variant<
    Number,
    Percentage,
    Dimension,
    IndirectNode<Sum>,
    IndirectNode<Product>,
    IndirectNode<Negate>,
    IndirectNode<Invert>,
    IndirectNode<Min>,
    IndirectNode<Max>,
    IndirectNode<Clamp>,
    IndirectNode<RoundNearest>,
    IndirectNode<RoundUp>,
    IndirectNode<RoundDown>,
    IndirectNode<RoundToZero>,
    IndirectNode<Mod>,
    IndirectNode<Rem>,
    IndirectNode<Sin>,
    IndirectNode<Cos>,
    IndirectNode<Tan>,
    IndirectNode<Asin>,
    IndirectNode<Acos>,
    IndirectNode<Atan>,
    IndirectNode<Atan2>,
    IndirectNode<Pow>,
    IndirectNode<Sqrt>,
    IndirectNode<Hypot>,
    IndirectNode<Log>,
    IndirectNode<Exp>,
    IndirectNode<Abs>,
    IndirectNode<Sign>,
    IndirectNode<Progress>,
    IndirectNode<Random>,
    IndirectNode<Blend>
>;

struct Child {
    Node value;

    template<typename T>
        requires std::constructible_from<Node, T>
    Child(T&&);

    FORWARD_VARIANT_FUNCTIONS(Child, value)

    bool operator==(const Child&) const = default;
};

struct ChildOrNone {
    Variant<Child, None> value;

    ChildOrNone(Child&&);
    ChildOrNone(None);

    FORWARD_VARIANT_FUNCTIONS(ChildOrNone, value)

    bool operator==(const ChildOrNone&) const = default;
};

struct Children {
    using iterator = typename Vector<Child>::iterator;
    using reverse_iterator = typename Vector<Child>::reverse_iterator;
    using const_iterator = typename Vector<Child>::const_iterator;
    using const_reverse_iterator = typename Vector<Child>::const_reverse_iterator;
    using value_type = typename Vector<Child>::value_type;

    Vector<Child> value;

    Children(Children&&);
    Children(Vector<Child>&&);
    Children& operator=(Children&&);
    Children& operator=(Vector<Child>&&);

    iterator begin();
    iterator end();
    reverse_iterator rbegin();
    reverse_iterator rend();

    const_iterator begin() const;
    const_iterator end() const;
    const_reverse_iterator rbegin() const;
    const_reverse_iterator rend() const;

    bool isEmpty() const;
    size_t size() const;

    Child& operator[](size_t i);
    const Child& operator[](size_t i) const;

    bool operator==(const Children&) const = default;
};

struct Tree {
    Child root;

    bool operator==(const Tree&) const = default;
};

// Math Operators.

struct Sum {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Sum);
    static constexpr auto op = Operator::Sum;

    Children children;

    bool operator==(const Sum&) const = default;
};

struct Product {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Product);
    static constexpr auto op = Operator::Product;

    Children children;

    bool operator==(const Product&) const = default;
};

struct Negate {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Negate);
    static constexpr auto op = Operator::Negate;

    Child a;

    bool operator==(const Negate&) const = default;
};

struct Invert {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Invert);
    static constexpr auto op = Operator::Invert;

    Child a;

    bool operator==(const Invert&) const = default;
};

// Math Functions

// Comparison Functions - https://drafts.csswg.org/css-values-4/#comp-func
struct Min {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Min);
    static constexpr auto op = Operator::Min;

    Children children;

    bool operator==(const Min&) const = default;
};

struct Max {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Max);
    static constexpr auto op = Operator::Max;

    Children children;

    bool operator==(const Max&) const = default;
};

struct Clamp {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Clamp);
    static constexpr auto op = Operator::Clamp;

    ChildOrNone min;
    Child val;
    ChildOrNone max;

    bool operator==(const Clamp&) const = default;
};

// Stepped Value Functions - https://drafts.csswg.org/css-values-4/#round-func
struct RoundNearest {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(RoundNearest);
    static constexpr auto op = Operator::Nearest;

    Child a;
    std::optional<Child> b;

    bool operator==(const RoundNearest&) const = default;
};

struct RoundUp {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(RoundUp);
    static constexpr auto op = Operator::Up;

    Child a;
    std::optional<Child> b;

    bool operator==(const RoundUp&) const = default;
};

struct RoundDown {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(RoundDown);
    static constexpr auto op = Operator::Down;

    Child a;
    std::optional<Child> b;

    bool operator==(const RoundDown&) const = default;
};

struct RoundToZero {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(RoundToZero);
    static constexpr auto op = Operator::ToZero;

    Child a;
    std::optional<Child> b;

    bool operator==(const RoundToZero&) const = default;
};

struct Mod {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Mod);
    static constexpr auto op = Operator::Mod;

    Child a;
    Child b;

    bool operator==(const Mod&) const = default;
};

struct Rem {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Rem);
    static constexpr auto op = Operator::Rem;

    Child a;
    Child b;

    bool operator==(const Rem&) const = default;
};

// Trigonometric Functions - https://drafts.csswg.org/css-values-4/#trig-funcs
struct Sin {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Sin);
    static constexpr auto op = Operator::Sin;

    Child a;

    bool operator==(const Sin&) const = default;
};

struct Cos {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Cos);
    static constexpr auto op = Operator::Cos;

    Child a;

    bool operator==(const Cos&) const = default;
};

struct Tan {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Tan);
    static constexpr auto op = Operator::Tan;

    Child a;

    bool operator==(const Tan&) const = default;
};

struct Asin {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Asin);
    static constexpr auto op = Operator::Asin;

    Child a;

    bool operator==(const Asin&) const = default;
};

struct Acos {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Acos);
    static constexpr auto op = Operator::Acos;

    Child a;

    bool operator==(const Acos&) const = default;
};

struct Atan {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Atan);
    static constexpr auto op = Operator::Atan;

    Child a;

    bool operator==(const Atan&) const = default;
};

struct Atan2 {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Atan2);
    static constexpr auto op = Operator::Atan2;

    Child a;
    Child b;

    bool operator==(const Atan2&) const = default;
};

// Exponential Functions - https://drafts.csswg.org/css-values-4/#exponent-funcs
struct Pow {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Atan2);
    static constexpr auto op = Operator::Pow;

    Child a;
    Child b;

    bool operator==(const Pow&) const = default;
};

struct Sqrt {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Sqrt);
    static constexpr auto op = Operator::Sqrt;

    Child a;

    bool operator==(const Sqrt&) const = default;
};

struct Hypot {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Hypot);
    static constexpr auto op = Operator::Hypot;

    Children children;

    bool operator==(const Hypot&) const = default;
};

struct Log {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Log);
    static constexpr auto op = Operator::Log;

    Child a;
    std::optional<Child> b;

    bool operator==(const Log&) const = default;
};

struct Exp {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Exp);
    static constexpr auto op = Operator::Exp;

    Child a;

    bool operator==(const Exp&) const = default;
};

// Sign-Related Functions - https://drafts.csswg.org/css-values-4/#sign-funcs
struct Abs {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Abs);
    static constexpr auto op = Operator::Abs;

    Child a;

    bool operator==(const Abs&) const = default;
};

struct Sign {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Sign);
    static constexpr auto op = Operator::Sign;

    Child a;

    bool operator==(const Sign&) const = default;
};

// Progress-Related Functions - https://drafts.csswg.org/css-values-5/#progress
struct Progress {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Progress);
    static constexpr auto op = Operator::Progress;

    Child progress;
    Child from;
    Child to;

    bool operator==(const Progress&) const = default;
};

// Random Function - https://drafts.csswg.org/css-values-5/#random
struct Random {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Random);
    static constexpr auto op = Operator::Random;

    struct Fixed {
        double baseValue;

        bool operator==(const Fixed&) const = default;
    };

    Fixed fixed;
    Child min;
    Child max;
    std::optional<Child> step;

    bool operator==(const Random&) const = default;
};

// Non-standard
struct Blend {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Blend);
    static constexpr auto op = Operator::Blend;

    double progress;
    Child from;
    Child to;

    bool operator==(const Blend&) const = default;
};

static_assert(sizeof(Child) <= 16, "Child should stay small");

// MARK: Construction

// Default implementation of ChildConstruction used for all indirect nodes.
template<typename Op> struct ChildConstruction {
    static Child make(Op&& op) { return Child { IndirectNode<Op> { makeUniqueRef<Op>(WTFMove(op)) } }; }
};

// Specialized implementation of ChildConstruction for Number, needed to avoid `makeUniqueRef`.
template<> struct ChildConstruction<Number> {
    static Child make(Number&& op) { return Child { WTFMove(op) }; }
};

// Specialized implementation of ChildConstruction for Percentage, needed to avoid `makeUniqueRef`.
template<> struct ChildConstruction<Percentage> {
    static Child make(Percentage&& op) { return Child { WTFMove(op) }; }
};

// Specialized implementation of ChildConstruction for Dimension, needed to avoid `makeUniqueRef`.
template<> struct ChildConstruction<Dimension> {
    static Child make(Dimension&& op) { return Child { WTFMove(op) }; }
};

template<typename Op> Child makeChild(Op&& op)
{
    return ChildConstruction<Op>::make(WTFMove(op));
}

// Convenience constructors

inline Child number(double value)
{
    return makeChild(Number { .value = value });
}

inline Child percentage(double value)
{
    return makeChild(Percentage { .value = value });
}

inline Child dimension(double value)
{
    return makeChild(Dimension { .value = value });
}

inline Child add(Child&& a, Child&& b)
{
    Vector<Child> sumChildren;
    sumChildren.append(WTFMove(a));
    sumChildren.append(WTFMove(b));
    return makeChild(Sum { .children = WTFMove(sumChildren) });
}

inline Child multiply(Child&& a, Child&& b)
{
    Vector<Child> productChildren;
    productChildren.append(WTFMove(a));
    productChildren.append(WTFMove(b));
    return makeChild(Product { .children = WTFMove(productChildren) });
}

inline Child subtract(Child&& a, Child&& b)
{
    return add(WTFMove(a), makeChild(Negate { .a = WTFMove(b) }));
}

inline Child blend(Child&& from, Child&& to, double progress)
{
    return makeChild(Blend { .progress = progress, .from = WTFMove(from), .to = WTFMove(to) });
}

// MARK: Dumping

TextStream& operator<<(TextStream&, const Tree&);

// MARK: Tuple Conformance

// get<> overload (along with std::tuple_size and std::tuple_element below) to support destructuring of operation nodes.

template<size_t I> const auto& get(const Sum& root)
{
    static_assert(!I);
    return root.children;
}

template<size_t I> const auto& get(const Product& root)
{
    static_assert(!I);
    return root.children;
}

template<size_t I> const auto& get(const Negate& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Invert& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Min& root)
{
    static_assert(!I);
    return root.children;
}

template<size_t I> const auto& get(const Max& root)
{
    static_assert(!I);
    return root.children;
}

template<size_t I> const auto& get(const Clamp& root)
{
    if constexpr (!I)
        return root.min;
    else if constexpr (I == 1)
        return root.val;
    else if constexpr (I == 2)
        return root.max;
}

template<size_t I> const auto& get(const RoundNearest& root)
{
    if constexpr (!I)
        return root.a;
    else if constexpr (I == 1)
        return root.b;
}

template<size_t I> const auto& get(const RoundUp& root)
{
    if constexpr (!I)
        return root.a;
    else if constexpr (I == 1)
        return root.b;
}

template<size_t I> const auto& get(const RoundDown& root)
{
    if constexpr (!I)
        return root.a;
    else if constexpr (I == 1)
        return root.b;
}

template<size_t I> const auto& get(const RoundToZero& root)
{
    if constexpr (!I)
        return root.a;
    else if constexpr (I == 1)
        return root.b;
}

template<size_t I> const auto& get(const Mod& root)
{
    if constexpr (!I)
        return root.a;
    else if constexpr (I == 1)
        return root.b;
}

template<size_t I> const auto& get(const Rem& root)
{
    if constexpr (!I)
        return root.a;
    else if constexpr (I == 1)
        return root.b;
}

template<size_t I> const auto& get(const Sin& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Cos& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Tan& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Asin& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Acos& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Atan& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Atan2& root)
{
    if constexpr (!I)
        return root.a;
    else if constexpr (I == 1)
        return root.b;
}

template<size_t I> const auto& get(const Pow& root)
{
    if constexpr (!I)
        return root.a;
    else if constexpr (I == 1)
        return root.b;
}

template<size_t I> const auto& get(const Sqrt& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Hypot& root)
{
    static_assert(!I);
    return root.children;
}

template<size_t I> const auto& get(const Log& root)
{
    if constexpr (!I)
        return root.a;
    else if constexpr (I == 1)
        return root.b;
}

template<size_t I> const auto& get(const Exp& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Abs& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Sign& root)
{
    static_assert(!I);
    return root.a;
}

template<size_t I> const auto& get(const Progress& root)
{
    if constexpr (!I)
        return root.progress;
    else if constexpr (I == 1)
        return root.from;
    else if constexpr (I == 2)
        return root.to;
}

template<size_t I> const auto& get(const Random& root)
{
    if constexpr (!I)
        return root.fixed;
    else if constexpr (I == 1)
        return root.min;
    else if constexpr (I == 2)
        return root.max;
    else if constexpr (I == 3)
        return root.step;
}

template<size_t I> const auto& get(const Blend& root)
{
    if constexpr (!I)
        return root.progress;
    else if constexpr (I == 1)
        return root.from;
    else if constexpr (I == 2)
        return root.to;
}

// MARK: Child Definition

template<typename T>
    requires std::constructible_from<Node, T>
Child::Child(T&& value)
    : value(std::forward<T>(value))
{
}

// MARK: ChildOrNone Definition

inline ChildOrNone::ChildOrNone(Child&& child)
    : value(WTFMove(child))
{
}

inline ChildOrNone::ChildOrNone(None none)
    : value(none)
{
}

// MARK: Children Definition

inline Children::Children(Children&& other)
    : value(WTFMove(other.value))
{
}

inline Children::Children(Vector<Child>&& other)
    : value(WTFMove(other))
{
}

inline Children& Children::operator=(Children&& other)
{
    value = WTFMove(other.value);
    return *this;
}

inline Children& Children::operator=(Vector<Child>&& other)
{
    value = WTFMove(other);
    return *this;
}

inline Children::iterator Children::begin()
{
    return value.begin();
}

inline Children::iterator Children::end()
{
    return value.end();
}

inline Children::reverse_iterator Children::rbegin()
{
    return value.rbegin();
}

inline Children::reverse_iterator Children::rend()
{
    return value.rend();
}

inline Children::const_iterator Children::begin() const
{
    return value.begin();
}

inline Children::const_iterator Children::end() const
{
    return value.end();
}

inline Children::const_reverse_iterator Children::rbegin() const
{
    return value.rbegin();
}

inline Children::const_reverse_iterator Children::rend() const
{
    return value.rend();
}

inline bool Children::isEmpty() const
{
    return value.isEmpty();
}

inline size_t Children::size() const
{
    return value.size();
}

inline Child& Children::operator[](size_t i)
{
    return value[i];
}

inline const Child& Children::operator[](size_t i) const
{
    return value[i];
}

} // namespace Calculation
} // namespace WebCore

namespace std {

#define OP_TUPLE_LIKE_CONFORMANCE(op, numberOfArguments) \
    template<> class tuple_size<WebCore::Calculation::op> : public std::integral_constant<size_t, numberOfArguments> { }; \
    template<size_t I> class tuple_element<I, WebCore::Calculation::op> { \
    public: \
        using type = decltype(WebCore::Calculation::get<I>(std::declval<WebCore::Calculation::op>())); \
    } \
\

OP_TUPLE_LIKE_CONFORMANCE(Sum, 1);
OP_TUPLE_LIKE_CONFORMANCE(Product, 1);
OP_TUPLE_LIKE_CONFORMANCE(Negate, 1);
OP_TUPLE_LIKE_CONFORMANCE(Invert, 1);
OP_TUPLE_LIKE_CONFORMANCE(Min, 1);
OP_TUPLE_LIKE_CONFORMANCE(Max, 1);
OP_TUPLE_LIKE_CONFORMANCE(Clamp, 3);
OP_TUPLE_LIKE_CONFORMANCE(RoundNearest, 2);
OP_TUPLE_LIKE_CONFORMANCE(RoundUp, 2);
OP_TUPLE_LIKE_CONFORMANCE(RoundDown, 2);
OP_TUPLE_LIKE_CONFORMANCE(RoundToZero, 2);
OP_TUPLE_LIKE_CONFORMANCE(Mod, 2);
OP_TUPLE_LIKE_CONFORMANCE(Rem, 2);
OP_TUPLE_LIKE_CONFORMANCE(Sin, 1);
OP_TUPLE_LIKE_CONFORMANCE(Cos, 1);
OP_TUPLE_LIKE_CONFORMANCE(Tan, 1);
OP_TUPLE_LIKE_CONFORMANCE(Asin, 1);
OP_TUPLE_LIKE_CONFORMANCE(Acos, 1);
OP_TUPLE_LIKE_CONFORMANCE(Atan, 1);
OP_TUPLE_LIKE_CONFORMANCE(Atan2, 2);
OP_TUPLE_LIKE_CONFORMANCE(Pow, 2);
OP_TUPLE_LIKE_CONFORMANCE(Sqrt, 1);
OP_TUPLE_LIKE_CONFORMANCE(Hypot, 1);
OP_TUPLE_LIKE_CONFORMANCE(Log, 2);
OP_TUPLE_LIKE_CONFORMANCE(Exp, 1);
OP_TUPLE_LIKE_CONFORMANCE(Abs, 1);
OP_TUPLE_LIKE_CONFORMANCE(Sign, 1);
OP_TUPLE_LIKE_CONFORMANCE(Progress, 3);
OP_TUPLE_LIKE_CONFORMANCE(Random, 4);
OP_TUPLE_LIKE_CONFORMANCE(Blend, 3);

#undef OP_TUPLE_LIKE_CONFORMANCE

} // namespace std
