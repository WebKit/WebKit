/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#import "KWQPointArray.h"

QPoint::QPoint() : xCoord(0), yCoord(0)
{
}

QPoint::QPoint(int xIn, int yIn) : xCoord(xIn), yCoord(yIn)
{
}

#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
QPoint::QPoint(const NSPoint &p) : xCoord((int)p.x), yCoord((int)p.y)
{
}
#endif

QPoint::QPoint(const CGPoint &p) : xCoord((int)p.x), yCoord((int)p.y)
{
}

#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
QPoint::operator NSPoint() const
{
    return NSMakePoint(xCoord, yCoord);
}
#endif

QPoint::operator CGPoint() const
{
    return CGPointMake(xCoord, yCoord);
}

QPoint operator+(const QPoint &a, const QPoint &b)
{
    return QPoint(a.xCoord + b.xCoord, a.yCoord + b.yCoord);
}

QPoint operator-(const QPoint &a, const QPoint &b)
{
    return QPoint(a.xCoord - b.xCoord, a.yCoord - b.yCoord);
}

const QPoint operator*(const QPoint &p, double s)
{
    return QPoint((int)(p.xCoord * s), (int)(p.yCoord * s));
}

#ifdef _KWQ_IOSTREAM_
std::ostream &operator<<(std::ostream &o, const QPoint &p)
{
    return o << "QPoint: [x: " << p.x() << "; h: " << p.y() << "]";
}
#endif
