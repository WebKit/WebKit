/*
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
#include "TextDirection.h"
#include "FontFamily.h"
#include "FontDescription.h"
#include <qfontmetrics.h>

#undef min
#undef max
#include <algorithm>

namespace WebCore {

class GraphicsContext;

enum Pitch { UnknownPitch, FixedPitch, VariablePitch };

class Font {
public:
    Font() :m_letterSpacing(0), m_wordSpacing(0) {}
    Font(const FontDescription& fd, short letterSpacing, short wordSpacing) 
      : m_fontDescription(fd),
        m_letterSpacing(letterSpacing),
        m_wordSpacing(wordSpacing)
    {}
    
    bool operator==(const Font& other) const {
        // The platform-specific pointers don't have to be checked, since
        // any result will be fine.
        return (m_fontDescription == other.m_fontDescription &&
                m_letterSpacing == other.m_letterSpacing &&
                m_wordSpacing == other.m_wordSpacing);
    }

    bool operator!=(const Font& other) const {
        return !(*this == other);
    }

    const FontDescription& fontDescription() const { return m_fontDescription; }
    
    const QFontMetrics& fontMetrics() const { return fm; }
    
    int pixelSize() const { return fontDescription().computedPixelSize(); }
    float size() const { return fontDescription().computedSize(); }
    
    void update() const;

    void drawText(GraphicsContext*, int x, int y, int tabWidth, int xpos, const QChar*, int slen, int pos, int len, int width,
        TextDirection, bool visuallyOrdered = false, int from = -1, int to = -1, Color bg = Color()) const;
    float floatWidth(const QChar*, int slen, int pos, int len, int tabWidth, int xpos) const;
    
    int checkSelectionPoint(const QChar*, int slen, int pos, int len, int toAdd, int tabWidth, int xpos,
        int x, TextDirection, bool visuallyOrdered, bool includePartialGlyphs) const;
    IntRect selectionRectForText(int x, int y, int h, int tabWidth, int xpos, 
        const QChar*, int slen, int pos, int len, int width,
        bool rtl, bool visuallyOrdered = false, int from = -1, int to = -1) const;
    void drawHighlightForText(GraphicsContext*, int x, int y, int h, int tabWidth, int xpos, 
        const QChar*, int slen, int pos, int len, int width,
        TextDirection, bool visuallyOrdered = false, int from = -1, int to = -1, Color bg = Color()) const;

    int width(const QChar*, int slen, int pos, int len, int tabWidth, int xpos) const;
    int width(const QChar*, int slen, int tabWidth, int xpos) const;

    bool isSmallCaps() const { return m_fontDescription.smallCaps(); }

    short wordSpacing() const { return m_wordSpacing; }
    short letterSpacing() const { return m_letterSpacing; }
    void setWordSpacing(short s) { m_wordSpacing = s; }
    void setLetterSpacing(short s) { m_letterSpacing = s; }

    bool isFixedPitch() const { return fm.isFixedPitch(); };

    bool isPrinterFont() const { return m_fontDescription.usePrinterFont(); }

    FontFamily& firstFamily() { return m_fontDescription.firstFamily(); }
    const FontFamily& family() const { return m_fontDescription.family(); }

    bool italic() const { return m_fontDescription.italic(); }
    unsigned weight() const { return m_fontDescription.weight(); }

#if __APPLE__
    NSString* getNSFamily() const { return m_fontDescription.family().getNSFamily(); }    
    NSFont* getNSFont() const { return getWebCoreFont().font; }
    const WebCoreFont& getWebCoreFont() const { return fm.getWebCoreFont(); }
#endif

private:
    FontDescription m_fontDescription;
    mutable QFontMetrics fm; // FIXME: This is going to morph into the platform-specific wrapper for measurement/drawing.
    short m_letterSpacing;
    short m_wordSpacing;
};

inline float Font::floatWidth(const QChar* chs, int slen, int pos, int len, int tabWidth, int xpos) const
{
    return fm.floatWidth(chs, slen, pos, len, tabWidth, xpos, m_letterSpacing, m_wordSpacing, m_fontDescription.smallCaps());
}

inline int Font::checkSelectionPoint(const QChar* s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, int x, TextDirection d, bool visuallyOrdered, bool includePartialGlyphs) const
{
    return fm.checkSelectionPoint(s + pos, std::min(slen - pos, len), 0, len, toAdd, tabWidth, xpos, m_letterSpacing, m_wordSpacing, 
        m_fontDescription.smallCaps(), x, d == RTL, visuallyOrdered, includePartialGlyphs);
}

inline int Font::width(const QChar* chs, int slen, int pos, int len, int tabWidth, int xpos) const
{
    // FIXME: Want to define an lroundf for win32.
#if __APPLE__
    return lroundf(fm.floatWidth(chs + pos, slen - pos, 0, len, tabWidth, xpos, 
                                 m_letterSpacing, m_wordSpacing, m_fontDescription.smallCaps()));
#else
    return fm.floatWidth(chs + pos, slen - pos, 0, len, tabWidth, xpos, m_letterSpacing, m_wordSpacing, m_fontDescription.smallCaps()) + 0.5f;
#endif
}

inline int Font::width(const QChar* chs, int slen, int tabWidth, int xpos) const
{
    return width(chs, slen, 0, 1, tabWidth, xpos);
}

}

#endif
