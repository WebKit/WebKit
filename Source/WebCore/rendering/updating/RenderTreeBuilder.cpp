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
#include "RenderTableRow.h"
#include "RenderText.h"
#include "RenderTreeBuilderFirstLetter.h"
#include "RenderTreeBuilderList.h"
#include "RenderTreeBuilderMultiColumn.h"
#include "RenderTreeBuilderRuby.h"
#include "RenderTreeBuilderTable.h"

namespace WebCore {

RenderTreeBuilder* RenderTreeBuilder::s_current;

RenderTreeBuilder::RenderTreeBuilder(RenderView& view)
    : m_view(view)
    , m_firstLetterBuilder(std::make_unique<FirstLetter>(*this))
    , m_listBuilder(std::make_unique<List>(*this))
    , m_multiColumnBuilder(std::make_unique<MultiColumn>(*this))
    , m_tableBuilder(std::make_unique<Table>(*this))
    , m_rubyBuilder(std::make_unique<Ruby>(*this))
{
    RELEASE_ASSERT(!s_current || &m_view != &s_current->m_view);
    m_previous = s_current;
    s_current = this;
}

RenderTreeBuilder::~RenderTreeBuilder()
{
    s_current = m_previous;
}

void RenderTreeBuilder::insertChild(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto insertRecursiveIfNeeded = [&](RenderElement& parentCandidate) {
        if (&parent == &parentCandidate) {
            parent.addChild(*this, WTFMove(child), beforeChild);
            return;
        }
        insertChild(parentCandidate, WTFMove(child), beforeChild);
    };

    ASSERT(&parent.view() == &m_view);

    if (is<RenderText>(beforeChild)) {
        if (auto* wrapperInline = downcast<RenderText>(*beforeChild).inlineWrapperForDisplayContents())
            beforeChild = wrapperInline;
    }

    if (is<RenderTableRow>(parent)) {
        insertRecursiveIfNeeded(tableBuilder().findOrCreateParentForChild(downcast<RenderTableRow>(parent), *child, beforeChild));
        return;
    }

    if (is<RenderTableSection>(parent)) {
        insertRecursiveIfNeeded(tableBuilder().findOrCreateParentForChild(downcast<RenderTableSection>(parent), *child, beforeChild));
        return;
    }

    if (is<RenderTable>(parent)) {
        insertRecursiveIfNeeded(tableBuilder().findOrCreateParentForChild(downcast<RenderTable>(parent), *child, beforeChild));
        return;
    }

    if (is<RenderRubyAsBlock>(parent)) {
        insertRecursiveIfNeeded(rubyBuilder().findOrCreateParentForChild(downcast<RenderRubyAsBlock>(parent), *child, beforeChild));
        return;
    }

    if (is<RenderRubyRun>(parent)) {
        rubyBuilder().insertChild(downcast<RenderRubyRun>(parent), WTFMove(child), beforeChild);
        return;
    }

    parent.addChild(*this, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::insertChild(RenderTreePosition& position, RenderPtr<RenderObject> child)
{
    insertChild(position.parent(), WTFMove(child), position.nextSibling());
}

void RenderTreeBuilder::updateAfterDescendants(RenderElement& renderer)
{
    if (is<RenderBlock>(renderer))
        firstLetterBuilder().updateAfterDescendants(downcast<RenderBlock>(renderer));
    if (is<RenderListItem>(renderer))
        listBuilder().updateItemMarker(downcast<RenderListItem>(renderer));
    if (is<RenderBlockFlow>(renderer))
        multiColumnBuilder().updateAfterDescendants(downcast<RenderBlockFlow>(renderer));
}

}
