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

#ifndef KHTMLFONT_H
#define KHTMLFONT_H

#if WIN32
#include <math.h> // FIXME: This is here because of the use of floorf instead of lroundf on Win32.
#endif

#include "Color.h"
#include <qfont.h>
#include <qfontmetrics.h>
#include <qpainter.h>

#undef min
#undef max
#include <algorithm>

namespace khtml {

class RenderStyle;
class CSSStyleSelector;

class FontDef {
public:
    enum GenericFamilyType { eNone, eStandard, eSerif, eSansSerif, eMonospace, eCursive, eFantasy };

    FontDef()
        : specifiedSize(0), computedSize(0), 
          italic(false), smallCaps(false), isAbsoluteSize(false), weight(50), 
          genericFamily(0), usePrinterFont(false)
          {}
    
    void setGenericFamily(GenericFamilyType aGenericFamily) { genericFamily = aGenericFamily; }
    
    FontFamily &firstFamily() { return family; }
    int computedPixelSize() const { return int(computedSize + 0.5f); }

    FontFamily family;

    float specifiedSize; // Specified CSS value. Independent of rendering issues such as integer
                         // rounding, minimum font sizes, and zooming.
    float computedSize;  // Computed size adjusted for the minimum font size and the zoom factor.  

    bool italic                 : 1;
    bool smallCaps              : 1;
    bool isAbsoluteSize         : 1;  // Whether or not CSS specified an explicit size
                                      // (logical sizes like "medium" don't count).
    unsigned int weight         : 8;
    unsigned int genericFamily  : 3;
    bool usePrinterFont         : 1;
};

inline bool operator==(const FontDef& a, const FontDef& b)
{
    return a.family == b.family
        && a.specifiedSize == b.specifiedSize
        && a.computedSize == b.computedSize
        && a.italic == b.italic
        && a.smallCaps == b.smallCaps
        && a.isAbsoluteSize == b.isAbsoluteSize
        && a.weight == b.weight
        && a.genericFamily == b.genericFamily
        && a.usePrinterFont == b.usePrinterFont;
}

class Font {
    friend class RenderStyle;
    friend class CSSStyleSelector;

public:
    Font() : letterSpacing(0), wordSpacing(0) {}
    Font(const FontDef &fd, int l, int w) : fontDef(fd), letterSpacing(l), wordSpacing(w) {}

    bool operator ==(const Font& other) const {
        return (fontDef == other.fontDef &&
                letterSpacing == other.letterSpacing &&
                wordSpacing == other.wordSpacing );
    }

    const FontDef& getFontDef() const { return fontDef; }
    
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

    bool isSmallCaps() const { return fontDef.smallCaps; }
    short getWordSpacing() const { return wordSpacing; }

private:
    FontDef fontDef;
    mutable QFont f;
    mutable QFontMetrics fm;
    short letterSpacing;
    short wordSpacing;
};

inline float Font::floatWidth(QChar *chs, int slen, int pos, int len, int tabWidth, int xpos) const
{
    return fm.floatWidth(chs, slen, pos, len, tabWidth, xpos, letterSpacing, wordSpacing, fontDef.smallCaps);
}

inline int Font::checkSelectionPoint(QChar *s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, int x, QPainter::TextDirection d, bool visuallyOrdered, bool includePartialGlyphs) const
{
    return fm.checkSelectionPoint(s + pos, std::min(slen - pos, len), 0, len, toAdd, tabWidth, xpos, letterSpacing, wordSpacing, fontDef.smallCaps, x, d == QPainter::RTL, visuallyOrdered, includePartialGlyphs);
}

inline int Font::width(QChar *chs, int slen, int pos, int len, int tabWidth, int xpos) const
{
    // FIXME: Want to define an lroundf for win32.
#if __APPLE__
    return lroundf(fm.floatWidth(chs + pos, slen - pos, 0, len, tabWidth, xpos, letterSpacing, wordSpacing, fontDef.smallCaps));
#else
    return floorf(fm.floatWidth(chs + pos, slen - pos, 0, len, tabWidth, xpos, letterSpacing, wordSpacing, fontDef.smallCaps) + 0.5);
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
