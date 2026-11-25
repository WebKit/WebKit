/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2016 Google Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "LayoutPoint.h"
#include "PaintPhase.h"
#include "RenderBlock.h"
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBox;
struct PaintInfo;

namespace LayoutIntegration {
class LineLayout;
}

namespace Paint {

// The static functions paint one block. paint() walks the block's subtree with PaintOrderIterator
// when the subtree can be iterated, using a BlockPainter instance for the walk.
class BlockPainter {
public:
    static void paint(RenderBlock&, PaintInfo&, const LayoutPoint& paintOffset);
    static void paintObject(RenderBlock&, PaintInfo&, const LayoutPoint& paintOffset);

    enum class ContentsAction : uint8_t { Paint, Skip, Stop };
    // The parts of paintObject() before and after the contents.
    static ContentsAction paintBeforeContents(RenderBlock&, PaintInfo&, const LayoutPoint& paintOffset);
    static void paintAfterContents(RenderBlock&, PaintInfo&, const LayoutPoint& paintOffset);

    static void paintInlineChildren(RenderBlock&, PaintInfo&, const LayoutPoint& scrolledOffset);
    static void paintChildren(RenderBlock&, PaintInfo&, const LayoutPoint& scrolledOffset, PaintInfo& paintInfoForChildren);

    enum PaintBlockType { PaintAsBlock, PaintAsInlineBlock };
    static void paintChild(RenderBlock&, PaintInfo&, const LayoutPoint& scrolledOffset, RenderBox&, PaintInfo& paintInfoForChild, PaintBlockType = PaintAsBlock);
    static void paintFloat(PaintInfo&, RenderBox&, const LayoutPoint& floatBoxLocation);

    static void paintBlockLevelContentInInline(RenderBlock&, PaintInfo&, const LayoutPoint& paintOffset);

    static PaintInfo paintInfoForBlockChildren(const RenderBlock&, const PaintInfo&);
    static void adjustPaintInfoForBlockChildren(const RenderBlock&, PaintInfo&);

    // Whether the containing block skips the child when painting its children in normal flow, because it is painted elsewhere or not at all.
    static bool isExcludedFromNormalFlowPainting(const RenderBox&);

    static bool isNonPaintingTraversalPhase(PaintPhase);
    static bool isVisualContentClippedOut(const RenderBlock&, const PaintInfo&, const LayoutPoint& adjustedPaintOffset);
    static void paintOverflowControls(const RenderBlock&, const PaintInfo&, PaintPhase, const LayoutPoint& adjustedPaintOffset);

private:
    BlockPainter(RenderBlock& root, PaintInfo&, const LayoutPoint& paintOffset);

    void paintSubtree();

    static LayoutPoint scrolledPaintOffset(const RenderBlock&, const LayoutPoint& paintOffset);
    static void paintContents(RenderBlock&, PaintInfo&, const LayoutPoint& scrolledOffset);
    static void paintFloatsAndBlockLevelContentInInline(RenderBlock&, PaintInfo&, const LayoutPoint& scrolledOffset, const LayoutIntegration::LineLayout&);
    static void paintColumnRules(RenderBlock&, PaintInfo&, const LayoutPoint& scrolledOffset);
    static void paintExcludedChildrenInBorder(RenderBlock&, PaintInfo&, const LayoutPoint& paintOffset);
    static void paintCarets(RenderBlock&, PaintInfo&, const LayoutPoint& paintOffset);
    static void paintCaret(RenderBlock&, PaintInfo&, const LayoutPoint& paintOffset, CaretType);

    // The parts of the paint info that adjustPaintInfoForBlockChildren() changes.
    struct PaintInfoState {
        PaintPhase phase;
        CheckedPtr<RenderObject> subtreePaintRoot;
        OptionSet<PaintBehavior> paintBehavior;
    };
    struct Parent {
        // The paint info state of the block's parent, restored by popParent(). Unset for the root.
        std::optional<PaintInfoState> savedPaintInfoState;
        // pushContentsClip() can change the phase, so remember the one the block started with.
        PaintPhase phase;
        bool pushedClip { false };
    };

    // Adjusts the paint info for the children of the parent block, if there is one.
    void pushParent(const RenderBlock* parentBlock);
    void popParent();
    Parent& parent() LIFETIME_BOUND { return m_parentStack.last(); }

    const CheckedRef<RenderBlock> m_root;
    PaintInfo& m_paintInfo;
    const LayoutPoint m_paintOffset;
    Vector<Parent, 16> m_parentStack;
};

} // namespace Paint
} // namespace WebCore
