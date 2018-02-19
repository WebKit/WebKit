/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "RenderTreeBuilderBlock.h"

#include "RenderButton.h"
#include "RenderChildIterator.h"
#include "RenderFullScreen.h"
#include "RenderRuby.h"
#include "RenderRubyRun.h"
#include "RenderTextControl.h"

namespace WebCore {

static RenderBlock* continuationBefore(RenderBlock& parent, RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == &parent)
        return &parent;

    RenderBlock* nextToLast = &parent;
    RenderBlock* last = &parent;
    for (auto* current = downcast<RenderBlock>(parent.continuation()); current; current = downcast<RenderBlock>(current->continuation())) {
        if (beforeChild && beforeChild->parent() == current) {
            if (current->firstChild() == beforeChild)
                return last;
            return current;
        }

        nextToLast = last;
        last = current;
    }

    if (!beforeChild && !last->firstChild())
        return nextToLast;
    return last;
}

RenderTreeBuilder::Block::Block(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::Block::insertChild(RenderBlock& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (parent.continuation() && !parent.isAnonymousBlock())
        insertChildToContinuation(parent, WTFMove(child), beforeChild);
    else
        insertChildIgnoringContinuation(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::Block::insertChildToContinuation(RenderBlock& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    RenderBlock* flow = continuationBefore(parent, beforeChild);
    ASSERT(!beforeChild || is<RenderBlock>(*beforeChild->parent()));
    RenderBoxModelObject* beforeChildParent = nullptr;
    if (beforeChild)
        beforeChildParent = downcast<RenderBoxModelObject>(beforeChild->parent());
    else {
        RenderBoxModelObject* continuation = flow->continuation();
        if (continuation)
            beforeChildParent = continuation;
        else
            beforeChildParent = flow;
    }

    if (child->isFloatingOrOutOfFlowPositioned()) {
        beforeChildParent->addChildIgnoringContinuation(m_builder, WTFMove(child), beforeChild);
        return;
    }

    bool childIsNormal = child->isInline() || !child->style().columnSpan();
    bool bcpIsNormal = beforeChildParent->isInline() || !beforeChildParent->style().columnSpan();
    bool flowIsNormal = flow->isInline() || !flow->style().columnSpan();

    if (flow == beforeChildParent) {
        flow->addChildIgnoringContinuation(m_builder, WTFMove(child), beforeChild);
        return;
    }

    // The goal here is to match up if we can, so that we can coalesce and create the
    // minimal # of continuations needed for the inline.
    if (childIsNormal == bcpIsNormal) {
        beforeChildParent->addChildIgnoringContinuation(m_builder, WTFMove(child), beforeChild);
        return;
    }
    if (flowIsNormal == childIsNormal) {
        flow->addChildIgnoringContinuation(m_builder, WTFMove(child), nullptr); // Just treat like an append.
        return;
    }
    beforeChildParent->addChildIgnoringContinuation(m_builder, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::Block::insertChildIgnoringContinuation(RenderBlock& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() != &parent) {
        RenderElement* beforeChildContainer = beforeChild->parent();
        while (beforeChildContainer->parent() != &parent)
            beforeChildContainer = beforeChildContainer->parent();
        ASSERT(beforeChildContainer);

        if (beforeChildContainer->isAnonymous()) {
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!beforeChildContainer->isInline());

            // If the requested beforeChild is not one of our children, then this is because
            // there is an anonymous container within this object that contains the beforeChild.
            RenderElement* beforeChildAnonymousContainer = beforeChildContainer;
            if (beforeChildAnonymousContainer->isAnonymousBlock()
#if ENABLE(FULLSCREEN_API)
                // Full screen renderers and full screen placeholders act as anonymous blocks, not tables:
                || beforeChildAnonymousContainer->isRenderFullScreen()
                || beforeChildAnonymousContainer->isRenderFullScreenPlaceholder()
#endif
                ) {
                // Insert the child into the anonymous block box instead of here.
                if (child->isInline() || beforeChild->parent()->firstChild() != beforeChild)
                    m_builder.insertChild(*beforeChild->parent(), WTFMove(child), beforeChild);
                else
                    m_builder.insertChild(parent, WTFMove(child), beforeChild->parent());
                return;
            }

            ASSERT(beforeChildAnonymousContainer->isTable());

            if (child->isTablePart()) {
                // Insert into the anonymous table.
                m_builder.insertChild(*beforeChildAnonymousContainer, WTFMove(child), beforeChild);
                return;
            }

            beforeChild = m_builder.splitAnonymousBoxesAroundChild(parent, beforeChild);

            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(beforeChild->parent() == &parent);
        }
    }

    bool madeBoxesNonInline = false;

    // A block has to either have all of its children inline, or all of its children as blocks.
    // So, if our children are currently inline and a block child has to be inserted, we move all our
    // inline children into anonymous block boxes.
    if (parent.childrenInline() && !child->isInline() && !child->isFloatingOrOutOfFlowPositioned()) {
        // This is a block with inline content. Wrap the inline content in anonymous blocks.
        m_builder.makeChildrenNonInline(parent, beforeChild);
        madeBoxesNonInline = true;

        if (beforeChild && beforeChild->parent() != &parent) {
            beforeChild = beforeChild->parent();
            ASSERT(beforeChild->isAnonymousBlock());
            ASSERT(beforeChild->parent() == &parent);
        }
    } else if (!parent.childrenInline() && (child->isFloatingOrOutOfFlowPositioned() || child->isInline())) {
        // If we're inserting an inline child but all of our children are blocks, then we have to make sure
        // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
        // a new one is created and inserted into our list of children in the appropriate position.
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : parent.lastChild();

        if (afterChild && afterChild->isAnonymousBlock()) {
            m_builder.insertChild(downcast<RenderBlock>(*afterChild), WTFMove(child));
            return;
        }

        if (child->isInline()) {
            // No suitable existing anonymous box - create a new one.
            auto newBox = parent.createAnonymousBlock();
            auto& box = *newBox;
            parent.RenderBox::addChild(m_builder, WTFMove(newBox), beforeChild);
            m_builder.insertChild(box, WTFMove(child));
            return;
        }
    }

    parent.invalidateLineLayoutPath();

    parent.RenderBox::addChild(m_builder, WTFMove(child), beforeChild);

    if (madeBoxesNonInline && is<RenderBlock>(parent.parent()) && parent.isAnonymousBlock())
        removeLeftoverAnonymousBlock(parent);
    // parent object may be dead here
}

void RenderTreeBuilder::Block::childBecameNonInline(RenderBlock& parent, RenderElement&)
{
    m_builder.makeChildrenNonInline(parent);
    if (parent.isAnonymousBlock() && is<RenderBlock>(parent.parent()))
        removeLeftoverAnonymousBlock(parent);
    // parent may be dead here
}

void RenderTreeBuilder::Block::removeLeftoverAnonymousBlock(RenderBlock& anonymousBlock)
{
    ASSERT(anonymousBlock.isAnonymousBlock());
    ASSERT(!anonymousBlock.childrenInline());
    ASSERT(anonymousBlock.parent());

    if (anonymousBlock.continuation())
        return;

    auto* parent = anonymousBlock.parent();
    if (is<RenderButton>(*parent) || is<RenderTextControl>(*parent) || is<RenderRubyAsBlock>(*parent) || is<RenderRubyRun>(*parent))
        return;

    // FIXME: This should really just be a moveAllChilrenTo (see webkit.org/b/182495)
    anonymousBlock.moveAllChildrenToInternal(*parent);
    auto toBeDestroyed = parent->takeChildInternal(anonymousBlock);
    // anonymousBlock is dead here.
}

}
