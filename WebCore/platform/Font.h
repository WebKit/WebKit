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

#if __APPLE__
#include "WebCoreTextRendererFactory.h"
#endif

#include <kxmlcore/RefPtr.h>
#include "Color.h"
#include "TextDirection.h"
#include "FontFamily.h"
#include "FontDescription.h"

#undef min
#undef max
#include <algorithm>

namespace WebCore {

class FontDataSet;
class GraphicsContext;
class IntRect;

enum Pitch { UnknownPitch, FixedPitch, VariablePitch };

class Font {
public:
    Font();
    Font(const FontDescription& fd, short letterSpacing, short wordSpacing);
    ~Font();
    
    Font(const Font&);
    Font& operator=(const Font&);

    bool operator==(const Font& other) const {
        // The renderer pointer doesn't have to be checked, since
        // checking the font description will be fine.
        return (m_fontDescription == other.m_fontDescription &&
                m_letterSpacing == other.m_letterSpacing &&
                m_wordSpacing == other.m_wordSpacing);
    }

    bool operator!=(const Font& other) const {
        return !(*this == other);
    }

    const FontDescription& fontDescription() const { return m_fontDescription; }

    int pixelSize() const { return fontDescription().computedPixelSize(); }
    float size() const { return fontDescription().computedSize(); }
    
    void update() const;

    void drawText(const GraphicsContext*, int x, int y, int tabWidth, int xpos,
                  const QChar*, int len, int from, int to, int toAdd, 
                  TextDirection, bool visuallyOrdered) const;
    void drawHighlightForText(const GraphicsContext*, int x, int y, int h, int tabWidth, int xpos,
                              const QChar*, int len, int from, int to, int toAdd, 
                              TextDirection d, bool visuallyOrdered, const Color& backgroundColor) const;
    void drawLineForText(const GraphicsContext*, int x, int y, int yOffset, int width) const;
    void drawLineForMisspelling(const GraphicsContext*, int x, int y, int width) const;
    int misspellingLineThickness(const GraphicsContext*) const;

    float floatWidth(const QChar*, int slen, int pos, int len, int tabWidth, int xpos) const;
    
    int checkSelectionPoint(const QChar*, int slen, int pos, int len, int toAdd, int tabWidth, int xpos,
        int x, TextDirection, bool visuallyOrdered, bool includePartialGlyphs) const;
    IntRect selectionRectForText(int x, int y, int h, int tabWidth, int xpos, 
        const QChar*, int slen, int pos, int len, int width,
        bool rtl, bool visuallyOrdered = false, int from = -1, int to = -1) const;
    
    int width(const QChar*, int slen, int pos, int len, int tabWidth, int xpos) const;
    int width(const QChar* chs, int slen, int tabWidth = 0, int xpos = 0) const { return width(chs, slen, 0, slen, tabWidth, xpos); }
    int width(const DeprecatedString& s) const { return width(s.unicode(), s.length(), 0, 0); }

    bool isSmallCaps() const { return m_fontDescription.smallCaps(); }

    short wordSpacing() const { return m_wordSpacing; }
    short letterSpacing() const { return m_letterSpacing; }
    void setWordSpacing(short s) { m_wordSpacing = s; }
    void setLetterSpacing(short s) { m_letterSpacing = s; }

    bool isFixedPitch() const;
    bool isPrinterFont() const { return m_fontDescription.usePrinterFont(); }

    FontFamily& firstFamily() { return m_fontDescription.firstFamily(); }
    const FontFamily& family() const { return m_fontDescription.family(); }

    bool italic() const { return m_fontDescription.italic(); }
    unsigned weight() const { return m_fontDescription.weight(); }

#if __APPLE__
    NSString* getNSFamily() const { return m_fontDescription.family().getNSFamily(); }    
    NSFont* getNSFont() const { return getWebCoreFont().font; }
    const WebCoreFont& getWebCoreFont() const;
#endif

    // Metrics that we query the FontDataSet for.
    int ascent() const;
    int descent() const;
    int height() const { return ascent() + descent(); }
    int lineSpacing() const;
    float xHeight() const;

private:
    FontDescription m_fontDescription;
    mutable RefPtr<FontDataSet> m_dataSet;
    short m_letterSpacing;
    short m_wordSpacing;
};

}

#endif
