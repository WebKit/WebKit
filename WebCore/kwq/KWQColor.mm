/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <qcolor.h>

#import <qnamespace.h>
#import <qstring.h>
#import <KWQAssertions.h>

const QColor Qt::black    (0x00, 0x00, 0x00);
const QColor Qt::white    (0xFF, 0xFF, 0xFF);
const QColor Qt::darkGray (0x80, 0x80, 0x80);
const QColor Qt::gray     (0xA0, 0xA0, 0xA0);
const QColor Qt::lightGray(0xC0, 0xC0, 0xC0);
const QColor Qt::red      (0xFF, 0x00, 0x00);
const QColor Qt::green    (0x00, 0xFF, 0x00);
const QColor Qt::blue     (0x00, 0x00, 0xFF);
const QColor Qt::cyan     (0x00, 0xFF, 0xFF);
const QColor Qt::magenta  (0xFF, 0x00, 0xFF);
const QColor Qt::yellow   (0xFF, 0xFF, 0x00);

static NSDictionary *getNamedColors()
{
    static NSDictionary *namedColors;
    
    if (namedColors)
        return namedColors;
    
    namedColors = [[NSDictionary alloc] initWithObjectsAndKeys:
        @"#f0f8ff", @"aliceblue",
        @"#faebd7", @"antiquewhite",
        @"#00ffff", @"aqua",
        @"#7fffd4", @"aquamarine",
        @"#f0ffff", @"azure",
        @"#f5f5dc", @"beige",
        @"#ffe4c4", @"bisque",
        @"#000000", @"black",
        @"#ffebcd", @"blanchedalmond",
        @"#0000ff", @"blue",
        @"#8a2be2", @"blueviolet",
        @"#a52a2a", @"brown",
        @"#deb887", @"burlywood",
        @"#5f9ea0", @"cadetblue",
        @"#7fff00", @"chartreuse",
        @"#d2691e", @"chocolate",
        @"#ff7f50", @"coral",
        @"#6495ed", @"cornflowerblue",
        @"#fff8dc", @"cornsilk",
        @"#dc143c", @"crimson",
        @"#00ffff", @"cyan",
        @"#00008b", @"darkblue",
        @"#008b8b", @"darkcyan",
        @"#b8860b", @"darkgoldenrod",
        @"#a9a9a9", @"darkgray",
        @"#006400", @"darkgreen",
        @"#bdb76b", @"darkkhaki",
        @"#8b008b", @"darkmagenta",
        @"#556b2f", @"darkolivegreen",
        @"#ff8c00", @"darkorange",
        @"#9932cc", @"darkorchid",
        @"#8b0000", @"darkred",
        @"#e9967a", @"darksalmon",
        @"#8fbc8f", @"darkseagreen",
        @"#483d8b", @"darkslateblue",
        @"#2f4f4f", @"darkslategray",
        @"#00ced1", @"darkturquoise",
        @"#9400d3", @"darkviolet",
        @"#ff1493", @"deeppink",
        @"#00bfff", @"deepskyblue",
        @"#696969", @"dimgray",
        @"#1e90ff", @"dodgerblue",
        @"#b22222", @"firebrick",
        @"#fffaf0", @"floralwhite",
        @"#228b22", @"forestgreen",
        @"#ff00ff", @"fuchsia",
        @"#dcdcdc", @"gainsboro",
        @"#f8f8ff", @"ghostwhite",
        @"#ffd700", @"gold",
        @"#daa520", @"goldenrod",
        @"#808080", @"gray",
        @"#008000", @"green",
        @"#adff2f", @"greenyellow",
        @"#f0fff0", @"honeydew",
        @"#ff69b4", @"hotpink",
        @"#cd5c5c", @"indianred ",
        @"#4b0082", @"indigo ",
        @"#fffff0", @"ivory",
        @"#f0e68c", @"khaki",
        @"#e6e6fa", @"lavender",
        @"#fff0f5", @"lavenderblush",
        @"#7cfc00", @"lawngreen",
        @"#fffacd", @"lemonchiffon",
        @"#add8e6", @"lightblue",
        @"#f08080", @"lightcoral",
        @"#e0ffff", @"lightcyan",
        @"#fafad2", @"lightgoldenrodyellow",
        @"#d3d3d3", @"lightgray",
        @"#90ee90", @"lightgreen",
        @"#ffb6c1", @"lightpink",
        @"#ffa07a", @"lightsalmon",
        @"#20b2aa", @"lightseagreen",
        @"#87cefa", @"lightskyblue",
        @"#8470ff", @"lightslateblue",
        @"#778899", @"lightslategray",
        @"#b0c4de", @"lightsteelblue",
        @"#ffffe0", @"lightyellow",
        @"#00ff00", @"lime",
        @"#32cd32", @"limegreen",
        @"#faf0e6", @"linen",
        @"#ff00ff", @"magenta",
        @"#800000", @"maroon",
        @"#66cdaa", @"mediumaquamarine",
        @"#0000cd", @"mediumblue",
        @"#ba55d3", @"mediumorchid",
        @"#9370d8", @"mediumpurple",
        @"#3cb371", @"mediumseagreen",
        @"#7b68ee", @"mediumslateblue",
        @"#00fa9a", @"mediumspringgreen",
        @"#48d1cc", @"mediumturquoise",
        @"#c71585", @"mediumvioletred",
        @"#191970", @"midnightblue",
        @"#f5fffa", @"mintcream",
        @"#ffe4e1", @"mistyrose",
        @"#ffe4b5", @"moccasin",
        @"#ffdead", @"navajowhite",
        @"#000080", @"navy",
        @"#fdf5e6", @"oldlace",
        @"#808000", @"olive",
        @"#6b8e23", @"olivedrab",
        @"#ffa500", @"orange",
        @"#ff4500", @"orangered",
        @"#da70d6", @"orchid",
        @"#eee8aa", @"palegoldenrod",
        @"#98fb98", @"palegreen",
        @"#afeeee", @"paleturquoise",
        @"#d87093", @"palevioletred",
        @"#ffefd5", @"papayawhip",
        @"#ffdab9", @"peachpuff",
        @"#cd853f", @"peru",
        @"#ffc0cb", @"pink",
        @"#dda0dd", @"plum",
        @"#b0e0e6", @"powderblue",
        @"#800080", @"purple",
        @"#ff0000", @"red",
        @"#bc8f8f", @"rosybrown",
        @"#4169e1", @"royalblue",
        @"#8b4513", @"saddlebrown",
        @"#fa8072", @"salmon",
        @"#f4a460", @"sandybrown",
        @"#2e8b57", @"seagreen",
        @"#fff5ee", @"seashell",
        @"#a0522d", @"sienna",
        @"#c0c0c0", @"silver",
        @"#87ceeb", @"skyblue",
        @"#6a5acd", @"slateblue",
        @"#708090", @"slategray",
        @"#fffafa", @"snow",
        @"#00ff7f", @"springgreen",
        @"#4682b4", @"steelblue",
        @"#d2b48c", @"tan",
        @"#008080", @"teal",
        @"#d8bfd8", @"thistle",
        @"#ff6347", @"tomato",
        @"#40e0d0", @"turquoise",
        @"#ee82ee", @"violet",
        @"#d02090", @"violetred",
        @"#f5deb3", @"wheat",
        @"#ffffff", @"white",
        @"#f5f5f5", @"whitesmoke",
        @"#ffff00", @"yellow",
        @"#9acd32", @"yellowgreen",
    nil];
    
    return namedColors;
}

QRgb qRgb(int r, int g, int b)
{
    return r << 16 | g << 8 | b;
}

QRgb qRgba(int r, int g, int b, int a)
{
    return a << 24 | r << 16 | g << 8 | b;
}

QColor::QColor(const QString &name)
{
    setNamedColor(name);
}

QColor::QColor(const char *name)
{
    setNamedColor(name);
}

QString QColor::name() const
{
    QString name;
    name.sprintf("#%02X%02X%02X", red(), green(), blue());
    return name;
}

static int hex2int(QChar hexchar)
{
    int v;
    
    if (hexchar.isDigit())
	v = hexchar.digitValue();
    else if (hexchar >= 'A' && hexchar <= 'F')
	v = hexchar.cell() - 'A' + 10;
    else if (hexchar >= 'a' && hexchar <= 'f')
	v = hexchar.cell() - 'a' + 10;
    else
	v = -1;
    
    return v;
}

static bool decodeColorFromHexColorString(const QString &string, int *r, int *g, int *b)
{
    int len = string.length();
    int offset = 0;
    
    if (len == 0)
        return false;

    if (string[0] == '#') {
        len -= 1;
        offset += 1;
    }
    
    for (int i = 0; i < len; i++)
        if (hex2int(string[i+offset]) == -1)
            return false;
    
    switch (len) {
    case 12:
        *r = (hex2int(string[0+offset]) << 4) + hex2int(string[1+offset]);
        *g = (hex2int(string[4+offset]) << 4) + hex2int(string[5+offset]);
        *b = (hex2int(string[8+offset]) << 4) + hex2int(string[9+offset]);
        return true;
    case 9:
        *r = (hex2int(string[0+offset]) << 4) + hex2int(string[1+offset]);
        *g = (hex2int(string[3+offset]) << 4) + hex2int(string[4+offset]);
        *b = (hex2int(string[6+offset]) << 4) + hex2int(string[7+offset]);
        return true;
    case 6:
        *r = (hex2int(string[0+offset]) << 4) + hex2int(string[1+offset]);
        *g = (hex2int(string[2+offset]) << 4) + hex2int(string[3+offset]);
        *b = (hex2int(string[4+offset]) << 4) + hex2int(string[5+offset]);
        return true;
    case 3:
        // Convert 0 to F to 0 to 255.
        *r = hex2int(string[0+offset]) * 0x11;
        *g = hex2int(string[1+offset]) * 0x11;
        *b = hex2int(string[2+offset]) * 0x11;
        return true;
    }
        
    return false;
}

void QColor::setNamedColor(const QString &name)
{
    // FIXME: The combination of this code with the code that
    // is in khtml/misc/helper.cpp makes setting colors by
    // name a real crock. We need to look at the process
    // of mapping names to colors and figure out something
    // better.
    // 
    // [kocienda: 2001-11-08]: I've made some improvements
    // but it's still a crock.
    
    int r, g, b;
    
    if (name.isEmpty()) {
        color = KWQInvalidColor;
    } 
    else if (decodeColorFromHexColorString(name, &r, &g, &b)) {
        setRgb(r, g, b);
    } 
    else {
        NSString *hexString;
        
        hexString = [getNamedColors() objectForKey:[name.getNSString() lowercaseString]];
        
        if (hexString && decodeColorFromHexColorString(QString::fromNSString(hexString), &r, &g, &b)) {
            setRgb(r, g, b);
        }
        else {
            ERROR("couldn't create color using name %s", name.ascii());
            color = KWQInvalidColor;
        }
    }
}

void QColor::hsv(int *h, int *s, int *v) const
{
    int r = red(); 
    int g = green(); 
    int b = blue(); 
    int i, w, x, f;
        
    x = w = r;
    
    if (g > x) {
        x = g;
    } 
    if (g < w) {
        w = g;
    }
    
    if (b > x) {
        x = b;
    } 
    if (b < w) {
        w = b;
    }
  
    if (w == x) {
        *h = -1;
        *s = 0;
        *v = w;
    }
    else {
        f = (r == x) ? g - b : ((g == x) ? b - r : r - g); 
        i = (r == x) ? 3 : ((g == x) ? 5 : 1); 
        *h = i - f /(w - x);
        if (w != 0)
            *s = (w - x)/w;
        else
            *s = 0;
        *v = w; 
    }
}

void QColor::setHsv(int h, int s, int v)
{
    int i, f, p, q, t;
    
    if( s == 0 ) {
        // achromatic (gray)
        setRgb(v, v, v);
        return;
    }
    
    h /= 60;			// sector 0 to 5
    i = (int)floor(h);
    f = h - i;			// factorial part of h
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));
    
    switch( i ) {
        case 0:
            setRgb(v, t, p);
            break;
        case 1:
            setRgb(q, v, p);
            break;
        case 2:
            setRgb(p, v, t);
            break;
        case 3:
            setRgb(p, q, v);
            break;
        case 4:
            setRgb(t, p, v);
            break;
        default:		// case 5:
            setRgb(v, p, q);
            break;
    }
}


QColor QColor::light(int factor) const
{
    if (factor <= 0) {
        return QColor(*this);
    }
    else if (factor < 100) {
        // NOTE: this is actually a darken operation
        return dark(10000 / factor); 
    }

    int h, s, v;
    
    hsv(&h, &s, &v);
    v = (factor * v) / 100;

    if (v > 255) {
        s -= (v - 255);
        if (s < 0) {
            s = 0;
        }
        v = 255;
    }

    QColor result;
    
    result.setHsv(h, s, v);
    
    return result;
}

QColor QColor::dark(int factor) const
{
    if (factor <= 0) {
        return QColor(*this);
    }
    else if (factor < 100) {
        // NOTE: this is actually a lighten operation
        return light(10000 / factor); 
    }

    int h, s, v;
    
    hsv(&h, &s, &v);
    v = (v * 100) / factor;

    QColor result;
    
    result.setHsv(h, s, v);
    
    return result;
}

NSColor *QColor::getNSColor() const
{
    return [NSColor colorWithCalibratedRed:red() / 255.0
                                     green:green() / 255.0
                                      blue:blue() / 255.0
                                     alpha:1.0];
}
