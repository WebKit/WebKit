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

#include <qcolor.h>

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
    if ( !globals_init )
	initGlobalColors();
    else
        _initialize (r, g, b);
}


QColor::QColor(const char *)
{
    if ( !globals_init )
	initGlobalColors();
    NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
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
    if (color == copyFrom.color)
        return;
    if (color != nil)
        [color release];
    if (copyFrom.color != nil)
        color = [copyFrom.color retain];
    else
        color = nil;
}


QString QColor::name() const
{
    NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QColor::setNamedColor(const QString&)
{
    NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool QColor::isValid() const
{
    return TRUE;
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


void QColor::hsv(int *, int *, int *) const
{
    NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}

QColor QColor::light(int f = 150) const
{
    NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


QColor QColor::dark(int f = 200) const
{
    NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
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
    NSAutoreleasePool *colorPool = [[NSAutoreleasePool allocWithZone:NULL] init];
     
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
}

