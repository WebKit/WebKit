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
        : size( 0.0f ), italic( false ), smallCaps( false ), sizeSpecified(false), weight( 50 ), 
          genericFamily(0), hasNbsp( true )
#if APPLE_CHANGES
          , usePrinterFont( false )
#endif
          , logicalSize(3)
          {}
    
    bool operator == ( const FontDef &other ) const {
        return ( family == other.family &&
                 size == other.size &&
                 italic == other.italic &&
                 smallCaps == other.smallCaps &&
                 weight == other.weight &&
                 sizeSpecified == other.sizeSpecified
#if APPLE_CHANGES
                 && usePrinterFont == other.usePrinterFont
#endif
                 && logicalSize == other.logicalSize
                 );
    }

    enum GenericFamilyType { eNone, eStandard, eSerif, eSansSerif, eMonospace, eCursive, eFantasy };

    void setGenericFamily(unsigned int aGenericFamily) { genericFamily = aGenericFamily; }
    
    KWQFontFamily &firstFamily() { return family; }
    
    KWQFontFamily family;

    int pixelSize() const { return int(size + 0.5f); }
    float floatSize() const { return size; }
    void setSize(float s) { size = s; }
    
private:
    float size; // Making this private forces people to think about what value they want,
                // either the floatSize or the pixelSize.

public:
    bool italic 		: 1;
    bool smallCaps 		: 1;
    bool sizeSpecified		: 1;  // Whether or not CSS specified an explicit size
                                      // (logical sizes like "medium" don't count).
    unsigned int weight 	: 8;
    unsigned int genericFamily	: 3;
    mutable bool hasNbsp	: 1;
#if APPLE_CHANGES
    bool usePrinterFont		: 1;
#endif
    short logicalSize;   	       // If a logical size is in effect, then we need to
                                       // cache this value for family changes (e.g., medium
                                       // monospace may be different than medium serif).
};


class Font
{
    friend class RenderStyle;
    friend class CSSStyleSelector;

public:
#if APPLE_CHANGES
    Font() : letterSpacing(0), wordSpacing(0) {}
    Font(const FontDef &fd) : fontDef(fd), letterSpacing(0), wordSpacing(0) {}
#else
    Font() : fontDef(), f(), fm( f ), scFont( 0 ), letterSpacing( 0 ), wordSpacing( 0 ) {}
    Font( const FontDef &fd )
        :  fontDef( fd ), f(), fm( f ), scFont( 0 ), letterSpacing( 0 ), wordSpacing( 0 )
        {}
#endif

    bool operator == ( const Font &other ) const {
        return (fontDef == other.fontDef &&
                letterSpacing == other.letterSpacing &&
                wordSpacing == other.wordSpacing );
    }

    const FontDef& getFontDef() const { return fontDef; }
    
    void update( QPaintDeviceMetrics *devMetrics ) const;

                   
    void drawText( QPainter *p, int x, int y, QChar *str, int slen, int pos, int len, int width,
                   QPainter::TextDirection d, int from=-1, int to=-1, QColor bg=QColor() ) const;

#if APPLE_CHANGES
    float floatWidth( QChar *str, int slen, int pos, int len ) const;
    void floatCharacterWidths( QChar *str, int slen, int pos, int len, int toAdd, float *buffer) const;
    bool isFixedPitch() const;
#endif
    int width( QChar *str, int slen, int pos, int len ) const;
    int width( QChar *str, int slen, int pos ) const;

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
