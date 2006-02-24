/*
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef FONT_H
#define FONT_H

#include "Color.h"
#include <qfont.h>
#include <qfontmetrics.h>
#include "FontDescription.h"
#include <qpainter.h>

#undef min
#undef max
#include <algorithm>

namespace WebCore {

class RenderStyle;
class CSSStyleSelector;

class Font {
    friend class RenderStyle;
    friend class CSSStyleSelector;

public:
    Font() : m_letterSpacing(0), m_wordSpacing(0) {}
    Font(const FontDescription& fd, int l, int w) : m_fontDescription(fd), m_letterSpacing(l), m_wordSpacing(w) {}

    bool operator==(const Font& other) const {
        return (m_fontDescription == other.m_fontDescription &&
                m_letterSpacing == other.m_letterSpacing &&
                m_wordSpacing == other.m_wordSpacing);
    }

    const FontDescription& fontDescription() const { return m_fontDescription; }
    
    void update() const;

    void drawText(QPainter *p, int x, int y, int tabWidth, int xpos, QChar *str, int slen, int pos, int len, int width,
                  QPainter::TextDirection d, bool visuallyOrdered = false, int from = -1, int to = -1, Color bg = Color()) const;
    float floatWidth(QChar *str, int slen, int pos, int len, int tabWidth, int xpos) const;
    bool isFixedPitch() const;
    int checkSelectionPoint(QChar *s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos,
        int x, QPainter::TextDirection d, bool visuallyOrdered, bool includePartialGlyphs) const;
    IntRect selectionRectForText(int x, int y, int h, int tabWidth, int xpos, 
        QChar *str, int slen, int pos, int len, int width,
        bool rtl, bool visuallyOrdered = false, int from = -1, int to = -1) const;
    void drawHighlightForText(QPainter *p, int x, int y, int h, int tabWidth, int xpos, 
        QChar *str, int slen, int pos, int len, int width,
        QPainter::TextDirection d, bool visuallyOrdered = false, int from = -1, int to = -1, Color bg = Color()) const;

    int width(QChar *str, int slen, int pos, int len, int tabWidth, int xpos) const;
    int width(QChar *str, int slen, int tabWidth, int xpos) const;

    bool isSmallCaps() const { return m_fontDescription.smallCaps(); }
    short wordSpacing() const { return m_wordSpacing; }

private:
    FontDescription m_fontDescription;
    mutable QFont f;
    mutable QFontMetrics fm;
    short m_letterSpacing;
    short m_wordSpacing;
};

inline float Font::floatWidth(QChar *chs, int slen, int pos, int len, int tabWidth, int xpos) const
{
    return fm.floatWidth(chs, slen, pos, len, tabWidth, xpos, m_letterSpacing, m_wordSpacing, m_fontDescription.smallCaps());
}

inline int Font::checkSelectionPoint(QChar *s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, int x, QPainter::TextDirection d, bool visuallyOrdered, bool includePartialGlyphs) const
{
    return fm.checkSelectionPoint(s + pos, std::min(slen - pos, len), 0, len, toAdd, tabWidth, xpos, m_letterSpacing, m_wordSpacing, 
                                  m_fontDescription.smallCaps(), x, d == QPainter::RTL, visuallyOrdered, includePartialGlyphs);
}

inline int Font::width(QChar *chs, int slen, int pos, int len, int tabWidth, int xpos) const
{
    // FIXME: Want to define an lroundf for win32.
#if __APPLE__
    return lroundf(fm.floatWidth(chs + pos, slen - pos, 0, len, tabWidth, xpos, 
                                 m_letterSpacing, m_wordSpacing, m_fontDescription.smallCaps()));
#else
    return fm.floatWidth(chs + pos, slen - pos, 0, len, tabWidth, xpos, m_letterSpacing, m_wordSpacing, m_fontDescription.smallCaps()) + 0.5f;
#endif
}

inline int Font::width(QChar *chs, int slen, int tabWidth, int xpos) const
{
    return width(chs, slen, 0, 1, tabWidth, xpos);
}

inline bool Font::isFixedPitch() const
{
    return f.isFixedPitch();
}

}

#endif
