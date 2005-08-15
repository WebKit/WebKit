/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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

#include "font.h"

#include "khtml_factory.h"
#include "khtml_settings.h"

#include <kdebug.h>
#include <kglobal.h>

#include <qpainter.h>
#include <qfontdatabase.h>
#include <qpaintdevicemetrics.h>

#include <algorithm>

using namespace khtml;

#if APPLE_CHANGES
void Font::drawHighlightForText( QPainter *p, int x, int y, int h, int tabWidth, int xpos, 
                     QChar *str, int slen, int pos, int len,
                     int toAdd, QPainter::TextDirection d, bool visuallyOrdered, int from, int to, QColor bg) const
{
    p->drawHighlightForText(x, y, h, tabWidth, xpos, str + pos, std::min(slen - pos, len), from, to, toAdd, bg, d, visuallyOrdered,
                letterSpacing, wordSpacing, fontDef.smallCaps);
}
#endif
                     
void Font::drawText( QPainter *p, int x, int y, int tabWidth, int xpos, QChar *str, int slen, int pos, int len,
                     int toAdd, QPainter::TextDirection d, bool visuallyOrdered, int from, int to, QColor bg ) const
{
#if APPLE_CHANGES
    p->drawText(x, y, tabWidth, xpos, str + pos, std::min(slen - pos, len), from, to, toAdd, bg, d, visuallyOrdered,
                letterSpacing, wordSpacing, fontDef.smallCaps);
#else
    QString qstr = QConstString(str, slen).string();

    //fprintf (stdout, "x %d, y %d, pos %d, qstr.length() %d, len %d, toAdd %d, from %d, to %d, str \"%s\"\n", x, y, pos, qstr.length(), len, toAdd, from, to, qstr.ascii());
    // hack for fonts that don't have a welldefined nbsp
    if ( !fontDef.hasNbsp ) {
        // str.setLength() always does a deep copy, so the replacement code below is safe.
        qstr.setLength( slen );
        QChar *uc = (QChar *)qstr.unicode();
        for( int i = 0; i < slen; i++ )
            if ( (uc+i)->unicode() == 0xa0 )
            *(uc+i) = ' ';
    }

    // ### fixme for RTL
    if ( !letterSpacing && !wordSpacing && !toAdd && from==-1 ) {
        // simply draw it
        p->drawText( x, y, tabWidth, xpos, qstr, pos, len, d );
    } else {
        int numSpaces = 0;
        if ( toAdd ) {
            for( int i = 0; i < len; i++ )
            if ( str[i+pos].direction() == QChar::DirWS )
                numSpaces++;
        }
    
        if ( d == QPainter::RTL ) {
            x += width( str, slen, pos, len ) + toAdd;
        }
        for( int i = 0; i < len; i++ ) {
            int chw = fm.charWidth( qstr, pos+i );
            if ( letterSpacing )
                chw += letterSpacing;
            if (i > 0 && (wordSpacing || toAdd) && str[i+pos].isSpace() && !str[i+pos-1].isSpace()) {
                chw += wordSpacing;
                if ( numSpaces ) {
                    int a = toAdd/numSpaces;
                    chw += a;
                    toAdd -= a;
                    numSpaces--;
                }
            }
            if ( d == QPainter::RTL )
                x -= chw;
            if ( to==-1 || (i>=from && i<to) )
            {
                if ( bg.isValid() )
                    p->fillRect( x, y-fm.ascent(), chw, fm.height(), bg );

                p->drawText( x, y, tabWidth, xpos, qstr, pos+i, 1, d );
            }
            if ( d != QPainter::RTL )
                x += chw;
        }
    }
#endif
}

#if APPLE_CHANGES

float Font::floatWidth( QChar *chs, int slen, int pos, int len, int tabWidth, int xpos ) const
{
    return fm.floatWidth(chs, slen, pos, len, tabWidth, xpos, letterSpacing, wordSpacing, fontDef.smallCaps);
}


void Font::floatCharacterWidths( QChar *str, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, float *buffer) const
{
    fm.floatCharacterWidths(str, slen, pos, len, toAdd, tabWidth, xpos, buffer, letterSpacing, wordSpacing, fontDef.smallCaps);
}

int Font::checkSelectionPoint (QChar *s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, int x, bool reversed, bool includePartialGlyphs) const
{
    return fm.checkSelectionPoint (s, slen, pos, len, toAdd, tabWidth, xpos, letterSpacing, wordSpacing, fontDef.smallCaps, x, reversed, includePartialGlyphs);
}

#endif

int Font::width( QChar *chs, int slen, int pos, int len, int tabWidth, int xpos ) const
{
#if APPLE_CHANGES
#ifndef ROUND_TO_INT
#define ROUND_TO_INT(x) (unsigned int)((x)+.5)
#endif
    return ROUND_TO_INT(fm.floatWidth(chs+pos, slen-pos, 0, len, tabWidth, xpos, letterSpacing, wordSpacing, fontDef.smallCaps));
//    return fm.width(chs + pos, len);
#else
    QString qstr = QConstString(chs+pos, len).string();
    // hack for fonts that don't have a welldefined nbsp
    if ( !fontDef.hasNbsp ) {
	// str.setLength() always does a deep copy, so the replacement code below is safe.
	qstr.setLength( len );
	QChar *uc = (QChar *)qstr.unicode();
	for( int i = 0; i < len; i++ )
	    if ( (uc+i)->unicode() == 0xa0 )
		*(uc+i) = ' ';
    }

    // ### might be a little inaccurate
    int w = fm.width( qstr );

    if ( letterSpacing )
	    w += len*letterSpacing;

    if ( wordSpacing )
        // add amount
        for( int i = 0; i < len; i++ ) {
            if( chs[i+pos].isSpace() )
            w += wordSpacing;
        }

    return w;
#endif
}

int Font::width( QChar *chs, int slen, int tabWidth, int xpos ) const
{
#if APPLE_CHANGES
//    return ROUND_TO_INT(fm.floatWidth(chs, slen, pos, 1, tabWidth, xpos, letterSpacing, wordSpacing));
    return width(chs, slen, 0, 1, tabWidth, xpos);
#else
    int w;
    if ( !fontDef.hasNbsp && (chs+pos)->unicode() == 0xa0 )
	w = fm.width( QChar( ' ' ) );
    else
	w = fm.charWidth( QConstString( chs, slen).string(), pos );

    if ( letterSpacing )
	w += letterSpacing;

    if ( wordSpacing && (chs+pos)->isSpace() )
        w += wordSpacing;
    return w;
#endif
}


void Font::update( QPaintDeviceMetrics* devMetrics ) const
{
#if APPLE_CHANGES
    if (fontDef.family.familyIsEmpty())
	f.setFamily(KHTMLFactory::defaultHTMLSettings()->stdFontName());
    else
	f.setFirstFamily(fontDef.family);
    f.setItalic(fontDef.italic);
    f.setWeight(fontDef.weight);
    f.setPixelSize(fontDef.computedPixelSize());
    f.setPrinterFont(fontDef.usePrinterFont);

    fm.setFont(f);
#else
    f.setFamily( fontDef.family.isEmpty() ? KHTMLFactory::defaultHTMLSettings()->stdFontName() : fontDef.family );
    f.setItalic( fontDef.italic );
    f.setWeight( fontDef.weight );

    QFontDatabase db;

    int size = fontDef.size;
    int lDpiY = kMax(devMetrics->logicalDpiY(), 96);

    // ok, now some magic to get a nice unscaled font
    // all other font properties should be set before this one!!!!
    if( !db.isSmoothlyScalable(f.family(), db.styleString(f)) )
    {
        QValueList<int> pointSizes = db.smoothSizes(f.family(), db.styleString(f));
        // lets see if we find a nice looking font, which is not too far away
        // from the requested one.
        // kdDebug(6080) << "khtml::setFontSize family = " << f.family() << " size requested=" << size << endl;

        QValueList<int>::Iterator it;
        float diff = 1; // ### 100% deviation
        float bestSize = 0;
        for( it = pointSizes.begin(); it != pointSizes.end(); ++it )
        {
            float newDiff = ((*it)*(lDpiY/72.) - float(size))/float(size);
            //kdDebug( 6080 ) << "smooth font size: " << *it << " diff=" << newDiff << endl;
            if(newDiff < 0) newDiff = -newDiff;
            if(newDiff < diff)
            {
                diff = newDiff;
                bestSize = *it;
            }
        }
        //kdDebug( 6080 ) << "best smooth font size: " << bestSize << " diff=" << diff << endl;
        if ( bestSize != 0 && diff < 0.2 ) // 20% deviation, otherwise we use a scaled font...
            size = (int)((bestSize*lDpiY) / 72);
    }

    // make sure we don't bust up X11
    size = KMAX(0, KMIN(255, size));

//      qDebug("setting font to %s, italic=%d, weight=%d, size=%d", fontDef.family.latin1(), fontDef.italic,
//   	   fontDef.weight, size );


    f.setPixelSize( size );

    fm = QFontMetrics( f );

    fontDef.hasNbsp = fm.inFont( 0xa0 );
#endif
}

#ifdef APPLE_CHANGES
bool Font::isFixedPitch() const
{
    return f.isFixedPitch();
}
#endif

