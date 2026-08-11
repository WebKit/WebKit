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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderTreeOrder.h"

#include "RenderElement.h"
#include "RenderObject.h"

namespace WebCore {

static size_t renderTreeDepth(const RenderObject& renderer)
{
    size_t depth = 0;
    CheckedPtr ancestor = &renderer;
    while ((ancestor = ancestor->parent()))
        ++depth;
    return depth;
}

struct AncestorAndChildren {
    const CheckedPtr<const RenderObject> commonAncestor;
    const CheckedPtr<const RenderObject> distinctAncestorA;
    const CheckedPtr<const RenderObject> distinctAncestorB;
};

static AncestorAndChildren commonInclusiveAncestorAndChildren(const RenderObject& a, const RenderObject& b)
{
    // This check isn't needed for correctness, but it is cheap and likely to be
    // common enough to be worth optimizing so we don't have to walk to the root.
    if (&a == &b)
        return { &a, nullptr, nullptr };

    auto [depthA, depthB] = std::make_tuple(renderTreeDepth(a), renderTreeDepth(b));
    auto [x, y, difference] = depthA >= depthB
    ? std::make_tuple(CheckedPtr<const RenderObject> { a }, CheckedPtr<const RenderObject> { b }, depthA - depthB)
    : std::make_tuple(CheckedPtr<const RenderObject> { b }, CheckedPtr<const RenderObject> { a }, depthB - depthA);

    CheckedPtr<const RenderObject> distinctAncestorA = nullptr;
    for (decltype(difference) i = 0; i < difference; ++i) {
        distinctAncestorA = x;
        x = x->parent();
    }

    CheckedPtr<const RenderObject>  distinctAncestorB = nullptr;
    while (x != y) {
        distinctAncestorA = x;
        distinctAncestorB = y;
        x = x->parent();
        y = y->parent();
    }

    if (depthA < depthB)
        std::swap(distinctAncestorA, distinctAncestorB);

    return { x, distinctAncestorA, distinctAncestorB };
}

static bool isSiblingSubsequent(const RenderObject& a, const RenderObject& b)
{
    ASSERT(a.parent());
    ASSERT(a.parent() == b.parent());
    ASSERT(&a != &b);

    for (CheckedPtr sibling = &a; sibling; sibling = sibling->nextSibling()) {
        if (sibling == &b)
            return true;
    }

    return false;
}

std::partial_ordering renderTreeOrder(const RenderObject& a, const RenderObject& b)
{
    if (&a == &b)
        return std::partial_ordering::equivalent;
    auto result = commonInclusiveAncestorAndChildren(a, b);
    if (!result.commonAncestor)
        return std::partial_ordering::unordered;
    if (!result.distinctAncestorA)
        return std::partial_ordering::less;
    if (!result.distinctAncestorB)
        return std::partial_ordering::greater;
    return isSiblingSubsequent(*result.distinctAncestorA, *result.distinctAncestorB) ? std::partial_ordering::less : std::partial_ordering::greater;
}

} // namespace WebCore
