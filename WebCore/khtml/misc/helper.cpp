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
 */
#include "helper.h"
#include <khtmllayout.h>
#include <qmap.h>
#include <qpainter.h>
#include <dom/dom_string.h>
#include <xml/dom_stringimpl.h>
#include <qptrlist.h>
#include <kstaticdeleter.h>
#include <kapplication.h>
#include <kconfig.h>
#include <qtooltip.h>

using namespace DOM;
using namespace khtml;

struct HTMLColors {
    QMap<QString,QColor> map;
    HTMLColors();
};

struct colorMap {
    const char * name;
    const char * value;
};

static const colorMap cmap[] = {
   { "black", "#000000" },
   { "green", "#008000" },
   { "silver", "#c0c0c0" },
   { "lime", "#00ff00" },
   { "gray", "#808080" },
   { "olive", "#808000" },
   { "white", "#ffffff" },
   { "yellow", "#ffff00" },
   { "maroon", "#800000" },
   { "navy", "#000080" },
   { "red", "#ff0000" },
   { "blue", "#0000ff" },
   { "purple", "#800080" },
   { "teal", "#008080" },
   { "fuchsia", "#ff00ff" },
   { "aqua", "#00ffff" },
   { "crimson", "#dc143c" },
   { "indigo", "#4b0082" },
   { 0, 0 }
};

struct uiColors {
    const char * name;
    const char * configGroup;
    const char * configEntry;
    QPalette::ColorGroup group;
    QColorGroup::ColorRole role;
};

const char * const wmgroup = "WM";
const char * const generalgroup = "General";

static const uiColors uimap[] = {
	// Active window border.
    { "activeborder", wmgroup, "background", QPalette::Active, QColorGroup::Light },
	// Active window caption.
    { "activecaption", wmgroup, "background", QPalette::Active, QColorGroup::Text },
        // Text in caption, size box, and scrollbar arrow box.
    { "captiontext", wmgroup, "activeForeground", QPalette::Active, QColorGroup::Text },
	// Face color for three-dimensional display elements.
    { "buttonface", wmgroup, 0, QPalette::Inactive, QColorGroup::Button },
	// Dark shadow for three-dimensional display elements (for edges facing away from the light source).
    { "buttonhighlight", wmgroup, 0, QPalette::Inactive, QColorGroup::Light },
	// Shadow color for three-dimensional display elements.
    { "buttonshadow", wmgroup, 0, QPalette::Inactive, QColorGroup::Shadow },
	// Text on push buttons.
    { "buttontext", wmgroup, "buttonForeground", QPalette::Inactive, QColorGroup::ButtonText },
	// Dark shadow for three-dimensional display elements.
    { "threeddarkshadow", wmgroup, 0, QPalette::Inactive, QColorGroup::Dark },
	// Face color for three-dimensional display elements.
    { "threedface", wmgroup, 0, QPalette::Inactive, QColorGroup::Button },
	// Highlight color for three-dimensional display elements.
    { "threedhighlight", wmgroup, 0, QPalette::Inactive, QColorGroup::Light },
	// Light color for three-dimensional display elements (for edges facing the light source).
    { "threedlightshadow", wmgroup, 0, QPalette::Inactive, QColorGroup::Midlight },
	// Dark shadow for three-dimensional display elements.
    { "threedshadow", wmgroup, 0, QPalette::Inactive, QColorGroup::Shadow },

    // Inactive window border.
    { "inactiveborder", wmgroup, "background", QPalette::Disabled, QColorGroup::Background },
    // Inactive window caption.
    { "inactivecaption", wmgroup, "inactiveBackground", QPalette::Disabled, QColorGroup::Background },
    // Color of text in an inactive caption.
    { "inactivecaptiontext", wmgroup, "inactiveForeground", QPalette::Disabled, QColorGroup::Text },
    { "graytext", wmgroup, 0, QPalette::Disabled, QColorGroup::Text },

	// Menu background
    { "menu", generalgroup, "background", QPalette::Inactive, QColorGroup::Background },
	// Text in menus
    { "menutext", generalgroup, "foreground", QPalette::Inactive, QColorGroup::Background },

        // Text of item(s) selected in a control.
    { "highlight", generalgroup, "selectBackground", QPalette::Inactive, QColorGroup::Background },

    // Text of item(s) selected in a control.
    { "highlighttext", generalgroup, "selectForeground", QPalette::Inactive, QColorGroup::Background },

	// Background color of multiple document interface.
    { "appworkspace", generalgroup, "background", QPalette::Inactive, QColorGroup::Text },

	// Scroll bar gray area.
    { "scrollbar", generalgroup, "background", QPalette::Inactive, QColorGroup::Background },

	// Window background.
    { "window", generalgroup, "windowBackground", QPalette::Inactive, QColorGroup::Background },
	// Window frame.
    { "windowframe", generalgroup, "windowBackground", QPalette::Inactive, QColorGroup::Background },
        // WindowText
    { "windowtext", generalgroup, "windowForeground", QPalette::Inactive, QColorGroup::Text },
    { "text", generalgroup, 0, QPalette::Inactive, QColorGroup::Text },
    { 0, 0, 0, QPalette::NColorGroups, QColorGroup::NColorRoles }
};

HTMLColors::HTMLColors()
{
    const colorMap *color = cmap;
    while ( color->name ) {
	map[color->name] = color->value;
	++color;
    }
    // ### react to style changes
    // see http://www.richinstyle.com for details

    /* Mapping system settings to CSS 2
     * Tried hard to get an appropriate mapping - schlpbch
     */

    KConfig *globalConfig = KGlobal::config();
    const QPalette &pal = kapp->palette();

    const uiColors *uicol = uimap;
    const char *lastConfigGroup = 0;
    while( uicol->name ) {
	if ( lastConfigGroup != uicol->configGroup ) {
	    lastConfigGroup = uicol->configGroup;
	    globalConfig->setGroup( lastConfigGroup );
	}
	QColor c = pal.color( uicol->group, uicol->role );
	if ( uicol->configEntry )
	    c = globalConfig->readColorEntry( uicol->configEntry, &c );
	map[uicol->name] = c;
	++uicol;
    }

#ifndef QT_NO_TOOLTIP
    // InfoBackground
    map["infobackground"] = QToolTip::palette().inactive().background();
    // InfoText
    map["infotext"] = QToolTip::palette().inactive().foreground();
#endif

    KConfig bckgrConfig("kdesktoprc", true, false); // No multi-screen support
    bckgrConfig.setGroup("Desktop0");
        // Desktop background.
    map["background"] = bckgrConfig.readColorEntry("Color1", &pal.disabled().background());
};



static HTMLColors *htmlColors = 0L;

static KStaticDeleter<HTMLColors> hcsd;

void khtml::setNamedColor(QColor &color, const QString &_name)
{
    if( !htmlColors )
        htmlColors = hcsd.setObject( new HTMLColors );

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
#ifdef APPLE_CHANGES
        // recognize #12345 (duplicate the last character)
        if(name[0] == '#') {
            name += name.right(1);
        }
        else if ((!name[0].isLetter() && !name[0].isDigit())) {
	    color = QColor();
	    return;
	}
        color.setNamedColor(name);
#else /* APPLE_CHANGES not defined */
        bool ok;
        int val = name.toInt(&ok, 16);
        if(ok)
        {
            color.setRgb((0xff << 24) | val);
            return;
        }
        // recognize #12345 (duplicate the last character)
        if(name[0] == '#') {
            bool ok;
            int val = name.right(5).toInt(&ok, 16);
            if(ok) {
                color.setRgb((0xff << 24) | (val * 16 + ( val&0xf )));
                return;
            }
        }
        if ( !name[0].isLetter() ) {
	    color = QColor();
	    return;
	}
#endif /* APPLE_CHANGES not defined */
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
        int count;
        khtml::Length* l = rgb.implementation()->toLengthArray(count);
        if (count != 3)
            // transparent in case of an invalid color.
            color = QColor();
        else {
            int c[3];
            for (int i = 0; i < 3; ++i) {
                c[i] = l[i].width(255);
                if (c[i] < 0) c[i] = 0;
                if (c[i] > 255) c[i] = 255;
            }
            color.setRgb(c[0], c[1], c[2]);
        }
        delete [] l;
    }
    else
    {
        QColor tc = htmlColors->map[name];
        if ( !tc.isValid() )
            tc = htmlColors->map[name.lower()];

        if (tc.isValid())
            color = tc;
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
