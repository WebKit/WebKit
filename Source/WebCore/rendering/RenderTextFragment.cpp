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
#include "RenderIterator.h"
#include "RenderMultiColumnFlow.h"
#include "RenderTreeBuilder.h"
#include "Text.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTextFragment);

RenderTextFragment::RenderTextFragment(Text& textNode, const String& text, int startOffset, int length)
    : RenderText(textNode, text.substring(startOffset, length))
    , m_start(startOffset)
    , m_end(length)
    , m_firstLetter(nullptr)
{
}

RenderTextFragment::RenderTextFragment(Document& document, const String& text, int startOffset, int length)
    : RenderText(document, text.substring(startOffset, length))
    , m_start(startOffset)
    , m_end(length)
    , m_firstLetter(nullptr)
{
}

RenderTextFragment::RenderTextFragment(Document& textNode, const String& text)
    : RenderText(textNode, text)
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
    return textNode() && textNode()->hasEditableStyle();
}

void RenderTextFragment::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderText::styleDidChange(diff, oldStyle);

    if (RenderBlock* block = blockForAccompanyingFirstLetter())
        block->mutableStyle().removeCachedPseudoStyle(PseudoId::FirstLetter);
}

void RenderTextFragment::setText(const String& newText, bool force)
{
    RenderText::setText(newText, force);
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

UChar RenderTextFragment::previousCharacter() const
{
    if (start()) {
        String original = textNode() ? textNode()->data() : contentString();
        if (!original.isNull() && start() <= original.length())
            return original[start() - 1];
    }

    return RenderText::previousCharacter();
}

RenderBlock* RenderTextFragment::blockForAccompanyingFirstLetter()
{
    if (!m_firstLetter)
        return nullptr;
    for (auto& block : ancestorsOfType<RenderBlock>(*m_firstLetter)) {
        if (is<RenderMultiColumnFlow>(block))
            break;
        if (block.style().hasPseudoStyle(PseudoId::FirstLetter) && block.canHaveChildren())
            return &block;
    }
    return nullptr;
}

void RenderTextFragment::setContentString(const String& text)
{
    m_contentString = text;
    setText(text);
}

} // namespace WebCore
