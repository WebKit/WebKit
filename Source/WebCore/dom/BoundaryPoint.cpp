/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BoundaryPoint.h"
#include "ContainerNode.h"

namespace WebCore {

template PartialOrdering treeOrder<Tree>(const BoundaryPoint&, const BoundaryPoint&);
template PartialOrdering treeOrder< ShadowIncludingTree >(const BoundaryPoint&, const BoundaryPoint&);
template PartialOrdering treeOrder<ComposedTree>(const BoundaryPoint&, const BoundaryPoint&);

std::optional<BoundaryPoint> makeBoundaryPointBeforeNode(Node& node)
{
    auto parent = node.parentNode();
    if (!parent)
        return std::nullopt;
    return BoundaryPoint { *parent, node.computeNodeIndex() };
}

std::optional<BoundaryPoint> makeBoundaryPointAfterNode(Node& node)
{
    auto parent = node.parentNode();
    if (!parent)
        return std::nullopt;
    return BoundaryPoint { *parent, node.computeNodeIndex() + 1 };
}

static bool isOffsetBeforeChild(ContainerNode& container, unsigned offset, Node& child)
{
    if (!offset)
        return true;
    // If the container is not the parent, the child is part of a shadow tree, which we sort between offset 0 and offset 1.
    if (child.parentNode() != &container)
        return false;
    unsigned currentOffset = 0;
    for (auto currentChild = container.firstChild(); currentChild && currentChild != &child; currentChild = currentChild->nextSibling()) {
        if (offset <= ++currentOffset)
            return true;
    }
    return false;
}

// FIXME: Once we move to C++20, replace with the C++20 <=> operator.
// FIXME: This could return std::strong_ordering if we had that, or the equivalent.
static PartialOrdering order(unsigned a, unsigned b)
{
    if (a < b)
        return PartialOrdering::less;
    if (a > b)
        return PartialOrdering::greater;
    return PartialOrdering::equivalent;
}

template<TreeType treeType> PartialOrdering treeOrder(const BoundaryPoint& a, const BoundaryPoint& b)
{
    if (a.container.ptr() == b.container.ptr())
        return order(a.offset, b.offset);

    for (auto ancestor = b.container.ptr(); ancestor; ) {
        auto nextAncestor = parent<treeType>(*ancestor);
        if (nextAncestor == a.container.ptr())
            return isOffsetBeforeChild(*nextAncestor, a.offset, *ancestor) ? PartialOrdering::less : PartialOrdering::greater;
        ancestor = nextAncestor;
    }

    for (auto ancestor = a.container.ptr(); ancestor; ) {
        auto nextAncestor = parent<treeType>(*ancestor);
        if (nextAncestor == b.container.ptr())
            return isOffsetBeforeChild(*nextAncestor, b.offset, *ancestor) ? PartialOrdering::greater : PartialOrdering::less;
        ancestor = nextAncestor;
    }

    return treeOrder<treeType>(a.container, b.container);
}

PartialOrdering treeOrderForTesting(TreeType type, const BoundaryPoint& a, const BoundaryPoint& b)
{
    switch (type) {
    case Tree:
        return treeOrder<Tree>(a, b);
    case ShadowIncludingTree:
        return treeOrder<ShadowIncludingTree>(a, b);
    case ComposedTree:
        return treeOrder<ComposedTree>(a, b);
    }
    ASSERT_NOT_REACHED();
    return PartialOrdering::unordered;
}

TextStream& operator<<(TextStream& stream, const BoundaryPoint& boundaryPoint)
{
    TextStream::GroupScope scope(stream);
    stream << "BoundaryPoint ";
    stream.dumpProperty("node", boundaryPoint.container->debugDescription());
    stream.dumpProperty("offset", boundaryPoint.offset);
    return stream;
}

}
