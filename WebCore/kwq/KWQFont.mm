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

#import "KWQFont.h"

#import "KWQString.h"
#import "WebCoreTextRendererFactory.h"

QFont::QFont()
    : _trait(0)
    , _size(12.0)
    , _isPrinterFont(false)
    , _NSFont(0)
{
}

QFont::~QFont()
{
    [_NSFont release];
}

QFont::QFont(const QFont &other)
    : _family(other._family)
    , _trait(other._trait)
    , _size(other._size)
    , _isPrinterFont(other._isPrinterFont)
    , _NSFont([other._NSFont retain])
{
}

QFont &QFont::operator=(const QFont &other)
{
    _family = other._family;
    _trait = other._trait;
    _size = other._size;
    _isPrinterFont = other._isPrinterFont;
    [other._NSFont retain];
    [_NSFont release];
    _NSFont = other._NSFont;
    return *this;
}

QString QFont::family() const
{
    return _family.family();
}

void QFont::setFamily(const QString &qfamilyName)
{
    _family.setFamily(qfamilyName);
    [_NSFont release];
    _NSFont = 0;
}

void QFont::setFirstFamily(const KWQFontFamily& family) 
{
    _family = family;
    [_NSFont release];
    _NSFont = 0;
}

void QFont::setPixelSize(float s)
{
    if (_size != s) {
        [_NSFont release]; 
        _NSFont = 0;
    }
    _size = s;
}

void QFont::setWeight(int weight)
{
    if (weight == Bold) {
        if (!(_trait & NSBoldFontMask)){
            [_NSFont release];
            _NSFont = 0;
        }
        _trait |= NSBoldFontMask;
    } else if (weight == Normal) {
        if ((_trait & NSBoldFontMask)){
            [_NSFont release];
            _NSFont = 0;
        }
        _trait &= ~NSBoldFontMask;
    }
}

void QFont::setPrinterFont(bool isPrinterFont)
{
    _isPrinterFont = isPrinterFont;
}

int QFont::weight() const
{
    return bold() ? Bold : Normal;
}

void QFont::setItalic(bool flag)
{
    if (flag) {
        if (!(_trait & NSItalicFontMask)){
            [_NSFont release];
            _NSFont = 0;
        }
        _trait |= NSItalicFontMask;
    } else {
        if ((_trait & NSItalicFontMask)){
            [_NSFont release];
            _NSFont = 0;
        }
        _trait &= ~NSItalicFontMask;
    }
}

bool QFont::italic() const
{
    return _trait & NSItalicFontMask;
}

bool QFont::bold() const
{
    return _trait & NSBoldFontMask;
}

bool QFont::isFixedPitch() const
{
    return [[WebCoreTextRendererFactory sharedFactory] isFontFixedPitch: getNSFont()];
}


bool QFont::operator==(const QFont &compareFont) const
{
    return _family == compareFont._family
        && _trait == compareFont._trait
        && _size == compareFont._size
        && _isPrinterFont == compareFont._isPrinterFont;
}

NSFont *QFont::getNSFont() const
{
    if (!_NSFont) {
        CREATE_FAMILY_ARRAY(*this, families);
        _NSFont = [[[WebCoreTextRendererFactory sharedFactory] 
            fontWithFamilies:families
                      traits:getNSTraits() 
                        size:getNSSize()] retain];
    }
    return _NSFont;
}
