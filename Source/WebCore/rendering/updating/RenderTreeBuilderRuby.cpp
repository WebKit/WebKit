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
#include "RenderInline.h"
#include "RenderTreeBuilder.h"
#include "RenderTreeBuilderBlock.h"
#include "RenderTreeBuilderBlockFlow.h"
#include "RenderTreeBuilderInline.h"
#include "UnicodeBidi.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(RenderTreeBuilderRuby, RenderTreeBuilder::Ruby);

RenderTreeBuilder::Ruby::Ruby(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

RenderStyle createAnonymousStyleForRuby(const RenderStyle& parentStyle, DisplayType display)
{
    ASSERT(display == DisplayType::Ruby || display == DisplayType::RubyBase);

    auto style = RenderStyle::createAnonymousStyleWithDisplay(parentStyle, display);
    style.setUnicodeBidi(UnicodeBidi::Isolate);
    if (display == DisplayType::RubyBase)
        style.setTextWrapMode(TextWrapMode::NoWrap);
    return style;
}

static RenderPtr<RenderElement> createAnonymousRendererForRuby(RenderElement& parent, DisplayType display)
{
    auto style = createAnonymousStyleForRuby(parent.style(), display);
    auto ruby = createRenderer<RenderInline>(RenderObject::Type::Inline, parent.document(), WTFMove(style));
    ruby->initializeStyle();
    return ruby;
}

RenderElement& RenderTreeBuilder::Ruby::findOrCreateParentForStyleBasedRubyChild(RenderElement& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    if (!child.isRenderText() && child.style().display() == DisplayType::Ruby && parent.style().display() == DisplayType::RubyBlock)
        return parent;

    if (parent.style().display() == DisplayType::RubyBlock) {
        // See if we have an anonymous ruby box already.
        // FIXME: It should be the immediate child but continuations can break this assumption.
        for (CheckedPtr first = parent.firstChild(); first; first = first->firstChildSlow()) {
            if (!first->isAnonymous()) {
                // <ruby blockified><ruby> is valid and still requires construction of an anonymous inline ruby box.
                ASSERT(first->style().display() == DisplayType::Ruby);
                break;
            }
            if (first->style().display() == DisplayType::Ruby)
                return downcast<RenderElement>(*first);
        }
    }

    if (parent.style().display() != DisplayType::Ruby) {
        auto rubyContainer = createAnonymousRendererForRuby(parent, DisplayType::Ruby);
        WeakPtr newParent = rubyContainer.get();
        m_builder.attach(parent, WTFMove(rubyContainer), beforeChild);
        beforeChild = nullptr;
        return *newParent;
    }

    if (!child.isRenderText() && (child.style().display() == DisplayType::RubyBase || child.style().display() == DisplayType::RubyAnnotation))
        return parent;

    if (beforeChild && beforeChild->parent()->style().display() == DisplayType::RubyBase)
        return *beforeChild->parent();

    auto* previous = beforeChild ? beforeChild->previousSibling() : parent.lastChild();
    if (previous && previous->style().display() == DisplayType::RubyBase) {
        beforeChild = nullptr;
        return downcast<RenderElement>(*previous);
    }

    auto rubyBase = createAnonymousRendererForRuby(parent, DisplayType::RubyBase);
    rubyBase->initializeStyle();
    WeakPtr newParent = rubyBase.get();
    m_builder.inlineBuilder().attach(downcast<RenderInline>(parent), WTFMove(rubyBase), beforeChild);
    beforeChild = nullptr;
    return *newParent;
}

void RenderTreeBuilder::Ruby::attachForStyleBasedRuby(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (parent.style().display() == DisplayType::RubyBlock) {
        ASSERT(child->style().display() == DisplayType::Ruby);
        m_builder.attachToRenderElementInternal(parent, WTFMove(child), beforeChild);
        return;
    }
    ASSERT(parent.style().display() == DisplayType::Ruby);
    ASSERT(child->style().display() == DisplayType::RubyBase || child->style().display() == DisplayType::RubyAnnotation);

    while (beforeChild && beforeChild->parent() && beforeChild->parent() != &parent)
        beforeChild = beforeChild->parent();

    if (child->style().display() == DisplayType::RubyAnnotation) {
        // Create an empty anonymous base if it is missing.
        WeakPtr previous = beforeChild ? beforeChild->previousSibling() : parent.lastChild();
        if (!previous || previous->style().display() != DisplayType::RubyBase) {
            auto rubyBase = createAnonymousRendererForRuby(parent, DisplayType::RubyBase);
            m_builder.attachToRenderElementInternal(parent, WTFMove(rubyBase), beforeChild);
        }
    }
    m_builder.attachToRenderElementInternal(parent, WTFMove(child), beforeChild);
}

}
