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

#ifndef FLOATPOINT_H_
#define FLOATPOINT_H_

#if __APPLE__
// workaround for <rdar://problem/4294625>
#if ! __LP64__ && ! NS_BUILD_32_LIKE_64
#undef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
#endif

#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
#else
typedef struct _NSPoint NSPoint;
#endif
typedef struct CGPoint CGPoint;
#endif

namespace WebCore {

class IntPoint;

class FloatPoint {
public:
    FloatPoint();
    FloatPoint(float, float);
    FloatPoint(const IntPoint&);
    
#if __APPLE__
#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
    explicit FloatPoint(const NSPoint&);
#endif
    explicit FloatPoint(const CGPoint&);
#endif

    float x() const { return xCoord; }
    float y() const { return yCoord; }

    void setX(int x) { xCoord = x; }
    void setY(int y) { yCoord = y; }

    bool isNull() const { return xCoord == 0.0f && yCoord == 0.0f; }

    FloatPoint& operator -=(const FloatPoint& two) { xCoord -= two.xCoord; yCoord -= two.yCoord; return *this; }
    friend const FloatPoint operator*(const FloatPoint& p, double s);
    friend FloatPoint operator+(const FloatPoint&, const FloatPoint&);
    friend FloatPoint operator-(const FloatPoint&, const FloatPoint&);

#if __APPLE__
#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
    operator NSPoint() const;
#endif
    operator CGPoint() const;
#endif

private:
    float xCoord;
    float yCoord;
};

}

// FIXME: Remove when everything is in the WebCore namespace.
using WebCore::FloatPoint;

#endif
