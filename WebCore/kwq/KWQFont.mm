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

#import <qfont.h>
#import <qstring.h>

#define Fixed MacFixed
#define Rect MacRect
#define Boolean MacBoolean
#include <Cocoa/Cocoa.h>
#undef Fixed
#undef Rect
#undef Boolean

QFont::QFont()
    : _family([@"" retain])
    , _trait(0)
    , _size(12.0)
{
}

QFont::QFont(const QFont &copyFrom)
    : _family([copyFrom._family retain])
    , _trait(copyFrom._trait)
    , _size(copyFrom._size)
{
}

QFont::~QFont()
{
    [_family release];
}

// member functions --------------------------------------------------------

int QFont::pixelSize() const
{
    return (int)_size;
}

QString QFont::family() const
{
    return NSSTRING_TO_QSTRING(_family);
}

void QFont::setFamily(const QString &qfamilyName)
{
    [_family release];
    _family = [_FAST_QSTRING_TO_NSSTRING(qfamilyName) retain];
}

void QFont::setPixelSize(int sz)
{
    _size = sz;
}

void QFont::setPixelSizeFloat(float sz)
{
    _size = sz;
}

void QFont::setWeight(int weight)
{
    if (weight == Bold) {
        _trait |= NSBoldFontMask;
    }
    else if (weight == Normal) {
        _trait &= ~NSBoldFontMask;
    }
}

int QFont::weight() const
{
    return bold() ? Bold : Normal;
}

bool QFont::setItalic(bool flag)
{
    if (flag) {
        _trait |= NSItalicFontMask;
    }
    else {
        _trait &= ~NSItalicFontMask;
    }
    return false;
}

bool QFont::italic() const
{
    return _trait & NSItalicFontMask;
}

bool QFont::bold() const
{
    return _trait & NSBoldFontMask;
}

QFont &QFont::operator=(const QFont &assignFrom)
{
    [assignFrom._family retain];
    [_family release];
    _family = assignFrom._family;
    _trait = assignFrom._trait;
    _size = assignFrom._size;
    return *this;
}

bool QFont::operator==(const QFont &compareFont) const
{
    return [_family isEqual:compareFont._family]
        && _trait == compareFont._trait
        && _size == compareFont._size;
}
