/*
 * Copyright (C) 2003-6 Apple Computer, Inc.  All rights reserved.
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

#ifndef INTSIZE_H_
#define INTSIZE_H_

#if __APPLE__
// workaround for <rdar://problem/4294625>
#if ! __LP64__ && ! NS_BUILD_32_LIKE_64
#undef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
#endif

#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGSize NSSize;
#else
typedef struct _NSSize NSSize;
#endif
typedef struct CGSize CGSize;
#endif

namespace WebCore {

class IntSize {
public:
    IntSize();
    IntSize(int,int);
    
#if __APPLE__
#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
    explicit IntSize(const NSSize &);
#endif
    explicit IntSize(const CGSize &);
#endif

    bool isValid() const;
    int width() const { return w; }
    int height() const { return h; }
    void setWidth(int width) { w = width; }
    void setHeight(int height) { h = height; }
    IntSize expandedTo(const IntSize &) const;

#if __APPLE__    
#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
    operator NSSize() const;
#endif
    operator CGSize() const;
#endif

    friend IntSize operator+(const IntSize &, const IntSize &);
    friend bool operator==(const IntSize &, const IntSize &);
    friend bool operator!=(const IntSize &, const IntSize &);

private:
    int w;
    int h;
};

}

// FIXME: Remove when the engine files have been converted to be in the WebCore namespace.
using WebCore::IntSize;

#endif
