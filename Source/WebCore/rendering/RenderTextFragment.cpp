/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderTextFragment.h"

#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderObjectInlines.h"
#include "RenderMultiColumnFlow.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderTreeBuilder.h"
#include "Text.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTextFragment);

RenderTextFragment::RenderTextFragment(Text& textNode, const String& text, int startOffset, int length)
    : RenderText(Type::TextFragment, textNode, text.substring(startOffset, length))
    , m_start(startOffset)
    , m_end(length)
    , m_firstLetter(nullptr)
{
}

RenderTextFragment::RenderTextFragment(Document& document, const String& text, int startOffset, int length)
    : RenderText(Type::TextFragment, document, text.substring(startOffset, length))
    , m_start(startOffset)
    , m_end(length)
    , m_firstLetter(nullptr)
{
}

RenderTextFragment::RenderTextFragment(Document& textNode, const String& text)
    : RenderText(Type::TextFragment, textNode, text)
    , m_start(0)
    , m_end(text.length())
    , m_contentString(text)
    , m_firstLetter(nullptr)
{
}

RenderTextFragment::~RenderTextFragment()
{
    ASSERT(!m_firstLetter);
}

bool RenderTextFragment::canBeSelectionLeaf() const
{
    if (RefPtr textNode = this->textNode()) {
        // Remaining (trailing) text fragments with first-letter are always selectable,
        // matching the base RenderText::canBeSelectionLeaf() behavior.
        return firstLetter() || textNode->hasEditableStyle();
    }
    // First-letter is always selectable.
    CheckedPtr anonymousInlineWrapper = dynamicDowncast<RenderInline>(this->parent());
    return anonymousInlineWrapper && anonymousInlineWrapper->firstLetterRemainingText();
}

void RenderTextFragment::setTextInternal(const String& newText, bool force)
{
    RenderText::setTextInternal(newText, force);

    m_start = 0;
    m_end = text().length();
    if (!m_firstLetter)
        return;
    if (RenderTreeBuilder::current())
        RenderTreeBuilder::current()->destroy(*m_firstLetter);
    else
        RenderTreeBuilder(*document().renderView()).destroy(*m_firstLetter);
    ASSERT(!m_firstLetter);
    ASSERT(!textNode() || textNode()->renderer() == this);
}

void RenderTextFragment::setTextWithOffset(const String& newText, unsigned offset)
{
    // Edits within the first-letter range invalidate the first-letter split.
    // The base class skips the update when the fragment text matches the new
    // content, but the split is stale and the tree builder needs to recreate it.
    if (m_firstLetter && offset < m_start)
        RenderTreeBuilder::current() ? RenderTreeBuilder::current()->destroy(*m_firstLetter) : RenderTreeBuilder(*document().renderView()).destroy(*m_firstLetter);
    RenderText::setTextWithOffset(newText, offset);
}

Node* RenderTextFragment::nodeForHitTest() const
{
    if (!textNode()) {
        // The anonymous first-letter text has no DOM node. Resolve to the DOM text
        // node via the remaining fragment so cursor and selection work.
        if (auto* parent = dynamicDowncast<RenderBoxModelObject>(this->parent()); parent && parent->isFirstLetter()) {
            if (auto* remainingText = parent->firstLetterRemainingText())
                return remainingText->textNode();
        }
    }
    return RenderText::nodeForHitTest();
}

char32_t RenderTextFragment::previousCharacter() const
{
    if (start()) {
        String original = textNode() ? textNode()->data() : contentString();
        if (start() <= original.length())
            return StringView(original).codePointBefore(start());
    }
    return RenderText::previousCharacter();
}

CheckedPtr<RenderBlock> RenderTextFragment::blockForAccompanyingFirstLetter()
{
    if (!m_firstLetter)
        return nullptr;
    for (CheckedRef block : ancestorsOfType<RenderBlock>(*m_firstLetter)) {
        if (is<RenderMultiColumnFlow>(block))
            break;
        if (block->style().hasPseudoStyle(PseudoElementType::FirstLetter) && block->canHaveChildren())
            return block;
    }
    return nullptr;
}

void RenderTextFragment::setContentString(const String& text)
{
    m_contentString = text;
    setText(text);
}

} // namespace WebCore
