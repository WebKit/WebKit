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

#ifndef RenderTextFragment_h
#define RenderTextFragment_h

#include "RenderText.h"

namespace WebCore {

// Used to represent a text substring of an element, e.g., for text runs that are split because of
// first letter and that must therefore have different styles (and positions in the render tree).
// We cache offsets so that text transformations can be applied in such a way that we can recover
// the original unaltered string from our corresponding DOM node.
class RenderTextFragment final : public RenderText {
public:
    RenderTextFragment(Text&, const String&, int startOffset, int length);
    RenderTextFragment(Document&, const String&, int startOffset, int length);
    RenderTextFragment(Document&, const String&);

    virtual ~RenderTextFragment();

    virtual bool isTextFragment() const override { return true; }

    virtual bool canBeSelectionLeaf() const override;

    unsigned start() const { return m_start; }
    unsigned end() const { return m_end; }

    RenderBoxModelObject* firstLetter() const { return m_firstLetter; }
    void setFirstLetter(RenderBoxModelObject& firstLetter) { m_firstLetter = &firstLetter; }
    
    RenderBlock* blockForAccompanyingFirstLetter();

    void setContentString(const String& text);
    StringImpl* contentString() const { return m_contentString.impl(); }

    virtual void setText(const String&, bool force = false) override;

    const String& altText() const { return m_altText; }
    void setAltText(const String& altText) { m_altText = altText; }
    
private:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    virtual void willBeDestroyed() override;

    virtual UChar previousCharacter() const override;

    unsigned m_start;
    unsigned m_end;
    // Alternative description that can be used for accessibility instead of the native text.
    String m_altText;
    String m_contentString;
    RenderBoxModelObject* m_firstLetter;
};

RENDER_OBJECT_TYPE_CASTS(RenderTextFragment, isText() && toRenderText(renderer).isTextFragment())

} // namespace WebCore

#endif // RenderTextFragment_h
