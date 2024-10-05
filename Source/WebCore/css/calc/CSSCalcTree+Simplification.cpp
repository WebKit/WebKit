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

#include "config.h"
#include "CSSCalcTree+Simplification.h"

#include "AnchorPositionEvaluator.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcTree+Evaluation.h"
#include "CSSCalcTree+NumericIdentity.h"
#include "CSSCalcTree+Traversal.h"
#include "CSSCalcTree.h"
#include "CSSPrimitiveValue.h"
#include "CalculationCategory.h"
#include "CalculationExecutor.h"
#include "RenderStyle.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderState.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace CSSCalc {

static auto copyAndSimplify(const Children&, const SimplificationOptions&) -> Children;
static auto copyAndSimplify(const std::optional<Child>&, const SimplificationOptions&) -> std::optional<Child>;
static auto copyAndSimplify(const CSS::NoneRaw&, const SimplificationOptions&) -> CSS::NoneRaw;
static auto copyAndSimplify(const ChildOrNone&, const SimplificationOptions&) -> ChildOrNone;

template<typename Op, typename... Args> static double executeMathOperation(Args&&... args)
{
    return Calculation::executeOperation<typename Op::Base>(std::forward<Args>(args)...);
}

// MARK: Predicate: percentageResolveToDimension

static bool percentageResolveToDimension(const SimplificationOptions& options)
{
    switch (options.category) {
    case Calculation::Category::Integer:
    case Calculation::Category::Number:
    case Calculation::Category::Length:
    case Calculation::Category::Percentage:
    case Calculation::Category::Angle:
    case Calculation::Category::Time:
    case Calculation::Category::Frequency:
    case Calculation::Category::Resolution:
    case Calculation::Category::Flex:
        return false;

    case Calculation::Category::AnglePercentage:
    case Calculation::Category::LengthPercentage:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

// MARK: Predicate: unitsMatch

constexpr bool unitsMatch(const Number&, const Number&, const SimplificationOptions&)
{
    return true;
}

constexpr bool unitsMatch(const Percentage&, const Percentage&, const SimplificationOptions&)
{
    return true;
}

static bool unitsMatch(const CanonicalDimension& a, const CanonicalDimension& b, const SimplificationOptions&)
{
    return a.dimension == b.dimension;
}

static bool unitsMatch(const NonCanonicalDimension& a, const NonCanonicalDimension& b, const SimplificationOptions& options)
{
    return a.unit == b.unit || options.allowNonMatchingUnits;
}

// MARK: Predicate: magnitudeComparable

constexpr bool magnitudeComparable(const Number&, const SimplificationOptions&)
{
    return true;
}

static bool magnitudeComparable(const Percentage&, const SimplificationOptions& options)
{
    return !percentageResolveToDimension(options);
}

constexpr bool magnitudeComparable(const CanonicalDimension&, const SimplificationOptions&)
{
    return true;
}

constexpr bool magnitudeComparable(const NonCanonicalDimension&, const SimplificationOptions&)
{
    return true;
}

// MARK: Predicate: fullyResolved

constexpr bool fullyResolved(const Number&, const SimplificationOptions&)
{
    return true;
}

static bool fullyResolved(const Percentage&, const SimplificationOptions& options)
{
    return !percentageResolveToDimension(options);
}

constexpr bool fullyResolved(const CanonicalDimension&, const SimplificationOptions&)
{
    return true;
}

constexpr bool fullyResolved(const NonCanonicalDimension&, const SimplificationOptions& options)
{
    return options.allowUnresolvedUnits;
}

std::optional<CanonicalDimension> canonicalize(NonCanonicalDimension root, const std::optional<CSSToLengthConversionData>& conversionData)
{
    auto makeCanonical = [&](double value, CanonicalDimension::Dimension dimension) -> std::optional<CanonicalDimension> {
        return CanonicalDimension { .value = value, .dimension = dimension };
    };

    auto tryMakeCanonical = [&](double value, CSSUnitType unit) -> std::optional<CanonicalDimension> {
        if (conversionData) {
            // We are only interested in canonicalizing to `px`, not adjusting for zoom, which will be handled later.
            return CanonicalDimension { .value = CSSPrimitiveValue::computeNonCalcLengthDouble(*conversionData, unit, value) / conversionData->style()->usedZoom(), .dimension = CanonicalDimension::Dimension::Length };
        }
        return std::nullopt;
    };

    switch (root.unit) {
    // Absolute Lengths (can be canonicalized without conversion data).
    case CSSUnitType::CSS_CM:
        return makeCanonical(root.value * CSS::pixelsPerCm,              CanonicalDimension::Dimension::Length);
    case CSSUnitType::CSS_MM:
        return makeCanonical(root.value * CSS::pixelsPerMm,              CanonicalDimension::Dimension::Length);
    case CSSUnitType::CSS_Q:
        return makeCanonical(root.value * CSS::pixelsPerQ,               CanonicalDimension::Dimension::Length);
    case CSSUnitType::CSS_IN:
        return makeCanonical(root.value * CSS::pixelsPerInch,            CanonicalDimension::Dimension::Length);
    case CSSUnitType::CSS_PT:
        return makeCanonical(root.value * CSS::pixelsPerPt,              CanonicalDimension::Dimension::Length);
    case CSSUnitType::CSS_PC:
        return makeCanonical(root.value * CSS::pixelsPerPc,              CanonicalDimension::Dimension::Length);

    // Font, Viewport and Container relative Lengths (require conversion data for canonicalization).
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        return tryMakeCanonical(root.value, root.unit);

    // <angle>
    case CSSUnitType::CSS_RAD:
        return makeCanonical(root.value * degreesPerRadianDouble,        CanonicalDimension::Dimension::Angle);
    case CSSUnitType::CSS_GRAD:
        return makeCanonical(root.value * degreesPerGradientDouble,      CanonicalDimension::Dimension::Angle);
    case CSSUnitType::CSS_TURN:
        return makeCanonical(root.value * degreesPerTurnDouble,          CanonicalDimension::Dimension::Angle);

    // <time>
    case CSSUnitType::CSS_MS:
        return makeCanonical(root.value * CSS::secondsPerMillisecond,    CanonicalDimension::Dimension::Time);

    // <frequency>
    case CSSUnitType::CSS_KHZ:
        return makeCanonical(root.value * CSS::hertzPerKilohertz,        CanonicalDimension::Dimension::Frequency);

    // <resolution>
    case CSSUnitType::CSS_X:
        return makeCanonical(root.value * CSS::dppxPerX,                 CanonicalDimension::Dimension::Resolution);
    case CSSUnitType::CSS_DPI:
        return makeCanonical(root.value * CSS::dppxPerDpi,               CanonicalDimension::Dimension::Resolution);
    case CSSUnitType::CSS_DPCM:
        return makeCanonical(root.value * CSS::dppxPerDpcm,              CanonicalDimension::Dimension::Resolution);

    // Canonical dimensional types should never be stored in a NonCanonicalDimension.
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_FR:
    // Non-dimensional types should never be stored in a NonCanonicalDimension.
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    // Non-numeric types should never be stored in a NonCanonicalDimension.
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_VALUE_ID:
    case CSSUnitType::CustomIdent:
        break;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}


// MARK: Generic partial evaluation functions

template<typename Op> static std::optional<Child> simplifyForOperation(Child& a, Child& b, const SimplificationOptions& options)
{
    if (a.index() != b.index())
        return std::nullopt;

    return WTF::switchOn(a,
        [&]<Numeric T>(T& numericA) -> std::optional<Child> {
            auto& numericB = std::get<T>(b);
            if (!unitsMatch(numericA, numericB, options) || !fullyResolved(numericA, options))
                return std::nullopt;

            return makeChildWithValueBasedOn(executeMathOperation<Op>(numericA.value, numericB.value), numericA);
        },
        [](auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

template<typename Op, typename Completion> static std::optional<Child> simplifyForOperationWithCompletion(Child& a, Child& b, const SimplificationOptions& options, Completion&& completion)
{
    if (a.index() != b.index())
        return std::nullopt;

    return WTF::switchOn(a,
        [&]<Numeric T>(T& numericA) -> std::optional<Child> {
            auto& numericB = std::get<T>(b);
            if (!unitsMatch(numericA, numericB, options) || !fullyResolved(numericA, options))
                return std::nullopt;

            return completion(executeMathOperation<Op>(numericA.value, numericB.value));
        },
        [](auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

template<typename Op> static std::optional<Child> simplifyForRound(Op& root, const SimplificationOptions& options)
{
    if (root.b)
        return simplifyForOperation<Op>(root.a, *root.b, options);

    if (auto* numberA = std::get_if<Number>(&root.a))
        return makeChild(Number { .value = executeMathOperation<Op>(numberA->value, 1.0) });

    return std::nullopt;
}

template<typename Op> static std::optional<Child> simplifyForTrig(Op& root, const SimplificationOptions&)
{
    // NOTE: `a` has been type checked by this point to be `<number>` or an `<angle>`, though they may not
    // be able to be fully resolved yet. If its an `<angle>`, it is also already been converted to canonical
    // units via earlier simplification.

    return WTF::switchOn(root.a,
        [&](Number& a) -> std::optional<Child> {
            return makeChild(Number { .value = executeMathOperation<Op>(a.value) });
        },
        [&](CanonicalDimension& a) -> std::optional<Child> {
            ASSERT(a.dimension == CanonicalDimension::Dimension::Angle);
            return makeChild(Number { .value = executeMathOperation<Op>(deg2rad(a.value)) });
        },
        [](auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

template<typename Op> static std::optional<Child> simplifyForArcTrig(Op& root, const SimplificationOptions&)
{
    // NOTE: `a` has been type checked by this point to be `<number>`, though they may not
    // be able to be fully resolved yet.

    return WTF::switchOn(root.a,
        [&](Number& a) -> std::optional<Child> {
            return makeChild(CanonicalDimension { .value = executeMathOperation<Op>(a.value), .dimension = CanonicalDimension::Dimension::Angle });
        },
        [](auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

template<typename Op> static std::optional<Child> simplifyForMinMax(Op& root, const SimplificationOptions& options)
{
    ASSERT(!root.children.isEmpty());

    // This function implements shared logic for Min and Max simplification:

    //   5.1. For each node child of root’s children:
    //        If child is a numeric value with enough information to compare magnitudes with another child of the same unit (see note in previous step), and there are other children of root that are numeric values with the same unit, combine all such children with the appropriate operator per root, and replace child with the result, removing all other child nodes involved.
    //   5.2. If root has only one child, return the child.
    //   5.3. Otherwise, return root.

    // --

    // These steps are implemented as a two phase procedure.
    //    1. Iterate children to find "merge opportunities", counting the total number of merges that will happen, and storing the index of the first child of each merge type in a lookup table.
    //    2. Perform merges based on data from step 1.
    //
    // By splitting it up, we can perform two optimizations:
    //    1. If the result of step 1 shows that the number of "merge opportunities" will lead to only one remaining child, we can avoid allocating a new Children Vector, and just merge directly into the child.
    //    2. If the result of step 1 shows that the number of "merge opportunities" will lead to more than one remaining child, we can precisely allocate the Children Vector to be (existing children - "merge opportunities").

    auto evaluate = [](const Child& a, const Child& b) -> Child {
        ASSERT(a.index() == b.index());

        return WTF::switchOn(a,
            [&]<Numeric T>(const T& aNumeric) -> Child {
                ASSERT(toNumericIdentity(aNumeric) == toNumericIdentity(std::get<T>(b)));
                return makeChildWithValueBasedOn(executeMathOperation<Op>(aNumeric.value, std::get<T>(b).value), aNumeric);
            },
            [](const auto&) -> Child {
                ASSERT_NOT_REACHED();
                return makeChild(Number { .value = 0 });
            }
        );
    };

    // Special case a root with one child to avoid doing any work at all, and just returning the child.
    if (root.children.size() == 1)
        return { WTFMove(root.children[0]) };

    // Map of unit types (via NumericIdentity) to the first index in `root.children` where a value with that unit can be found.
    // More specifically, it maps the unit to the index + 1, as 0 is used to indicate no units of that type have been found.
    // FIXME: This should be turned into a type with an interface that doesn't require explicit use of static_cast<uint8_t> by the caller.
    std::array<size_t, numberOfNumericIdentityTypes> offsetOfFirstInstance { };

    bool canMergePercentages = !percentageResolveToDimension(options);

    unsigned numberOfMergeOpportunities = 0;
    for (size_t i = 0; i < root.children.size(); ++i) {
        numberOfMergeOpportunities += WTF::switchOn(root.children[i],
            [&]<Numeric T>(const T& child) {
                auto id = toNumericIdentity(child);
                if (id == NumericIdentity::Percentage && !canMergePercentages)
                    return 0;

                if (auto offset = offsetOfFirstInstance[static_cast<uint8_t>(id)]) {
                    // There has already been an instance of this type. This is a merge opportunity.

                    // Merge the value into first instance.
                    root.children[offset - 1] = evaluate(root.children[offset - 1], root.children[i]);

                    // Return 1 to increment the number of merge opportunities observed.
                    return 1;
                }

                // First instance of this. Store the index (well, index + 1, since 0 is the unset value).
                offsetOfFirstInstance[static_cast<uint8_t>(id)] = i + 1;

                // Give this was the first instance, it is not yet a merge opportunity.
                return 0;
            },
            [](const auto&) {
                return 0;
            }
        );
    }

    // If there are no merge opportunities, no further simplification is possible.
    if (!numberOfMergeOpportunities)
        return std::nullopt;

    auto combinedChildrenSize = root.children.size() - numberOfMergeOpportunities;

    // If all the removal from merges leaves a single child, that means everything merged into the first child.
    if (combinedChildrenSize == 1)
        return { WTFMove(root.children[0]) };

    Children combinedChildren;
    combinedChildren.reserveInitialCapacity(combinedChildrenSize);

    for (size_t i = 0; i < root.children.size(); ++i) {
        WTF::switchOn(root.children[i],
            [&]<Numeric T>(const T& child) {
                auto offset = offsetOfFirstInstance[static_cast<uint8_t>(toNumericIdentity(child))];

                // If the stored offset for this type is unset (as it would be for percentages if merging them is disallowed) or is set to this index (as it would be for the first instance of a merged type), append the child as normal.
                if (!offset || (offset - 1) == i) {
                    combinedChildren.append(WTFMove(root.children[i]));
                    return;
                }

                // Otherwise, it's one that can be dropped.
            },
            [&](const auto&) {
                combinedChildren.append(WTFMove(root.children[i]));
            }
        );
    }
    root.children = WTFMove(combinedChildren);

    return std::nullopt;
}

// MARK: In-place simplification / replacement finding.

std::optional<Child> simplify(Number&, const SimplificationOptions&)
{
    // No further simplification possible for <number>.
    return std::nullopt;
}

std::optional<Child> simplify(Percentage&, const SimplificationOptions&)
{
    // 1.1. If root is a percentage that will be resolved against another value, and there is enough information available to resolve it, do so, and express the resulting numeric value in the appropriate canonical unit. Return the value.
    // NOTE: Handled by the Calculation::Tree / CalculationValue types at use time.
    return std::nullopt;
}

std::optional<Child> simplify(CanonicalDimension&, const SimplificationOptions&)
{
    // No further simplification possible for canonical <dimension>.
    return std::nullopt;
}

std::optional<Child> simplify(NonCanonicalDimension& root, const SimplificationOptions& options)
{
    // NOTE: This implements the non-canonical dimension relevant parts of the numeric value simplification steps.

    // 1.2. If root is a dimension that is not expressed in its canonical unit, and there is enough information available to convert it to the canonical unit, do so, and return the value.
    if (auto canonical = canonicalize(root, options.conversionData))
        return makeChild(WTFMove(*canonical));

    return std::nullopt;
}

std::optional<Child> simplify(Symbol& root, const SimplificationOptions& options)
{
    // NOTE: This implements the keyword relevant parts of the numeric value simplification steps.

    // 1.3. If root is a <calc-keyword> that can be resolved, return what it resolves to, simplified.
    if (auto value = options.symbolTable.get(root.id))
        return copyAndSimplify(makeNumeric(value->value, root.unit), options);

    return std::nullopt;
}

std::optional<Child> simplify(Sum& root, const SimplificationOptions& options)
{
    ASSERT(!root.children.isEmpty());

    // 8. If root is a Sum node:

    // 8.1. For each of root’s children that are Sum nodes, replace them with their children.
    if (std::ranges::any_of(root.children, [](auto& child) { return std::holds_alternative<IndirectNode<Sum>>(child); })) {
        Children newChildren;
        for (auto& child : root.children) {
            if (auto* childSum = std::get_if<IndirectNode<Sum>>(&child))
                newChildren.appendVector(WTFMove((*childSum)->children));
            else
                newChildren.append(WTFMove(child));
        }
        root.children = WTFMove(newChildren);
    }

    // 8.2. For each set of root’s children that are numeric values with identical units, remove those children and replace them with a single numeric value containing the sum of the removed nodes, and with the same unit. (E.g. combine numbers, combine percentages, combine px values, etc.)
    // 8.3. If root has only a single child at this point, return the child.
    // 8.4. Otherwise, return root

    // These steps are implemented as a two phase procedure.
    //    1. Iterate children to find "merge/removal opportunities", counting the total number of opportunities that will happen, and storing the index of the first child of each type in a lookup table.
    //    2. Perform merges and removals based on data from step 1.
    //
    // By splitting it up, we can perform two optimizations:
    //    1. If the result of step 1 shows that the number of "merge/removal opportunities" will lead to only one remaining child, we can avoid allocating a new Children Vector, and just merge directly into the child.
    //    2. If the result of step 1 shows that the number of "merge/removal opportunities" will lead to more than one remaining child, we can precisely allocate the Children Vector to be (existing children - "merge/removal opportunities").

    auto evaluate = [](const Child& a, const Child& b) -> std::pair<Child, double> {
        ASSERT(a.index() == b.index());

        return WTF::switchOn(a,
            [&]<Numeric T>(const T& aNumeric) -> std::pair<Child, double> {
                ASSERT(toNumericIdentity(aNumeric) == toNumericIdentity(std::get<T>(b)));
                auto result = executeMathOperation<Sum>(aNumeric.value, std::get<T>(b).value);
                return { makeChildWithValueBasedOn(result, aNumeric), result };
            },
            [](const auto&) -> std::pair<Child, double> {
                ASSERT_NOT_REACHED();
                return { makeChild(Number { .value = 0 }), 0 };
            }
        );
    };

    // Special case a root with one child to avoid doing any work at all, and just returning the child.
    if (root.children.size() == 1)
        return { WTFMove(root.children[0]) };

    // Map of unit types (via NumericIdentity) to the first index in `root.children` where a value with that unit can be found.
    // More specifically, it maps the unit to the index + 1, as 0 is used to indicate no units of that type have been found.
    // FIXME: This should be turned into a type with an interface that doesn't require explicit use of static_cast<uint8_t> by the caller.
    struct FirstInstance {
        size_t offset = 0;
        unsigned merges = 0;
        bool canRemove = false;
    };
    std::array<FirstInstance, numberOfNumericIdentityTypes> firstInstances { };

    for (size_t i = 0; i < root.children.size(); ++i) {
        WTF::switchOn(root.children[i],
            [&]<Numeric T>(const T& child) {
                auto id = toNumericIdentity(child);
                bool canRemoveIfZero = isLength(id) && options.allowZeroValueLengthRemovalFromSum;

                if (auto& firstInstance = firstInstances[static_cast<uint8_t>(id)]; firstInstance.offset) {
                    // There has already been an instance of this type. This is a merge opportunity.

                    // Calculate the merged value.
                    auto [mergedChild, mergedValue] = evaluate(root.children[firstInstance.offset - 1], root.children[i]);

                    // Store the merged value in the original array.
                    root.children[firstInstance.offset - 1] = WTFMove(mergedChild);

                    // Update the `merges` count and `canRemove` bit for the new merged value.
                    firstInstance.merges += 1;
                    firstInstance.canRemove = canRemoveIfZero && !mergedValue;
                    return;
                }

                // First instance of this. Store the index (well, index + 1, since 0 is the unset value) and the canRemove bit.
                firstInstances[static_cast<uint8_t>(id)] = {
                    .offset = i + 1,
                    .merges = 0,
                    .canRemove = canRemoveIfZero && !child.value
                };
            },
            [](const auto&) {
                // Non-numeric values are not eligible for merge or removal.
            }
        );
    }

    // Calculate the total number of children we will be able to remove from merges and removals.
    unsigned childrenToRemoveFromMerges = 0;
    unsigned childrenToRemoveTotal = 0;
    for (auto& firstInstance : firstInstances) {
        if (firstInstance.offset) {
            childrenToRemoveFromMerges += firstInstance.merges;
            childrenToRemoveTotal += firstInstance.merges + (firstInstance.canRemove ? 1 : 0);
        }
    }

    // If there are no merge/removal opportunities, no further simplification is possible.
    if (!childrenToRemoveTotal)
        return std::nullopt;

    // If all the removal from merges leaves a single child, that means everything merged into the first child.
    if ((root.children.size() - childrenToRemoveFromMerges) == 1)
        return { WTFMove(root.children[0]) };

    auto combinedChildrenSize = root.children.size() - childrenToRemoveTotal;

    // If the new size is 0, we removed too much. Return a single 0 value of type `length` to keep things valid. A value of type `length` is returned because the only kind of node that can be removed is of type `length`.
    if (!combinedChildrenSize)
        return { makeChild(CanonicalDimension { .value = 0, .dimension = CanonicalDimension::Dimension::Length }) };

    // If the new size is 1, we know there is one child, we just don't know which one yet.
    if (combinedChildrenSize == 1) {
        for (size_t i = 0; i < root.children.size(); ++i) {
            auto replacement = WTF::switchOn(root.children[i],
                [&]<Numeric T>(const T& child) -> std::optional<Child> {
                    auto& firstInstance = firstInstances[static_cast<uint8_t>(toNumericIdentity(child))];
                    ASSERT(firstInstance.offset);

                    // If the stored offset for this type is set to this index and it's not one that can be removed, this is the 1 child to return.
                    if ((firstInstance.offset - 1) == i && !firstInstance.canRemove)
                        return { WTFMove(root.children[i]) };

                    // Otherwise, it's one that can be dropped.
                    return std::nullopt;
                },
                [&](const auto&) -> std::optional<Child> {
                    return { WTFMove(root.children[i]) };
                }
            );
            if (replacement)
                return { WTFMove(*replacement) };
        }
    }

    Children combinedChildren;
    combinedChildren.reserveInitialCapacity(combinedChildrenSize);

    for (size_t i = 0; i < root.children.size(); ++i) {
        WTF::switchOn(root.children[i],
            [&]<Numeric T>(const T& child) {
                auto& firstInstance = firstInstances[static_cast<uint8_t>(toNumericIdentity(child))];
                ASSERT(firstInstance.offset);

                // If the stored offset for this type is set to this index and it's not one that can be removed, append the child as normal
                if ((firstInstance.offset - 1) == i && !firstInstance.canRemove) {
                    combinedChildren.append(WTFMove(root.children[i]));
                    return;
                }

                // Otherwise, it's one that can be dropped.
            },
            [&](const auto&) {
                combinedChildren.append(WTFMove(root.children[i]));
            }
        );
    }
    root.children = WTFMove(combinedChildren);

    return std::nullopt;
}

std::optional<Child> simplify(Product& root, const SimplificationOptions& options)
{
    ASSERT(!root.children.isEmpty());

    // 9. If root is a Product node:

    // NOTE: We merge steps 9.1. and 9.2, as they have significant overlap.

    // 9.1. For each of root’s children that are Product nodes, replace them with their children.
    //
    //   -- and --
    //
    // 9.2. If root has multiple children that are numbers (not percentages or dimensions), remove them and replace them with a single number containing the product of the removed nodes.

    Children newChildren;
    std::optional<Number> numericProduct;

    auto processChild = [&newChildren, &numericProduct](Child& child) {
        if (auto* childValue = std::get_if<Number>(&child)) {
            if (numericProduct)
                numericProduct = Number { .value = childValue->value * numericProduct->value };
            else
                numericProduct = Number { .value = childValue->value };
        } else
            newChildren.append(WTFMove(child));
    };

    for (auto& child : root.children) {
        if (auto* childProduct = std::get_if<IndirectNode<Product>>(&child)) {
            for (auto& childProductChild : (*childProduct)->children)
                processChild(childProductChild);
        } else
            processChild(child);
    }

    // If `numericProduct` has a value and `newChildren` is empty, that means all the children were numbers and the product can be returned directly.
    if (numericProduct) {
        if (newChildren.isEmpty())
            return makeChild(*numericProduct);

        // 9.3. If root contains only two children, one of which is a number (not a percentage or dimension) and the other of which is a Sum whose children are all numeric values, multiply all of the Sum’s children by the number, then return the Sum.

        // We extend this step to include numeric and Invert children for the non-number child as an optimization taking advantage of step 9.4, but for the case where the check is cheaper.

        // NOTE: Since we just merged all numeric values into `numericProduct`, we know that if `numericProduct` is not std::nullopt the last child is a singular `number` child. Therefore, we only need to check if there is one child and is a Sum (or Numeric or Invert).

        if (newChildren.size() == 1) {
            auto replacement = WTF::switchOn(newChildren[0],
                [&]<Numeric T>(T& numeric) -> std::optional<Child> {
                    return makeChildWithValueBasedOn(numeric.value * numericProduct->value, numeric);
                },
                [&](IndirectNode<Sum>& sum) -> std::optional<Child> {
                    if (!std::ranges::all_of(sum->children, [](auto& child) { return isNumeric(child); }))
                        return std::nullopt;

                    for (auto& child : sum->children) {
                        WTF::switchOn(child,
                            [&]<Numeric T>(T& child) { child.value *= numericProduct->value; },
                            [](auto&) { }
                        );
                    }

                    return { Child { WTFMove(sum) } };
                },
                [&](IndirectNode<Invert>& invert) -> std::optional<Child> {
                    return WTF::switchOn(invert->a,
                        [&]<Numeric T>(T& child) -> std::optional<Child> {
                            return makeChildWithValueBasedOn(child.value * numericProduct->value, child);
                        },
                        [](auto&) -> std::optional<Child> {
                            return std::nullopt;
                        }
                    );
                },
                [](auto&) -> std::optional<Child> {
                    return std::nullopt;
                }
            );

            if (replacement)
                return { WTFMove(*replacement) };
        }

        // If there was more than one child or no replacement was found, append the product from step 9.2 into the newChildren array.
        newChildren.append(makeChild(*numericProduct));
    }

    root.children = WTFMove(newChildren);

    // 9.4. If root contains only numeric values and/or Invert nodes containing numeric values, and multiplying the types of all the children (noting that the type of an Invert node is the inverse of its child’s type) results in a type that matches any of the types that a math function can resolve to, return the result of multiplying all the values of the children (noting that the value of an Invert node is the reciprocal of its child’s value), expressed in the result’s canonical unit.

    struct ProductResult {
        double value;
        Type type;
    };
    auto productResult = ProductResult { .value = 1, .type = Type { } };

    bool success = false;
    for (auto& child : root.children) {
        success = WTF::switchOn(child,
            [&](const Number& number) -> bool {
                // <number> is the identity type, so multiplying by it has no effect.
                productResult.value *= number.value;
                return true;
            },
            [&](const Percentage& percentage) -> bool {
                auto multipliedType = Type::multiply(productResult.type, getType(percentage));
                if (!multipliedType)
                    return false;

                productResult.type = *multipliedType;
                productResult.value *= percentage.value;
                return true;
            },
            [&](const CanonicalDimension& canonicalDimension) -> bool {
                auto multipliedType = Type::multiply(productResult.type, getType(canonicalDimension.dimension));
                if (!multipliedType)
                    return false;

                productResult.type = *multipliedType;
                productResult.value *= canonicalDimension.value;
                return true;
            },
            [&](IndirectNode<Invert>& invertChild) -> bool {
                return WTF::switchOn(invertChild->a,
                    [&](const Number& number) -> bool {
                        // <number> is the identity type, so multiplying / inverting by it has no effect.
                        productResult.value /= number.value;
                        return true;
                    },
                    [&](const Percentage& percentage) -> bool {
                        auto invertedPercentageChildType = Type::invert(getType(percentage));
                        auto multipliedType = Type::multiply(productResult.type, invertedPercentageChildType);
                        if (!multipliedType)
                            return false;

                        productResult.type = *multipliedType;
                        productResult.value /= percentage.value;
                        return true;
                    },
                    [&](const CanonicalDimension& canonicalDimension) -> bool {
                        auto invertedCanonicalDimensionType = Type::invert(getType(canonicalDimension));
                        auto multipliedType = Type::multiply(productResult.type, invertedCanonicalDimensionType);
                        if (!multipliedType)
                            return false;

                        productResult.type = *multipliedType;
                        productResult.value /= canonicalDimension.value;
                        return true;
                    },
                    [](const auto&) -> bool {
                        return false;
                    }
                );
            },
            [](const auto&) -> bool {
                return false;
            }
        );
        if (!success)
            break;
    }
    if (success) {
        if (auto category = productResult.type.calculationCategory()) {
            switch (*category) {
            case Calculation::Category::Integer:
            case Calculation::Category::Number:
                return makeChild(Number { .value = productResult.value });
            case Calculation::Category::Percentage:
                return makeChild(Percentage { .value = productResult.value, .hint = Type::determinePercentHint(options.category) });
            case Calculation::Category::LengthPercentage:
                return makeChild(Percentage { .value = productResult.value, .hint = PercentHint::Length });
            case Calculation::Category::Length:
                return makeChild(CanonicalDimension { .value = productResult.value, .dimension = CanonicalDimension::Dimension::Length });
            case Calculation::Category::Angle:
                return makeChild(CanonicalDimension { .value = productResult.value, .dimension = CanonicalDimension::Dimension::Angle });
            case Calculation::Category::AnglePercentage:
                return makeChild(Percentage { .value = productResult.value, .hint = PercentHint::Angle });
            case Calculation::Category::Time:
                return makeChild(CanonicalDimension { .value = productResult.value, .dimension = CanonicalDimension::Dimension::Time });
            case Calculation::Category::Frequency:
                return makeChild(CanonicalDimension { .value = productResult.value, .dimension = CanonicalDimension::Dimension::Frequency });
            case Calculation::Category::Resolution:
                return makeChild(CanonicalDimension { .value = productResult.value, .dimension = CanonicalDimension::Dimension::Resolution });
            case Calculation::Category::Flex:
                return makeChild(CanonicalDimension { .value = productResult.value, .dimension = CanonicalDimension::Dimension::Flex });
            }
        }
    }

    // 9.5. Return root.
    return std::nullopt;
}

std::optional<Child> simplify(Negate& root, const SimplificationOptions&)
{
    // 6. If root is a Negate node:

    return WTF::switchOn(root.a,
        [&]<Numeric T>(T& a) -> std::optional<Child> {
            // 6.1. If root’s child is a numeric value, return an equivalent numeric value, but with the value negated (0 - value).
            return makeChildWithValueBasedOn(0.0 - a.value, a);
        },
        [](IndirectNode<Negate>& a) -> std::optional<Child> {
            // 6.2. If root’s child is a Negate node, return the child’s child.
            return { WTFMove(a->a) };
        },
        [](IndirectNode<Sum>& a) -> std::optional<Child> {
            // Not stated in spec, but needed for tests.

            if (!std::ranges::all_of(a->children, [](auto& child) { return isNumeric(child); }))
                return std::nullopt;

            for (auto& child : a->children) {
                WTF::switchOn(child,
                    [&]<Numeric T>(T& child) { child.value = -child.value; },
                    [](auto&) { }
                );
            }

            return { Child { WTFMove(a) } };
        },
        [](IndirectNode<Product>& a) -> std::optional<Child> {
            // Not stated in spec, but needed for tests.

            if (!std::ranges::all_of(a->children, [](auto& child) { return isNumeric(child); }))
                return std::nullopt;

            for (auto& child : a->children) {
                WTF::switchOn(child,
                    [&]<Numeric T>(T& child) { child.value = -child.value; },
                    [](auto&) { }
                );
            }

            return { Child { WTFMove(a) } };
        },
        [](auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(Invert& root, const SimplificationOptions&)
{
    // 7. If root is an Invert node:

    return WTF::switchOn(root.a,
        [&](Number& a) -> std::optional<Child> {
            // 7.1. If root’s child is a number (not a percentage or dimension) return the reciprocal of the child’s value.
            return makeChild(Number { .value = (1.0 / a.value) });
        },
        [](IndirectNode<Invert>& a) -> std::optional<Child> {
            // 7.2. If root’s child is an Invert node, return the child’s child.
            return { WTFMove(a->a) };
        },
        [](auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(Min& root, const SimplificationOptions& options)
{
    return simplifyForMinMax(root, options);
}

std::optional<Child> simplify(Max& root, const SimplificationOptions& options)
{
    return simplifyForMinMax(root, options);
}

std::optional<Child> simplify(Clamp& root, const SimplificationOptions& options)
{
    auto minIsNone = std::holds_alternative<CSS::NoneRaw>(root.min);
    auto maxIsNone = std::holds_alternative<CSS::NoneRaw>(root.max);

    if (minIsNone && maxIsNone) {
        // - clamp(none, VAL, none) is equivalent to just calc(VAL).
        return { WTFMove(root.val) };
    }

    // FIXME: Are any of these transforms kosher?
    // If only MIN and VAL have matching units, we can transform clamp(MIN, VAL, MAX) aka (max(MIN, min(VAL, MAX)) into a min(newVAL, MAX).
    // If only VAL and MAX have matching units, we can transform clamp(MIN, VAL, MAX) aka (max(MIN, min(VAL, MAX)) into a max(MIN, newVAL).

    return WTF::switchOn(root.val,
        [&]<Numeric T>(T& val) -> std::optional<Child> {
            if (minIsNone) {
                auto& maxChild = std::get<Child>(root.max);
                if (!std::holds_alternative<T>(maxChild))
                    return std::nullopt;

                auto& max = std::get<T>(maxChild);

                if (!unitsMatch(val, max, options))
                    return std::nullopt;

                // As units already match, we only have to check that one of the arguments is `magnitudeComparable`.
                if (!magnitudeComparable(val, options))
                    return std::nullopt;

                // - clamp(none, VAL, MAX) is equivalent to min(VAL, MAX)
                return makeChildWithValueBasedOn(executeMathOperation<Min>(val.value, max.value), val);
            } else if (maxIsNone) {
                auto& minChild = std::get<Child>(root.min);
                if (!std::holds_alternative<T>(minChild))
                    return std::nullopt;

                auto& min = std::get<T>(minChild);

                if (!unitsMatch(min, val, options))
                    return std::nullopt;

                // As units already match, we only have to check that one of the arguments is `magnitudeComparable`.
                if (!magnitudeComparable(val, options))
                    return std::nullopt;

                // - clamp(MIN, VAL, none) is equivalent to max(MIN, VAL)
                return makeChildWithValueBasedOn(executeMathOperation<Max>(min.value, val.value), val);
            } else {
                auto& minChild = std::get<Child>(root.min);
                auto& maxChild = std::get<Child>(root.max);

                // If all three parameters have the same unit, we can perform the clamp in full.
                if (!std::holds_alternative<T>(minChild) || !std::holds_alternative<T>(maxChild))
                    return std::nullopt;

                auto& min = std::get<T>(minChild);
                auto& max = std::get<T>(maxChild);

                if (!unitsMatch(min, val, options) || !unitsMatch(val, max, options))
                    return std::nullopt;

                // As units already match, we only have to check that one of the arguments is `magnitudeComparable`.
                if (!magnitudeComparable(val, options))
                    return std::nullopt;

                return makeChildWithValueBasedOn(executeMathOperation<Clamp>(min.value, val.value, max.value), val);
            }
        },
        [](const auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(RoundNearest& root, const SimplificationOptions& options)
{
    return simplifyForRound(root, options);
}

std::optional<Child> simplify(RoundUp& root, const SimplificationOptions& options)
{
    return simplifyForRound(root, options);
}

std::optional<Child> simplify(RoundDown& root, const SimplificationOptions& options)
{
    return simplifyForRound(root, options);
}

std::optional<Child> simplify(RoundToZero& root, const SimplificationOptions& options)
{
    return simplifyForRound(root, options);
}

std::optional<Child> simplify(Mod& root, const SimplificationOptions& options)
{
    return simplifyForOperation<Mod>(root.a, root.b, options);
}

std::optional<Child> simplify(Rem& root, const SimplificationOptions& options)
{
    return simplifyForOperation<Rem>(root.a, root.b, options);
}

std::optional<Child> simplify(Sin& root, const SimplificationOptions& options)
{
    return simplifyForTrig(root, options);
}

std::optional<Child> simplify(Cos& root, const SimplificationOptions& options)
{
    return simplifyForTrig(root, options);
}

std::optional<Child> simplify(Tan& root, const SimplificationOptions& options)
{
    return simplifyForTrig(root, options);
}

std::optional<Child> simplify(Asin& root, const SimplificationOptions& options)
{
    return simplifyForArcTrig(root, options);
}

std::optional<Child> simplify(Acos& root, const SimplificationOptions& options)
{
    return simplifyForArcTrig(root, options);
}

std::optional<Child> simplify(Atan& root, const SimplificationOptions& options)
{
    return simplifyForArcTrig(root, options);
}

std::optional<Child> simplify(Atan2& root, const SimplificationOptions& options)
{
    return simplifyForOperationWithCompletion<Atan2>(root.a, root.b, options, [](double value) {
        return makeChild(CanonicalDimension { .value = value, .dimension = CanonicalDimension::Dimension::Angle });
    });
}

std::optional<Child> simplify(Pow& root, const SimplificationOptions&)
{
    // NOTE: `a` and `b` have been type checked by this point to be `<number>`, though they may not
    // be able to be fully resolved yet.

    if (root.a.index() != root.b.index())
        return std::nullopt;

    return WTF::switchOn(root.a,
        [&](const Number& a) -> std::optional<Child> {
            return makeChild(Number { .value = executeMathOperation<Pow>(a.value, std::get<Number>(root.b).value) });
        },
        [](const auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(Sqrt& root, const SimplificationOptions&)
{
    // NOTE: `a` has been type checked by this point to be `<number>`, though they may not
    // be able to be fully resolved yet.

    return WTF::switchOn(root.a,
        [&](const Number& a) -> std::optional<Child> {
            return makeChild(Number { .value = executeMathOperation<Sqrt>(a.value) });
        },
        [](const auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(Hypot& root, const SimplificationOptions& options)
{
    // Hypot can be simplified if all its children are the same type, and it is both canonical (for lengths) and fully resolved (for percentages). We optimistically assume that the children fit this criteria, and execute the operation over the children, checking each one as it is requested. If we find out our assumption was incorrect (e.g. a child is non-canonical or non-resolved), we set a flag indicating the evaluation failed, but due to the evaluation API's interface, must evaluate all the remaining children. Once the evaluation is complete, if the fail bit is set, we failed to simplify, if it is not, we can return the new numeric result.

    struct NumberTag { };
    struct PercentageTag { };
    struct DimensionTag { CanonicalDimension::Dimension dimension; };
    struct FailureTag { };
    std::variant<std::monostate, NumberTag, PercentageTag, DimensionTag, FailureTag> result;

    double value = executeMathOperation<Hypot>(root.children, [&](const auto& child) {
        return WTF::switchOn(result,
            [&](const std::monostate&) -> double {
                // First iteration.
                return WTF::switchOn(child,
                    [&](const Number& number) -> double {
                        result = NumberTag { };
                        return number.value;
                    },
                    [&](const Percentage& percentage) -> double {
                        if (percentageResolveToDimension(options)) {
                            result = FailureTag { };
                            return std::numeric_limits<double>::quiet_NaN();
                        }
                        result = PercentageTag { };
                        return percentage.value;
                    },
                    [&](const CanonicalDimension& dimension) -> double {
                        result = DimensionTag { dimension.dimension };
                        return dimension.value;
                    },
                    [&](const auto&) -> double {
                        result = FailureTag { };
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                );
            },
            [&](const NumberTag&) -> double {
                if (auto* numberChild = std::get_if<Number>(&child))
                    return numberChild->value;
                result = FailureTag { };
                return std::numeric_limits<double>::quiet_NaN();
            },
            [&](const PercentageTag&) -> double {
                if (auto* percentageChild = std::get_if<Percentage>(&child))
                    return percentageChild->value;
                result = FailureTag { };
                return std::numeric_limits<double>::quiet_NaN();
            },
            [&](const DimensionTag& tag) -> double {
                if (auto* dimensionChild = std::get_if<CanonicalDimension>(&child); dimensionChild && dimensionChild->dimension == tag.dimension)
                    return dimensionChild->value;
                result = FailureTag { };
                return std::numeric_limits<double>::quiet_NaN();
            },
            [&](const FailureTag&) -> double {
                return std::numeric_limits<double>::quiet_NaN();
            }
        );
    });

    return WTF::switchOn(result,
        [&](const NumberTag&) -> std::optional<Child> {
            return makeChild(Number { .value = value });
        },
        [&](const PercentageTag&) -> std::optional<Child> {
            return makeChild(Percentage { .value = value, .hint = Type::determinePercentHint(options.category) });
        },
        [&](const DimensionTag& tag) -> std::optional<Child> {
            return makeChild(CanonicalDimension { .value = value, .dimension = tag.dimension });
        },
        [&](const auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(Log& root, const SimplificationOptions&)
{
    // NOTE: `a` and `b` have been type checked by this point to be `<number>`, though they may not
    // be able to be fully resolved yet.

    if (root.b) {
        if (root.a.index() != root.b->index())
            return std::nullopt;

        return WTF::switchOn(root.a,
            [&](const Number& a) -> std::optional<Child> {
                return makeChild(Number { .value = executeMathOperation<Log>(a.value, std::get<Number>(*root.b).value) });
            },
            [](const auto&) -> std::optional<Child> {
                return std::nullopt;
            }
        );
    }

    return WTF::switchOn(root.a,
        [](const Number& a) -> std::optional<Child> {
            return makeChild(Number { .value = executeMathOperation<Log>(a.value) });
        },
        [](const auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(Exp& root, const SimplificationOptions&)
{
    // NOTE: `a` has been type checked by this point to be `<number>`, though they may not
    // be able to be fully resolved yet.

    return WTF::switchOn(root.a,
        [](const Number& a) -> std::optional<Child> {
            return makeChild(Number { .value = executeMathOperation<Exp>(a.value) });
        },
        [](const auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(Abs& root, const SimplificationOptions& options)
{
    return WTF::switchOn(root.a,
        [&]<Numeric T>(const T& a) -> std::optional<Child> {
            if (!magnitudeComparable(a, options))
                return std::nullopt;
            return makeChildWithValueBasedOn(executeMathOperation<Abs>(a.value), a);
        },
        [](const auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(Sign& root, const SimplificationOptions& options)
{
    return WTF::switchOn(root.a,
        [&]<Numeric T>(const T& a) -> std::optional<Child> {
            if (!magnitudeComparable(a, options))
                return std::nullopt;
            return makeChild(Number { .value = executeMathOperation<Sign>(a.value) });
        },
        [](const auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(Progress& root, const SimplificationOptions& options)
{
    if (root.progress.index() != root.from.index() || root.from.index() != root.to.index())
        return std::nullopt;

    return WTF::switchOn(root.progress,
        [&]<Numeric T>(T& numericProgress) -> std::optional<Child> {
            auto& numericFrom = std::get<T>(root.from);
            auto& numericTo = std::get<T>(root.to);

            if (!unitsMatch(numericProgress, numericFrom, options) || !unitsMatch(numericFrom, numericTo, options) || !fullyResolved(numericProgress, options))
                return std::nullopt;

            return makeChild(Number { .value = executeMathOperation<Progress>(numericProgress.value, numericFrom.value, numericTo.value) });
        },
        [](auto&) -> std::optional<Child> {
            return std::nullopt;
        }
    );
}

std::optional<Child> simplify(Anchor& anchor, const SimplificationOptions& options)
{
    if (!options.conversionData || !options.conversionData->styleBuilderState())
        return { };

    auto evaluationOptions = EvaluationOptions { .conversionData = options.conversionData, .symbolTable = options.symbolTable };

    auto result = evaluateWithoutFallback(anchor, evaluationOptions);
    if (!result) {
        // https://drafts.csswg.org/css-anchor-position-1/#anchor-valid
        // "If any of these conditions are false, the anchor() function resolves to its specified fallback value.
        // If no fallback value is specified, it makes the declaration referencing it invalid at computed-value time."

        if (!anchor.fallback)
            options.conversionData->styleBuilderState()->setCurrentPropertyInvalidAtComputedValueTime();

        // Replace the anchor node with the fallback node.
        return std::exchange(anchor.fallback, { });
    }
    return CanonicalDimension { .value = *result, .dimension = CanonicalDimension::Dimension::Length };
}

std::optional<Child> simplify(AnchorSize&, const SimplificationOptions&)
{
    // FIXME (webkit.org/b/280789): evaluate anchor-size()
    return CanonicalDimension { .value = 0, .dimension = CanonicalDimension::Dimension::Length };
}

// MARK: Copy & Simplify.

CSS::NoneRaw copyAndSimplify(const CSS::NoneRaw& root, const SimplificationOptions&)
{
    return root;
}

static ChildOrNone copyAndSimplify(const ChildOrNone& root, const SimplificationOptions& options)
{
    return WTF::switchOn(root, [&](auto& root) { return ChildOrNone { copyAndSimplify(root, options) }; });
}

std::optional<Child> copyAndSimplify(const std::optional<Child>& root, const SimplificationOptions& options)
{
    if (root)
        return copyAndSimplify(*root, options);
    return std::nullopt;
}

Children copyAndSimplify(const Children& children, const SimplificationOptions& options)
{
    return WTF::map(children, [&](auto& child) { return copyAndSimplify(child, options); });
}

template<Leaf Op> static auto copyAndSimplifyChildren(const Op& op, const SimplificationOptions&) -> Op
{
    return op;
}

template<typename Op> static auto copyAndSimplifyChildren(const IndirectNode<Op>& root, const SimplificationOptions& options) -> Op
{
    return WTF::apply([&](const auto& ...x) { return Op { copyAndSimplify(x, options)... }; } , *root);
}

static auto copyAndSimplifyChildren(const IndirectNode<Anchor>& anchor, const SimplificationOptions& options) -> Anchor
{
    return Anchor { .elementName = anchor->elementName, .side = copy(anchor->side), .fallback = copyAndSimplify(anchor->fallback, options) };
}

Child copyAndSimplify(const Child& root, const SimplificationOptions& options)
{
    return WTF::switchOn(root,
        [&](const auto& root) -> Child {
            // Create a simplified copy by recursively calling simplify on all children.
            auto simplified = copyAndSimplifyChildren(root, options);

            // Attempt to simplify the term itself, using the result as a replacement if successful.
            if (auto replacement = simplify(simplified, options))
                return WTFMove(*replacement);

            return makeChild(WTFMove(simplified), getType(root));
        }
    );
}

Tree copyAndSimplify(const Tree& tree, const SimplificationOptions& options)
{
    return Tree {
        .root = copyAndSimplify(tree.root, options),
        .type = tree.type,
        .category = tree.category,
        .stage = tree.stage,
        .range = tree.range,
        .requiresConversionData = tree.requiresConversionData
    };
}

} // namespace CSSCalc
} // namespace WebCore
