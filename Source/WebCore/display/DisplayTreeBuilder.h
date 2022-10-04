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

#pragma once

#include "DisplayBoxFactory.h"
#include <wtf/IsoMalloc.h>

#if ENABLE(TREE_DEBUGGING)
#include <wtf/Forward.h>
#endif

namespace WebCore {

class LayoutSize;

namespace Layout {
class Box;
class BoxGeometry;
class ElementBox;
class LayoutState;
}

namespace Display {

class Box;
class ContainerBox;
class PositioningContext;
class StackingItem;
class Tree;
struct BuildingState;

class TreeBuilder {
    friend class BoxFactory;
public:
    explicit TreeBuilder(float pixelSnappingFactor);
    ~TreeBuilder();

    std::unique_ptr<Tree> build(const Layout::LayoutState&);

private:
    struct InsertionPosition {
        ContainerBox& container;
        Box* currentChild { nullptr };
    };

    void recursiveBuildDisplayTree(const Layout::LayoutState&, const Layout::Box&, InsertionPosition&);
    void buildInlineDisplayTree(const Layout::LayoutState&, const Layout::ElementBox&, InsertionPosition&);

    enum class WillTraverseDescendants { Yes, No };
    StackingItem* insertIntoTree(std::unique_ptr<Box>&&, InsertionPosition&, WillTraverseDescendants);
    void insert(std::unique_ptr<Box>&&, InsertionPosition&) const;

    void accountForBoxPaintingExtent(const Box&);

    void pushStateForBoxDescendants(const Layout::ElementBox&, const Layout::BoxGeometry&, const ContainerBox&, StackingItem*);
    void popState(const BoxModelBox& currentBox);

    void didAppendNonContainerStackingItem(StackingItem&);

    Tree& tree() const { return *m_tree; }

    BuildingState& currentState();
    const PositioningContext& positioningContext();

    BoxFactory m_boxFactory;
    RootBackgroundPropagation m_rootBackgroundPropgation { RootBackgroundPropagation::None };

    std::unique_ptr<Tree> m_tree;
    std::unique_ptr<Vector<BuildingState>> m_stateStack;
};

#if ENABLE(TREE_DEBUGGING)
String displayTreeAsText(const Box&);
void showDisplayTree(const Box&);

String displayTreeAsText(const StackingItem&);
void showDisplayTree(const StackingItem&);
#endif

} // namespace Display
} // namespace WebCore

