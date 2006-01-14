/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
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
#include "IntPoint.h"
#include "FloatPoint.h"

namespace WebCore {

FloatPoint::FloatPoint() : xCoord(0), yCoord(0)
{
}

FloatPoint::FloatPoint(float xIn, float yIn) : xCoord(xIn), yCoord(yIn)
{
}

FloatPoint::FloatPoint(const IntPoint& p) :xCoord(p.x()), yCoord(p.y())
{
}

FloatPoint operator+(const FloatPoint& a, const FloatPoint& b)
{
    return FloatPoint(a.xCoord + b.xCoord, a.yCoord + b.yCoord);
}

FloatPoint operator-(const FloatPoint& a, const FloatPoint& b)
{
    return FloatPoint(a.xCoord - b.xCoord, a.yCoord - b.yCoord);
}

const FloatPoint operator*(const FloatPoint& p, double s)
{
    return FloatPoint(p.xCoord * s, p.yCoord * s);
}

}
