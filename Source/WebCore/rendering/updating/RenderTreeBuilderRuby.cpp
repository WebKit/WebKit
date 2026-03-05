/*
 * Copyright (C) 20170-2024 Apple Inc. All rights reserved.
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

#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderObjectInlines.h"
#include "RenderTreeBuilder.h"
#include "RenderTreeBuilderBlock.h"
#include "RenderTreeBuilderInline.h"
#include "UnicodeBidi.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeBuilder::Ruby);

RenderTreeBuilder::Ruby::Ruby(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

RenderStyle createAnonymousStyleForRuby(const RenderStyle& parentStyle, Style::Display display)
{
    ASSERT(display == Style::DisplayType::InlineRuby || display == Style::DisplayType::RubyBase);

    auto style = RenderStyle::createAnonymousStyleWithDisplay(parentStyle, display);
    style.setUnicodeBidi(UnicodeBidi::Isolate);
    if (display == Style::DisplayType::RubyBase)
        style.setTextWrapMode(TextWrapMode::NoWrap);
    return style;
}

static RenderPtr<RenderElement> createAnonymousRendererForRuby(RenderElement& parent, Style::Display display)
{
    auto style = createAnonymousStyleForRuby(parent.style(), display);
    auto ruby = createRenderer<RenderInline>(RenderObject::Type::Inline, parent.document(), WTF::move(style));
    ruby->initializeStyle();
    return ruby;
}

CheckedRef<RenderElement> RenderTreeBuilder::Ruby::findOrCreateParentForStyleBasedRubyChild(RenderElement& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    CheckedRef<RenderElement> beforeChildAncestor = parent;
    if (auto* rubyInline = dynamicDowncast<RenderInline>(parent); rubyInline && rubyInline->continuation())
        beforeChildAncestor = RenderTreeBuilder::Inline::parentCandidateInContinuation(*rubyInline, beforeChild);
    else if (auto* rubyBlock = dynamicDowncast<RenderBlock>(parent); rubyBlock && rubyBlock->continuation())
        beforeChildAncestor = *RenderTreeBuilder::Block::continuationBefore(*rubyBlock, beforeChild);

    if (!child.isRenderText() && child.style().display() == Style::DisplayType::InlineRuby && beforeChildAncestor->style().display() == Style::DisplayType::BlockRuby)
        return beforeChildAncestor;

    if (beforeChildAncestor->style().display() == Style::DisplayType::BlockRuby) {
        // See if we have an anonymous ruby box already.
        // FIXME: It should be the immediate child but continuations can break this assumption.
        for (CheckedPtr first = beforeChildAncestor->firstChild(); first; first = first->firstChildSlow()) {
            if (!first->isAnonymous()) {
                // <ruby blockified><ruby> is valid and still requires construction of an anonymous inline ruby box.
                ASSERT(first->style().display() == Style::DisplayType::InlineRuby);
                break;
            }
            if (first->style().display() == Style::DisplayType::InlineRuby) {
                if (beforeChild && !beforeChild->isDescendantOf(first.get()))
                    beforeChild = nullptr;
                return downcast<RenderElement>(*first);
            }
        }
    }

    if (beforeChildAncestor->style().display() != Style::DisplayType::InlineRuby) {
        auto rubyContainer = createAnonymousRendererForRuby(beforeChildAncestor, Style::DisplayType::InlineRuby);
        WeakPtr newParent = rubyContainer.get();
        m_builder.attach(parent, WTF::move(rubyContainer), beforeChild);
        beforeChild = nullptr;
        return *newParent;
    }

    if (!child.isRenderText() && (child.style().display() == Style::DisplayType::RubyBase || child.style().display() == Style::DisplayType::RubyText))
        return beforeChildAncestor;

    if (beforeChild && beforeChild->parent()->style().display() == Style::DisplayType::RubyBase)
        return *beforeChild->parent();

    auto* previous = beforeChild ? beforeChild->previousSibling() : beforeChildAncestor->lastChild();
    if (previous && previous->style().display() == Style::DisplayType::RubyBase) {
        beforeChild = nullptr;
        return downcast<RenderElement>(*previous);
    }

    auto rubyBase = createAnonymousRendererForRuby(beforeChildAncestor, Style::DisplayType::RubyBase);
    rubyBase->initializeStyle();
    WeakPtr newParent = rubyBase.get();
    m_builder.inlineBuilder().attach(downcast<RenderInline>(parent), WTF::move(rubyBase), beforeChild);
    beforeChild = nullptr;
    return *newParent;
}

void RenderTreeBuilder::Ruby::attachForStyleBasedRuby(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (parent.style().display() == Style::DisplayType::BlockRuby) {
        ASSERT(child->style().display() == Style::DisplayType::InlineRuby);
        m_builder.attachToRenderElementInternal(parent, WTF::move(child), beforeChild);
        return;
    }
    ASSERT(parent.style().display() == Style::DisplayType::InlineRuby);
    ASSERT(child->style().display() == Style::DisplayType::RubyBase || child->style().display() == Style::DisplayType::RubyText);

    while (beforeChild && beforeChild->parent() && beforeChild->parent() != &parent)
        beforeChild = beforeChild->parent();

    if (child->style().display() == Style::DisplayType::RubyText) {
        // Create an empty anonymous base if it is missing.
        WeakPtr previous = beforeChild ? beforeChild->previousSibling() : parent.lastChild();
        if (!previous || previous->style().display() != Style::DisplayType::RubyBase) {
            auto rubyBase = createAnonymousRendererForRuby(parent, Style::DisplayType::RubyBase);
            m_builder.attachToRenderElementInternal(parent, WTF::move(rubyBase), beforeChild);
        }
    }
    m_builder.attachToRenderElementInternal(parent, WTF::move(child), beforeChild);
}

}
