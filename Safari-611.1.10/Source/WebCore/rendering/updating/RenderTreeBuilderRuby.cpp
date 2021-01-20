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
#include "RenderTreeBuilderRuby.h"

#include "RenderAncestorIterator.h"
#include "RenderRuby.h"
#include "RenderRubyBase.h"
#include "RenderRubyRun.h"
#include "RenderTreeBuilder.h"
#include "RenderTreeBuilderBlock.h"
#include "RenderTreeBuilderBlockFlow.h"
#include "RenderTreeBuilderInline.h"

namespace WebCore {

static inline RenderRubyRun& findRubyRunParent(RenderObject& child)
{
    return *lineageOfType<RenderRubyRun>(child).first();
}

static inline bool isAnonymousRubyInlineBlock(const RenderObject* object)
{
    ASSERT(!object
        || !isRuby(object->parent())
        || is<RenderRubyRun>(*object)
        || (object->isInline() && (object->isBeforeContent() || object->isAfterContent()))
        || (object->isAnonymous() && is<RenderBlock>(*object) && object->style().display() == DisplayType::InlineBlock)
        || object->isRenderMultiColumnFlow()
        || object->isRenderMultiColumnSet());

    return object
        && isRuby(object->parent())
        && is<RenderBlock>(*object)
        && !is<RenderRubyRun>(*object);
}

static inline bool isRubyBeforeBlock(const RenderObject* object)
{
    return isAnonymousRubyInlineBlock(object)
        && !object->previousSibling()
        && downcast<RenderBlock>(*object).firstChild()
        && downcast<RenderBlock>(*object).firstChild()->style().styleType() == PseudoId::Before;
}

static inline bool isRubyAfterBlock(const RenderObject* object)
{
    return isAnonymousRubyInlineBlock(object)
        && !object->nextSibling()
        && downcast<RenderBlock>(*object).firstChild()
        && downcast<RenderBlock>(*object).firstChild()->style().styleType() == PseudoId::After;
}

#if ASSERT_ENABLED
static inline bool isRubyChildForNormalRemoval(const RenderObject& object)
{
    return object.isRubyRun()
    || object.isBeforeContent()
    || object.isAfterContent()
    || object.isRenderMultiColumnFlow()
    || object.isRenderMultiColumnSet()
    || isAnonymousRubyInlineBlock(&object);
}
#endif // ASSERT_ENABLED

static inline RenderBlock* rubyBeforeBlock(const RenderElement* ruby)
{
    RenderObject* child = ruby->firstChild();
    return isRubyBeforeBlock(child) ? downcast<RenderBlock>(child) : nullptr;
}

static inline RenderBlock* rubyAfterBlock(const RenderElement* ruby)
{
    RenderObject* child = ruby->lastChild();
    return isRubyAfterBlock(child) ? downcast<RenderBlock>(child) : nullptr;
}

static auto createAnonymousRubyInlineBlock(RenderObject& ruby)
{
    auto newBlock = createRenderer<RenderBlockFlow>(ruby.document(), RenderStyle::createAnonymousStyleWithDisplay(ruby.style(), DisplayType::InlineBlock));
    newBlock->initializeStyle();
    return newBlock;
}

static RenderRubyRun* lastRubyRun(const RenderElement* ruby)
{
    RenderObject* child = ruby->lastChild();
    if (child && !is<RenderRubyRun>(*child))
        child = child->previousSibling();
    if (!is<RenderRubyRun>(child)) {
        ASSERT(!child || child->isBeforeContent() || child == rubyBeforeBlock(ruby) || child->isRenderMultiColumnFlow() || child->isRenderMultiColumnSet());
        return nullptr;
    }
    return downcast<RenderRubyRun>(child);
}

RenderTreeBuilder::Ruby::Ruby(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::Ruby::moveInlineChildren(RenderRubyBase& from, RenderRubyBase& to, RenderObject* beforeChild)
{
    ASSERT(from.childrenInline());

    if (!from.firstChild())
        return;

    RenderBlock* toBlock = nullptr;
    if (to.childrenInline()) {
        // The standard and easy case: move the children into the target base
        toBlock = &to;
    } else {
        // We need to wrap the inline objects into an anonymous block.
        // If toBase has a suitable block, we re-use it, otherwise create a new one.
        auto* lastChild = to.lastChild();
        if (lastChild && lastChild->isAnonymousBlock() && lastChild->childrenInline())
            toBlock = downcast<RenderBlock>(lastChild);
        else {
            auto newToBlock = to.createAnonymousBlock();
            toBlock = newToBlock.get();
            m_builder.attachToRenderElementInternal(to, WTFMove(newToBlock));
        }
    }
    ASSERT(toBlock);
    // Move our inline children into the target block we determined above.
    m_builder.moveChildren(from, *toBlock, from.firstChild(), beforeChild, RenderTreeBuilder::NormalizeAfterInsertion::No);
}

void RenderTreeBuilder::Ruby::moveBlockChildren(RenderRubyBase& from, RenderRubyBase& to, RenderObject* beforeChild)
{
    ASSERT(!from.childrenInline());

    if (!from.firstChild())
        return;

    if (to.childrenInline())
        m_builder.makeChildrenNonInline(to);

    // If an anonymous block would be put next to another such block, then merge those.
    auto* firstChildHere = from.firstChild();
    auto* lastChildThere = to.lastChild();
    if (firstChildHere->isAnonymousBlock() && firstChildHere->childrenInline()
        && lastChildThere && lastChildThere->isAnonymousBlock() && lastChildThere->childrenInline()) {
        auto* anonBlockHere = downcast<RenderBlock>(firstChildHere);
        auto* anonBlockThere = downcast<RenderBlock>(lastChildThere);
        m_builder.moveAllChildren(*anonBlockHere, *anonBlockThere, RenderTreeBuilder::NormalizeAfterInsertion::Yes);
        anonBlockHere->deleteLines();
        m_builder.destroy(*anonBlockHere);
    }
    // Move all remaining children normally.
    m_builder.moveChildren(from, to, from.firstChild(), beforeChild, RenderTreeBuilder::NormalizeAfterInsertion::No);
}

void RenderTreeBuilder::Ruby::moveChildren(RenderRubyBase& from, RenderRubyBase& to)
{
    moveChildrenInternal(from, to);
    from.addFloatsToNewParent(to);
}

void RenderTreeBuilder::Ruby::moveChildrenInternal(RenderRubyBase& from, RenderRubyBase& to, RenderObject* beforeChild)
{
    // This function removes all children that are before (!) beforeChild
    // and appends them to toBase.
    if (beforeChild && beforeChild->parent() != &from)
        beforeChild = m_builder.splitAnonymousBoxesAroundChild(from, *beforeChild);

    if (from.childrenInline())
        moveInlineChildren(from, to, beforeChild);
    else
        moveBlockChildren(from, to, beforeChild);

    from.setNeedsLayoutAndPrefWidthsRecalc();
    to.setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderTreeBuilder::Ruby::attach(RenderRubyRun& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (child->isRubyText()) {
        if (!beforeChild) {
            // RenderRuby has already ascertained that we can add the child here.
            ASSERT(!parent.hasRubyText());
            // prepend ruby texts as first child
            m_builder.blockFlowBuilder().attach(parent, WTFMove(child), parent.firstChild());
            return;
        }
        if (beforeChild->isRubyText()) {
            // New text is inserted just before another.
            // In this case the new text takes the place of the old one, and
            // the old text goes into a new run that is inserted as next sibling.
            ASSERT(beforeChild->parent() == &parent);
            RenderElement* ruby = parent.parent();
            ASSERT(isRuby(ruby));
            auto newRun = RenderRubyRun::staticCreateRubyRun(ruby);
            auto& run = *newRun;
            m_builder.attach(*ruby, WTFMove(newRun), parent.nextSibling());
            // Add the new ruby text and move the old one to the new run
            // Note: Doing it in this order and not using RenderRubyRun's methods,
            // in order to avoid automatic removal of the ruby run in case there is no
            // other child besides the old ruby text.
            m_builder.blockFlowBuilder().attach(parent, WTFMove(child), beforeChild);
            auto takenBeforeChild = m_builder.blockBuilder().detach(parent, *beforeChild);

            m_builder.attach(run, WTFMove(takenBeforeChild));
            return;
        }
        if (parent.hasRubyBase()) {
            // Insertion before a ruby base object.
            // In this case we need insert a new run before the current one and split the base.
            RenderElement* ruby = parent.parent();
            auto newRun = RenderRubyRun::staticCreateRubyRun(ruby);
            auto& run = *newRun;
            m_builder.attach(*ruby, WTFMove(newRun), &parent);
            m_builder.attach(run, WTFMove(child));
            moveChildrenInternal(rubyBaseSafe(parent), rubyBaseSafe(run), beforeChild);
        }
        return;
    }
    // child is not a text -> insert it into the base
    // (append it instead if beforeChild is the ruby text)
    if (beforeChild && beforeChild->isRubyText())
        beforeChild = nullptr;
    m_builder.attach(rubyBaseSafe(parent), WTFMove(child), beforeChild);
}

RenderElement& RenderTreeBuilder::Ruby::findOrCreateParentForChild(RenderRubyAsBlock& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    // Insert :before and :after content before/after the RenderRubyRun(s)
    if (child.isBeforeContent()) {
        // Add generated inline content normally
        if (child.isInline())
            return parent;
        // Wrap non-inline content in an anonymous inline-block.
        auto* beforeBlock = rubyBeforeBlock(&parent);
        if (!beforeBlock) {
            auto newBlock = createAnonymousRubyInlineBlock(parent);
            beforeBlock = newBlock.get();
            m_builder.blockFlowBuilder().attach(parent, WTFMove(newBlock), parent.firstChild());
        }
        beforeChild = nullptr;
        return *beforeBlock;
    }

    if (child.isAfterContent()) {
        // Add generated inline content normally
        if (child.isInline())
            return parent;
        // Wrap non-inline content with an anonymous inline-block.
        auto* afterBlock = rubyAfterBlock(&parent);
        if (!afterBlock) {
            auto newBlock = createAnonymousRubyInlineBlock(parent);
            afterBlock = newBlock.get();
            m_builder.blockFlowBuilder().attach(parent, WTFMove(newBlock), nullptr);
        }
        beforeChild = nullptr;
        return *afterBlock;
    }

    // If the child is a ruby run, just add it normally.
    if (child.isRubyRun())
        return parent;

    if (beforeChild && !parent.isAfterContent(beforeChild)) {
        // insert child into run
        ASSERT(!beforeChild->isRubyRun());
        auto* run = beforeChild->parent();
        while (run && !run->isRubyRun())
            run = run->parent();
        if (run)
            return *run;
        ASSERT_NOT_REACHED(); // beforeChild should always have a run as parent!
        // Emergency fallback: fall through and just append.
    }

    // If the new child would be appended, try to add the child to the previous run
    // if possible, or create a new run otherwise.
    // (The RenderRubyRun object will handle the details)
    auto* lastRun = lastRubyRun(&parent);
    if (!lastRun || lastRun->hasRubyText()) {
        auto newRun = RenderRubyRun::staticCreateRubyRun(&parent);
        lastRun = newRun.get();
        m_builder.blockFlowBuilder().attach(parent, WTFMove(newRun), beforeChild);
    }
    beforeChild = nullptr;
    return *lastRun;
}

RenderElement& RenderTreeBuilder::Ruby::findOrCreateParentForChild(RenderRubyAsInline& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    // Insert :before and :after content before/after the RenderRubyRun(s)
    if (child.isBeforeContent()) {
        // Add generated inline content normally
        if (child.isInline())
            return parent;
        // Wrap non-inline content with an anonymous inline-block.
        auto* beforeBlock = rubyBeforeBlock(&parent);
        if (!beforeBlock) {
            auto newBlock = createAnonymousRubyInlineBlock(parent);
            beforeBlock = newBlock.get();
            m_builder.inlineBuilder().attach(parent, WTFMove(newBlock), parent.firstChild());
        }
        beforeChild = nullptr;
        return *beforeBlock;
    }

    if (child.isAfterContent()) {
        // Add generated inline content normally
        if (child.isInline())
            return parent;
        // Wrap non-inline content with an anonymous inline-block.
        auto* afterBlock = rubyAfterBlock(&parent);
        if (!afterBlock) {
            auto newBlock = createAnonymousRubyInlineBlock(parent);
            afterBlock = newBlock.get();
            m_builder.inlineBuilder().attach(parent, WTFMove(newBlock), nullptr);
        }
        beforeChild = nullptr;
        return *afterBlock;
    }

    // If the child is a ruby run, just add it normally.
    if (child.isRubyRun())
        return parent;

    if (beforeChild && !parent.isAfterContent(beforeChild)) {
        // insert child into run
        ASSERT(!beforeChild->isRubyRun());
        auto* run = beforeChild->parent();
        while (run && !run->isRubyRun())
            run = run->parent();
        if (run)
            return *run;
        ASSERT_NOT_REACHED(); // beforeChild should always have a run as parent!
        // Emergency fallback: fall through and just append.
    }

    // If the new child would be appended, try to add the child to the previous run
    // if possible, or create a new run otherwise.
    // (The RenderRubyRun object will handle the details)
    auto* lastRun = lastRubyRun(&parent);
    if (!lastRun || lastRun->hasRubyText()) {
        auto newRun = RenderRubyRun::staticCreateRubyRun(&parent);
        lastRun = newRun.get();
        m_builder.inlineBuilder().attach(parent, WTFMove(newRun), beforeChild);
    }
    beforeChild = nullptr;
    return *lastRun;
}

RenderRubyBase& RenderTreeBuilder::Ruby::rubyBaseSafe(RenderRubyRun& rubyRun)
{
    auto* base = rubyRun.rubyBase();
    if (!base) {
        auto newBase = rubyRun.createRubyBase();
        base = newBase.get();
        m_builder.blockFlowBuilder().attach(rubyRun, WTFMove(newBase), nullptr);
    }
    return *base;
}

RenderPtr<RenderObject> RenderTreeBuilder::Ruby::detach(RenderRubyAsInline& parent, RenderObject& child)
{
    // If the child's parent is *this (must be a ruby run or generated content or anonymous block),
    // just use the normal remove method.
    if (child.parent() == &parent) {
        ASSERT(isRubyChildForNormalRemoval(child));
        return m_builder.detachFromRenderElement(parent, child);
    }
    // If the child's parent is an anoymous block (must be generated :before/:after content)
    // just use the block's remove method.
    if (isAnonymousRubyInlineBlock(child.parent())) {
        ASSERT(child.isBeforeContent() || child.isAfterContent());
        auto& parent = *child.parent();
        auto takenChild = m_builder.detach(parent, child);
        m_builder.destroy(parent);
        return takenChild;
    }

    // Otherwise find the containing run and remove it from there.
    return m_builder.detach(findRubyRunParent(child), child);
}

RenderPtr<RenderObject> RenderTreeBuilder::Ruby::detach(RenderRubyAsBlock& parent, RenderObject& child)
{
    // If the child's parent is *this (must be a ruby run or generated content or anonymous block),
    // just use the normal remove method.
    if (child.parent() == &parent) {
        ASSERT(isRubyChildForNormalRemoval(child));
        return m_builder.blockBuilder().detach(parent, child);
    }
    // If the child's parent is an anoymous block (must be generated :before/:after content)
    // just use the block's remove method.
    if (isAnonymousRubyInlineBlock(child.parent())) {
        ASSERT(child.isBeforeContent() || child.isAfterContent());
        auto& parent = *child.parent();
        auto takenChild = m_builder.detach(parent, child);
        m_builder.destroy(parent);
        return takenChild;
    }

    // Otherwise find the containing run and remove it from there.
    return m_builder.detach(findRubyRunParent(child), child);
}

RenderPtr<RenderObject> RenderTreeBuilder::Ruby::detach(RenderRubyRun& parent, RenderObject& child)
{
    // If the child is a ruby text, then merge the ruby base with the base of
    // the right sibling run, if possible.
    if (!parent.beingDestroyed() && !parent.renderTreeBeingDestroyed() && child.isRubyText()) {
        RenderRubyBase* base = parent.rubyBase();
        RenderObject* rightNeighbour = parent.nextSibling();
        if (base && is<RenderRubyRun>(rightNeighbour)) {
            // Ruby run without a base can happen only at the first run.
            RenderRubyRun& rightRun = downcast<RenderRubyRun>(*rightNeighbour);
            if (rightRun.hasRubyBase()) {
                RenderRubyBase* rightBase = rightRun.rubyBase();
                // Collect all children in a single base, then swap the bases.
                moveChildren(*rightBase, *base);
                m_builder.move(parent, rightRun, *base, RenderTreeBuilder::NormalizeAfterInsertion::No);
                m_builder.move(rightRun, parent, *rightBase, RenderTreeBuilder::NormalizeAfterInsertion::No);
                // The now empty ruby base will be removed below.
                ASSERT(!parent.rubyBase()->firstChild());
            }
        }
    }

    auto takenChild = m_builder.blockBuilder().detach(parent, child);

    if (!parent.beingDestroyed() && !parent.renderTreeBeingDestroyed()) {
        // Check if our base (if any) is now empty. If so, destroy it.
        RenderBlock* base = parent.rubyBase();
        if (base && !base->firstChild()) {
            auto takenBase = m_builder.blockBuilder().detach(parent, *base);
            base->deleteLines();
        }
    }
    return takenChild;
}

}
