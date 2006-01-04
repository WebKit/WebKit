/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "KWQSizeF.h"
#import "KWQSize.h"

QSizeF::QSizeF() : w(-1.0f), h(-1.0f)
{
}

QSizeF::QSizeF(float width, float height) : w(width), h(height)
{
}

QSizeF::QSizeF(const QSize& o) : w(o.width()), h(o.height())
{
}

#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
QSizeF::QSizeF(const NSSize& s) : w(s.width), h(s.height)
{
}
#endif

QSizeF::QSizeF(const CGSize& s) : w(s.width), h(s.height)
{
}

bool QSizeF::isValid() const
{
    return w >= 0.0f && h >= 0.0f;
}

QSizeF QSizeF::expandedTo(const QSizeF& o) const
{
    return QSizeF(w > o.w ? w : o.w, h > o.h ? h : o.h);
}

#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
QSizeF::operator NSSize() const
{
    return NSMakeSize(w, h);
}
#endif

QSizeF::operator CGSize() const
{
    return CGSizeMake(w, h);
}

QSizeF operator+(const QSizeF& a, const QSizeF& b)
{
    return QSizeF(a.w + b.w, a.h + b.h);
}

bool operator==(const QSizeF& a, const QSizeF& b)
{
    return a.w == b.w && a.h == b.h;
}

bool operator!=(const QSizeF& a, const QSizeF& b)
{
    return a.w != b.w || a.h != b.h;
}
