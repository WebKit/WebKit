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

#import <Cocoa/Cocoa.h>
#import "WebCoreTextRendererFactory.h"

QFontFamily::QFontFamily()
    : _family(@"")
    , _next(0)
    , _refCnt(0)
{
}

QFontFamily::QFontFamily(const QFontFamily& other) 
{
    _next = other._next;
    if (_next)
        _next->ref();
    _family = other._family;
}

QFontFamily& QFontFamily::operator=(const QFontFamily& other) {
    if (this != &other) {
        if (_next)
            _next->deref();
        _next = other._next;
        if (_next)
            _next->ref();
        _family = other._family;
    }
    return *this;
}

QString QFontFamily::family() const
{
    return QString::fromNSString(_family);
}

void QFontFamily::setFamily(const QString &qfamilyName)
{
    // Use an immutable copy of the name, but keep a set of
    // all family names so we don't end up with too many objects.
    static NSMutableSet *families;
    if (families == nil) {
        families = [[NSMutableSet alloc] init];
    }
    NSString *mutableName = qfamilyName.getNSString();
    _family = [families member:mutableName];
    if (!_family) {
        [families addObject:mutableName];
        _family = [families member:mutableName];
    }
}

bool QFontFamily::operator==(const QFontFamily &compareFontFamily) const
{
    if ((!_next && compareFontFamily._next) || 
        (_next && !compareFontFamily._next) ||
        ((_next && compareFontFamily._next) && (*_next != *(compareFontFamily._next))))
        return false;
    
    return _family == compareFontFamily._family;
}

QFont::QFont()
    : _trait(0)
    , _size(12.0)
{
}

QString QFont::family() const
{
    return _family.family();
}

void QFont::setFamily(const QString &qfamilyName)
{
    _family.setFamily(qfamilyName);
}

void QFont::setWeight(int weight)
{
    if (weight == Bold) {
        _trait |= NSBoldFontMask;
    } else if (weight == Normal) {
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
        _trait |= NSItalicFontMask;
    } else {
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
    return [[WebCoreTextRendererFactory sharedFactory] 
    	fontWithFamily:getNSFamily()
                traits:getNSTraits() 
                  size:getNSSize()];
}

NSFont *QFont::getNSFontWithFamily(QFontFamily* family) const
{
    return [[WebCoreTextRendererFactory sharedFactory] 
    	fontWithFamily:family->getNSFamily()
                traits:getNSTraits() 
                  size:getNSSize()];
}

