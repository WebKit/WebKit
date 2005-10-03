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
#include "KWQFontFamily.h"
#include "xml/dom_stringimpl.h"

using DOM::AtomicString;
using DOM::DOMStringImpl;

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

const void *retainDOMStringImpl(CFAllocatorRef allocator, const void *value)
{
    ((DOMStringImpl*)value)->ref();
    return value;
}

void releaseDOMStringImpl(CFAllocatorRef allocator, const void *value)
{
    ((DOMStringImpl*)value)->deref();
}

const CFDictionaryKeyCallBacks CFDictionaryFamilyKeyCallBacks = { 0, retainDOMStringImpl, releaseDOMStringImpl, 0, 0, 0 };

NSString *KWQFontFamily::getNSFamily() const
{
    if (!_NSFamily) {
        if (_family.isEmpty())
            _NSFamily = @"";
        else {
            // Use an immutable copy of the name, but keep a set of
            // all family names so we don't end up with too many objects.
            static CFMutableDictionaryRef families;
            if (families == NULL)
                families = CFDictionaryCreateMutable(NULL, 0, &CFDictionaryFamilyKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            _NSFamily = (NSString *)CFDictionaryGetValue(families, _family.impl());
            if (!_NSFamily) {
                _NSFamily = [NSString stringWithCharacters:(const unichar *)_family.unicode() length:_family.length()];
                CFDictionarySetValue(families, _family.impl(), _NSFamily);
            }
        }
    }
    return _NSFamily;
}

void KWQFontFamily::setFamily(const AtomicString &family)
{
    _family = family;
    _NSFamily = nil;
}

bool KWQFontFamily::operator==(const KWQFontFamily &compareFontFamily) const
{
    if ((!_next && compareFontFamily._next) || 
        (_next && !compareFontFamily._next) ||
        ((_next && compareFontFamily._next) && (*_next != *(compareFontFamily._next))))
        return false;
    
    return _family == compareFontFamily._family;
}
