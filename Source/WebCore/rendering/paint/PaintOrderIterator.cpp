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

#include "config.h"
#include "PaintOrderIterator.h"

#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"

namespace WebCore {
namespace Paint {

static bool isIterableBlockType(const RenderObject& renderer)
{
    // Blocks that the generic RenderBlock code paints and hit tests.
    switch (renderer.type()) {
    case RenderObject::Type::BlockFlow:
    case RenderObject::Type::FlexibleBox:
    case RenderObject::Type::Grid:
    case RenderObject::Type::ListItem:
    case RenderObject::Type::TableCaption:
    case RenderObject::Type::TableCell:
        return true;
    default:
        return false;
    }
}

bool PaintOrderIterator::canIterate(const RenderBlock& block)
{
    return isIterableBlockType(block);
}

PaintOrderIterator::PaintOrderIterator(RenderBlock& root, const LayoutPoint& paintOffset, Direction direction, FloatOrder floatOrder)
    : m_direction(direction)
    , m_floatOrder(floatOrder)
{
    ASSERT(canIterate(root));
    m_parentStack.append({ root, paintOffset + root.location() });
}

LayoutPoint PaintOrderIterator::scrolledOffset() const
{
    CheckedRef block = parent().block;
    auto offset = parent().paintOffset;
    offset.moveBy(-block->scrollPosition());
    return offset;
}

RenderBlock* PaintOrderIterator::enclosingBlock() const
{
    if (m_parentStack.size() < 2)
        return nullptr;
    return m_parentStack[m_parentStack.size() - 2].block.ptr();
}

RenderBox& PaintOrderIterator::child() const
{
    ASSERT(event() == Event::Child);
    return *parent().child;
}

LayoutPoint PaintOrderIterator::childPaintOffset() const
{
    ASSERT(event() == Event::Child);
    CheckedRef block = parent().block;
    return block->blockStartRelativeToPhysicalForChild(*parent().child, scrolledOffset());
}

void PaintOrderIterator::skipBlock()
{
    ASSERT(event() == Event::EnterBlock);
    parent().skip = Skip::Block;
}

void PaintOrderIterator::skipContents()
{
    ASSERT(event() == firstSelfEvent());
    parent().skip = Skip::Contents;
}

void PaintOrderIterator::skipToLeave()
{
    ASSERT(event() == firstSelfEvent());
    parent().skip = Skip::ToLeave;
}

void PaintOrderIterator::reenterBlock()
{
    ASSERT(event() == Event::LeaveBlock && isStackingContainer());
    parent().skip = Skip::Reenter;
}

void PaintOrderIterator::beginContents()
{
    CheckedRef block = parent().block;
    if (block->childrenInline()) {
        parent().event = Event::InlineContent;
        return;
    }
    if (!is<RenderBlockFlow>(block)) {
        advanceToGridOrFlexItem();
        return;
    }
    // Forward, the floats come in the second pass, in reverse in the first.
    parent().isFloatsPass = m_floatOrder == FloatOrder::AfterOtherChildren && m_direction == Direction::Reverse;
    advanceToChild(firstChild(block));
}

RenderBox* PaintOrderIterator::firstChild(const RenderBlock& block) const
{
    return m_direction == Direction::Forward ? block.firstChildBox() : block.lastChildBox();
}

RenderBox* PaintOrderIterator::nextChild(const RenderBox& child) const
{
    return m_direction == Direction::Forward ? child.nextSiblingBox() : child.previousSiblingBox();
}

bool PaintOrderIterator::isInCurrentPass(const RenderBox& child) const
{
    if (m_floatOrder == FloatOrder::TreeOrder)
        return true;
    return child.isFloating() == parent().isFloatsPass;
}

void PaintOrderIterator::advanceToChild(CheckedPtr<RenderBox> candidate)
{
    for (CheckedPtr child = WTF::move(candidate); child; child = nextChild(*child)) {
        if (child->hasSelfPaintingLayer())
            continue;
        if (!isInCurrentPass(*child)) {
            parent().hasChildForOtherPass = true;
            continue;
        }

        visitChild(*child);
        return;
    }
    if (m_floatOrder == FloatOrder::AfterOtherChildren) {
        bool firstPassWasFloats = m_direction == Direction::Reverse;
        // Most blocks have no floats, so there is often nothing for the second pass.
        if (parent().isFloatsPass == firstPassWasFloats && parent().hasChildForOtherPass) {
            parent().isFloatsPass = !firstPassWasFloats;
            CheckedRef block = parent().block;
            advanceToChild(firstChild(block));
            return;
        }
    }
    parent().child = nullptr;
    parent().event = lastSelfEvent();
}

// Grid and flex items in their containers' order, which can differ from the tree order.
static size_t gridOrFlexItemCount(const RenderBlock& container)
{
    if (CheckedPtr grid = dynamicDowncast<RenderGrid>(container)) {
        ASSERT(!grid->currentGrid().needsItemsPlacement());
        return std::ranges::size(grid->currentGrid().orderIterator().gridItems());
    }
    return downcast<RenderFlexibleBox>(container).flexLayout().flexItems().size();
}

static RenderBox* gridOrFlexItemAt(const RenderBlock& container, size_t index)
{
    if (CheckedPtr grid = dynamicDowncast<RenderGrid>(container))
        return &grid->currentGrid().orderIterator().gridItems()[index];
    return downcast<RenderFlexibleBox>(container).flexLayout().flexItems()[index].get();
}

void PaintOrderIterator::advanceToGridOrFlexItem()
{
    CheckedRef container = parent().block;
    auto count = gridOrFlexItemCount(container);
    while (parent().gridOrFlexItemPosition < count) {
        auto position = parent().gridOrFlexItemPosition++;
        CheckedPtr item = gridOrFlexItemAt(container, m_direction == Direction::Forward ? position : count - 1 - position);
        if (!item || item->hasSelfPaintingLayer())
            continue;
        visitChild(*item);
        return;
    }
    parent().child = nullptr;
    parent().event = lastSelfEvent();
}

void PaintOrderIterator::visitChild(RenderBox& child)
{
    parent().child = &child;

    CheckedPtr childBlock = dynamicDowncast<RenderBlock>(child);
    if (!childBlock || !isIterableBlockType(*childBlock)) {
        parent().event = Event::Child;
        return;
    }

    CheckedRef block = parent().block;
    auto childPaintOffset = block->blockStartRelativeToPhysicalForChild(child, scrolledOffset()) + toLayoutSize(child.location());
    // Floats and grid and flex items paint all their phases at once.
    bool isStackingContainer = child.isFloating() || !is<RenderBlockFlow>(block);
    // May reallocate the parent stack.
    m_parentStack.append({ *childBlock, childPaintOffset });
    parent().isStackingContainer = isStackingContainer;
}

void PaintOrderIterator::continueAfterChild()
{
    if (!is<RenderBlockFlow>(parent().block)) {
        advanceToGridOrFlexItem();
        return;
    }
    // The current child was either entered as a parent or reported as an opaque item.
    CheckedPtr child = parent().child;
    advanceToChild(child ? nextChild(*child) : nullptr);
}

void PaintOrderIterator::popParent()
{
    m_parentStack.removeLast();
    if (!m_parentStack.isEmpty())
        continueAfterChild();
}

PaintOrderIterator& PaintOrderIterator::traverseNext()
{
    // Forward: EnterBlock, BeforeContents, contents, AfterContents, LeaveBlock.
    // Reverse: EnterBlock, AfterContents, contents in reverse, BeforeContents, LeaveBlock.
    auto event = parent().event;
    if (event == Event::EnterBlock) {
        if (parent().skip == Skip::Block)
            popParent();
        else
            parent().event = firstSelfEvent();
        return *this;
    }
    if (event == firstSelfEvent()) {
        switch (std::exchange(parent().skip, Skip::None)) {
        case Skip::None:
            beginContents();
            break;
        case Skip::Contents:
            parent().event = lastSelfEvent();
            break;
        case Skip::ToLeave:
            parent().event = Event::LeaveBlock;
            break;
        case Skip::Block:
        case Skip::Reenter:
            ASSERT_NOT_REACHED();
            break;
        }
        return *this;
    }
    if (event == lastSelfEvent()) {
        parent().event = Event::LeaveBlock;
        return *this;
    }
    switch (event) {
    case Event::InlineContent:
        parent().event = lastSelfEvent();
        break;
    case Event::Child:
        continueAfterChild();
        break;
    case Event::LeaveBlock:
        if (parent().skip == Skip::Reenter) {
            auto& reentered = parent();
            reentered = { reentered.block, reentered.paintOffset };
            reentered.isStackingContainer = true;
            break;
        }
        popParent();
        break;
    case Event::EnterBlock:
    case Event::BeforeContents:
    case Event::AfterContents:
        ASSERT_NOT_REACHED();
        break;
    }
    return *this;
}

} // namespace Paint
} // namespace WebCore
