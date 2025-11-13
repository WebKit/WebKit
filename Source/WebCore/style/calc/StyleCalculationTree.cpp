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
#include "StyleCalculationTree.h"

#include "StyleCalculationTree+Executor.h"
#include "StyleCalculationTree+NumericIdentity.h"
#include "StyleCalculationTree+Traversal.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {
namespace Calculation {

static constexpr size_t maximumCalculationTreeDepth = 128;

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Abs);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Acos);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Asin);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Atan);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Atan2);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Clamp);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Cos);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Exp);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Hypot);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Invert);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Log);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Max);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Min);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Mod);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Negate);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Pow);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Product);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Progress);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Random);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Rem);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundDown);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundNearest);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundToZero);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundUp);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sign);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sin);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sqrt);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sum);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Tan);

// MARK: - Logging

template<typename Op>
static auto dumpVariadic(TextStream&, const IndirectNode<Op>&, ASCIILiteral prefix, ASCIILiteral between) -> TextStream&;

template<typename Op>
static auto operator<<(TextStream&, const IndirectNode<Op>&) -> TextStream&;
static auto operator<<(TextStream&, const Random::Fixed&) -> TextStream&;
static auto operator<<(TextStream&, const ChildOrNone&) -> TextStream&;
static auto operator<<(TextStream&, const Child&) -> TextStream&;
static auto operator<<(TextStream&, const Number&) -> TextStream&;
static auto operator<<(TextStream&, const Percentage&) -> TextStream&;
static auto operator<<(TextStream&, const Dimension&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Sum>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Product>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Negate>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Invert>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Min>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Max>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Hypot>&) -> TextStream&;

template<typename Op> TextStream& dumpVariadic(TextStream& ts, const IndirectNode<Op>& root, ASCIILiteral prefix, ASCIILiteral between)
{
    ts << prefix << '(';

    auto separator = ""_s;
    for (auto& child : root->children)
        ts << std::exchange(separator, between) << child;

    return ts << ')';
}

template<typename Op> auto operator<<(TextStream& ts, const IndirectNode<Op>& root) -> TextStream&
{
    ts << Op::op << '(';

    auto separator = ""_s;
    forAllChildren(*root, WTF::makeVisitor(
        [&](const std::optional<Child>& root) {
            if (root)
                ts << std::exchange(separator, ", "_s) << *root;
        },
        [&](const auto& root) {
            ts << std::exchange(separator, ", "_s) << root;
        }
    ));

    return ts << ')';
}

TextStream& operator<<(TextStream& ts, const Random::Fixed& fixed)
{
    return ts << "fixed "_s << fixed.baseValue;
}

TextStream& operator<<(TextStream& ts, const ChildOrNone& root)
{
    return WTF::switchOn(root, [&](const auto& root) -> TextStream& { return ts << root; });
}

TextStream& operator<<(TextStream& ts, const Child& root)
{
    return WTF::switchOn(root, [&](const auto& root) -> TextStream& { return ts << root; });
}

TextStream& operator<<(TextStream& ts, const Number& root)
{
    return ts << TextStream::FormatNumberRespectingIntegers(root.value);
}

TextStream& operator<<(TextStream& ts, const Percentage& root)
{
    return ts << TextStream::FormatNumberRespectingIntegers(root.value) << '%';
}

TextStream& operator<<(TextStream& ts, const Dimension& root)
{
    return ts << TextStream::FormatNumberRespectingIntegers(root.value);
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Sum>& root)
{
    return dumpVariadic(ts, root, ""_s, " + "_s);
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Product>& root)
{
    return dumpVariadic(ts, root, ""_s, " * "_s);
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Negate>& root)
{
    return ts << "-("_s << root->a << ')';
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Invert>& root)
{
    return ts << "1.0 / ("_s << root->a << ')';
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Min>& root)
{
    return dumpVariadic(ts, root, "min"_s, " * "_s);
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Max>& root)
{
    return dumpVariadic(ts, root, "max"_s, " * "_s);
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Hypot>& root)
{
    return dumpVariadic(ts, root, "hypot"_s, ", "_s);
}


TextStream& operator<<(TextStream& ts, const Tree& tree)
{
    return ts << tree.root;
}

// MARK: - Depth Computation

static size_t computeDepth(const Child& root)
{
    size_t maximumChildDepth = 0;
    forAllChildren(root, WTF::makeVisitor(
        [&](const std::optional<Child>& child) {
            if (child)
                maximumChildDepth = std::max(computeDepth(*child), maximumChildDepth);
        },
        [&](const Child& child) {
            maximumChildDepth = std::max(computeDepth(child), maximumChildDepth);
        },
        [&](const ChildOrNone& childOrNone) {
            if (childOrNone.holdsAlternative<Child>())
                maximumChildDepth = std::max(computeDepth(get<Child>(childOrNone)), maximumChildDepth);
        },
        [&](const auto&) {
            maximumChildDepth = std::max<size_t>(1, maximumChildDepth);
        }
    ));
    return maximumChildDepth + 1;
}

size_t computeDepth(const Tree& tree)
{
    return computeDepth(tree.root);
}

// MARK: - Simplification

static std::optional<Child> simplify(Sum& root)
{
    // This is almost identical to the Sum simplification in CSSCalcTree+Simplification.cpp with the following differences:
    // - It operates on Style::Calculation::Sum, not CSSCalc::Sum.
    // - It returns a Style::Calculation::Child, not CSSCalc::Child.
    // - It does not take SimplificationOptions, and therefore does not handle the `allowZeroValueLengthRemovalFromSum` option. It is instead always allowed.
    // FIXME: Find a way to share code with version in CSSCalcTree+Simplification.cpp.

    // 8. If root is a Sum node:

    // 8.1. For each of root’s children that are Sum nodes, replace them with their children.
    if (std::ranges::any_of(root.children, [](auto& child) { return WTF::holdsAlternative<IndirectNode<Sum>>(child); })) {
        Vector<Child> newChildren;
        for (auto& child : root.children) {
            if (auto* childSum = get_if<IndirectNode<Sum>>(&child))
                newChildren.appendVector(WTFMove((*childSum)->children.value));
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
                ASSERT(toNumericIdentity(aNumeric) == toNumericIdentity(get<T>(b)));
                auto result = executeMathOperation<Sum>(aNumeric.value, get<T>(b).value);
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
                bool canRemoveIfZero = isLength(id);

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
        return { };

    // If all the removal from merges leaves a single child, that means everything merged into the first child.
    if ((root.children.size() - childrenToRemoveFromMerges) == 1)
        return { WTFMove(root.children[0]) };

    auto combinedChildrenSize = root.children.size() - childrenToRemoveTotal;

    // If the new size is 0, we removed too much. Return a single 0 value of type `dimension` to keep things valid. A value of type `dimension` is returned because the only kind of node that can be removed is of type `dimension`.
    if (!combinedChildrenSize)
        return { makeChild(Dimension { .value = 0 }) };

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
                    return { };
                },
                [&](const auto&) -> std::optional<Child> {
                    return { WTFMove(root.children[i]) };
                }
            );
            if (replacement)
                return { WTFMove(*replacement) };
        }
    }

    Vector<Child> combinedChildren;
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

    return { };
}

static std::optional<Child> simplify(Negate& root)
{
    // This is almost identical to the Negate simplification in CSSCalcTree+Simplification.cpp with the following differences:
    // - It operates on Style::Calculation::Negate, not CSSCalc::Negate.
    // - It returns a Style::Calculation::Child, not CSSCalc::Child.
    // - It does not take SimplificationOptions.
    // FIXME: Find a way to share code with version in CSSCalcTree+Simplification.cpp.

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

            if (!std::ranges::all_of(a->children, isNumeric))
                return { };

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

            if (!std::ranges::all_of(a->children, isNumeric))
                return { };

            for (auto& child : a->children) {
                WTF::switchOn(child,
                    [&]<Numeric T>(T& child) { child.value = -child.value; },
                    [](auto&) { }
                );
            }

            return { Child { WTFMove(a) } };
        },
        [](auto&) -> std::optional<Child> {
            return { };
        }
    );
}

// MARK: - Tree building

// Specialized version of multiply that takes a Number node and Child node, rather than two Child nodes.
static Child multiplyAllowingAnyDepth(Number a, Child&& b)
{
    // NOTE: We merge steps 9.1. and 9.2, as they have significant overlap.

    // 9.1. For each of root’s children that are Product nodes, replace them with their children.
    //
    //   -- and --
    //
    // 9.2. If root has multiple children that are numbers (not percentages or dimensions), remove them and replace them with a single number containing the product of the removed nodes.

    Vector<Child> newChildren;
    Number numericProduct = a;

    auto processChild = [&newChildren, &numericProduct](Child& child) {
        if (auto* childValue = get_if<Number>(&child))
            numericProduct = Number { .value = childValue->value * numericProduct.value };
        else
            newChildren.append(WTFMove(child));
    };

    if (auto* childProduct = get_if<IndirectNode<Product>>(&b)) {
        for (auto& childProductChild : (*childProduct)->children)
            processChild(childProductChild);
    } else
        processChild(b);

    // If `newChildren` is empty, that means all the children were numbers and the product can be returned directly.
    if (newChildren.isEmpty())
        return makeChild(WTFMove(numericProduct));

    // 9.3. If root contains only two children, one of which is a number (not a percentage or dimension) and the other of which is a Sum whose children are all numeric values, multiply all of the Sum’s children by the number, then return the Sum.

    // We extend this step to include numeric and Invert children for the non-number child as an optimization taking advantage of step 9.4, but for the case where the check is cheaper.

    // NOTE: Since we just merged all numeric values into `numericProduct`, we know that the last child will be a singular `number` child (should we add it to newChildren). Therefore, we only need to check if there is one child and is a Sum (or Numeric or Invert).

    if (newChildren.size() == 1) {
        auto replacement = WTF::switchOn(newChildren[0],
            [&]<Numeric T>(T& numeric) -> std::optional<Child> {
                return makeChildWithValueBasedOn(numeric.value * numericProduct.value, numeric);
            },
            [&](IndirectNode<Sum>& sum) -> std::optional<Child> {
                if (!std::ranges::all_of(sum->children, isNumeric))
                    return { };

                for (auto& child : sum->children) {
                    WTF::switchOn(child,
                        [&]<Numeric T>(T& child) { child.value *= numericProduct.value; },
                        [](auto&) { }
                    );
                }

                return { Child { WTFMove(sum) } };
            },
            [&](IndirectNode<Invert>& invert) -> std::optional<Child> {
                return WTF::switchOn(invert->a,
                    [&]<Numeric T>(const T& child) -> std::optional<Child> {
                        return makeChildWithValueBasedOn(child.value * numericProduct.value, child);
                    },
                    [](const auto&) -> std::optional<Child> {
                        return { };
                    }
                );
            },
            [](auto&) -> std::optional<Child> {
                return { };
            }
        );

        if (replacement)
            return { WTFMove(*replacement) };
    }

    // If there was more than one child or no replacement was found, append the product from step 9.2 into the newChildren array.
    newChildren.append(makeChild(WTFMove(numericProduct)));

    return makeChild(Product { .children = WTFMove(newChildren) });
}

static Child negateAllowingAnyDepth(Child&& a)
{
    auto negate = Negate { .a = WTFMove(a) };

    if (auto replacement = simplify(negate))
        return WTFMove(*replacement);

    return makeChild(WTFMove(negate));
}

Child addAllowingAnyDepth(Child&& a, Child&& b)
{
    Vector<Child> sumChildren;
    sumChildren.append(WTFMove(a));
    sumChildren.append(WTFMove(b));
    auto sum = Sum { .children = WTFMove(sumChildren) };

    if (auto replacement = simplify(sum))
        return WTFMove(*replacement);

    return makeChild(WTFMove(sum));
}

Child subtractAllowingAnyDepth(Child&& a, Child&& b)
{
    return addAllowingAnyDepth(WTFMove(a), negateAllowingAnyDepth(WTFMove(b)));
}

Child blendAllowingAnyDepth(Child&& from, Child&& to, double progress)
{
    return addAllowingAnyDepth(
        multiplyAllowingAnyDepth(Number { .value = 1.0 - progress }, WTFMove(from)),
        multiplyAllowingAnyDepth(Number { .value = progress }, WTFMove(to))
    );
}

std::optional<Child> add(Child&& a, Child&& b)
{
    auto result = addAllowingAnyDepth(WTFMove(a), WTFMove(b));

    if (computeDepth(result) > maximumCalculationTreeDepth)
        return { };

    return result;
}

std::optional<Child> subtract(Child&& a, Child&& b)
{
    auto result = subtractAllowingAnyDepth(WTFMove(a), WTFMove(b));

    if (computeDepth(result) > maximumCalculationTreeDepth)
        return { };

    return result;
}

std::optional<Child> blend(Child&& from, Child&& to, double progress)
{
    auto result = blendAllowingAnyDepth(WTFMove(from), WTFMove(to), progress);

    if (computeDepth(result) > maximumCalculationTreeDepth)
        return { };

    return result;
}

} // namespace Calculation
} // namespace Style
} // namespace WebCore
