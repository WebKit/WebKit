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

#pragma once

#include "RenderSVGInlineText.h"

namespace WebCore {

// SVG analogue of RenderTextFragment, used for the ::first-letter split on
// SVG <text>. Behaves like RenderSVGInlineText for SVG layout/painting (so
// the existing pipeline continues to walk RenderSVGInline/RenderSVGInlineText
// without widening), and like RenderTextFragment for accessibility / text
// editing semantics (originalText returns the full Text node data; m_start
// / m_end track the substring offsets).
class RenderSVGInlineTextFragment final : public RenderSVGInlineText {
    WTF_MAKE_TZONE_ALLOCATED(RenderSVGInlineTextFragment);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSVGInlineTextFragment);
public:
    RenderSVGInlineTextFragment(Text&, const String&, unsigned startOffset, unsigned length);
    RenderSVGInlineTextFragment(Document&, const String&, unsigned startOffset, unsigned length);
    virtual ~RenderSVGInlineTextFragment();

    unsigned start() const { return m_start; }
    unsigned end() const { return m_end; }

    RenderBoxModelObject* firstLetter() const { return m_firstLetter.get(); }
    void setFirstLetter(RenderBoxModelObject& firstLetter) { m_firstLetter = firstLetter; }

    CheckedPtr<RenderBlock> blockForAccompanyingFirstLetter();

    void setContentString(const String&);
    StringImpl* contentString() const { return m_contentString.impl(); }

private:
    ASCIILiteral renderName() const final { return "RenderSVGInlineTextFragment"_s; }

    String originalText() const final;
    bool canBeSelectionLeaf() const final;
    void setTextInternal(const String&, bool force) final;
    void setTextWithOffset(const String&, unsigned offset) final;
    Node* nodeForHitTest() const final;
    char32_t previousCharacter() const final;

    unsigned m_start;
    unsigned m_end;
    String m_contentString;
    SingleThreadWeakPtr<RenderBoxModelObject> m_firstLetter;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::RenderSVGInlineTextFragment)
    static bool isType(const WebCore::RenderObject& renderer) { return renderer.isRenderSVGInlineTextFragment(); }
SPECIALIZE_TYPE_TRAITS_END()
