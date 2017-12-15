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
#include "RenderTreeBuilder.h"

#include "RenderElement.h"
#include "RenderRuby.h"
#include "RenderRubyBase.h"
#include "RenderRubyRun.h"
#include "RenderText.h"
#include "RenderTreeUpdater.h"

namespace WebCore {

RenderTreeBuilder* RenderTreeBuilder::m_current;

RenderTreeBuilder::RenderTreeBuilder()
{
    RELEASE_ASSERT(!m_current);
    m_current = this;
}

RenderTreeBuilder::~RenderTreeBuilder()
{
    m_current = nullptr;
}

void RenderTreeBuilder::insertChild(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    // We don't yet have any local access, ensure we are still called with non-null this ptr.
    ASSERT(this);

    if (is<RenderText>(beforeChild)) {
        if (auto* wrapperInline = downcast<RenderText>(*beforeChild).inlineWrapperForDisplayContents())
            beforeChild = wrapperInline;
    }

    if (is<RenderRubyRun>(parent)) {
        rubyRunInsertChild(downcast<RenderRubyRun>(parent), WTFMove(child), beforeChild);
        return;
    }

    parent.addChild(*this, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChild(RenderTreePosition& position, RenderPtr<RenderObject> child)
{
    insertChild(position.parent(), WTFMove(child), position.nextSibling());
}

void RenderTreeBuilder::rubyRunInsertChild(RenderRubyRun& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (child->isRubyText()) {
        if (!beforeChild) {
            // RenderRuby has already ascertained that we can add the child here.
            ASSERT(!parent.hasRubyText());
            // prepend ruby texts as first child
            parent.addChild(*this, WTFMove(child), parent.firstChild());
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
            insertChild(*ruby, WTFMove(newRun), parent.nextSibling());
            // Add the new ruby text and move the old one to the new run
            // Note: Doing it in this order and not using RenderRubyRun's methods,
            // in order to avoid automatic removal of the ruby run in case there is no
            // other child besides the old ruby text.
            parent.addChild(*this, WTFMove(child), beforeChild);
            auto takenBeforeChild = parent.RenderBlockFlow::takeChild(*beforeChild);
            insertChild(*newRun, WTFMove(takenBeforeChild));
            return;
        }
        if (parent.hasRubyBase()) {
            // Insertion before a ruby base object.
            // In this case we need insert a new run before the current one and split the base.
            RenderElement* ruby = parent.parent();
            auto newRun = RenderRubyRun::staticCreateRubyRun(ruby);
            auto& run = *newRun;
            insertChild(*ruby, WTFMove(newRun), &parent);
            insertChild(run, WTFMove(child));
            parent.rubyBaseSafe()->moveChildren(run.rubyBaseSafe(), beforeChild);
        }
        return;
    }
    // child is not a text -> insert it into the base
    // (append it instead if beforeChild is the ruby text)
    if (beforeChild && beforeChild->isRubyText())
        beforeChild = nullptr;
    insertChild(*parent.rubyBaseSafe(), WTFMove(child), beforeChild);
}

}
