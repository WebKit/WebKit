/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "RenderSVGInlineTextFragment.h"

#include "RenderAncestorIterator.h"
#include "RenderBlock.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderInline.h"
#include "RenderMultiColumnFlow.h"
#include "RenderObjectInlines.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "Text.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSVGInlineTextFragment);

RenderSVGInlineTextFragment::RenderSVGInlineTextFragment(Text& textNode, const String& fullText, unsigned startOffset, unsigned length)
    : RenderSVGInlineText(Type::SVGInlineTextFragment, textNode, fullText.substring(startOffset, length))
    , m_start(startOffset)
    , m_end(length)
{
    ASSERT(isRenderSVGInlineTextFragment());
}

RenderSVGInlineTextFragment::RenderSVGInlineTextFragment(Document& document, const String& fullText, unsigned startOffset, unsigned length)
    : RenderSVGInlineText(Type::SVGInlineTextFragment, document, fullText.substring(startOffset, length))
    , m_start(startOffset)
    , m_end(length)
{
    ASSERT(isRenderSVGInlineTextFragment());
}

RenderSVGInlineTextFragment::~RenderSVGInlineTextFragment()
{
    ASSERT(!m_firstLetter);
}

String RenderSVGInlineTextFragment::originalText() const
{
    if (RefPtr node = textNode())
        return node->data();
    if (!m_contentString.isNull())
        return m_contentString;
    return text();
}

bool RenderSVGInlineTextFragment::canBeSelectionLeaf() const
{
    if (RefPtr node = textNode())
        return firstLetter() || node->hasEditableStyle();
    // The anonymous first-letter fragment is always selectable when its
    // wrapper still tracks a remaining-text fragment.
    CheckedPtr anonymousInlineWrapper = dynamicDowncast<RenderInline>(this->parent());
    return anonymousInlineWrapper && anonymousInlineWrapper->firstLetterRemainingText();
}

void RenderSVGInlineTextFragment::setTextInternal(const String& newText, bool force)
{
    RenderSVGInlineText::setTextInternal(newText, force);

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

void RenderSVGInlineTextFragment::setTextWithOffset(const String& newText, unsigned offset)
{
    if (m_firstLetter && offset < m_start)
        RenderTreeBuilder::current() ? RenderTreeBuilder::current()->destroy(*m_firstLetter) : RenderTreeBuilder(*document().renderView()).destroy(*m_firstLetter);
    RenderSVGInlineText::setTextWithOffset(newText, offset);
}

Node* RenderSVGInlineTextFragment::nodeForHitTest() const
{
    if (!textNode()) {
        if (auto* parent = dynamicDowncast<RenderBoxModelObject>(this->parent()); parent && parent->isFirstLetter()) {
            if (auto* remainingText = parent->firstLetterRemainingText())
                return remainingText->textNode();
        }
    }
    return RenderSVGInlineText::nodeForHitTest();
}

char32_t RenderSVGInlineTextFragment::previousCharacter() const
{
    if (m_start) {
        String original = textNode() ? textNode()->data() : (m_contentString.isNull() ? text() : m_contentString);
        if (m_start <= original.length())
            return StringView(original).codePointBefore(m_start);
    }
    return RenderSVGInlineText::previousCharacter();
}

void RenderSVGInlineTextFragment::setContentString(const String& fullText)
{
    m_contentString = fullText;
    setText(fullText);
}

CheckedPtr<RenderBlock> RenderSVGInlineTextFragment::blockForAccompanyingFirstLetter()
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

} // namespace WebCore
