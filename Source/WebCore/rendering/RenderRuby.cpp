/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "RenderRuby.h"

#include "RenderIterator.h"
#include "RenderRubyRun.h"
#include "RenderStyle.h"
#include "StyleInheritedData.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RefPtr.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderRubyAsInline);
WTF_MAKE_ISO_ALLOCATED_IMPL(RenderRubyAsBlock);

//=== generic helper functions to avoid excessive code duplication ===

static inline bool isAnonymousRubyInlineBlock(const RenderObject* object)
{
    ASSERT(!object
        || !isRuby(object->parent())
        || is<RenderRubyRun>(*object)
        || (object->isInline() && (object->isBeforeContent() || object->isAfterContent()))
        || (object->isAnonymous() && is<RenderBlock>(*object) && object->style().display() == INLINE_BLOCK));

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
        && downcast<RenderBlock>(*object).firstChild()->style().styleType() == BEFORE;
}

static inline bool isRubyAfterBlock(const RenderObject* object)
{
    return isAnonymousRubyInlineBlock(object)
        && !object->nextSibling()
        && downcast<RenderBlock>(*object).firstChild()
        && downcast<RenderBlock>(*object).firstChild()->style().styleType() == AFTER;
}

#ifndef ASSERT_DISABLED
static inline bool isRubyChildForNormalRemoval(const RenderObject& object)
{
    return object.isRubyRun()
    || object.isBeforeContent()
    || object.isAfterContent()
    || object.isRenderMultiColumnFlow()
    || object.isRenderMultiColumnSet()
    || isAnonymousRubyInlineBlock(&object);
}
#endif

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
    auto newBlock = createRenderer<RenderBlockFlow>(ruby.document(), RenderStyle::createAnonymousStyleWithDisplay(ruby.style(), INLINE_BLOCK));
    newBlock->initializeStyle();
    return newBlock;
}

static RenderRubyRun* lastRubyRun(const RenderElement* ruby)
{
    RenderObject* child = ruby->lastChild();
    if (child && !is<RenderRubyRun>(*child))
        child = child->previousSibling();
    if (!is<RenderRubyRun>(child)) {
        ASSERT(!child || child->isBeforeContent() || child == rubyBeforeBlock(ruby));
        return nullptr;
    }
    return downcast<RenderRubyRun>(child);
}

static inline RenderRubyRun& findRubyRunParent(RenderObject& child)
{
    return *lineageOfType<RenderRubyRun>(child).first();
}

//=== ruby as inline object ===

RenderRubyAsInline::RenderRubyAsInline(Element& element, RenderStyle&& style)
    : RenderInline(element, WTFMove(style))
{
}

RenderRubyAsInline::~RenderRubyAsInline() = default;

void RenderRubyAsInline::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderInline::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren(PropagateToAllChildren);
}

void RenderRubyAsInline::addChild(RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    // Insert :before and :after content before/after the RenderRubyRun(s)
    if (child->isBeforeContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            RenderInline::addChild(WTFMove(child), firstChild());
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            RenderBlock* beforeBlock = rubyBeforeBlock(this);
            if (!beforeBlock) {
                auto newBlock = createAnonymousRubyInlineBlock(*this);
                beforeBlock = newBlock.get();
                RenderInline::addChild(WTFMove(newBlock), firstChild());
            }
            beforeBlock->addChild(WTFMove(child));
        }
        return;
    }
    if (child->isAfterContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            RenderInline::addChild(WTFMove(child));
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            RenderBlock* afterBlock = rubyAfterBlock(this);
            if (!afterBlock) {
                auto newBlock = createAnonymousRubyInlineBlock(*this);
                afterBlock = newBlock.get();
                RenderInline::addChild(WTFMove(newBlock));
            }
            afterBlock->addChild(WTFMove(child));
        }
        return;
    }

    // If the child is a ruby run, just add it normally.
    if (child->isRubyRun()) {
        RenderInline::addChild(WTFMove(child), beforeChild);
        return;
    }

    if (beforeChild && !isAfterContent(beforeChild)) {
        // insert child into run
        ASSERT(!beforeChild->isRubyRun());
        RenderElement* run = beforeChild->parent();
        while (run && !run->isRubyRun())
            run = run->parent();
        if (run) {
            run->addChild(WTFMove(child), beforeChild);
            return;
        }
        ASSERT_NOT_REACHED(); // beforeChild should always have a run as parent!
        // Emergency fallback: fall through and just append.
    }

    // If the new child would be appended, try to add the child to the previous run
    // if possible, or create a new run otherwise.
    // (The RenderRubyRun object will handle the details)
    RenderRubyRun* lastRun = lastRubyRun(this);
    if (!lastRun || lastRun->hasRubyText()) {
        auto newRun = RenderRubyRun::staticCreateRubyRun(this);
        lastRun = newRun.get();
        RenderInline::addChild(WTFMove(newRun), beforeChild);
    }
    lastRun->addChild(WTFMove(child));
}

RenderPtr<RenderObject> RenderRubyAsInline::takeChild(RenderObject& child)
{
    // If the child's parent is *this (must be a ruby run or generated content or anonymous block),
    // just use the normal remove method.
    if (child.parent() == this) {
#ifndef ASSERT_DISABLED
        ASSERT(isRubyChildForNormalRemoval(child));
#endif
        return RenderInline::takeChild(child);
    }
    // If the child's parent is an anoymous block (must be generated :before/:after content)
    // just use the block's remove method.
    if (isAnonymousRubyInlineBlock(child.parent())) {
        ASSERT(child.isBeforeContent() || child.isAfterContent());
        auto& parent = *child.parent();
        auto takenChild = parent.takeChild(child);
        parent.removeFromParentAndDestroy();
        return takenChild;
    }

    // Otherwise find the containing run and remove it from there.
    return findRubyRunParent(child).takeChild(child);
}

//=== ruby as block object ===

RenderRubyAsBlock::RenderRubyAsBlock(Element& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
{
}

RenderRubyAsBlock::~RenderRubyAsBlock() = default;

void RenderRubyAsBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren(PropagateToAllChildren);
}

void RenderRubyAsBlock::addChild(RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    // Insert :before and :after content before/after the RenderRubyRun(s)
    if (child->isBeforeContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            RenderBlockFlow::addChild(WTFMove(child), firstChild());
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            RenderBlock* beforeBlock = rubyBeforeBlock(this);
            if (!beforeBlock) {
                auto newBlock = createAnonymousRubyInlineBlock(*this);
                beforeBlock = newBlock.get();
                RenderBlockFlow::addChild(WTFMove(newBlock), firstChild());
            }
            beforeBlock->addChild(WTFMove(child));
        }
        return;
    }
    if (child->isAfterContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            RenderBlockFlow::addChild(WTFMove(child));
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            RenderBlock* afterBlock = rubyAfterBlock(this);
            if (!afterBlock) {
                auto newBlock = createAnonymousRubyInlineBlock(*this);
                afterBlock = newBlock.get();
                RenderBlockFlow::addChild(WTFMove(newBlock));
            }
            afterBlock->addChild(WTFMove(child));
        }
        return;
    }

    // If the child is a ruby run, just add it normally.
    if (child->isRubyRun()) {
        RenderBlockFlow::addChild(WTFMove(child), beforeChild);
        return;
    }

    if (beforeChild && !isAfterContent(beforeChild)) {
        // insert child into run
        ASSERT(!beforeChild->isRubyRun());
        RenderElement* run = beforeChild->parent();
        while (run && !run->isRubyRun())
            run = run->parent();
        if (run) {
            run->addChild(WTFMove(child), beforeChild);
            return;
        }
        ASSERT_NOT_REACHED(); // beforeChild should always have a run as parent!
        // Emergency fallback: fall through and just append.
    }

    // If the new child would be appended, try to add the child to the previous run
    // if possible, or create a new run otherwise.
    // (The RenderRubyRun object will handle the details)
    RenderRubyRun* lastRun = lastRubyRun(this);
    if (!lastRun || lastRun->hasRubyText()) {
        auto newRun = RenderRubyRun::staticCreateRubyRun(this);
        lastRun = newRun.get();
        RenderBlockFlow::addChild(WTFMove(newRun), beforeChild);
    }
    lastRun->addChild(WTFMove(child));
}

RenderPtr<RenderObject> RenderRubyAsBlock::takeChild(RenderObject& child)
{
    // If the child's parent is *this (must be a ruby run or generated content or anonymous block),
    // just use the normal remove method.
    if (child.parent() == this) {
#ifndef ASSERT_DISABLED
        ASSERT(isRubyChildForNormalRemoval(child));
#endif
        return RenderBlockFlow::takeChild(child);
    }
    // If the child's parent is an anoymous block (must be generated :before/:after content)
    // just use the block's remove method.
    if (isAnonymousRubyInlineBlock(child.parent())) {
        ASSERT(child.isBeforeContent() || child.isAfterContent());
        auto& parent = *child.parent();
        auto takenChild = parent.takeChild(child);
        parent.removeFromParentAndDestroy();
        return takenChild;
    }

    // Otherwise find the containing run and remove it from there.
    return findRubyRunParent(child).takeChild(child);
}

} // namespace WebCore
