/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#include "DocumentMarker.h"
#include "InlineRunBox.h"
#include "RenderText.h"

namespace WebCore {

const unsigned short cNoTruncation = USHRT_MAX;
const unsigned short cFullTruncation = USHRT_MAX - 1;

class String;
class StringImpl;
class HitTestResult;
class Position;

struct CompositionUnderline;

// Helper functions shared by InlineTextBox / SVGRootInlineBox
void updateGraphicsContext(GraphicsContext* context, const Color& fillColor, const Color& strokeColor, float strokeThickness);
Color correctedTextColor(Color textColor, Color backgroundColor);

class InlineTextBox : public InlineRunBox {
public:
    InlineTextBox(RenderObject* obj)
        : InlineRunBox(obj)
        , m_start(0)
        , m_len(0)
        , m_truncation(cNoTruncation)
    {
    }

    InlineTextBox* nextTextBox() const { return static_cast<InlineTextBox*>(nextLineBox()); }
    InlineTextBox* prevTextBox() const { return static_cast<InlineTextBox*>(prevLineBox()); }

    unsigned start() const { return m_start; }
    unsigned end() const { return m_len ? m_start + m_len - 1 : m_start; }
    unsigned len() const { return m_len; }

    void setStart(unsigned start) { m_start = start; }
    void setLen(unsigned len) { m_len = len; }

    void offsetRun(int d) { m_start += d; }

    virtual int selectionTop();
    virtual int selectionHeight();

    virtual IntRect selectionRect(int absx, int absy, int startPos, int endPos);
    bool isSelected(int startPos, int endPos) const;
    void selectionStartEnd(int& sPos, int& ePos);

    virtual void paint(RenderObject::PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty);

    RenderText* textObject() const;

    virtual void deleteLine(RenderArena*);
    virtual void extractLine();
    virtual void attachLine();

    virtual RenderObject::SelectionState selectionState();

    virtual void clearTruncation() { m_truncation = cNoTruncation; }
    virtual int placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool& foundBox);

    virtual bool isLineBreak() const;

    void setSpaceAdd(int add) { m_width -= m_toAdd; m_toAdd = add; m_width += m_toAdd; }
    int spaceAdd() { return m_toAdd; }

    virtual bool isInlineTextBox() { return true; }    
    virtual bool isText() const { return m_treatAsText; }
    void setIsText(bool b) { m_treatAsText = b; }

    void paintDecoration(GraphicsContext*, int tx, int ty, int decoration);
    void paintSelection(GraphicsContext*, int tx, int ty, RenderStyle*, const Font*);
    void paintCompositionBackground(GraphicsContext*, int tx, int ty, RenderStyle*, const Font*, int startPos, int endPos);
    void paintDocumentMarkers(GraphicsContext*, int tx, int ty, RenderStyle*, const Font*, bool background);
    void paintSpellingOrGrammarMarker(GraphicsContext*, int tx, int ty, DocumentMarker, RenderStyle*, const Font*, bool grammar);
    void paintTextMatchMarker(GraphicsContext*, int tx, int ty, DocumentMarker, RenderStyle*, const Font*);
    void paintCompositionUnderline(GraphicsContext*, int tx, int ty, const CompositionUnderline&);
#if PLATFORM(MAC)
    void paintCustomHighlight(int tx, int ty, const AtomicString& type);
#endif
    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;

    int textPos() const;
    virtual int offsetForPosition(int x, bool includePartialGlyphs = true) const;
    virtual int positionForOffset(int offset) const;

    bool containsCaretOffset(int offset) const; // false for offset after line break

    int m_start;
    unsigned short m_len;

    unsigned short m_truncation; // Where to truncate when text overflow is applied.  We use special constants to
                      // denote no truncation (the whole run paints) and full truncation (nothing paints at all).

private:
    friend class RenderText;
};

inline RenderText* InlineTextBox::textObject() const
{
    return static_cast<RenderText*>(m_object);
}

} // namespace WebCore

#endif // InlineTextBox_h
