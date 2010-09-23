/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef InlineTextBox_h
#define InlineTextBox_h

#include "InlineBox.h"
#include "RenderText.h" // so textRenderer() can be inline

namespace WebCore {

struct CompositionUnderline;

const unsigned short cNoTruncation = USHRT_MAX;
const unsigned short cFullTruncation = USHRT_MAX - 1;

// Helper functions shared by InlineTextBox / SVGRootInlineBox
void updateGraphicsContext(GraphicsContext*, const Color& fillColor, const Color& strokeColor, float strokeThickness, ColorSpace);
Color correctedTextColor(Color textColor, Color backgroundColor);

class InlineTextBox : public InlineBox {
public:
    InlineTextBox(RenderObject* obj)
        : InlineBox(obj)
        , m_prevTextBox(0)
        , m_nextTextBox(0)
        , m_start(0)
        , m_len(0)
        , m_truncation(cNoTruncation)
    {
    }

    InlineTextBox* prevTextBox() const { return m_prevTextBox; }
    InlineTextBox* nextTextBox() const { return m_nextTextBox; }
    void setNextTextBox(InlineTextBox* n) { m_nextTextBox = n; }
    void setPreviousTextBox(InlineTextBox* p) { m_prevTextBox = p; }

    unsigned start() const { return m_start; }
    unsigned end() const { return m_len ? m_start + m_len - 1 : m_start; }
    unsigned len() const { return m_len; }

    void setStart(unsigned start) { m_start = start; }
    void setLen(unsigned len) { m_len = len; }

    void offsetRun(int d) { m_start += d; }

    unsigned short truncation() { return m_truncation; }

    bool hasHyphen() const { return m_hasEllipsisBoxOrHyphen; }
    void setHasHyphen(bool hasHyphen) { m_hasEllipsisBoxOrHyphen = hasHyphen; }
    static inline bool compareByStart(const InlineTextBox* first, const InlineTextBox* second) { return first->start() < second->start(); }

private:
    virtual int selectionTop();
    virtual int selectionHeight();

public:
    virtual IntRect calculateBoundaries() const { return IntRect(x(), y(), logicalWidth(), logicalHeight()); }

    virtual IntRect selectionRect(int absx, int absy, int startPos, int endPos);
    bool isSelected(int startPos, int endPos) const;
    virtual void selectionStartEnd(int& sPos, int& ePos);

protected:
    virtual void paint(PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty);

public:
    RenderText* textRenderer() const;

private:
    virtual void deleteLine(RenderArena*);
    virtual void extractLine();
    virtual void attachLine();

public:
    virtual RenderObject::SelectionState selectionState();

private:
    virtual void clearTruncation() { m_truncation = cNoTruncation; }
    virtual int placeEllipsisBox(bool flowIsLTR, int visibleLeftEdge, int visibleRightEdge, int ellipsisWidth, bool& foundBox);

public:
    virtual bool isLineBreak() const;

    void setSpaceAdd(int add) { m_logicalWidth -= m_toAdd; m_toAdd = add; m_logicalWidth += m_toAdd; }

private:
    virtual bool isInlineTextBox() const { return true; }    

public:
    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;

private:
    virtual unsigned caretMaxRenderedOffset() const;

    int textPos() const;

public:
    virtual int offsetForPosition(int x, bool includePartialGlyphs = true) const;
    virtual int positionForOffset(int offset) const;

    bool containsCaretOffset(int offset) const; // false for offset after line break

    // Needs to be public, so the static paintTextWithShadows() function can use it.
    static FloatSize applyShadowToGraphicsContext(GraphicsContext*, const ShadowData*, const FloatRect& textRect, bool stroked, bool opaque);

private:
    InlineTextBox* m_prevTextBox; // The previous box that also uses our RenderObject
    InlineTextBox* m_nextTextBox; // The next box that also uses our RenderObject

    int m_start;
    unsigned short m_len;

    unsigned short m_truncation; // Where to truncate when text overflow is applied.  We use special constants to
                      // denote no truncation (the whole run paints) and full truncation (nothing paints at all).

protected:
    void paintCompositionBackground(GraphicsContext*, int tx, int ty, RenderStyle*, const Font&, int startPos, int endPos);
    void paintDocumentMarkers(GraphicsContext*, int tx, int ty, RenderStyle*, const Font&, bool background);
    void paintCompositionUnderline(GraphicsContext*, int tx, int ty, const CompositionUnderline&);
#if PLATFORM(MAC)
    void paintCustomHighlight(int tx, int ty, const AtomicString& type);
#endif

private:
    void paintDecoration(GraphicsContext*, int tx, int ty, int decoration, const ShadowData*);
    void paintSelection(GraphicsContext*, int tx, int ty, RenderStyle*, const Font&);
    void paintSpellingOrGrammarMarker(GraphicsContext*, int tx, int ty, const DocumentMarker&, RenderStyle*, const Font&, bool grammar);
    void paintTextMatchMarker(GraphicsContext*, int tx, int ty, const DocumentMarker&, RenderStyle*, const Font&);
    void computeRectForReplacementMarker(int tx, int ty, const DocumentMarker&, RenderStyle*, const Font&);
};

inline RenderText* InlineTextBox::textRenderer() const
{
    return toRenderText(renderer());
}

} // namespace WebCore

#endif // InlineTextBox_h
