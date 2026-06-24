/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSCalcTree+Serialization.h"

#include "AnchorPositionEvaluator.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcTree+Traversal.h"
#include "CSSCalcTree.h"
#include "CSSMarkup.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPrimitiveValue.h"
#include "CSSUnits.h"
#include <limits>
#include <ranges>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSSCalc {

struct SerializationState {
    enum class GroupingParenthesis {
        Omit,
        Include
    };

    ASCIILiteral openGroup() const { return groupingParenthesis == GroupingParenthesis::Omit ? ""_s : "("_s; }
    ASCIILiteral closeGroup() const { return groupingParenthesis == GroupingParenthesis::Omit ? ""_s : ")"_s; }

    GroupingParenthesis groupingParenthesis = GroupingParenthesis::Include;
    Stage stage = Stage::Specified;
    CSS::Range range = CSS::All;
    const CSS::SerializationContext& serializationContext;
};

struct ParenthesisSaver {
    ParenthesisSaver(SerializationState& state)
        : state { state }
        , savedGroupingParenthesis { state.groupingParenthesis }
    {
    }

    ~ParenthesisSaver()
    {
        state.groupingParenthesis = savedGroupingParenthesis;
    }

    SerializationState& state;
    SerializationState::GroupingParenthesis savedGroupingParenthesis;
};

// https://drafts.csswg.org/css-values-4/#serialize-a-math-function
static void serializeMathFunction(StringBuilder&, const Child&, SerializationState&);
static void serializeMathFunction(StringBuilder&, const Symbol&, SerializationState&);
static void serializeMathFunction(StringBuilder&, const SiblingCount&, SerializationState&);
static void serializeMathFunction(StringBuilder&, const SiblingIndex&, SerializationState&);
static void serializeMathFunction(StringBuilder&, const IndirectNode<Deg2Rad>&, SerializationState&);
template<Numeric Op> static void serializeMathFunction(StringBuilder&, const Op&, SerializationState&);
template<typename Op> static void serializeMathFunction(StringBuilder&, const IndirectNode<Op>&, SerializationState&);

static void serializeMathFunctionPrefix(StringBuilder&, const IndirectNode<Sum>&, SerializationState&);
static void serializeMathFunctionPrefix(StringBuilder&, const IndirectNode<Product>&, SerializationState&);
static void serializeMathFunctionPrefix(StringBuilder&, const IndirectNode<Negate>&, SerializationState&);
static void serializeMathFunctionPrefix(StringBuilder&, const IndirectNode<Invert>&, SerializationState&);
static void serializeMathFunctionPrefix(StringBuilder&, const IndirectNode<RoundNearest>&, SerializationState&);
static void serializeMathFunctionPrefix(StringBuilder&, const IndirectNode<RoundUp>&, SerializationState&);
static void serializeMathFunctionPrefix(StringBuilder&, const IndirectNode<RoundDown>&, SerializationState&);
static void serializeMathFunctionPrefix(StringBuilder&, const IndirectNode<RoundToZero>&, SerializationState&);
template<typename Op> static void serializeMathFunctionPrefix(StringBuilder&, const IndirectNode<Op>&, SerializationState&);

static void serializeMathFunctionArguments(StringBuilder&, const IndirectNode<Sum>&, SerializationState&);
static void serializeMathFunctionArguments(StringBuilder&, const IndirectNode<Product>&, SerializationState&);
static void serializeMathFunctionArguments(StringBuilder&, const IndirectNode<Random>&, SerializationState&);
static void serializeMathFunctionArguments(StringBuilder&, const IndirectNode<CalcMix>&, SerializationState&);
static void serializeMathFunctionArguments(StringBuilder&, const IndirectNode<Anchor>&, SerializationState&);
static void serializeMathFunctionArguments(StringBuilder&, const IndirectNode<AnchorSize>&, SerializationState&);
template<typename Op> static void serializeMathFunctionArguments(StringBuilder&, const IndirectNode<Op>&, SerializationState&);

void serializeWithoutOmittingPrefix(StringBuilder&, const Child&, SerializationState&);

// https://drafts.csswg.org/css-values-4/#serialize-a-calculation-tree
static void serializeCalculationTree(StringBuilder&, const Child&, SerializationState&);
static void serializeCalculationTree(StringBuilder&, const ChildOrNone&, SerializationState&);
static void serializeCalculationTree(StringBuilder&, const CSS::Keyword::None&, SerializationState&);
static void serializeCalculationTree(StringBuilder&, const Symbol&, SerializationState&);
static void serializeCalculationTree(StringBuilder&, const SiblingCount&, SerializationState&);
static void serializeCalculationTree(StringBuilder&, const SiblingIndex&, SerializationState&);
static void serializeCalculationTree(StringBuilder&, const IndirectNode<Sum>&, SerializationState&);
static void serializeCalculationTree(StringBuilder&, const IndirectNode<Product>&, SerializationState&);
static void serializeCalculationTree(StringBuilder&, const IndirectNode<Negate>&, SerializationState&);
static void serializeCalculationTree(StringBuilder&, const IndirectNode<Invert>&, SerializationState&);
static void serializeCalculationTree(StringBuilder&, const IndirectNode<Deg2Rad>&, SerializationState&);
template<Numeric Op> void serializeCalculationTree(StringBuilder&, const Op&, SerializationState&);
template<typename Op> static void serializeCalculationTree(StringBuilder&, const IndirectNode<Op>&, SerializationState&);

// MARK: Sorting

// Sort keys are assigned sequentially via __COUNTER__ rather than hand-numbered,
// so that adding, removing or reordering a case cannot accidentally collide with
// or skip a value (which is how 'vmax' came to share 'svb's key and skip its own).
// The base is captured once so the keys start at 0 regardless of any prior
// __COUNTER__ use in this translation unit.
static constexpr unsigned sortPriorityBase = __COUNTER__;
#define SORT_PRIORITY_NEXT (__COUNTER__ - sortPriorityBase - 1)

// Sentinels that sort after every real unit, independent of how many there are.
static constexpr unsigned errorSortPriority = std::numeric_limits<unsigned>::max() - 1;
static constexpr unsigned otherSortPriority = std::numeric_limits<unsigned>::max();

static unsigned NODELETE sortPriority(CSSUnitType unit)
{
    // Sort order: number, percentage, dimension (by unit, ordered ASCII case-insensitively), other.

    switch (unit) {
    // number
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:      return SORT_PRIORITY_NEXT;
    // percentage
    case CSSUnitType::CSS_PERCENTAGE:   return SORT_PRIORITY_NEXT;

    // dimension (by unit, ordered ASCII case-insensitively)
    case CSSUnitType::CSS_CAP:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_CH:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_CM:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_CQB:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_CQH:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_CQI:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_CQMAX:        return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_CQMIN:        return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_CQW:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_DEG:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_DPCM:         return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_DPI:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_DPPX:         return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_DVB:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_DVH:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_DVI:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_DVMAX:        return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_DVMIN:        return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_DVW:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_EM:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_EX:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_FR:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_GRAD:         return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_HZ:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_IC:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_IN:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_KHZ:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_LH:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_LVB:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_LVH:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_LVI:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_LVMAX:        return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_LVMIN:        return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_LVW:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_MM:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_MS:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_PC:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_PT:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_PX:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_Q:            return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_RAD:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_RCAP:         return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_RCH:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_REM:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_REX:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_RIC:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_RLH:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_S:            return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_SVB:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_SVH:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_SVI:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_SVMAX:        return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_SVMIN:        return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_SVW:          return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_TURN:         return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_VB:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_VH:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_VI:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_VMAX:         return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_VMIN:         return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_VW:           return SORT_PRIORITY_NEXT;
    case CSSUnitType::CSS_X:            return SORT_PRIORITY_NEXT;

    // Non-numeric types are not supported.
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_UNKNOWN:
        break;
    }

    ASSERT_NOT_REACHED();
    return errorSortPriority;
}

#undef SORT_PRIORITY_NEXT

static unsigned sortPriority(const Child& child)
{
    // https://drafts.csswg.org/css-values-4/#sort-a-calculations-children

    return WTF::switchOn(child,
        []<Numeric T>(const T& root) -> unsigned {
            return sortPriority(toCSSUnit(root));
        },
        [](const auto&) -> unsigned {
            return otherSortPriority; // Sorts after every numeric unit, even the error case.
        }
    );
}

struct ChildRepresentation {
    // Offset in the operations `children` vector.
    size_t index;

    // Value used to order children during sort, based on unit.
    unsigned sortPriority;
};

static Vector<ChildRepresentation, 16> generateSortedChildrenMap(const Children& children)
{
    Vector<ChildRepresentation, 16> sortedChildrenMap;
    sortedChildrenMap.reserveInitialCapacity(children.size());

    for (size_t i = 0; i < children.size(); ++i)
        sortedChildrenMap.append(ChildRepresentation { .index = i, .sortPriority = sortPriority(children[i]) });

    std::ranges::stable_sort(sortedChildrenMap, { }, &ChildRepresentation::sortPriority);

    return sortedChildrenMap;
}

// MARK: Math Function
// https://drafts.csswg.org/css-values-4/#serialize-a-math-function

static double clampValue(double value, CSS::Range range)
{
    value = std::isnan(value) ? 0 : value;
    return std::clamp(value, range.min, range.max);
}

void serializeMathFunction(StringBuilder& builder, const Child& fn, SerializationState& state)
{
    WTF::switchOn(fn, [&builder, &state](const auto& root) { serializeMathFunction(builder, root, state); });
}

template<Numeric Op> void serializeMathFunction(StringBuilder& builder, const Op& fn, SerializationState& state)
{
    // 1. If the root of the calculation tree fn represents is a numeric value (number, percentage, or dimension), and the serialization being produced is of a computed value or later, then clamp the value to the range allowed for its context (if necessary), then serialize the value as normal and return the result.

    if (state.stage == Stage::Computed) {
        auto clampedFn = makeChildWithValueBasedOn(clampValue(fn.value, state.range), fn);
        serializeCalculationTree(builder, clampedFn, state);
        return;
    }

    // `CSS::SerializableNumber` serialization implements the appropriate logic for steps 2 & steps 3-5 for Numeric expressions.

    // 2. If fn represents an infinite or NaN value: let s be the string "calc(".
    // 2.1. Let s be the string "calc(".
    // 2.2. Serialize the keyword infinity, -infinity, or NaN, as appropriate to represent the value, and append it to s.
    // 2.3. If fn’s type is anything other than «[ ]» (empty, representing a <number>), append " * " to s. Create a numeric value in the canonical unit for fn’s type (such as px for <length>), with a value of 1. Serialize this numeric value and append it to s.

    // [...]

    // 3. If the calculation tree’s root node is a numeric value, or a calc-operator node, let s be a string initially containing "calc(".
    // 4. For each child of the root node, serialize the calculation tree. [...]
    // 5. Append ")" (close parenthesis) to s.

    builder.append("calc("_s);
    serializeCalculationTree(builder, fn, state);
    builder.append(')');
}

void serializeMathFunction(StringBuilder& builder, const Symbol& fn, SerializationState& state)
{
    builder.append("calc("_s);
    serializeCalculationTree(builder, fn, state);
    builder.append(')');
}

void serializeMathFunction(StringBuilder& builder, const SiblingCount& fn, SerializationState& state)
{
    serializeCalculationTree(builder, fn, state);
}

void serializeMathFunction(StringBuilder& builder, const SiblingIndex& fn, SerializationState& state)
{
    serializeCalculationTree(builder, fn, state);
}

template<typename Op> void serializeMathFunction(StringBuilder& builder, const IndirectNode<Op>& fn, SerializationState& state)
{
    // 3. If the calculation tree’s root node is a numeric value, or a calc-operator node, let s be a string initially containing "calc(".
    //
    //    Otherwise, let s be a string initially containing the name of the root node, lowercased (such as "sin" or "max"), followed by a "(" (open parenthesis).

    // Both clauses of step 3 are handle via the appropriate overloaded function.

    serializeMathFunctionPrefix(builder, fn, state);

    // 4. For each child of the root node, serialize the calculation tree. If a result of this serialization starts with a "(" (open parenthesis) and ends with a ")" (close parenthesis), remove those characters from the result. Concatenate all of the results using ", " (comma followed by space), then append the result to s.
    {
        ParenthesisSaver saver { state };
        state.groupingParenthesis = SerializationState::GroupingParenthesis::Omit;

        serializeMathFunctionArguments(builder, fn, state);
    }

    // 5. Append ")" (close parenthesis) to s.
    builder.append(')');
}


void serializeMathFunctionPrefix(StringBuilder& builder, const IndirectNode<Sum>&, SerializationState&)
{
    builder.append("calc("_s);
}

void serializeMathFunctionPrefix(StringBuilder& builder, const IndirectNode<Product>&, SerializationState&)
{
    builder.append("calc("_s);
}

void serializeMathFunctionPrefix(StringBuilder& builder, const IndirectNode<Negate>&, SerializationState&)
{
    builder.append("calc("_s);
}

void serializeMathFunctionPrefix(StringBuilder& builder, const IndirectNode<Invert>&, SerializationState&)
{
    builder.append("calc("_s);
}

void serializeMathFunctionPrefix(StringBuilder& builder, const IndirectNode<RoundNearest>&, SerializationState&)
{
    builder.append(nameLiteralForSerialization(CSSValueRound), '(', nameLiteralForSerialization(RoundNearest::id), ", "_s);
}

void serializeMathFunctionPrefix(StringBuilder& builder, const IndirectNode<RoundUp>&, SerializationState&)
{
    builder.append(nameLiteralForSerialization(CSSValueRound), '(', nameLiteralForSerialization(RoundUp::id), ", "_s);
}

void serializeMathFunctionPrefix(StringBuilder& builder, const IndirectNode<RoundDown>&, SerializationState&)
{
    builder.append(nameLiteralForSerialization(CSSValueRound), '(', nameLiteralForSerialization(RoundDown::id), ", "_s);
}

void serializeMathFunctionPrefix(StringBuilder& builder, const IndirectNode<RoundToZero>&, SerializationState&)
{
    builder.append(nameLiteralForSerialization(CSSValueRound), '(', nameLiteralForSerialization(RoundToZero::id), ", "_s);
}

template<typename Op> void serializeMathFunctionPrefix(StringBuilder& builder, const IndirectNode<Op>&, SerializationState&)
{
    builder.append(nameLiteralForSerialization(Op::id), '(');
}

void serializeMathFunctionArguments(StringBuilder& builder, const IndirectNode<Sum>& fn, SerializationState& state)
{
    serializeCalculationTree(builder, fn, state);
}

void serializeMathFunctionArguments(StringBuilder& builder, const IndirectNode<Product>& fn, SerializationState& state)
{
    serializeCalculationTree(builder, fn, state);
}

void serializeMathFunctionArguments(StringBuilder& builder, const IndirectNode<Random>& fn, SerializationState& state)
{
    WTF::switchOn(fn->sharing,
        [&](const Random::SharingOptions& options) {
            WTF::switchOn(options.identifier,
                [&](const Random::SharingOptions::Auto&) {
                    // Noting to do.
                },
                [&](const CSS::CustomIdent& customIdent) {
                    if (!customIdent.value.isNull()) {
                        CSS::serializationForCSS(builder, state.serializationContext, customIdent);
                        if (options.elementScoped)
                            builder.append(' ', nameLiteralForSerialization(CSSValueElementScoped), ", "_s);
                        else
                            builder.append(", "_s);
                    } else if (options.elementScoped)
                        builder.append(' ', nameLiteralForSerialization(CSSValueElementScoped), ", "_s);
                }
            );
        },
        [&](const Random::SharingFixed& fixed) {
            builder.append(nameLiteralForSerialization(CSSValueFixed), ' ');
            CSS::serializationForCSS(builder, state.serializationContext, fixed.value);
            builder.append(", "_s);
        }
    );

    serializeCalculationTree(builder, fn->min, state);
    builder.append(", "_s);
    serializeCalculationTree(builder, fn->max, state);

    if (fn->step) {
        builder.append(", "_s);
        serializeCalculationTree(builder, *fn->step, state);
    }
}

void serializeMathFunctionArguments(StringBuilder& builder, const IndirectNode<CalcMix>& fn, SerializationState& state)
{
    auto separator = ""_s;
    for (const auto& item : fn->children) {
        builder.append(std::exchange(separator, ", "_s));
        serializeCalculationTree(builder, item.value, state);
        if (item.weight) {
            builder.append(' ');
            CSS::serializationForCSS(builder, state.serializationContext, *item.weight);
        }
    }
}

void serializeMathFunctionArguments(StringBuilder& builder, const IndirectNode<Anchor>& anchor, SerializationState& state)
{
    if (anchor->elementName) {
        CSS::serializationForCSS(builder, state.serializationContext, *anchor->elementName);
        builder.append(' ');
    }

    WTF::switchOn(anchor->side,
        [&](CSSValueID valueID) {
            builder.append(nameLiteralForSerialization(valueID));
        },
        [&](const Child& percentage) {
            // As anchor() is not actually a "math function", calc() can't be omitted in arguments.
            serializeWithoutOmittingPrefix(builder, percentage, state);
        }
    );

    if (anchor->fallback) {
        builder.append(", "_s);
        serializeWithoutOmittingPrefix(builder, *anchor->fallback, state);
    }
}

static void serializeAnchorSizeDimension(StringBuilder& builder, Style::AnchorSizeDimension dimension)
{
    switch (dimension) {
    case Style::AnchorSizeDimension::Width:
        builder.append("width"_s);
        break;
    case Style::AnchorSizeDimension::Height:
        builder.append("height"_s);
        break;
    case Style::AnchorSizeDimension::Block:
        builder.append("block"_s);
        break;
    case Style::AnchorSizeDimension::Inline:
        builder.append("inline"_s);
        break;
    case Style::AnchorSizeDimension::SelfBlock:
        builder.append("self-block"_s);
        break;
    case Style::AnchorSizeDimension::SelfInline:
        builder.append("self-inline"_s);
        break;
    }
}

void serializeMathFunctionArguments(StringBuilder& builder, const IndirectNode<AnchorSize>& anchorSize, SerializationState& state)
{
    if (anchorSize->elementName)
        CSS::serializationForCSS(builder, state.serializationContext, *anchorSize->elementName);

    if (anchorSize->dimension) {
        if (anchorSize->elementName)
            builder.append(' ');
        serializeAnchorSizeDimension(builder, *anchorSize->dimension);
    }

    if (anchorSize->fallback) {
        if (anchorSize->elementName || anchorSize->dimension)
            builder.append(", "_s);

        serializeWithoutOmittingPrefix(builder, *anchorSize->fallback, state);
    }
}

template<typename Op> void serializeMathFunctionArguments(StringBuilder& builder, const IndirectNode<Op>& fn, SerializationState& state)
{
    auto separator = ""_s;
    forAllChildren(*fn, WTF::makeVisitor(
        [&](const std::optional<Child>& root) {
            if (root) {
                builder.append(std::exchange(separator, ", "_s));
                serializeCalculationTree(builder, *root, state);
            }
        },
        [&](const CSS::CustomIdent& root) {
            if (!root.value.isNull()) {
                builder.append(std::exchange(separator, ", "_s));
                CSS::serializationForCSS(builder, state.serializationContext, root);
            }
        },
        [&](const auto& root) {
            builder.append(std::exchange(separator, ", "_s));
            serializeCalculationTree(builder, root, state);
        }
    ));
}

void serializeWithoutOmittingPrefix(StringBuilder& builder, const Child& child, SerializationState& state)
{
    WTF::switchOn(child,
        [&](Leaf auto& op) {
            serializeCalculationTree(builder, op, state);
        },
        [&](auto& op) {
            serializeMathFunction(builder, op, state);
        }
    );
}

// MARK: Calculation Tree
// https://drafts.csswg.org/css-values-4/#serialize-a-calculation-tree

void serializeCalculationTree(StringBuilder& builder, const Child& root, SerializationState& state)
{
    WTF::switchOn(root, [&builder, &state](const auto& root) { serializeCalculationTree(builder, root, state); });
}

void serializeCalculationTree(StringBuilder& builder, const ChildOrNone& root, SerializationState& state)
{
    WTF::switchOn(root, [&builder, &state](const auto& root) { serializeCalculationTree(builder, root, state); });
}

void serializeCalculationTree(StringBuilder& builder, const CSS::Keyword::None& root, SerializationState& state)
{
    CSS::serializationForCSS(builder, state.serializationContext, root);
}

template<Numeric Op> void serializeCalculationTree(StringBuilder& builder, const Op& root, SerializationState& state)
{
    // 2. If root is a numeric value, or a non-math function, serialize root per the normal rules for it and return the result.

    CSS::serializationForCSS(builder, state.serializationContext, CSS::SerializableNumber { root.value, unitTypeString(toCSSUnit(root)) });
}

void serializeCalculationTree(StringBuilder& builder, const Symbol& root, SerializationState&)
{
    // 2. If root is a numeric value, or a non-math function, serialize root per the normal rules for it and return the result.

    builder.append(nameLiteralForSerialization(root.id));
}

void serializeCalculationTree(StringBuilder& builder, const SiblingCount& root, SerializationState&)
{
    // 2. If root is a numeric value, or a non-math function, serialize root per the normal rules for it and return the result.

    builder.append(nameLiteralForSerialization(root.id), "()"_s);
}

void serializeCalculationTree(StringBuilder& builder, const SiblingIndex& root, SerializationState&)
{
    // 2. If root is a numeric value, or a non-math function, serialize root per the normal rules for it and return the result.

    builder.append(nameLiteralForSerialization(root.id), "()"_s);
}

void serializeCalculationTree(StringBuilder& builder, const IndirectNode<Sum>& root, SerializationState& state)
{
    ASSERT(!root->children.isEmpty());

    // 6. If root is a Sum node,

    // - let s be a string initially containing "(".
    builder.append(state.openGroup());

    // - Sort root’s children.

    // NOTE: Rather than actually sorting the children, which we can't because they are immutable, we generate
    // a map of offsets to sorted offsets we can use while iterating.
    auto sortedChildrenMap = generateSortedChildrenMap(root->children);

    {
        ParenthesisSaver saver { state };
        state.groupingParenthesis = SerializationState::GroupingParenthesis::Include;

        // - Serialize root’s first child, and append it to s.
        serializeCalculationTree(builder, root->children[sortedChildrenMap[0].index], state);

        // - For each child of root beyond the first:
        for (size_t i = 1; i < root->children.size(); ++i) {
            WTF::switchOn(root->children[sortedChildrenMap[i].index],
                [&builder, &state](const IndirectNode<Negate>& child) {
                    // 1. If child is a Negate node, append " - " to s, then serialize the Negate’s child and append the result to s.
                    builder.append(" - "_s);
                    serializeCalculationTree(builder, child->a, state);
                },
                [&builder, &state]<Numeric T>(const T& child) {
                    // 2. If child is a negative numeric value, append " - " to s, then serialize the negation of child as normal and append the result to s.
                    if (child.value < 0) {
                        builder.append(" - "_s);
                        serializeCalculationTree(builder, makeChildWithValueBasedOn(-child.value, child), state);
                        return;
                    }

                    // 3. Otherwise, append " + " to s, then serialize child and append the result to s.
                    builder.append(" + "_s);
                    serializeCalculationTree(builder, child, state);
                },
                [&builder, &state](const auto& child) {
                    // 3. Otherwise, append " + " to s, then serialize child and append the result to s.
                    builder.append(" + "_s);
                    serializeCalculationTree(builder, child, state);
                }
            );
        }
    }

    // - Finally, append ")" to s and return it.
    builder.append(state.closeGroup());
}

void serializeCalculationTree(StringBuilder& builder, const IndirectNode<Product>& root, SerializationState& state)
{
    ASSERT(!root->children.isEmpty());

    // 7. If root is a Product node,

    // - let s be a string initially containing "(".
    builder.append(state.openGroup());

    // - Sort root’s children.

    // NOTE: Rather than actually sorting the children, which we can't because they are immutable, we generate
    // a map of offsets to sorted offsets we can use while iterating.
    auto sortedChildrenMap = generateSortedChildrenMap(root->children);

    {
        ParenthesisSaver saver { state };
        state.groupingParenthesis = SerializationState::GroupingParenthesis::Include;

        // - Serialize root’s first child, and append it to s.
        serializeCalculationTree(builder, root->children[sortedChildrenMap[0].index], state);

        // - For each child of root beyond the first:
        for (size_t i = 1; i < root->children.size(); ++i) {
            WTF::switchOn(root->children[sortedChildrenMap[i].index],
                [&builder, &state](const IndirectNode<Invert>& child) {
                    // 1. If child is an Invert node, append " / " to s, then serialize the Invert’s child and append the result to s.
                    builder.append(" / "_s);
                    serializeCalculationTree(builder, child->a, state);
                },
                [&builder, &state](const auto& child) {
                    // 2. Otherwise, append " * " to s, then serialize child and append the result to s.
                    builder.append(" * "_s);
                    serializeCalculationTree(builder, child, state);
                }
            );
        }
    }

    // - Finally, append ")" to s and return it.
    builder.append(state.closeGroup());
}

void serializeCalculationTree(StringBuilder& builder, const IndirectNode<Negate>& root, SerializationState& state)
{
    // 4. If root is a Negate node,

    // - let s be a string initially containing "(-1 * ".
    builder.append(state.openGroup(), "-1 * "_s);

    {
        ParenthesisSaver saver { state };
        state.groupingParenthesis = SerializationState::GroupingParenthesis::Include;

        // - Serialize root’s child, and append it to s.
        serializeCalculationTree(builder, root->a, state);
    }

    // - Append ")" to s, then return it.
    builder.append(state.closeGroup());
}

void serializeCalculationTree(StringBuilder& builder, const IndirectNode<Invert>& root, SerializationState& state)
{
    // 5. If root is an Invert node,

    // - let s be a string initially containing "(1 / ".
    builder.append(state.openGroup(), "1 / "_s);

    {
        ParenthesisSaver saver { state };
        state.groupingParenthesis = SerializationState::GroupingParenthesis::Include;

        // - Serialize root’s child, and append it to s.
        serializeCalculationTree(builder, root->a, state);
    }

    // - Append ")" to s, then return it.
    builder.append(state.closeGroup());
}

void serializeCalculationTree(StringBuilder& builder, const IndirectNode<Deg2Rad>& root, SerializationState& state)
{
    // Deg2Rad is an implementation-only node inserted at parse time inside trig functions. It has
    // no CSS-level representation, so serialize it transparently by just serializing its child.
    serializeCalculationTree(builder, root->angle, state);
}

void serializeMathFunction(StringBuilder& builder, const IndirectNode<Deg2Rad>& root, SerializationState& state)
{
    // Deg2Rad has no CSS-level representation, so defer to the child.
    serializeMathFunction(builder, root->angle, state);
}

template<typename Op> void serializeCalculationTree(StringBuilder& builder, const IndirectNode<Op>& root, SerializationState& state)
{
    // 3. If root is anything but a Sum, Negate, Product, or Invert node, serialize a math function for the function corresponding to the node type, treating the node’s children as the function’s comma-separated calculation arguments, and return the result.
    serializeMathFunction(builder, root, state);
}

// MARK: Exposed interface

void serializationForCSS(StringBuilder& builder, const Tree& tree, const SerializationOptions& options)
{
    SerializationState state {
        .stage = tree.stage,
        .range = options.range,
        .serializationContext = options.serializationContext,
    };
    serializeMathFunction(builder, tree.root, state);
}

String serializationForCSS(const Tree& tree, const SerializationOptions& options)
{
    StringBuilder builder;
    serializationForCSS(builder, tree, options);
    return builder.toString();
}

void serializationForCSS(StringBuilder& builder, const Child& child, const SerializationOptions& options)
{
    SerializationState state {
        .range = options.range,
        .serializationContext = options.serializationContext,
    };
    serializeCalculationTree(builder, child, state);
}

String serializationForCSS(const Child& child, const SerializationOptions& options)
{
    StringBuilder builder;
    serializationForCSS(builder, child, options);
    return builder.toString();
}

} // namespace CSSCalc
} // namespace WebCore
