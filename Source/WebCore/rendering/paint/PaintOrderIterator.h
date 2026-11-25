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

#include "LayoutPoint.h"
#include "RenderBlock.h"
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBox;

namespace Paint {

// Walks a block subtree in paint order, or in reverse paint order for hit testing, without recursion.
//
// Each block that is painted by the generic block painting code is entered as a parent. The
// iterator reports entering and leaving it, so the consumer can set up and tear down per-block state
// such as the contents clip and the paint phase. Within a parent, it reports the block's own
// painting before and after its contents, and the contents themselves: inline content, or the
// children in paint order. Child blocks painted by the generic code are entered. Anything else
// (replaced content, tables, children of grid and flex containers) is an opaque item that the
// consumer paints with the existing per-renderer code. Floats are opaque items in tree order,
// or after the other children. Excluded children (fieldset legends, outside list markers) and
// skipped content are reported too, and the consumer decides what to do with them.
//
// The iterator only knows the tree and the geometry. Paint state such as PaintInfo belongs to
// the consumer.
class PaintOrderIterator {
public:
    enum class Event : uint8_t {
        EnterBlock,
        BeforeContents,
        InlineContent,
        Children,
        Child,
        AfterContents,
        LeaveBlock,
    };

    enum class Direction : bool { Forward, Reverse };
    enum class FloatOrder : bool { TreeOrder, AfterOtherChildren };

    static bool canIterate(const RenderBlock&);

    // The paint offset is the one RenderBlock::paint() receives, before adding the block's location.
    PaintOrderIterator(RenderBlock& root, const LayoutPoint& paintOffset, Direction = Direction::Forward, FloatOrder = FloatOrder::TreeOrder);

    bool atEnd() const { return m_parentStack.isEmpty(); }
    Event event() const { return parent().event; }

    // The current block and its paint offset (including its location).
    RenderBlock& block() const { return parent().block; }
    LayoutPoint paintOffset() const { return parent().paintOffset; }
    LayoutPoint scrolledOffset() const;
    // The block whose contents the current block is part of, or null for the root.
    RenderBlock* enclosingBlock() const;

    // The entered blocks, from the root (0) to the current one (depth() - 1).
    size_t depth() const { return m_parentStack.size(); }
    RenderBlock& blockAt(size_t index) const { return m_parentStack[index].block; }
    LayoutPoint paintOffsetAt(size_t index) const { return m_parentStack[index].paintOffset; }

    // For Event::Child, the child to paint and the offset to paint it at (before adding its location).
    RenderBox& child() const;
    LayoutPoint childPaintOffset() const;

    PaintOrderIterator& traverseNext();

    // From EnterBlock: continue after the block without reporting its contents or LeaveBlock.
    void skipBlock();
    // From the first of BeforeContents and AfterContents in the iteration direction: continue with the other one.
    void skipContents();
    // From the first of BeforeContents and AfterContents in the iteration direction: continue with LeaveBlock.
    void skipToLeave();

private:
    enum class Skip : uint8_t { None, Block, Contents, ToLeave };

    struct Parent {
        CheckedRef<RenderBlock> block;
        LayoutPoint paintOffset;
        CheckedPtr<RenderBox> child { };
        Event event { Event::EnterBlock };
        Skip skip { Skip::None };
        // With FloatOrder::AfterOtherChildren, the children are walked twice: once for the floats, once for the rest.
        bool isFloatsPass { false };
        bool hasChildForOtherPass { false };
    };

    const Parent& parent() const { return m_parentStack.last(); }
    Parent& parent() { return m_parentStack.last(); }

    Event firstSelfEvent() const { return m_direction == Direction::Forward ? Event::BeforeContents : Event::AfterContents; }
    Event lastSelfEvent() const { return m_direction == Direction::Forward ? Event::AfterContents : Event::BeforeContents; }
    RenderBox* firstChild(const RenderBlock&) const;
    RenderBox* nextChild(const RenderBox&) const;
    bool isInCurrentPass(const RenderBox&) const;

    void beginContents();
    void advanceToChild(CheckedPtr<RenderBox> candidate);
    void continueAfterChild();
    void popParent();

    Vector<Parent, 16> m_parentStack;
    const Direction m_direction;
    const FloatOrder m_floatOrder;
};

} // namespace Paint
} // namespace WebCore
