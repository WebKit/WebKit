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
#include "helper.h"
#include <qmap.h>
#include <qpainter.h>
#include <dom/dom_string.h>
#include <xml/dom_stringimpl.h>
#include <qlist.h>
#include <khtmllayout.h>
#include <kstaticdeleter.h>
#include <kapp.h>
#include <kconfig.h>
#include <qtooltip.h>

using namespace DOM;
using namespace khtml;

// ### make it const if possible...
 struct HTMLColors {
    QMap<QString,QColor> map;
    HTMLColors()
    {
        map["black"] = "#000000";
        map["green"] = "#008000";
        map["silver"] = "#c0c0c0";
        map["lime"] = "#00ff00";
        map["gray"] = "#808080";
        map["olive"] = "#808000";
        map["white"] = "#ffffff";
        map["yellow"] = "#ffff00";
        map["maroon"] = "#800000";
        map["navy"] = "#000080";
        map["red"] = "#ff0000";
        map["blue"] = "#0000ff";
        map["purple"] = "#800080";
        map["teal"] = "#008080";
        map["fuchsia"] = "#ff00ff";
        map["aqua"] = "#00ffff";
	map["crimson"] = "#dc143c";
	map["indigo"] = "#4b0082";
        // ### react to style changes
        // see http://www.richinstyle.com for details

	/* Mapping system settings to CSS 2
	 * Tried hard to get an appropriate mapping - schlpbch
	 */

	KConfig *globalConfig = KGlobal::config();
	globalConfig->setGroup("WM");

	QColorGroup cg = kapp->palette().active();

	// Active window border.
        map["activeborder"] = globalConfig->readColorEntry( "background", &cg.light());
	// Active window caption.
        map["activecaption"] = globalConfig->readColorEntry( "activeBackground", &cg.text());
        // Text in caption, size box, and scrollbar arrow box.
	map["captiontext"] = globalConfig->readColorEntry( "activeForeground", &cg.text());

        cg = kapp->palette().inactive();

	/* Don't know how to deal with buttons correctly */

	// Face color for three-dimensional display elements.
        map["buttonface"] = cg.button();
	// Dark shadow for three-dimensional display elements (for edges facing away from the light source).
        map["buttonhighlight"] = cg.light();
	// Shadow color for three-dimensional display elements.
        map["buttonshadow"] = cg.shadow();
	// Text on push buttons.
        map["buttontext"] = globalConfig->readColorEntry( "buttonForeground", &cg.buttonText());

	// Dark shadow for three-dimensional display elements.
        map["threeddarkshadow"] = cg.dark();
	// Face color for three-dimensional display elements.
        map["threedface"] = cg.button();
	// Highlight color for three-dimensional display elements.
        map["threedhighlight"] = cg.light();
	// Light color for three-dimensional display elements (for edges facing the light source).
        map["threedlightshadow"] = cg.midlight();
	// Dark shadow for three-dimensional display elements.
        map["threedshadow"] = cg.shadow();

	// InfoBackground
#ifndef QT_NO_TOOLTIP
        map["infobackground"] = QToolTip::palette().inactive().background();
	// InfoText
        map["infotext"] = QToolTip::palette().inactive().foreground();
#endif

	globalConfig->setGroup("General");
	// Menu background
        map["menu"] = globalConfig->readColorEntry( "background", &cg.background());
	// Text in menus
        map["menutext"] = globalConfig->readColorEntry( "foreground", &cg.background());

	// Item(s) selected in a control.
        map["highlight"] = globalConfig->readColorEntry( "background", &cg.background()).light(110);
        // Text of item(s) selected in a control.
	map["highlighttext"] = globalConfig->readColorEntry( "foreground", &cg.background());

	// Background color of multiple document interface.
        map["appworkspace"] = globalConfig->readColorEntry( "background", &cg.text());

	// Scroll bar gray area.
        map["scrollbar"] = globalConfig->readColorEntry( "background", &cg.background());

	// Window background.
        map["window"] = globalConfig->readColorEntry( "windowBackground", &cg.background());
	// Window frame.
        map["windowframe"] = globalConfig->readColorEntry( "windowBackground",&cg.background());
        // WindowText
	map["windowtext"] = globalConfig->readColorEntry( "windowForeground", &cg.text());
        map["text"] = cg.text();

        cg = kapp->palette().disabled();
	globalConfig->setGroup("WM");
	// Inactive window border.
        map["inactiveborder"] = globalConfig->readColorEntry( "background", &cg.background());
	// Inactive window caption.
        map["inactivecaption"] = globalConfig->readColorEntry( "inactiveBackground", &cg.background());
	// Color of text in an inactive caption.
        map["inactivecaptiontext"] = globalConfig->readColorEntry( "inactiveForeground", &cg.text());
        map["graytext"] = cg.text();

	KConfig *bckgrConfig = new KConfig("kdesktoprc"); // No multi-screen support
	bckgrConfig->setGroup("Desktop0");
        // Desktop background.
	map["background"] = bckgrConfig->readColorEntry("Color1", &cg.background());
	delete bckgrConfig;
    };
};

static HTMLColors *htmlColors = 0L;

// FIXME: do we really need static delete support here?
//static KStaticDeleter<HTMLColors> hcsd;

void khtml::setNamedColor(QColor &color, const QString &_name)
{
    if( !htmlColors ) {
        // FIXME: do we really need static delete support here?
        // htmlColors = hcsd.setObject( new HTMLColors );
        htmlColors = new HTMLColors();
    }

    int pos;
    QString name = _name;
    // remove white spaces for those broken websites out there :-(
    while ( ( pos = name.find( ' ' ) ) != -1 )  name.remove( pos, 1 );

    int len = name.length();

    if(len == 0 || (len == 11 && name.find("transparent", 0, false) == 0) )
    {
        color = QColor(); // invalid color == transparent
        return;
    }

    // also recognize "color=ffffff"
    if (len == 6)
    {
        // recognize #12345 (append a '0')
        if(name[0] == '#') {
            name += '0';
        }
        else if ((!name[0].isLetter() && !name[0].isDigit())) {
            color = QColor();
            return;
	    }
        
        color.setNamedColor(name);
    }

    // #fffffff as found on msdn.microsoft.com
    if ( name[0] == '#' && len > 7)
    {
        name = name.left(7);
    }
	      
    if ( len > 4 && name[0].lower() == 'r' && name[1].lower() == 'g' &&
         name[2].lower() == 'b' && name[3] == '(' &&
         name[len-1] == ')')
    {
        // CSS like rgb(r, g, b) style
        DOMString rgb = name.mid(4, name.length()-5);
        QList<Length> *l = rgb.implementation()->toLengthList();
        if(l->count() != 3)
        {
	    // transparent in case of an invalid color.
            color = QColor();
        } else {
	    int r = l->at(0)->isUndefined() ? 0 : l->at(0)->width(255);
	    if(r < 0) r = 0;
	    if(r > 255) r = 255;
	    int g = l->at(1)->isUndefined() ? 0 : l->at(1)->width(255);
	    if(g < 0) g = 0;
	    if(g > 255) g = 255;
	    int b = l->at(2)->isUndefined() ? 0 : l->at(2)->width(255);
	    if(b < 0) b = 0;
	    if(b > 255) b = 255;

	    color.setRgb(r, g, b);
	}
        delete l;
    }
    else
    {
        QColor tc = htmlColors->map[name];
        
        if (!tc.isValid()) {
            tc = htmlColors->map[name.lower()];
        }
        if (tc.isValid()) {
            color = tc;
        }
        else {
            color.setNamedColor(name);
            if ( !color.isValid() )  color.setNamedColor( name.lower() );
            if(!color.isValid()) {
                bool hasalpha = false;
                for(unsigned int i = 0; i < name.length(); i++)
                    if(name[i].isLetterOrNumber()) {
                        hasalpha = true;
                        break;
                    }

                if(!hasalpha)
                  color = Qt::black;
            }
        }
    }
}

QPainter *khtml::printpainter = 0;

void khtml::setPrintPainter( QPainter *printer )
{
    printpainter = printer;
}

QFontMetrics khtml::fontMetrics( const QFont &f )
{
    if(printpainter) {
	printpainter->setFont( f );
	return printpainter->fontMetrics();
    }
    return QFontMetrics( f );
}
