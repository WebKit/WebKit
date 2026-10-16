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

#pragma once

#include "RenderBoxModelObject.h"
#include "RenderText.h"

namespace WebCore {

// Used for text that is not the whole text of a DOM text node:
// - A substring of a text node, e.g., for text runs that are split because of first letter and that must
//   therefore have different styles (and positions in the render tree). We cache offsets so that text
//   transformations can be applied in such a way that we can recover the original unaltered string from
//   our corresponding DOM node.
// - Text with no DOM node at all, e.g., 'content' strings, quote marks and the value of an input button.
class RenderTextFragment final : public RenderText {
    WTF_MAKE_TZONE_ALLOCATED(RenderTextFragment);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderTextFragment);
public:
    RenderTextFragment(Text&, const String&, int startOffset, int length);
    RenderTextFragment(Document&, const String&, int startOffset, int length);
    RenderTextFragment(Document&, const String&);

    virtual ~RenderTextFragment();

    bool canBeSelectionLeaf() const override;

    unsigned start() const { return m_start; }
    unsigned end() const { return m_end; }

    RenderBoxModelObject* firstLetter() const { return m_firstLetter.get(); }
    void setFirstLetter(RenderBoxModelObject& firstLetter) { m_firstLetter = firstLetter; }
    
    CheckedPtr<RenderBlock> blockForAccompanyingFirstLetter();

    void setContentString(const String& text);
    StringImpl* contentString() const { return m_contentString.impl(); }

    const String& altText() const LIFETIME_BOUND { return m_altText; }
    void setAltText(const String& altText) { m_altText = altText; }

    // Which quote mark an open-quote/close-quote shows depends on the quotes before it in document order,
    // so RenderTreeUpdater::GeneratedContent fills in the content string once the tree is built.
    std::optional<QuoteType> quoteType() const { return m_quoteType; }
    void setQuoteType(QuoteType quoteType) { m_quoteType = quoteType; }
    // ::first-letter replaces the quote's renderer with the text after the letter. That remaining text keeps the whole
    // quote mark as its content string, which is what tells RenderTreeUpdater::GeneratedContent the mark is unchanged.
    void setQuoteTypeForRemainingText(QuoteType quoteType, const String& quoteMark)
    {
        m_quoteType = quoteType;
        m_contentString = quoteMark;
    }

private:
    void setTextInternal(const String&, bool force) override;
    void setTextWithOffset(const String&, unsigned offset) override;
    Node* nodeForHitTest() const override;
    char32_t previousCharacter() const override;
    void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle) override;
    void insertedIntoTree() override;
    void willBeRemovedFromTree() override;

    unsigned m_start;
    unsigned m_end;
    // Alternative description that can be used for accessibility instead of the native text.
    String m_altText;
    String m_contentString;
    SingleThreadWeakPtr<RenderBoxModelObject> m_firstLetter;
    std::optional<QuoteType> m_quoteType;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::RenderTextFragment)
    static bool isType(const WebCore::RenderText& renderer) { return renderer.isRenderTextFragment(); }
    static bool isType(const WebCore::RenderObject& renderer)
    {
        auto* text = dynamicDowncast<WebCore::RenderText>(renderer);
        return text && isType(*text);
    }
SPECIALIZE_TYPE_TRAITS_END()
