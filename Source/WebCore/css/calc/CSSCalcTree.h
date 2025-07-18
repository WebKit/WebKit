/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSCalcType.h"
#include "CSSPrimitiveNumeric.h"
#include "CSSPrimitiveNumericRange.h"
#include "CSSUnits.h"
#include "CSSValueKeywords.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

namespace Style {
enum class AnchorSizeDimension : uint8_t;
}

enum class CSSUnitType : uint8_t;

namespace CSSCalc {

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
struct Asin;
struct Acos;
struct Atan;
struct Atan2;
struct Pow;
struct Sqrt;
struct Hypot;
struct Log;
struct Exp;
struct Abs;
struct Sign;
struct Random;
struct Progress;

// CSS Anchor Positioning functions.
struct Anchor;
struct AnchorSize;

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
    Type::PercentHintValue hint;

    bool operator==(const Percentage&) const = default;
};

struct CanonicalDimension {
    static constexpr bool isLeaf = true;
    static constexpr bool isNumeric = true;

    enum class Dimension: uint8_t {
        Length,
        Angle,
        Time,
        Frequency,
        Resolution,
        Flex
    };

    double value;
    Dimension dimension;

    bool operator==(const CanonicalDimension&) const = default;
};

struct NonCanonicalDimension {
    static constexpr bool isLeaf = true;
    static constexpr bool isNumeric = true;

    double value;
    CSSUnitType unit;

    bool operator==(const NonCanonicalDimension&) const = default;
};

struct Symbol {
    static constexpr bool isLeaf = true;

    CSSValueID id;
    CSSUnitType unit;

    bool operator==(const Symbol&) const = default;
};

// Tree Counting Functions - https://drafts.csswg.org/css-values-5/#tree-counting
// NOTE: As these functions don't have any parameters, they are leaf nodes.

struct SiblingCount {
    static constexpr bool isLeaf = true;
    static constexpr auto id = CSSValueSiblingCount;

    // <sibling-count()> = sibling-count()
    //     - INPUT: none
    //     - OUTPUT: integer

    bool operator==(const SiblingCount&) const = default;
};

struct SiblingIndex {
    static constexpr bool isLeaf = true;
    static constexpr auto id = CSSValueSiblingIndex;

    // <sibling-index()> = sibling-index()
    //     - INPUT: none
    //     - OUTPUT: integer

    bool operator==(const SiblingIndex&) const = default;
};

template<typename Op> struct IndirectNode {
    Type type;
    UniqueRef<Op> op;

    // Forward * and -> to the operation for convenience.
    const Op& operator*() const { return op.get(); }
    Op& operator*() { return op.get(); }
    const Op* operator->() const { return op.ptr(); }
    Op* operator->() { return op.ptr(); }
    operator const Op&() const { return op.get(); }
    operator Op&() { return op.get(); }

    bool operator==(const IndirectNode<Op>& other) const { return type == other.type && arePointingToEqualData(op, other.op); }
};

using Node = Variant<
    Number,
    Percentage,
    CanonicalDimension,
    NonCanonicalDimension,
    Symbol,
    SiblingCount,
    SiblingIndex,
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
    IndirectNode<Random>,
    IndirectNode<Progress>,
    IndirectNode<Anchor>,
    IndirectNode<AnchorSize>
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
    Variant<Child, CSS::Keyword::None> value;

    ChildOrNone(Child&&);
    ChildOrNone(CSS::Keyword::None);

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

enum class Stage : bool { Specified, Computed };

struct Tree {
    Child root;
    Type type;
    Stage stage;

    // `requiresConversionData` is used both to both indicate whether eager evaluation of the tree (at parse time) is possible or not and to trigger a warning in `CSSCalcValue::doubleValueDeprecated` that the evaluation results will be incorrect.
    bool requiresConversionData = false;

    bool operator==(const Tree&) const = default;
};

struct Sum {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Sum);

    Children children;

    bool operator==(const Sum&) const = default;
};

struct Product {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Product);

    Children children;

    bool operator==(const Product&) const = default;
};

struct Negate {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Negate);

    Child a;

    bool operator==(const Negate&) const = default;
};

struct Invert {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Invert);

    Child a;

    bool operator==(const Invert&) const = default;
};

// Math Functions

// Comparison Functions - https://drafts.csswg.org/css-values-4/#comp-func
struct Min {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Min);
    static constexpr auto id = CSSValueMin;

    // <min()>   = min( <calc-sum># )
    //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
    //     - OUTPUT: consistent type
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    Children children;

    bool operator==(const Min&) const = default;
};

struct Max {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Max);
    static constexpr auto id = CSSValueMax;

    // <max()>   = max( <calc-sum># )
    //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
    //     - OUTPUT: consistent type
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    Children children;

    bool operator==(const Max&) const = default;
};

struct Clamp {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Clamp);
    static constexpr auto id = CSSValueClamp;

    // <clamp()> = clamp( [ <calc-sum> | none ], <calc-sum>, [ <calc-sum> | none ] )
    //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
    //     - OUTPUT: consistent type
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    ChildOrNone min;
    Child val;
    ChildOrNone max;

    bool operator==(const Clamp&) const = default;
};

// Stepped Value Functions - https://drafts.csswg.org/css-values-4/#round-func
struct RoundNearest {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(RoundNearest);
    static constexpr auto id = CSSValueNearest;

    // <round()> = round( <rounding-strategy>?, <calc-sum>, <calc-sum> )
    //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
    //     - OUTPUT: consistent type
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    //    -- and --

    // <round()> = round( <rounding-strategy>?, <calc-sum> )
    //     - INPUT: "consistent" <number>
    //     - OUTPUT: consistent type

    // NOTE: This is special cased in the code.

    Child a;
    std::optional<Child> b;

    bool operator==(const RoundNearest&) const = default;
};

struct RoundUp {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(RoundUp);
    static constexpr auto id = CSSValueUp;

    // <round()> = round( <rounding-strategy>?, <calc-sum>, <calc-sum> )
    //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
    //     - OUTPUT: consistent type
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    //    -- and --

    // <round()> = round( <rounding-strategy>?, <calc-sum> )
    //     - INPUT: "consistent" <number>
    //     - OUTPUT: consistent type

    // NOTE: This is special cased in the code.

    Child a;
    std::optional<Child> b;

    bool operator==(const RoundUp&) const = default;
};

struct RoundDown {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(RoundDown);
    static constexpr auto id = CSSValueDown;

    // <round()> = round( <rounding-strategy>?, <calc-sum>, <calc-sum> )
    //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
    //     - OUTPUT: consistent type
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    //    -- and --

    // <round()> = round( <rounding-strategy>?, <calc-sum> )
    //     - INPUT: "consistent" <number>
    //     - OUTPUT: consistent type

    // NOTE: This is special cased in the code.

    Child a;
    std::optional<Child> b;

    bool operator==(const RoundDown&) const = default;
};

struct RoundToZero {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(RoundToZero);
    static constexpr auto id = CSSValueToZero;

    // <round()> = round( <rounding-strategy>?, <calc-sum>, <calc-sum> )
    //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
    //     - OUTPUT: consistent type
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    //    -- and --

    // <round()> = round( <rounding-strategy>?, <calc-sum> )
    //     - INPUT: "consistent" <number>
    //     - OUTPUT: consistent type

    // NOTE: This is special cased in the code.

    Child a;
    std::optional<Child> b;

    bool operator==(const RoundToZero&) const = default;
};

struct Mod {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Mod);
    static constexpr auto id = CSSValueMod;

    // <mod()>   = mod( <calc-sum>, <calc-sum> )
    //     - INPUT: "same" <number>, <dimension>, or <percentage>
    //     - OUTPUT: same type
    static constexpr auto input = AllowedTypes::Any;
    // FIXME: Spec says this should enforce same type, but tests indicate it should be consistent type.
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    Child a;
    Child b;

    bool operator==(const Mod&) const = default;
};

struct Rem {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Rem);
    static constexpr auto id = CSSValueRem;

    // <rem()>   = rem( <calc-sum>, <calc-sum> )
    //     - INPUT: "same" <number>, <dimension>, or <percentage>
    //     - OUTPUT: same type
    static constexpr auto input = AllowedTypes::Any;
    // FIXME: Spec says this should enforce same type, but tests indicate it should be consistent type.
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    Child a;
    Child b;

    bool operator==(const Rem&) const = default;
};

// Trigonometric Functions - https://drafts.csswg.org/css-values-4/#trig-funcs
struct Sin {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Sin);
    static constexpr auto id = CSSValueSin;

    // <sin()>   = sin( <calc-sum> )
    //     - INPUT: <number> or <angle>
    //     - OUTPUT: <number> "made consistent"
    static constexpr auto input = AllowedTypes::NumberOrAngle;
    static constexpr auto output = OutputTransform::NumberMadeConsistent;

    Child a;

    bool operator==(const Sin&) const = default;
};

struct Cos {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Cos);
    static constexpr auto id = CSSValueCos;

    // <cos()>   = cos( <calc-sum> )
    //     - INPUT: <number> or <angle>
    //     - OUTPUT: <number> "made consistent"
    static constexpr auto input = AllowedTypes::NumberOrAngle;
    static constexpr auto output = OutputTransform::NumberMadeConsistent;

    Child a;

    bool operator==(const Cos&) const = default;
};

struct Tan {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Tan);
    static constexpr auto id = CSSValueTan;

    // <tan()>   = tan( <calc-sum> )
    //     - INPUT: <number> or <angle>
    //     - OUTPUT: <number> "made consistent"
    static constexpr auto input = AllowedTypes::NumberOrAngle;
    static constexpr auto output = OutputTransform::NumberMadeConsistent;

    Child a;

    bool operator==(const Tan&) const = default;
};

struct Asin {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Asin);
    static constexpr auto id = CSSValueAsin;

    // <asin()>  = asin( <calc-sum> )
    //     - INPUT: <number>
    //     - OUTPUT: <angle> "made consistent"
    static constexpr auto input = AllowedTypes::Number;
    static constexpr auto output = OutputTransform::AngleMadeConsistent;

    Child a;

    bool operator==(const Asin&) const = default;
};

struct Acos {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Acos);
    static constexpr auto id = CSSValueAcos;

    // <acos()>  = acos( <calc-sum> )
    //     - INPUT: <number>
    //     - OUTPUT: <angle> "made consistent"
    static constexpr auto input = AllowedTypes::Number;
    static constexpr auto output = OutputTransform::AngleMadeConsistent;

    Child a;

    bool operator==(const Acos&) const = default;
};

struct Atan {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Atan);
    static constexpr auto id = CSSValueAtan;

    // <atan()>  = atan( <calc-sum> )
    //     - INPUT: <number>
    //     - OUTPUT: <angle> "made consistent"
    static constexpr auto input = AllowedTypes::Number;
    static constexpr auto output = OutputTransform::AngleMadeConsistent;

    Child a;

    bool operator==(const Atan&) const = default;
};

struct Atan2 {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Atan2);
    static constexpr auto id = CSSValueAtan2;

    // <atan2()> = atan2( <calc-sum>, <calc-sum> )
    //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
    //     - OUTPUT: <angle> "made consistent"
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::AngleMadeConsistent;

    Child a;
    Child b;

    bool operator==(const Atan2&) const = default;
};

// Exponential Functions - https://drafts.csswg.org/css-values-4/#exponent-funcs
struct Pow {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Pow);
    static constexpr auto id = CSSValuePow;

    // <pow()>   = pow( <calc-sum>, <calc-sum> )
    //     - INPUT: "consistent" <number>
    //     - OUTPUT: consistent type
    static constexpr auto input = AllowedTypes::Number;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    Child a;
    Child b;

    bool operator==(const Pow&) const = default;
};

struct Sqrt {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Sqrt);
    static constexpr auto id = CSSValueSqrt;

    // <sqrt()>  = sqrt( <calc-sum> )
    //     - INPUT: <number>
    //     - OUTPUT: <number> "made consistent"
    static constexpr auto input = AllowedTypes::Number;
    static constexpr auto output = OutputTransform::NumberMadeConsistent;

    Child a;

    bool operator==(const Sqrt&) const = default;
};

struct Hypot {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Hypot);
    static constexpr auto id = CSSValueHypot;

    // <hypot()> = hypot( <calc-sum># )
    //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
    //     - OUTPUT: consistent type
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::None;

    Children children;

    bool operator==(const Hypot&) const = default;
};

struct Log {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Log);
    static constexpr auto id = CSSValueLog;

    // <log()>   = log( <calc-sum>, <calc-sum>? )
    //     - INPUT: <number>
    //     - OUTPUT: <number> "made consistent"
    static constexpr auto input = AllowedTypes::Number;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::NumberMadeConsistent;

    Child a;
    std::optional<Child> b;

    bool operator==(const Log&) const = default;
};

struct Exp {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Exp);
    static constexpr auto id = CSSValueExp;

    // <exp()>   = exp( <calc-sum> )
    //     - INPUT: <number>
    //     - OUTPUT: <number> "made consistent"
    static constexpr auto input = AllowedTypes::Number;
    static constexpr auto output = OutputTransform::NumberMadeConsistent;

    Child a;

    bool operator==(const Exp&) const = default;
};

// Sign-Related Functions - https://drafts.csswg.org/css-values-4/#sign-funcs
struct Abs {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Abs);
    static constexpr auto id = CSSValueAbs;

    // <abs()>   = abs( <calc-sum> )
    //     - INPUT: any
    //     - OUTPUT: input type
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto output = OutputTransform::None;

    Child a;

    bool operator==(const Abs&) const = default;
};

struct Sign {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Sign);
    static constexpr auto id = CSSValueSign;

    // <sign()>  = sign( <calc-sum> )
    //     - INPUT: any
    //     - OUTPUT: <number> "made consistent"
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto output = OutputTransform::NumberMadeConsistent;

    Child a;

    bool operator==(const Sign&) const = default;
};

// Random Function - https://drafts.csswg.org/css-values-5/#random
struct Random {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Random);
    static constexpr auto id = CSSValueRandom;

    // <random-value-sharing> = [ [ auto | <dashed-ident> ] || element-shared ] | fixed <number [0,1]>
    struct SharingOptions {
        struct Auto {
            CSSPropertyID property;
            unsigned index;

            bool operator==(const Auto&) const = default;
        };
        Variant<Auto, AtomString> identifier;
        std::optional<CSS::Keyword::ElementShared> elementShared;

        bool operator==(const SharingOptions&) const = default;
    };
    struct SharingFixed {
        CSS::Number<CSS::ClosedUnitRange> value;

        bool operator==(const SharingFixed&) const = default;
    };
    using Sharing = Variant<SharingOptions, SharingFixed>;

    // <random()> = random( <random-value-sharing>? , <calc-sum>, <calc-sum>, <calc-sum>? )
    //     - INPUT: "same" <number>, <dimension>, or <percentage>
    //     - OUTPUT: same type
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Same;
    static constexpr auto output = OutputTransform::None;

    Sharing sharing;
    Child min;
    Child max;
    std::optional<Child> step;

    bool operator==(const Random&) const = default;
};

// Progress-Related Functions - https://drafts.csswg.org/css-values-5/#progress
struct Progress {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Progress);
    static constexpr auto id = CSSValueProgress;

    // <progress()> = progress( <calc-sum>, <calc-sum>, <calc-sum> )
    //     - INPUT: "consistent" <number>, <dimension>, or <percentage>
    //     - OUTPUT: <number> "made consistent"
    static constexpr auto input = AllowedTypes::Any;
    static constexpr auto merge = MergePolicy::Consistent;
    static constexpr auto output = OutputTransform::NumberMadeConsistent;

    Child value;
    Child start;
    Child end;

    bool operator==(const Progress&) const = default;
};

// Anchor Positioning Related Functions - https://drafts.csswg.org/css-anchor-position-1/

struct AnchorSide {
    // <anchor-side> = inside | outside | top | left | right | bottom | start | end | self-start | self-end | <percentage> | center
    Variant<CSSValueID, Child> value;

    AnchorSide(CSSValueID);
    AnchorSide(Child&&);

    FORWARD_VARIANT_FUNCTIONS(AnchorSide, value)

    bool operator==(const AnchorSide&) const = default;
};

struct Anchor {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(Anchor);
    static constexpr auto id = CSSValueAnchor;

    // <anchor()> = anchor( <anchor-element>? && <anchor-side>, <length-percentage>? )

    // Can't use Style::ScopedName here, since the scope ordinal is not available at
    // parsing time.
    AtomString elementName;
    AnchorSide side;
    std::optional<Child> fallback;

    bool operator==(const Anchor&) const = default;
};

struct AnchorSize {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(AnchorSize);
    static constexpr auto id = CSSValueAnchorSize;

    // anchor-size() = anchor-size( [ <anchor-element> || <anchor-size> ]? , <length-percentage>? )
    // <anchor-element> = <dashed-ident>
    // <anchor-size> = width | height | block | inline | self-block | self-inline

    // Can't use Style::ScopedName here, since the scope ordinal is not available at
    // parsing time.
    AtomString elementName; // <anchor-element>
    std::optional<Style::AnchorSizeDimension> dimension; // <anchor-size>
    std::optional<Child> fallback;

    bool operator==(const AnchorSize&) const = default;
};


// MARK: TextStream

TextStream& operator<<(TextStream&, Tree);

// MARK: Construction

// Default implementation of ChildConstruction used for all indirect nodes.
template<typename Op> struct ChildConstruction {
    static Child make(Op&& op, Type type) { return Child { IndirectNode<Op> { type, makeUniqueRef<Op>(WTFMove(op)) } }; }
};

// Specialized implementation of ChildConstruction for leaf nodes, needed to avoid `makeUniqueRef`.
template<Leaf T> struct ChildConstruction<T> {
    static Child make(T&& op, Type) { return Child { WTFMove(op) }; }
    static Child make(T&& op) { return Child { WTFMove(op) }; }
};

template<typename Op> Child makeChild(Op op)
{
    return ChildConstruction<Op>::make(WTFMove(op));
}

template<typename Op> Child makeChild(Op op, Type type)
{
    return ChildConstruction<Op>::make(WTFMove(op), type);
}

// MARK: CSSCalc::Type Evaluation

// Gets the type stored in the IndirectNode.
template<typename T> inline Type getType(const IndirectNode<T>& root)
{
    return root.type;
}

// Gets the Type representing a CanonicalDimension::Dimension.
Type getType(CanonicalDimension::Dimension);

// Gets the Type of the leaf node.
Type getType(const Number&);
Type getType(const Percentage&);
Type getType(const NonCanonicalDimension&);
Type getType(const CanonicalDimension&);
Type getType(const Symbol&);
Type getType(const SiblingCount&);
Type getType(const SiblingIndex&);

// Gets the Type of the child node.
Type getType(const Child&);

// Constructs the nodes's type from it the types of its arguments. Returns `std::nullopt` on failure.
std::optional<Type> toType(const Sum&);
std::optional<Type> toType(const Product&);
std::optional<Type> toType(const Negate&);
std::optional<Type> toType(const Invert&);
std::optional<Type> toType(const Min&);
std::optional<Type> toType(const Max&);
std::optional<Type> toType(const Clamp&);
std::optional<Type> toType(const RoundNearest&);
std::optional<Type> toType(const RoundUp&);
std::optional<Type> toType(const RoundDown&);
std::optional<Type> toType(const RoundToZero&);
std::optional<Type> toType(const Mod&);
std::optional<Type> toType(const Rem&);
std::optional<Type> toType(const Sin&);
std::optional<Type> toType(const Cos&);
std::optional<Type> toType(const Tan&);
std::optional<Type> toType(const Asin&);
std::optional<Type> toType(const Acos&);
std::optional<Type> toType(const Atan&);
std::optional<Type> toType(const Atan2&);
std::optional<Type> toType(const Pow&);
std::optional<Type> toType(const Sqrt&);
std::optional<Type> toType(const Hypot&);
std::optional<Type> toType(const Log&);
std::optional<Type> toType(const Exp&);
std::optional<Type> toType(const Abs&);
std::optional<Type> toType(const Sign&);
std::optional<Type> toType(const Random&);
std::optional<Type> toType(const Progress&);

// MARK: CSSUnitType Evaluation

// Maps CanonicalDimension::Dimension to its CSSUnitType counterpart.
constexpr CSSUnitType toCSSUnit(const CanonicalDimension::Dimension& dimension)
{
    switch (dimension) {
    case CanonicalDimension::Dimension::Length:         return CSSUnitType::CSS_PX;
    case CanonicalDimension::Dimension::Angle:          return CSSUnitType::CSS_DEG;
    case CanonicalDimension::Dimension::Time:           return CSSUnitType::CSS_S;
    case CanonicalDimension::Dimension::Frequency:      return CSSUnitType::CSS_HZ;
    case CanonicalDimension::Dimension::Resolution:     return CSSUnitType::CSS_DPPX;
    case CanonicalDimension::Dimension::Flex:           return CSSUnitType::CSS_FR;
    }

    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSUnitType::CSS_PX;
}

// Maps Numeric type to its CSSUnitType counterpart.
constexpr CSSUnitType toCSSUnit(const Number&) { return CSSUnitType::CSS_NUMBER; }
constexpr CSSUnitType toCSSUnit(const Percentage&) { return CSSUnitType::CSS_PERCENTAGE; }
constexpr CSSUnitType toCSSUnit(const CanonicalDimension& dimension) { return toCSSUnit(dimension.dimension); }
constexpr CSSUnitType toCSSUnit(const NonCanonicalDimension& dimension) { return dimension.unit; }

// MARK: Predicates

inline bool isNumeric(const Child& root)
{
    return WTF::switchOn(root,
        []<Numeric T>(const T&) { return true; },
        [](const auto&) { return false; }
    );
}

// Convenience constructors

// Makes the appropriate child type (number, percentage, canonical-dimensions, non-canonical-dimension) based on the CSSUnitType.
Child makeNumeric(double, CSSUnitType);

inline Sum add(Child&& a, Child&& b)
{
    Vector<Child> sumChildren;
    sumChildren.append(WTFMove(a));
    sumChildren.append(WTFMove(b));
    return Sum { .children = WTFMove(sumChildren) };
}

inline Product multiply(Child&& a, Child&& b)
{
    Vector<Child> productChildren;
    productChildren.append(WTFMove(a));
    productChildren.append(WTFMove(b));
    return Product { .children = WTFMove(productChildren) };
}

inline Sum subtract(Child&& a, Child&& b)
{
    return add(WTFMove(a), makeChild(Negate { .a = WTFMove(b) }, getType(b)));
}

inline Child makeChildWithValueBasedOn(double value, const Number&)
{
    return makeChild(Number { .value = value });
}

inline Child makeChildWithValueBasedOn(double value, const Percentage& a)
{
    return makeChild(Percentage { .value = value, .hint = a.hint });
}

inline Child makeChildWithValueBasedOn(double value, const CanonicalDimension& a)
{
    return makeChild(CanonicalDimension { .value = value, .dimension = a.dimension });
}

inline Child makeChildWithValueBasedOn(double value, const NonCanonicalDimension& a)
{
    return makeChild(NonCanonicalDimension { .value = value, .unit = a.unit });
}

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

template<size_t I> const auto& get(const Random& root)
{
    if constexpr (!I)
        return root.sharing;
    else if constexpr (I == 1)
        return root.min;
    else if constexpr (I == 2)
        return root.max;
    else if constexpr (I == 3)
        return root.step;
}

template<size_t I> const auto& get(const Progress& root)
{
    if constexpr (!I)
        return root.value;
    else if constexpr (I == 1)
        return root.start;
    else if constexpr (I == 2)
        return root.end;
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

inline ChildOrNone::ChildOrNone(CSS::Keyword::None none)
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

// AnchorSize

inline AnchorSide::AnchorSide(CSSValueID valueID)
    : value(valueID)
{
}

inline AnchorSide::AnchorSide(Child&& child)
    : value(WTFMove(child))
{
}

// MARK: Size assertions

static_assert(sizeof(Child) <= 24, "Child should stay small");

} // namespace CSSCalc
} // namespace WebCore

namespace std {

#define OP_TUPLE_LIKE_CONFORMANCE(op, numberOfArguments) \
    template<> class tuple_size<WebCore::CSSCalc::op> : public std::integral_constant<size_t, numberOfArguments> { }; \
    template<size_t I> class tuple_element<I, WebCore::CSSCalc::op> { \
    public: \
        using type = decltype(WebCore::CSSCalc::get<I>(std::declval<WebCore::CSSCalc::op>())); \
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
// FIXME (webkit.org/b/280798): make Anchor and AnchorSize tuple-like
OP_TUPLE_LIKE_CONFORMANCE(Anchor, 0);
OP_TUPLE_LIKE_CONFORMANCE(AnchorSize, 0);

#undef OP_TUPLE_LIKE_CONFORMANCE

} // namespace std
