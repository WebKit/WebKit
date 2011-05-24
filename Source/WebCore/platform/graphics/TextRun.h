/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2011 Apple Inc. All rights reserved.
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

#ifndef TextRun_h
#define TextRun_h

#include "TextDirection.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FloatPoint;
class FloatRect;
class Font;
class GraphicsContext;

class TextRun {
public:
    enum ExpansionBehaviorFlags {
        ForbidTrailingExpansion = 0 << 0,
        AllowTrailingExpansion = 1 << 0,
        ForbidLeadingExpansion = 0 << 1,
        AllowLeadingExpansion = 1 << 1,
    };

    typedef unsigned ExpansionBehavior;

    TextRun(const UChar* c, int len, bool allowTabs = false, float xpos = 0, float expansion = 0, ExpansionBehavior expansionBehavior = AllowTrailingExpansion | ForbidLeadingExpansion, TextDirection direction = LTR, bool directionalOverride = false)
        : m_characters(c)
        , m_len(len)
        , m_xpos(xpos)
        , m_expansion(expansion)
        , m_expansionBehavior(expansionBehavior)
#if ENABLE(SVG)
        , m_horizontalGlyphStretch(1)
#endif
        , m_allowTabs(allowTabs)
        , m_direction(direction)
        , m_directionalOverride(directionalOverride)
        , m_disableSpacing(false)
    {
    }

    TextRun(const String& s, bool allowTabs = false, float xpos = 0, float expansion = 0, ExpansionBehavior expansionBehavior = AllowTrailingExpansion | ForbidLeadingExpansion, TextDirection direction = LTR, bool directionalOverride = false)
        : m_characters(s.characters())
        , m_len(s.length())
        , m_xpos(xpos)
        , m_expansion(expansion)
        , m_expansionBehavior(expansionBehavior)
#if ENABLE(SVG)
        , m_horizontalGlyphStretch(1)
#endif
        , m_allowTabs(allowTabs)
        , m_direction(direction)
        , m_directionalOverride(directionalOverride)
        , m_disableSpacing(false)
    {
    }

    UChar operator[](int i) const { return m_characters[i]; }
    const UChar* data(int i) const { return &m_characters[i]; }

    const UChar* characters() const { return m_characters; }
    int length() const { return m_len; }

    void setText(const UChar* c, int len) { m_characters = c; m_len = len; }

#if ENABLE(SVG)
    float horizontalGlyphStretch() const { return m_horizontalGlyphStretch; }
    void setHorizontalGlyphStretch(float scale) { m_horizontalGlyphStretch = scale; }
#endif

    bool allowTabs() const { return m_allowTabs; }
    void setAllowTabs(bool allowTabs) { m_allowTabs = allowTabs; }
    float xPos() const { return m_xpos; }
    void setXPos(float xPos) { m_xpos = xPos; }
    float expansion() const { return m_expansion; }
    bool allowsLeadingExpansion() const { return m_expansionBehavior & AllowLeadingExpansion; }
    bool allowsTrailingExpansion() const { return m_expansionBehavior & AllowTrailingExpansion; }
    TextDirection direction() const { return m_direction; }
    bool rtl() const { return m_direction == RTL; }
    bool ltr() const { return m_direction == LTR; }
    bool directionalOverride() const { return m_directionalOverride; }
    bool spacingDisabled() const { return m_disableSpacing; }

    void disableSpacing() { m_disableSpacing = true; }
    void setDirection(TextDirection direction) { m_direction = direction; }
    void setDirectionalOverride(bool override) { m_directionalOverride = override; }

    class RenderingContext : public RefCounted<RenderingContext> {
    public:
        virtual ~RenderingContext() { }

#if ENABLE(SVG_FONTS)
        // FIXME: Note that the SVG Font integration APIs will be more abstract and simpler once the SVG Fonts rewrite patch lands (59085).
        virtual void drawTextUsingSVGFont(const Font&, GraphicsContext*, const TextRun&, const FloatPoint&, int from, int to) const = 0;
        virtual float floatWidthUsingSVGFont(const Font&, const TextRun&) const = 0;
        virtual float floatWidthUsingSVGFont(const Font&, const TextRun&, int extraCharsAvailable, int& charsConsumed, String& glyphName) const = 0;
        virtual FloatRect selectionRectForTextUsingSVGFont(const Font&, const TextRun&, const FloatPoint&, int h, int from, int to) const = 0;
        virtual int offsetForPositionForTextUsingSVGFont(const Font&, const TextRun&, float position, bool includePartialGlyphs) const = 0;
#endif
    };

    RenderingContext* renderingContext() const { return m_renderingContext.get(); }
    void setRenderingContext(PassRefPtr<RenderingContext> context) { m_renderingContext = context; }

private:
    const UChar* m_characters;
    int m_len;

    // m_xpos is the x position relative to the left start of the text line, not relative to the left
    // start of the containing block. In the case of right alignment or center alignment, left start of
    // the text line is not the same as left start of the containing block.
    float m_xpos;  
    float m_expansion;
    ExpansionBehavior m_expansionBehavior;
#if ENABLE(SVG)
    float m_horizontalGlyphStretch;
#endif
    bool m_allowTabs;
    TextDirection m_direction;
    bool m_directionalOverride; // Was this direction set by an override character.
    bool m_disableSpacing;
    RefPtr<RenderingContext> m_renderingContext;
};

}

#endif
