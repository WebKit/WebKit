/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "IntPointArray.h"
#include "IntRect.h"

IntPointArray::IntPointArray(int nPoints, const int *points)
{
    setPoints(nPoints, points);
}

IntPointArray::IntPointArray(const IntRect &rect)
{
    setPoints(4, rect.topLeft().x(), rect.topLeft().y(),
              rect.topRight().x(), rect.topRight().y(),
              rect.bottomRight().x(), rect.bottomRight().y(),
              rect.bottomLeft().x(), rect.bottomLeft().y());
}

IntPointArray IntPointArray::copy() const
{
    IntPointArray copy;
    copy.duplicate(*this);
    return copy;
}

IntRect IntPointArray::boundingRect() const
{
    int nPoints = count();
    
    if (nPoints < 1) return IntRect(0,0,0,0);
    
    int minX = INT_MAX, maxX = 0;
    int minY = INT_MAX, maxY = 0;
    
    while (nPoints > 0) {
        IntPoint p = at(nPoints);
        int x = p.x(), y = p.y();
        
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
        
        nPoints--;
    }
    
    return IntRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

void IntPointArray::point(uint index, int *x, int *y)
{
    IntPoint p = at(index);
    *x = p.x();
    *y = p.y();
}

void IntPointArray::setPoint( uint index, int x, int y )
{
    Array<IntPoint>::at( index ) = IntPoint( x, y );
}


bool IntPointArray::setPoints( int nPoints, const int *points )
{
    if ( !resize(nPoints) )
        return false;
    int i = 0;
    while ( nPoints-- ) {                   // make array of points
        setPoint( i++, *points, *(points+1) );
        points++;
        points++;
    }
    return true;
}

bool IntPointArray::setPoints( int nPoints, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3)
{
    if ( !resize(nPoints) )
        return false;
    setPoint( 0, x0, y0 );
    setPoint( 1, x1, y1 );
    setPoint( 2, x2, y2 );
    setPoint( 3, x3, y3 );
    return true;
}
