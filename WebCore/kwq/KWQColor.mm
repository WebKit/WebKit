/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <qcolor.h>
#include <qstring.h>

#include <kwqdebug.h>

static NSDictionary *namedColors = NULL;

#define ROUND_TO_INT(f) ((int)rint((f)))

QRgb qRgb(int r, int g, int b)
{
    return r << 16 | g << 8 | b;
}


QRgb qRgba(int r, int g, int b, int a)
{
    return a << 24 | r << 16 | g << 8 | b;
}



QColor::QColor()
{
    color = nil;
}


QColor::QColor(int r, int g, int b)
{
    color = nil;
    if (!globals_init) {
	    initGlobalColors();
	}
    else {
        _initialize (r, g, b);
    }
}

QColor::QColor(const QString &name)
{
    color = nil;
    setNamedColor(name);
}

QColor::QColor(const char *name)
{
    color = nil;
    setNamedColor( QString(name) );
}


void QColor::_initialize(int r, int g, int b)
{
    color = [[NSColor colorWithCalibratedRed: ((float)(r)) / (float)255.0
                    green: ((float)(g)) / (float)255.0
                    blue: ((float)(b)) / (float)255.0
                    alpha: 1.0] retain];
}


QColor::~QColor(){
    if (color != nil)
        [color release];
}


QColor::QColor(const QColor &copyFrom)
{
    if (copyFrom.color != nil) {
        color = [copyFrom.color retain];
    }
    else {
        color = nil;
    }
}

QString QColor::name() const
{
    NSString *name;

    name = [NSString stringWithFormat:@"#%02x%02x%02x", red(), green(), blue()];
    
    return NSSTRING_TO_QSTRING(name);
}

static int hex2int( QChar hexchar )
{
    int v;
    if ( hexchar.isDigit() )
	v = hexchar.digitValue();
    else if ( hexchar >= 'A' && hexchar <= 'F' )
	v = hexchar.cell() - 'A' + 10;
    else if ( hexchar >= 'a' && hexchar <= 'f' )
	v = hexchar.cell() - 'a' + 10;
    else
	v = 0;
    return v;
}

static bool looksLikeSixLetterHexColorString(const QString &string)
{
    bool result;
    
    result = TRUE;
    
    for (int i = 0; i < 6; i++) {
        QChar c = string.at(i);
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F')) {
            continue;
        }
        else {
            result = FALSE;
            break;   
        }
    }
    
    return result;
}

static bool decodeColorFromHexColorString(const QString &string, int *r, int *g, int *b)
{
    bool decoded;
    
    decoded = FALSE;

    if ((string.length() == 7 && string[0] == '#') ||
        (string.length() == 4 && string[0] == '#') ||
        (string.length() == 6 && looksLikeSixLetterHexColorString(string))) {
        int offset = 0;
        int len;
        if (string[0] == '#') {
            offset = 1;
            len = string.length() - 1;
        }
        else {
            len = string.length();
        }
        const QChar *p = string.unicode() + offset;
        if (len == 12) {
            *r = (hex2int(p[0]) << 4) + hex2int(p[1]);
            *g = (hex2int(p[4]) << 4) + hex2int(p[5]);
            *b = (hex2int(p[8]) << 4) + hex2int(p[9]);
            decoded = TRUE;    
        } else if (len == 9) {
            *r = (hex2int(p[0]) << 4) + hex2int(p[1]);
            *g = (hex2int(p[3]) << 4) + hex2int(p[4]);
            *b = (hex2int(p[6]) << 4) + hex2int(p[7]);
            decoded = TRUE;    
        } else if (len == 6) {
            *r = (hex2int(p[0]) << 4) + hex2int(p[1]);
            *g = (hex2int(p[2]) << 4) + hex2int(p[3]);
            *b = (hex2int(p[4]) << 4) + hex2int(p[5]);
            decoded = TRUE;    
        } else if (len == 3) {
            // Convert 0 to 15, to 0 to 255.
            *r = ROUND_TO_INT((1.0f/15.0f) * hex2int(p[0]) * 255.0f);
            *g = ROUND_TO_INT((1.0f/15.0f) * hex2int(p[1]) * 255.0f);
            *b = ROUND_TO_INT((1.0f/15.0f) * hex2int(p[2]) * 255.0f);
            decoded = TRUE;    
        }
    }
        
    return decoded;
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
    
    r = g = b = 0;
    
    if ( name.isEmpty() ) {
	    setRgb( 0 );
    } 
    else if (decodeColorFromHexColorString(name, &r, &g, &b)) {
	    setRgb(r, g, b);
    } 
    else {
        NSString *hexString;
        
        hexString = [namedColors objectForKey:[QSTRING_TO_NSSTRING(name) lowercaseString]];
        
        if (hexString && decodeColorFromHexColorString(NSSTRING_TO_QSTRING(hexString), &r, &g, &b)) {
            setRgb(r, g, b);
        }
        else {
            KWQDEBUG ("couldn't create color using name %s\n", name.ascii());
            setRgb(0, 0, 0);
        }
    }
}


bool QColor::isValid() const
{
    bool result;

    result = TRUE;

    if (color == nil) {
        result = FALSE;
    }

    return result;
}


int QColor::red() const
{
    if (color == nil)
        return 0;
    return (int)([color redComponent] * 255);
}


int QColor::QColor::green() const
{
    if (color == nil)
        return 0;
    return (int)([color greenComponent] * 255);
}

int QColor::blue() const
{
    if (color == nil)
        return 0;
    return (int)([color blueComponent] * 255);
}


QRgb QColor::rgb() const
{
    if (color == nil)
        return 0;
    return qRgb (red(),green(),blue());
}


void QColor::setRgb(int r, int g, int b)
{
    if (color != nil)
        [color release];
    color = [[NSColor colorWithCalibratedRed: ((float)(r)) / (float)255.0
                    green: ((float)(g)) / (float)255.0
                    blue: ((float)(b)) / (float)255.0
                    alpha: 1.0] retain];
}


void QColor::setRgb(int rgb)
{
    if (color != nil)
        [color release];
    color = [[NSColor colorWithCalibratedRed: ((float)(rgb >> 16)) / 255.0
                    green: ((float)(rgb >> 8)) / 255.0
                    blue: ((float)(rgb & 0xff)) / 255.0
                    alpha: 1.0] retain];
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
        *s = (w - x)/w;
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


QColor &QColor::operator=(const QColor &assignFrom)
{
    if ( !globals_init )
	initGlobalColors();
    if (color != assignFrom.color){ 
        if (color != nil)
            [color release];
        if (assignFrom.color != nil)
            color = [assignFrom.color retain];
        else
            color = nil;
    }
    return *this;
}


bool QColor::operator==(const QColor &compareTo) const
{
    return [color isEqual: compareTo.color];
}


bool QColor::operator!=(const QColor &compareTo) const
{
    return !(operator==(compareTo));
}




/*****************************************************************************
  Global colors
 *****************************************************************************/

bool QColor::globals_init = FALSE;		// global color not initialized


static QColor stdcol[19];

QT_STATIC_CONST_IMPL QColor & Qt::color0 = stdcol[0];
QT_STATIC_CONST_IMPL QColor & Qt::color1  = stdcol[1];
QT_STATIC_CONST_IMPL QColor & Qt::black  = stdcol[2];
QT_STATIC_CONST_IMPL QColor & Qt::white = stdcol[3];
QT_STATIC_CONST_IMPL QColor & Qt::darkGray = stdcol[4];
QT_STATIC_CONST_IMPL QColor & Qt::gray = stdcol[5];
QT_STATIC_CONST_IMPL QColor & Qt::lightGray = stdcol[6];
QT_STATIC_CONST_IMPL QColor & Qt::red = stdcol[7];
QT_STATIC_CONST_IMPL QColor & Qt::green = stdcol[8];
QT_STATIC_CONST_IMPL QColor & Qt::blue = stdcol[9];
QT_STATIC_CONST_IMPL QColor & Qt::cyan = stdcol[10];
QT_STATIC_CONST_IMPL QColor & Qt::magenta = stdcol[11];
QT_STATIC_CONST_IMPL QColor & Qt::yellow = stdcol[12];
QT_STATIC_CONST_IMPL QColor & Qt::darkRed = stdcol[13];
QT_STATIC_CONST_IMPL QColor & Qt::darkGreen = stdcol[14];
QT_STATIC_CONST_IMPL QColor & Qt::darkBlue = stdcol[15];
QT_STATIC_CONST_IMPL QColor & Qt::darkCyan = stdcol[16];
QT_STATIC_CONST_IMPL QColor & Qt::darkMagenta = stdcol[17];
QT_STATIC_CONST_IMPL QColor & Qt::darkYellow = stdcol[18];



void QColor::initGlobalColors()
{
    NSAutoreleasePool *colorPool;
    
    colorPool = [[NSAutoreleasePool allocWithZone:NULL] init];
     
    globals_init = TRUE;

    stdcol[ 0].setRgb(255,   255,   255 );
    stdcol[ 1].setRgb(   0,   0,   0 );
    stdcol[ 2].setRgb(   0,   0,   0 );
    stdcol[ 3].setRgb( 255, 255, 255 );
    stdcol[ 4].setRgb( 128, 128, 128 );
    stdcol[ 5].setRgb( 160, 160, 164 );
    stdcol[ 6].setRgb( 192, 192, 192 );
    stdcol[ 7].setRgb( 255,   0,   0 );
    stdcol[ 8].setRgb(   0, 255,   0 );
    stdcol[ 9].setRgb(   0,   0, 255 );
    stdcol[10].setRgb(   0, 255, 255 );
    stdcol[11].setRgb( 255,   0, 255 );
    stdcol[12].setRgb( 255, 255,   0 );
    stdcol[13].setRgb( 128,   0,   0 );
    stdcol[14].setRgb(   0, 128,   0 );
    stdcol[15].setRgb(   0,   0, 128 );
    stdcol[16].setRgb(   0, 128, 128 );
    stdcol[17].setRgb( 128,   0, 128 );
    stdcol[18].setRgb( 128, 128,   0 );

    namedColors = [NSDictionary dictionaryWithObjectsAndKeys: \
        @"#f0f8ff",@"aliceblue", \
        @"#faebd7",@"antiquewhite", \
        @"#00ffff",@"aqua", \
        @"#7fffd4",@"aquamarine", \
        @"#f0ffff",@"azure", \
        @"#f5f5dc",@"beige", \
        @"#ffe4c4",@"bisque", \
        @"#000000",@"black", \
        @"#ffebcd",@"blanchedalmond", \
        @"#0000ff",@"blue", \
        @"#8a2be2",@"blueviolet", \
        @"#a52a2a",@"brown", \
        @"#deb887",@"burlywood", \
        @"#5f9ea0",@"cadetblue", \
        @"#7fff00",@"chartreuse", \
        @"#d2691e",@"chocolate", \
        @"#ff7f50",@"coral", \
        @"#6495ed",@"cornflowerblue", \
        @"#fff8dc",@"cornsilk", \
        @"#dc143c",@"crimson", \
        @"#00ffff",@"cyan", \
        @"#00008b",@"darkblue", \
        @"#008b8b",@"darkcyan", \
        @"#b8860b",@"darkgoldenrod", \
        @"#a9a9a9",@"darkgray", \
        @"#006400",@"darkgreen", \
        @"#bdb76b",@"darkkhaki", \
        @"#8b008b",@"darkmagenta", \
        @"#556b2f",@"darkolivegreen", \
        @"#ff8c00",@"darkorange", \
        @"#9932cc",@"darkorchid", \
        @"#8b0000",@"darkred", \
        @"#e9967a",@"darksalmon", \
        @"#8fbc8f",@"darkseagreen", \
        @"#483d8b",@"darkslateblue", \
        @"#2f4f4f",@"darkslategray", \
        @"#00ced1",@"darkturquoise", \
        @"#9400d3",@"darkviolet", \
        @"#ff1493",@"deeppink", \
        @"#00bfff",@"deepskyblue", \
        @"#696969",@"dimgray", \
        @"#1e90ff",@"dodgerblue", \
        @"#b22222",@"firebrick", \
        @"#fffaf0",@"floralwhite", \
        @"#228b22",@"forestgreen", \
        @"#ff00ff",@"fuchsia", \
        @"#dcdcdc",@"gainsboro", \
        @"#f8f8ff",@"ghostwhite", \
        @"#ffd700",@"gold", \
        @"#daa520",@"goldenrod", \
        @"#808080",@"gray", \
        @"#008000",@"green", \
        @"#adff2f",@"greenyellow", \
        @"#f0fff0",@"honeydew", \
        @"#ff69b4",@"hotpink", \
        @"#cd5c5c",@"indianred ", \
        @"#4b0082",@"indigo ", \
        @"#fffff0",@"ivory", \
        @"#f0e68c",@"khaki", \
        @"#e6e6fa",@"lavender", \
        @"#fff0f5",@"lavenderblush", \
        @"#7cfc00",@"lawngreen", \
        @"#fffacd",@"lemonchiffon", \
        @"#add8e6",@"lightblue", \
        @"#f08080",@"lightcoral", \
        @"#e0ffff",@"lightcyan", \
        @"#fafad2",@"lightgoldenrodyellow", \
        @"#d3d3d3",@"lightgray", \
        @"#90ee90",@"lightgreen", \
        @"#ffb6c1",@"lightpink", \
        @"#ffa07a",@"lightsalmon", \
        @"#20b2aa",@"lightseagreen", \
        @"#87cefa",@"lightskyblue", \
        @"#8470ff",@"lightslateblue", \
        @"#778899",@"lightslategray", \
        @"#b0c4de",@"lightsteelblue", \
        @"#ffffe0",@"lightyellow", \
        @"#00ff00",@"lime", \
        @"#32cd32",@"limegreen", \
        @"#faf0e6",@"linen", \
        @"#ff00ff",@"magenta", \
        @"#800000",@"maroon", \
        @"#66cdaa",@"mediumaquamarine", \
        @"#0000cd",@"mediumblue", \
        @"#ba55d3",@"mediumorchid", \
        @"#9370d8",@"mediumpurple", \
        @"#3cb371",@"mediumseagreen", \
        @"#7b68ee",@"mediumslateblue", \
        @"#00fa9a",@"mediumspringgreen", \
        @"#48d1cc",@"mediumturquoise", \
        @"#c71585",@"mediumvioletred", \
        @"#191970",@"midnightblue", \
        @"#f5fffa",@"mintcream", \
        @"#ffe4e1",@"mistyrose", \
        @"#ffe4b5",@"moccasin", \
        @"#ffdead",@"navajowhite", \
        @"#000080",@"navy", \
        @"#fdf5e6",@"oldlace", \
        @"#808000",@"olive", \
        @"#6b8e23",@"olivedrab", \
        @"#ffa500",@"orange", \
        @"#ff4500",@"orangered", \
        @"#da70d6",@"orchid", \
        @"#eee8aa",@"palegoldenrod", \
        @"#98fb98",@"palegreen", \
        @"#afeeee",@"paleturquoise", \
        @"#d87093",@"palevioletred", \
        @"#ffefd5",@"papayawhip", \
        @"#ffdab9",@"peachpuff", \
        @"#cd853f",@"peru", \
        @"#ffc0cb",@"pink", \
        @"#dda0dd",@"plum", \
        @"#b0e0e6",@"powderblue", \
        @"#800080",@"purple", \
        @"#ff0000",@"red", \
        @"#bc8f8f",@"rosybrown", \
        @"#4169e1",@"royalblue", \
        @"#8b4513",@"saddlebrown", \
        @"#fa8072",@"salmon", \
        @"#f4a460",@"sandybrown", \
        @"#2e8b57",@"seagreen", \
        @"#fff5ee",@"seashell", \
        @"#a0522d",@"sienna", \
        @"#c0c0c0",@"silver", \
        @"#87ceeb",@"skyblue", \
        @"#6a5acd",@"slateblue", \
        @"#708090",@"slategray", \
        @"#fffafa",@"snow", \
        @"#00ff7f",@"springgreen", \
        @"#4682b4",@"steelblue", \
        @"#d2b48c",@"tan", \
        @"#008080",@"teal", \
        @"#d8bfd8",@"thistle", \
        @"#ff6347",@"tomato", \
        @"#40e0d0",@"turquoise", \
        @"#ee82ee",@"violet", \
        @"#d02090",@"violetred", \
        @"#f5deb3",@"wheat", \
        @"#ffffff",@"white", \
        @"#f5f5f5",@"whitesmoke", \
        @"#ffff00",@"yellow", \
        @"#9acd32",@"yellowgreen", \
        NULL \
    ];        
    [namedColors retain];
}

