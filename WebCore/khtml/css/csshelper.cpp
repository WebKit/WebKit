/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
 * $Id$
 */
#include "csshelper.h"

#include <qfontmetrics.h>
#include <qfontinfo.h>
#include <qpaintdevice.h>
#include <qpaintdevicemetrics.h>
#include <qfontdatabase.h>

#include <kcharsets.h>
#include <kglobal.h>
#include <kdebug.h>

#include "rendering/render_style.h"
#include "css_valueimpl.h"
#include "dom/css_value.h"
#include "misc/helper.h"
#include "xml/dom_stringimpl.h"
#include "khtml_settings.h"

using namespace DOM;
using namespace khtml;

int khtml::computeLength(DOM::CSSPrimitiveValueImpl *val, RenderStyle *style, QPaintDeviceMetrics *devMetrics )
{
    return ( int ) computeLengthFloat( val, style, devMetrics );
}

float khtml::computeLengthFloat(DOM::CSSPrimitiveValueImpl *val, RenderStyle *style, QPaintDeviceMetrics *devMetrics )
{
    unsigned short type = val->primitiveType();

    float dpiY = 72.; // fallback
    if ( devMetrics )
        dpiY = devMetrics->logicalDpiY();
    if ( !khtml::printpainter && dpiY < 96 )
        dpiY = 96.;

    float factor = 1.;
    switch(type)
    {
    case CSSPrimitiveValue::CSS_EMS:
       	factor = style->font().pixelSize();
		break;
    case CSSPrimitiveValue::CSS_EXS:
	{
        QFontMetrics fm = khtml::fontMetrics(style->font());
        QRect b = fm.boundingRect('x');
        factor = b.height();
        break;
	}
    case CSSPrimitiveValue::CSS_PX:
        break;
    case CSSPrimitiveValue::CSS_CM:
	factor = dpiY/2.54; //72dpi/(2.54 cm/in)
        break;
    case CSSPrimitiveValue::CSS_MM:
	factor = dpiY/25.4;
        break;
    case CSSPrimitiveValue::CSS_IN:
            factor = dpiY;
        break;
    case CSSPrimitiveValue::CSS_PT:
            factor = dpiY/72.;
        break;
    case CSSPrimitiveValue::CSS_PC:
        // 1 pc == 12 pt
            factor = dpiY*12./72.;
        break;
    default:
        return -1;
    }
    return val->getFloatValue(type)*factor;
}

DOMString khtml::parseURL(const DOMString &url)
{
    DOMStringImpl* i = url.implementation();
    if(!i) return DOMString();

    int o = 0;
    int l = i->l;
    while(o < l && (i->s[o] <= ' ')) { o++; l--; }
    while(l > 0 && (i->s[o+l-1] <= ' ')) l--;

    if(l >= 5 &&
       (i->s[o].lower() == 'u') &&
       (i->s[o+1].lower() == 'r') &&
       (i->s[o+2].lower() == 'l') &&
       i->s[o+3].latin1() == '(' &&
       i->s[o+l-1].latin1() == ')') {
        o += 4;
        l -= 5;
    }

    while(o < l && (i->s[o] <= ' ')) { o++; l--; }
    while(l > 0 && (i->s[o+l-1] <= ' ')) l--;

    if(l >= 2 && i->s[o] == i->s[o+l-1] &&
       (i->s[o].latin1() == '\'' || i->s[o].latin1() == '\"')) {
        o++;
        l -= 2;
    }

    while(o < l && (i->s[o] <= ' ')) { o++; l--; }
    while(l > 0 && (i->s[o+l-1] <= ' ')) l--;

    DOMStringImpl* j = new DOMStringImpl(i->s+o,l);

    int nl = 0;
    for(int k = o; k < o+l; k++)
        if(i->s[k].unicode() > '\r')
            j->s[nl++] = i->s[k];

    j->l = nl;

    return j;
}


void khtml::setFontSize( QFont &f,  int  pixelsize, const KHTMLSettings *s, QPaintDeviceMetrics *devMetrics )
{
    QFontDatabase db;

    float size = pixelsize;

    float toPix = 1.;
    if ( !khtml::printpainter )
        toPix = devMetrics->logicalDpiY()/72.;

    // ok, now some magic to get a nice unscaled font
    // ### all other font properties should be set before this one!!!!
    // ####### make it use the charset needed!!!!
    QFont::CharSet cs = s->charset();
    QString charset = KGlobal::charsets()->xCharsetName( cs );

    if( !db.isSmoothlyScalable(f.family(), db.styleString(f), charset) )
    {
        QValueList<int> pointSizes = db.smoothSizes(f.family(), db.styleString(f), charset);
        // lets see if we find a nice looking font, which is not too far away
        // from the requested one.
        //kdDebug() << "khtml::setFontSize family = " << f.family() << " size requested=" << size << endl;

        QValueList<int>::Iterator it;
        float diff = 1; // ### 100% deviation
        float bestSize = 0;
        for( it = pointSizes.begin(); it != pointSizes.end(); ++it )
        {
            float newDiff = ((*it)*toPix - size)/size;
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
            size = bestSize*toPix;
//         else if ( size > 4 && size < 16 )
//             size = float( int( ( size + 1 ) / 2 )*2 );
    }

    //qDebug(" -->>> using %f pixel font", size);

    f.setPixelSizeFloat( size );
}
