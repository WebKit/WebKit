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

#include <qcolor.h>
#include <qfont.h>
#include <qfontmetrics.h>
#include <qpainter.h>

class QPaintDeviceMetrics;


namespace khtml
{
class RenderStyle;
class CSSStyleSelector;

class FontDef
{
public:
    FontDef()
        : specifiedSize(0), computedSize(0), 
          italic( false ), smallCaps( false ), isAbsoluteSize(false), weight( 50 ), 
          genericFamily(0), hasNbsp( true )
#if APPLE_CHANGES
          , usePrinterFont( false )
#endif
          {}
    
    bool operator == ( const FontDef &other ) const {
        return ( family == other.family &&
                 specifiedSize == other.specifiedSize &&
                 computedSize == other.computedSize &&
                 italic == other.italic &&
                 smallCaps == other.smallCaps &&
                 weight == other.weight &&
                 isAbsoluteSize == other.isAbsoluteSize
#if APPLE_CHANGES
                 && usePrinterFont == other.usePrinterFont
#endif
                 );
    }

    enum GenericFamilyType { eNone, eStandard, eSerif, eSansSerif, eMonospace, eCursive, eFantasy };

    void setGenericFamily(unsigned int aGenericFamily) { genericFamily = aGenericFamily; }
    
    KWQFontFamily &firstFamily() { return family; }
    
    KWQFontFamily family;

    int computedPixelSize() const { return int(computedSize+0.5); }
    
    float specifiedSize; // This is the specified CSS value.  It is independent of rendering issues such as
                         // integer rounding, minimum font sizes, and zooming.
    float computedSize; // This is the computed size adjusted for the minimum font size and the zoom
                        // factor.  
    
    bool italic 		: 1;
    bool smallCaps 		: 1;
    bool isAbsoluteSize    : 1;  // Whether or not CSS specified an explicit size
                                      // (logical sizes like "medium" don't count).
    unsigned int weight 	: 8;
    unsigned int genericFamily	: 3;
    mutable bool hasNbsp	: 1;
#if APPLE_CHANGES
    bool usePrinterFont		: 1;
#endif
};


class Font
{
    friend class RenderStyle;
    friend class CSSStyleSelector;

public:
#if APPLE_CHANGES
    Font() : letterSpacing(0), wordSpacing(0) {}
    Font(const FontDef &fd, int l, int w) : fontDef(fd), letterSpacing(l), wordSpacing(w) {}
#else
    Font() : fontDef(), f(), fm( f ), scFont( 0 ), letterSpacing( 0 ), wordSpacing( 0 ) {}
    Font( const FontDef &fd, int l, int w )
        :  fontDef( fd ), f(), fm( f ), scFont( 0 ), letterSpacing( l ), wordSpacing( w )
        {}
#endif

    bool operator == ( const Font &other ) const {
        return (fontDef == other.fontDef &&
                letterSpacing == other.letterSpacing &&
                wordSpacing == other.wordSpacing );
    }

    const FontDef& getFontDef() const { return fontDef; }
    
    void update( QPaintDeviceMetrics *devMetrics ) const;

                   
#if !APPLE_CHANGES
    void drawText( QPainter *p, int x, int y, int tabWidth, int xpos, QChar *str, int slen, int pos, int len, int width,
                   QPainter::TextDirection d, int from=-1, int to=-1, QColor bg=QColor() ) const;

#else
    void drawText( QPainter *p, int x, int y, int tabWidth, int xpos, QChar *str, int slen, int pos, int len, int width,
                   QPainter::TextDirection d, bool visuallyOrdered = false, int from=-1, int to=-1, QColor bg=QColor() ) const;
    float floatWidth( QChar *str, int slen, int pos, int len, int tabWidth, int xpos ) const;
    void floatCharacterWidths( QChar *str, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, float *buffer) const;
    bool isFixedPitch() const;
    int checkSelectionPoint (QChar *s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, int x, bool reversed, bool includePartialGlyphs) const;
    void drawHighlightForText( QPainter *p, int x, int y, int h, int tabWidth, int xpos, 
                   QChar *str, int slen, int pos, int len, int width,
                   QPainter::TextDirection d, bool visuallyOrdered = false, int from=-1, int to=-1, QColor bg=QColor()) const;
#endif
    int width( QChar *str, int slen, int pos, int len, int tabWidth, int xpos ) const;
    int width( QChar *str, int slen, int tabWidth, int xpos ) const;

    bool isSmallCaps() const { return fontDef.smallCaps; }
    short getWordSpacing() const { return wordSpacing; }
private:
    FontDef fontDef;
    mutable QFont f;
    mutable QFontMetrics fm;
#if !APPLE_CHANGES
    QFont *scFont;
#endif
    short letterSpacing;
    short wordSpacing;
};

};

#endif
