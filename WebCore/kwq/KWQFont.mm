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

#import "KWQFont.h"

#import "KWQString.h"
#import "WebCoreTextRendererFactory.h"

KWQFontFamily::KWQFontFamily()
    : _next(0)
    , _refCnt(0)
    , _NSFamily(0)
{
}

KWQFontFamily::KWQFontFamily(const KWQFontFamily& other)
    : _family(other._family)
    , _next(other._next)
    , _refCnt(0)
    , _NSFamily(other._NSFamily)
{
    if (_next)
        _next->ref();
}

KWQFontFamily& KWQFontFamily::operator=(const KWQFontFamily& other)
{
    if (other._next)
        other._next->ref();
    if (_next)
        _next->deref();
    _family = other._family;
    _next = other._next;
    _NSFamily = other._NSFamily;
    return *this;
}

NSString *KWQFontFamily::getNSFamily() const
{
    if (!_NSFamily) {
        // Use an immutable copy of the name, but keep a set of
        // all family names so we don't end up with too many objects.
        static CFMutableDictionaryRef families;
        if (families == NULL) {
            families = CFDictionaryCreateMutable(NULL, 0, &CFDictionaryQStringKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        }
        _NSFamily = (NSString *)CFDictionaryGetValue(families, &_family);
        if (!_NSFamily) {
            _NSFamily = _family.getNSString();
            CFDictionarySetValue(families, &_family, _NSFamily);
        }
    }
    return _NSFamily;
}

void KWQFontFamily::setFamily(const QString &family)
{
    if (family == _family) {
        return;
    }
    _family = family;
    _NSFamily = nil;
}

bool KWQFontFamily::operator==(const KWQFontFamily &compareFontFamily) const
{
    if ((!_next && compareFontFamily._next) || 
        (_next && !compareFontFamily._next) ||
        ((_next && compareFontFamily._next) && (*_next != *(compareFontFamily._next))))
        return false;
    
    return getNSFamily() == compareFontFamily.getNSFamily();
}

QFont::QFont()
    : _trait(0)
    , _size(12.0)
    , _nsfont(0)
{
}

QFont::~QFont()
{ 
    [_nsfont release];
}

QString QFont::family() const
{
    return _family.family();
}

void QFont::setFamily(const QString &qfamilyName)
{
    _family.setFamily(qfamilyName);
    [_nsfont release];
    _nsfont = 0;
}

void QFont::setFirstFamily(const KWQFontFamily& family) 
{
    _family = family;
    [_nsfont release];
    _nsfont = 0;
}

void QFont::setPixelSize(float s)
{
    if (_size != s) {
        [_nsfont release]; 
        _nsfont = 0;
    }
    _size = s;
}

void QFont::setWeight(int weight)
{
    if (weight == Bold) {
        if (!(_trait & NSBoldFontMask)){
            [_nsfont release];
            _nsfont = 0;
        }
        _trait |= NSBoldFontMask;
    } else if (weight == Normal) {
        if ((_trait & NSBoldFontMask)){
            [_nsfont release];
            _nsfont = 0;
        }
        _trait &= ~NSBoldFontMask;
    }
}

int QFont::weight() const
{
    return bold() ? Bold : Normal;
}

void QFont::setItalic(bool flag)
{
    if (flag) {
        if (!(_trait & NSItalicFontMask)){
            [_nsfont release];
            _nsfont = 0;
        }
        _trait |= NSItalicFontMask;
    } else {
        if ((_trait & NSItalicFontMask)){
            [_nsfont release];
            _nsfont = 0;
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

bool QFont::operator==(const QFont &compareFont) const
{
    return _family == compareFont._family
        && _trait == compareFont._trait
        && _size == compareFont._size;
}

NSFont *QFont::getNSFont() const
{
    if (!_nsfont) {
        CREATE_FAMILY_ARRAY(*this, families);
        _nsfont = [[[WebCoreTextRendererFactory sharedFactory] 
            fontWithFamilies:families
                      traits:getNSTraits() 
                        size:getNSSize()] retain];
    }
    return _nsfont;
}
