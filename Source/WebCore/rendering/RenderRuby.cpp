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
#include "RenderTreeBuilder.h"
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

RenderPtr<RenderObject> RenderRubyAsInline::takeChild(RenderTreeBuilder& builder, RenderObject& child)
{
    // If the child's parent is *this (must be a ruby run or generated content or anonymous block),
    // just use the normal remove method.
    if (child.parent() == this) {
#ifndef ASSERT_DISABLED
        ASSERT(isRubyChildForNormalRemoval(child));
#endif
        return RenderInline::takeChild(builder, child);
    }
    // If the child's parent is an anoymous block (must be generated :before/:after content)
    // just use the block's remove method.
    if (isAnonymousRubyInlineBlock(child.parent())) {
        ASSERT(child.isBeforeContent() || child.isAfterContent());
        auto& parent = *child.parent();
        auto takenChild = parent.takeChild(builder, child);
        parent.removeFromParentAndDestroy();
        return takenChild;
    }

    // Otherwise find the containing run and remove it from there.
    return findRubyRunParent(child).takeChild(builder, child);
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

RenderPtr<RenderObject> RenderRubyAsBlock::takeChild(RenderTreeBuilder& builder, RenderObject& child)
{
    // If the child's parent is *this (must be a ruby run or generated content or anonymous block),
    // just use the normal remove method.
    if (child.parent() == this) {
#ifndef ASSERT_DISABLED
        ASSERT(isRubyChildForNormalRemoval(child));
#endif
        return RenderBlockFlow::takeChild(builder, child);
    }
    // If the child's parent is an anoymous block (must be generated :before/:after content)
    // just use the block's remove method.
    if (isAnonymousRubyInlineBlock(child.parent())) {
        ASSERT(child.isBeforeContent() || child.isAfterContent());
        auto& parent = *child.parent();
        auto takenChild = parent.takeChild(builder, child);
        parent.removeFromParentAndDestroy();
        return takenChild;
    }

    // Otherwise find the containing run and remove it from there.
    return findRubyRunParent(child).takeChild(builder, child);
}

} // namespace WebCore
