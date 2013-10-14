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
#include "Text.h"

namespace WebCore {

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
}

String RenderTextFragment::originalText() const
{
    String result = textNode() ? textNode()->data() : contentString();
    return result.substring(start(), end());
}

bool RenderTextFragment::canBeSelectionLeaf() const
{
    return textNode() && textNode()->rendererIsEditable();
}

void RenderTextFragment::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderText::styleDidChange(diff, oldStyle);

    if (RenderBlock* block = blockForAccompanyingFirstLetter()) {
        block->style()->removeCachedPseudoStyle(FIRST_LETTER);
        block->updateFirstLetter();
    }
}

void RenderTextFragment::willBeDestroyed()
{
    if (m_firstLetter)
        m_firstLetter->destroy();
    RenderText::willBeDestroyed();
}

void RenderTextFragment::setText(const String& text, bool force)
{
    RenderText::setText(text, force);

    m_start = 0;
    m_end = textLength();
    if (!m_firstLetter)
        return;
    ASSERT(!m_contentString);
    m_firstLetter->destroy();
    m_firstLetter = 0;
    if (!textNode())
        return;
    ASSERT(!textNode()->renderer());
    textNode()->setRenderer(this);
}

void RenderTextFragment::transformText()
{
    // Don't reset first-letter here because we are only transforming the truncated fragment.
    String textToTransform = originalText();
    if (!textToTransform.isNull())
        RenderText::setText(textToTransform, true);
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

RenderBlock* RenderTextFragment::blockForAccompanyingFirstLetter() const
{
    if (!m_firstLetter)
        return 0;
    for (RenderObject* block = m_firstLetter->parent(); block; block = block->parent()) {
        if (block->style()->hasPseudoStyle(FIRST_LETTER) && block->canHaveChildren() && block->isRenderBlock())
            return toRenderBlock(block);
    }
    return 0;
}

} // namespace WebCore
