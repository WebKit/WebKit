/****************************************************************************
** $Id$
**
** Implementation of QColor class
**
** Created : 940112
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of the kernel module of the Qt GUI Toolkit.
**
** This file may be distributed under the terms of the Q Public License
** as defined by Trolltech AS of Norway and appearing in the file
** LICENSE.QPL included in the packaging of this file.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
** licenses may use this file in accordance with the Qt Commercial License
** Agreement provided with the Software.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
** See http://www.trolltech.com/qpl/ for QPL licensing information.
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

#include "qcolor.h"
#include "qnamespace.h"

#ifndef QT_NO_DATASTREAM
#include "qdatastream.h"
#endif 

#include <stdlib.h>
#include <ctype.h>

// NOT REVISED
/*!
  \class QColor qcolor.h
  \brief The QColor class provides colors based on RGB.

  \ingroup color
  \ingroup drawing
  \ingroup appearance

  A color is normally specified in terms of RGB (red,green and blue)
  components, but it is also possible to specify HSV (hue,saturation
  and value) or set a color name (the names are copied from from the
  X11 color database).

  In addition to the RGB value, a QColor also has a pixel value.  This
  value is used by the underlying window system to refer to a color.  It
  can be thought of as an index into the display hardware's color table.

  There are 19 predefined QColor objects: \c black, \c white, \c
  darkGray, \c gray, \c lightGray, \c red, \c green, \c blue, \c cyan,
  \c magenta, \c yellow, \c darkRed, \c darkGreen, \c darkBlue, \c
  darkCyan, \c darkMagenta, \c darkYellow, \c color0 and \c color1.

  The colors \c color0 (zero pixel value) and \c color1 (non-zero
  pixel value) are special colors for drawing in \link QBitmap
  bitmaps\endlink.

  The QColor class has an efficient, dynamic color allocation
  strategy.  A color is normally allocated the first time it is used
  (lazy allocation), that is, whenever the pixel() function is called:

  <ol>
  <li>Is the pixel value valid? If it is, just return it, otherwise,
  allocate a pixel value.
  <li>Check an internal hash table to see if we allocated an equal RGB
  value earlier. If we did, set the pixel value and return.
  <li>Try to allocate the RGB value. If we succeed, we get a pixel value
  which we save in the internal table with the RGB value.
  Return the pixel value.
  <li>The color could not be allocated. Find the closest matching
  color and save it in the internal table.
  </ol>

  Since many people don't know the HSV color model very well, we'll
  cover it briefly here.

  The RGB model is hardware-oriented.  Its representation is close to
  what most monitors show.  In contrast, HSV represents color in a way
  more suited to traditional human perception of color.  For example,
  the relationships "stronger than", "darker than", "the opposite of"
  are easily expressed in HSV, but are much harder to express in RGB.

  HSV, like RGB, has three components.  They are: <ul> <li> H, for
  hue, is either 0-360 if the color is chromatic (not gray), or
  meaningless if it is gray.  It represents degrees on the color wheel
  familiar to most people.  Red is 0 (degrees), green is 120 and blue
  is 240. <li> S, for saturation, is 0-255 and the bigger it is, the
  stronger the color is.  Grayish colors have saturation near 0, very
  strong colors have saturation near 255. <li> V, for value, is 0-255
  and represents lightness or brightness of the color.  0 is black,
  255 is far from black as possible. </ul>

  Here are some examples: Pure red is H=0, S=255, V=255.  A dark red,
  moving slightly towards the magenta, could be H=350 (equivalent to
  -10), S=255, V=180.  A grayish light red could have H about 0 (say
  350-359 or 0-10), S about 50-100, and S=255.

  \sa QPalette, QColorGroup, QApplication::setColorSpec(),
  <a href="http://www.inforamp.net/~poynton/Poynton-color.html">Color FAQ.</a>
*/

/*****************************************************************************
  Global colors
 *****************************************************************************/

#if defined(_WS_WIN_)
#define COLOR0_PIX 0x00ffffff
#define COLOR1_PIX 0
#else
#define COLOR0_PIX 0
#define COLOR1_PIX 1
#endif

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


/*****************************************************************************
  QColor member functions
 *****************************************************************************/

bool QColor::color_init   = FALSE;		// color system not initialized
bool QColor::globals_init = FALSE;		// global color not initialized
bool QColor::lazy_alloc = TRUE;			// lazy color allocation


QColor* QColor::globalColors()
{
    return stdcol;
}


/*!
  Initializes the global colors.  This function is called if a global
  color variable is initialized before the constructors for our global
  color objects are executed.  Without this mechanism, assigning a
  color might assign an uninitialized value.

  Example:
  \code
     QColor myColor = red;			// will initialize red etc.

     int main( int argc, char **argc )
     {
     }
  \endcode
*/

void QColor::initGlobalColors()
{
    globals_init = TRUE;
    stdcol[ 0].pix = COLOR0_PIX;
    stdcol[ 1].pix = COLOR1_PIX;
#ifdef _WS_QWS_
    stdcol[ 0].rgbVal = 0;
    stdcol[ 1].rgbVal = 0x00ffffff; //######### QWS color allocation is a mess
#else
    stdcol[ 0].rgbVal = 0x00ffffff;
    stdcol[ 1].rgbVal = 0;
#endif
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
}


/*!
  \fn QColor::QColor()

  Constructs an invalid color with the RGB value (0,0,0). An invalid color
  is a color that is not properly set up for the underlying window system.

  \sa isValid()
*/


/*!
  \fn QColor::QColor( int r, int g, int b )

  Constructs a color with the RGB value \a (r,g,b).
  \a r, \a g and \a b must be in the range 0..255.

  \sa setRgb()
*/


/*!
  Constructs a color with a RGB value and a custom pixel value.

  If the \a pixel = 0xffffffff, then the color uses the RGB value in a
  standard way.	 If \a pixel is something else, then the pixel value will
  be set directly to \a pixel (skips the normal allocation procedure).
*/

QColor::QColor( QRgb rgb, uint pixel )
{
    if ( pixel == 0xffffffff ) {
	setRgb( rgb );
    } else {
	rgbVal = (rgb & RGB_MASK) | RGB_DIRECT;
	pix    = pixel;
    }
}


/*!
  Constructs a color with the RGB \e or HSV value \a (x,y,z).

  The \e (x,y,z) triplet defines an RGB value if \a colorSpec == \c
  QColor::Rgb.	\a x (red), \a y (green) and \a z (blue) must be in the
  range 0..255.

  The \a (x,y,z) triplet defines a HSV value if \a colorSpec == \c
  QColor::Hsv.	\a x (hue) must be in the range -1..360 (-1 means
  achromatic), and \a y (saturation) and \a z (value) must be in the range
  0..255.

  \sa setRgb(), setHsv()
*/

QColor::QColor( int x, int y, int z, Spec colorSpec )
{
    if ( colorSpec == Hsv )
	setHsv( x, y, z );
    else
	setRgb( x, y, z );
}


/*!
  Constructs a named color in the same way as setNamedColor().
  \sa setNamedColor()
*/

QColor::QColor( const QString& name )
{
    setNamedColor( name );
}


/*!
  Constructs a named color in the same way as setNamedColor().
  \sa setNamedColor()
*/

QColor::QColor( const char *name )
{
    setNamedColor( QString(name) );
}



/*!
  Constructs a color that is a copy of \a c.
*/

QColor::QColor( const QColor &c )
{
    if ( !globals_init )
	initGlobalColors();
    rgbVal = c.rgbVal;
    pix	   = c.pix;
}


/*!
  Assigns a copy of the color \c and returns a reference to this color.
*/

QColor &QColor::operator=( const QColor &c )
{
    if ( !globals_init )
	initGlobalColors();
    rgbVal = c.rgbVal;
    pix	   = c.pix;
    return *this;
}


/*!
  \fn bool QColor::isValid() const
  Returns FALSE if the color is invalid, i.e. it was constructed using the
  default constructor.
*/

/*!
  \fn bool QColor::isDirty() const
  Returns TRUE if the color is dirty, i.e. lazy allocation is enabled and
  an RGB/HSV value has been set but not allocated.
  \sa setLazyAlloc(), alloc(), pixel()
*/


/*!
  Returns the name of the color in the format #RRGGBB.

  \sa setNamedColor()
*/

QString QColor::name() const
{
    QString s;
    s.sprintf( "#%02x%02x%02x", red(), green(), blue() );
    return s;
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


/*!
  Sets the RGB value to that of the named color.

  The color name may be in one of five formats: <ul>
  <li> #RGB (each of R, G and B is a single hex digit)
  <li> #RRGGBB
  <li> #RRRGGGBBB
  <li> #RRRRGGGGBBBB
  <li> A named from the X color database (rgb.txt),  e.g.
  "steelblue" or "gainsboro".  These color names also work
  under Qt for Windows.
  </ul>
*/

void QColor::setNamedColor( const QString& name )
{
    if ( name.isEmpty() ) {
	setRgb( 0 );
    } else if ( name[0] == '#' ) {
	const QChar *p = name.unicode()+1;
	int len = name.length()-1;
	int r, g, b;
	if ( len == 12 ) {
	    r = (hex2int(p[0]) << 4) + hex2int(p[1]);
	    g = (hex2int(p[4]) << 4) + hex2int(p[5]);
	    b = (hex2int(p[8]) << 4) + hex2int(p[9]);
	} else if ( len == 9 ) {
	    r = (hex2int(p[0]) << 4) + hex2int(p[1]);
	    g = (hex2int(p[3]) << 4) + hex2int(p[4]);
	    b = (hex2int(p[6]) << 4) + hex2int(p[7]);
	} else if ( len == 6 ) {
	    r = (hex2int(p[0]) << 4) + hex2int(p[1]);
	    g = (hex2int(p[2]) << 4) + hex2int(p[3]);
	    b = (hex2int(p[4]) << 4) + hex2int(p[5]);
	} else if ( len == 3 ) {
	    r = (hex2int(p[0]) << 4) + hex2int(p[0]);
	    g = (hex2int(p[1]) << 4) + hex2int(p[1]);
	    b = (hex2int(p[2]) << 4) + hex2int(p[2]);
	} else {
	    r = g = b = 0;
	}
	setRgb( r, g, b );
    } else {
	setSystemNamedColor( name );
    }
}


#undef max
#undef min

/*! \obsolete */

void QColor::getHsv( int &h, int &s, int &v ) const
{
    hsv( &h, &s, &v );
}


/*!
  Returns the current RGB value as HSV.

  \arg \e *h, hue.
  \arg \e *s, saturation.
  \arg \e *v, value.

  The hue defines the color. Its range is 0..359 if the color is chromatic
  and -1 if the color is achromatic.  The saturation and value both vary
  between 0 and 255 inclusive.

  \sa setHsv(), rgb()
*/

void QColor::hsv( int *h, int *s, int *v ) const
{
    int r = qRed(rgbVal);
    int g = qGreen(rgbVal);
    int b = qBlue(rgbVal);
    uint max = r;				// maximum RGB component
    int whatmax = 0;				// r=>0, g=>1, b=>2
    if ( (uint)g > max ) {
	max = g;
	whatmax = 1;
    }
    if ( (uint)b > max ) {
	max = b;
	whatmax = 2;
    }
    uint min = r;				// find minimum value
    if ( (uint)g < min ) min = g;
    if ( (uint)b < min ) min = b;
    int delta = max-min;
    *v = max;					// calc value
    *s = max ? (510*delta+max)/(2*max) : 0;
    if ( *s == 0 ) {
	*h = -1;				// undefined hue
    } else {
	switch ( whatmax ) {
	    case 0:				// red is max component
		if ( g >= b )
		    *h = (120*(g-b)+delta)/(2*delta);
		else
		    *h = (120*(g-b+delta)+delta)/(2*delta) + 300;
		break;
	    case 1:				// green is max component
		if ( b > r )
		    *h = 120 + (120*(b-r)+delta)/(2*delta);
		else
		    *h = 60 + (120*(b-r+delta)+delta)/(2*delta);
		break;
	    case 2:				// blue is max component
		if ( r > g )
		    *h = 240 + (120*(r-g)+delta)/(2*delta);
		else
		    *h = 180 + (120*(r-g+delta)+delta)/(2*delta);
		break;
	}
    }
}


/*!
  Sets a HSV color value.

  \arg \e h, hue (-1,0..360).  -1 means achromatic.
  \arg \e s, saturation (0..255).
  \arg \e v, value (0..255).

  \sa hsv(), setRgb()
*/

void QColor::setHsv( int h, int s, int v )
{
#if defined(CHECK_RANGE)
    if ( h < -1 || (uint)s > 255 || (uint)v > 255 ) {
	qWarning( "QColor::setHsv: HSV parameters out of range" );
	return;
    }
#endif
    int r=v, g=v, b=v;
    if ( s == 0 || h == -1 ) {			// achromatic case
	// Ignore
    } else {					// chromatic case
	if ( (uint)h >= 360 )
	    h %= 360;
	uint f = h%60;
	h /= 60;
	uint p = (uint)(2*v*(255-s)+255)/510;
	uint q, t;
	if ( h&1 ) {
	    q = (uint)(2*v*(15300-s*f)+15300)/30600;
	    switch( h ) {
		case 1: r=(int)q; g=(int)v, b=(int)p; break;
		case 3: r=(int)p; g=(int)q, b=(int)v; break;
		case 5: r=(int)v; g=(int)p, b=(int)q; break;
	    }
	} else {
	    t = (uint)(2*v*(15300-(s*(60-f)))+15300)/30600;
	    switch( h ) {
		case 0: r=(int)v; g=(int)t, b=(int)p; break;
		case 2: r=(int)p; g=(int)v, b=(int)t; break;
		case 4: r=(int)t; g=(int)p, b=(int)v; break;
	    }
	}
    }
    setRgb( r, g, b );
}


/*!
  \fn QRgb QColor::rgb() const
  Returns the RGB value.

  The return type \e QRgb is equivalent to \c unsigned \c int.

  \sa setRgb(), hsv(), qRed(), qBlue(), qGreen()
*/

/*!
  Returns the red, green and blue components of the RGB value in
  \e *r, \e *g and \e *b.  The value range for a component is 0..255.
  \sa setRgb(), hsv()
*/

void QColor::rgb( int *r, int *g, int *b ) const
{
    *r = qRed(rgbVal);
    *g = qGreen(rgbVal);
    *b = qBlue(rgbVal);
}


/*!
  Sets the RGB value to \a (r,g,b).
  \a r, \a g and \a b must be in the range 0..255.
  \sa rgb(), setHsv()
*/

void QColor::setRgb( int r, int g, int b )
{
#if defined(CHECK_RANGE)
    if ( (uint)r > 255 || (uint)g > 255 || (uint)b > 255 )
	qWarning( "QColor::setRgb: RGB parameter(s) out of range" );
#endif
    rgbVal = ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
    if ( lazy_alloc || !color_init ) {
	rgbVal |= RGB_DIRTY;			// alloc later
	pix = 0;
    } else {
	alloc();				// alloc now
    }
}


/*!
  Sets the RGB value to \a rgb.

  The type \e QRgb is equivalent to \c unsigned \c int.

  \sa rgb(), setHsv()
*/

void QColor::setRgb( QRgb rgb )
{
    if ( lazy_alloc || !color_init ) {
	rgbVal = (rgb & RGB_MASK) | RGB_DIRTY;	// alloc later
	pix = 0;
    } else {
	rgbVal = (rgb & RGB_MASK);
	alloc();				// alloc now
    }
}

/*!
  \fn int QColor::red() const
  Returns the R (red) component of the RGB value.
*/


/*!
  \fn int QColor::green() const
  Returns the G (green) component of the RGB value.
*/

/*!
  \fn int QColor::blue() const
  Returns the B (blue) component of the RGB value.
*/


/*!
  Returns a lighter (or darker) color.

  Returns a lighter color if \e factor is greater than 100.
  Setting \e factor to 150 returns a color that is 50% brighter.

  Returns a darker color if \e factor is less than 100, equal to
  dark(10000 / \e factor).

  This function converts the current RGB color to HSV, multiplies V with
  \e factor and converts back to RGB.

  \sa dark()
*/

QColor QColor::light( int factor ) const
{
    if ( factor <= 0 )				// invalid lightness factor
	return *this;
    else if ( factor < 100 )			// makes color darker
	return dark( 10000/factor );

    int h, s, v;
    hsv( &h, &s, &v );
    v = (factor*v)/100;
    if ( v > 255 ) {				// overflow
	s -= v-255;				// adjust saturation
	if ( s < 0 )
	    s = 0;
	v = 255;
    }
    QColor c;
    c.setHsv( h, s, v );
    return c;
}


/*!
  Returns a darker (or lighter) color.

  Returns a darker color if \e factor is greater than 100.
  Setting \e factor to 300 returns a color that has
  one third the brightness.

  Returns a lighter color if \e factor is less than 100, equal to
  light(10000 / \e factor).

  This function converts the current RGB color to HSV, divides V by
  \e factor and converts back to RGB.

  \sa light()
*/

QColor QColor::dark( int factor ) const
{
    if ( factor <= 0 )				// invalid darkness factor
	return *this;
    else if ( factor < 100 )			// makes color lighter
	return light( 10000/factor );
    int h, s, v;
    hsv( &h, &s, &v );
    v = (v*100)/factor;
    QColor c;
    c.setHsv( h, s, v );
    return c;
}


/*!
  \fn bool QColor::operator==( const QColor &c ) const
  Returns TRUE if this color has the same RGB value as \e c,
  or FALSE if they have different RGB values.
*/

/*!
  \fn bool QColor::operator!=( const QColor &c ) const
  Returns TRUE if this color has different RGB value from \e c,
  or FALSE if they have equal RGB values.
*/


/*!
  \fn bool QColor::lazyAlloc()
  Returns TRUE if lazy color allocation is enabled (on-demand allocation),
  or FALSE if it is disabled (immediate allocation).
  \sa setLazyAlloc()
*/

/*!
  Enables or disables lazy color allocation.

  If lazy allocation is enabled, colors are allocated the first time they
  are used (upon calling the pixel() function).	 If lazy allocation is
  disabled, colors are allocated when they are constructed or when either
  setRgb() or setHsv() is called.

  Lazy color allocation is enabled by default.

  \sa lazyAlloc(), pixel(), alloc()
*/

void QColor::setLazyAlloc( bool enable )
{
    lazy_alloc = enable;
}


/*!
  \fn uint QColor::pixel() const
  Returns the pixel value.

  This value is used by the underlying window system to refer to a color.
  It can be thought of as an index into the display hardware's color table,
  but the value is an arbitrary 32-bit value.

  \sa setLazyAlloc(), alloc()
*/


/*****************************************************************************
  QColor stream functions
 *****************************************************************************/
#ifndef QT_NO_DATASTREAM
/*!
  \relates QColor
  Writes a color object to the stream.

  \sa \link datastreamformat.html Format of the QDataStream operators \endlink
*/

QDataStream &operator<<( QDataStream &s, const QColor &c )
{
    Q_UINT32 p = (Q_UINT32)c.rgb();
    if ( s.version() == 1 )			// Swap red and blue
	p = ((p << 16) & 0xff0000) | ((p >> 16) & 0xff) | (p & 0xff00ff00);
    return s << p;
}

/*!
  \relates QColor
  Reads a color object from the stream.

  \sa \link datastreamformat.html Format of the QDataStream operators \endlink
*/

QDataStream &operator>>( QDataStream &s, QColor &c )
{
    Q_UINT32 p;
    s >> p;
    if ( s.version() == 1 )			// Swap red and blue
	p = ((p << 16) & 0xff0000) | ((p >> 16) & 0xff) | (p & 0xff00ff00);
    c.setRgb( p );
    return s;
}
#endif

/*****************************************************************************
  QColor global functions (documentation only)
 *****************************************************************************/

/*!
  \fn int qRed( QRgb rgb )
  \relates QColor
  Returns the red component of the RGB triplet \e rgb.
  \sa qRgb(), QColor::red()
*/

/*!
  \fn int qGreen( QRgb rgb )
  \relates QColor
  Returns the green component of the RGB triplet \e rgb.
  \sa qRgb(), QColor::green()
*/

/*!
  \fn int qBlue( QRgb rgb )
  \relates QColor
  Returns the blue component of the RGB triplet \e rgb.
  \sa qRgb(), QColor::blue()
*/

/*!
  \fn int qAlpha( QRgb rgba )
  \relates QColor
  Returns the alpha component of the RGBA quadruplet \e rgb.
*/

/*!
  \fn QRgb qRgb( int r, int g, int b )
  \relates QColor
  Returns the RGB triplet \a (r,g,b).

  The return type \e QRgb is equivalent to \c unsigned \c int.

  \sa qRgba(), qRed(), qGreen(), qBlue()
*/

/*!
  \fn QRgb qRgba( int r, int g, int b, int a )
  \relates QColor
  Returns the RGBA quadruplet \a (r,g,b,a).

  The return type \e QRgb is equivalent to \c unsigned \c int.

  \sa qRgb(), qRed(), qGreen(), qBlue()
*/

/*!
  \fn int qGray( int r, int g, int b )
  \relates QColor
  Returns a gray value 0..255 from the \a (r,g,b) triplet.

  The gray value is calculated using the formula:
  <code>(r*11 + g*16 + b*5)/32</code>.
*/

/*!
  \overload int qGray( qRgb rgb )
  \relates QColor
*/
