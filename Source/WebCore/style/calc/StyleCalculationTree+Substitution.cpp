/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "StyleCalculationTree+Substitution.h"

#include "StyleCalculationTree+Copy.h"
#include "StyleCalculationTree+Traversal.h"
#include "StyleCalculationTree.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace Style {
namespace Calculation {

static constexpr size_t maximumSubstitutionTreeDepth = 128;

// MARK: - Leaf mapping
//
// Replaces a leaf node with the result of the functor.

template<typename F> static auto mapLeaves(const std::optional<Child>&, const F&) -> std::optional<Child>;
template<typename F> static auto mapLeaves(const Random::Fixed&, const F&) -> Random::Fixed;
template<typename F> static auto mapLeaves(const CalcMix::Item&, const F&) -> CalcMix::Item;
template<typename F> static auto mapLeaves(const Vector<CalcMix::Item>&, const F&) -> Vector<CalcMix::Item>;
template<typename F> static auto mapLeaves(const ChildOrNone&, const F&) -> ChildOrNone;
template<typename F> static auto mapLeaves(const Children&, const F&) -> Children;
template<typename F> static auto mapLeaves(const Child&, const F&) -> Child;
template<Leaf Op, typename F> static auto mapLeaves(const Op&, const F&) -> Child;
template<typename Op, typename F> static auto mapLeaves(const IndirectNode<Op>&, const F&) -> Child;

template<typename F> std::optional<Child> mapLeaves(const std::optional<Child>& root, const F& functor)
{
    if (root)
        return mapLeaves(*root, functor);
    return { };
}

template<typename F> Random::Fixed mapLeaves(const Random::Fixed& root, const F&)
{
    return root;
}

template<typename F> CalcMix::Item mapLeaves(const CalcMix::Item& item, const F& functor)
{
    return { mapLeaves(item.value, functor), item.weight };
}

template<typename F> Vector<CalcMix::Item> mapLeaves(const Vector<CalcMix::Item>& children, const F& functor)
{
    return WTF::map(children, [&](const auto& child) {
        return mapLeaves(child, functor);
    });
}

template<typename F> ChildOrNone mapLeaves(const ChildOrNone& root, const F& functor)
{
    return WTF::switchOn(root,
        [&](const CSS::Keyword::None& none) { return ChildOrNone { none }; },
        [&](const Child& child) { return ChildOrNone { mapLeaves(child, functor) }; }
    );
}

template<typename F> Children mapLeaves(const Children& children, const F& functor)
{
    return WTF::map(children, [&](const auto& child) {
        return mapLeaves(child, functor);
    });
}

template<typename F> Child mapLeaves(const Child& root, const F& functor)
{
    return WTF::switchOn(root, [&](const auto& root) {
        return mapLeaves(root, functor);
    });
}

template<Leaf Op, typename F> Child mapLeaves(const Op& root, const F& functor)
{
    return functor(root);
}

template<typename Op, typename F> Child mapLeaves(const IndirectNode<Op>& root, const F& functor)
{
    return makeChild(WTF::apply([&](const auto& ...x) {
        return Op { mapLeaves(x, functor)... };
    }, *root));
}

// MARK: - Counting

static size_t accumulateOverChildren(const Child& root, const auto& functor)
{
    size_t total = 0;
    forAllChildren(root, WTF::makeVisitor(
        [&](const std::optional<Child>& child) {
            if (child)
                total += functor(*child);
        },
        [&](const Child& child) {
            total += functor(child);
        },
        [&](const ChildOrNone& childOrNone) {
            if (childOrNone.holdsAlternative<Child>())
                total += functor(get<Child>(childOrNone));
        },
        [&](const auto&) { }
    ));
    return total;
}

static size_t countSizeLeaves(const Child& root)
{
    if (root.holdsAlternative<Size>())
        return 1;
    return accumulateOverChildren(root, [](const Child& child) {
        return countSizeLeaves(child);
    });
}

// MARK: - Substitution
//
// Preparing a calc-size() for interpolation uses these two rewrites.
// https://drafts.csswg.org/css-values-5/#simplifying-calc-size

std::optional<Tree> substituteSize(const Tree& tree, const Child& insertion)
{
    auto occurrences = countSizeLeaves(tree.root);
    if (!occurrences)
        return Tree { .root = copy(tree.root) };

    // Avoid constructing an oversized tree in the first place.
    auto predictedNodeCount = computeNodeCount(tree.root) - occurrences + occurrences * computeNodeCount(insertion);
    if (predictedNodeCount > maximumCalcSizeNodeCount)
        return { };

    auto result = Tree { .root = mapLeaves(tree.root, [&](const auto& leaf) -> Child {
        if constexpr (std::same_as<std::remove_cvref_t<decltype(leaf)>, Size>)
            return copy(insertion);
        else
            return Child { leaf };
    }) };

    if (computeDepth(result) > maximumSubstitutionTreeDepth)
        return { };
    return result;
}

Tree dePercentify(const Tree& tree)
{
    return Tree { .root = mapLeaves(tree.root, [&](const auto& leaf) -> Child {
        if constexpr (std::same_as<std::remove_cvref_t<decltype(leaf)>, Percentage>)
            return multiply(Child { Size { } }, number(leaf.value / 100.0));
        else
            return Child { leaf };
    }) };
}

// MARK: - Detection

template<typename LeafType> static bool contains(const Child& root)
{
    if (root.holdsAlternative<LeafType>())
        return true;

    bool found = false;
    forAllChildren(root, WTF::makeVisitor(
        [&](const std::optional<Child>& child) {
            if (child)
                found = found || contains<LeafType>(*child);
        },
        [&](const Child& child) {
            found = found || contains<LeafType>(child);
        },
        [&](const ChildOrNone& childOrNone) {
            if (childOrNone.holdsAlternative<Child>())
                found = found || contains<LeafType>(get<Child>(childOrNone));
        },
        [&](const auto&) { }
    ));
    return found;
}

bool containsSize(const Tree& tree)
{
    return contains<Size>(tree.root);
}

bool containsPercentage(const Tree& tree)
{
    return contains<Percentage>(tree.root);
}

} // namespace Calculation
} // namespace Style
} // namespace WebCore
