/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQPointArray.h"
#import <stdarg.h>

QPointArray::QPointArray(int nPoints, const int *points)
{
    setPoints( nPoints, points );
}


void QPointArray::setPoint( uint index, int x, int y )
{
    QMemArray<QPoint>::at( index ) = QPoint( x, y );
}


bool QPointArray::setPoints( int nPoints, const int *points )
{
    if ( !resize(nPoints) )
	return FALSE;
    int i = 0;
    while ( nPoints-- ) {			// make array of points
	setPoint( i++, *points, *(points+1) );
	points++;
	points++;
    }
    return TRUE;
}

// FIXME: Workaround for Radar 2921061
#if 0

bool QPointArray::setPoints( int nPoints, int firstx, int firsty, ... )
{
    va_list ap;
    if ( !resize(nPoints) )
	return FALSE;
    setPoint( 0, firstx, firsty );		// set first point
    int i = 1, x, y;
    nPoints--;
    va_start( ap, firsty );
    while ( nPoints-- ) {
	x = va_arg( ap, int );
	y = va_arg( ap, int );
	setPoint( i++, x, y );
    }
    va_end( ap );
    return TRUE;
}

#else

bool QPointArray::setPoints( int nPoints, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3)
{
    if ( !resize(nPoints) )
	return FALSE;
    setPoint( 0, x0, y0 );
    setPoint( 1, x1, y1 );
    setPoint( 2, x2, y2 );
    setPoint( 3, x3, y3 );
    return TRUE;
}

#endif
