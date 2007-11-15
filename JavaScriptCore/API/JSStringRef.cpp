// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "JSStringRef.h"

#include <wtf/Platform.h>

#include "APICast.h"
#include <kjs/JSLock.h>
#include <kjs/JSType.h>
#include <kjs/internal.h>
#include <kjs/operations.h>
#include <kjs/ustring.h>
#include <kjs/value.h>
#include <wtf/unicode/UTF8.h>

using namespace KJS;
using namespace WTF::Unicode;

JSStringRef JSStringCreateWithCharacters(const JSChar* chars, size_t numChars)
{
    JSLock lock;
    return toRef(UString(reinterpret_cast<const KJS::UChar*>(chars), static_cast<int>(numChars)).rep()->ref());
}

JSStringRef JSStringCreateWithUTF8CString(const char* string)
{
    JSLock lock;

    size_t length = strlen(string);
    Vector< ::UChar, 1024> buffer(length);
    ::UChar* p = buffer.data();
    if (conversionOK != convertUTF8ToUTF16(&string, string + length, &p, p + length))
        return 0;

    return toRef(UString(reinterpret_cast<KJS::UChar*>(buffer.data()), p - buffer.data()).rep()->ref());
}

JSStringRef JSStringRetain(JSStringRef string)
{
    JSLock lock;
    UString::Rep* rep = toJS(string);
    return toRef(rep->ref());
}

void JSStringRelease(JSStringRef string)
{
    JSLock lock;
    UString::Rep* rep = toJS(string);
    rep->deref();
}

size_t JSStringGetLength(JSStringRef string)
{
    UString::Rep* rep = toJS(string);
    return rep->size();
}

const JSChar* JSStringGetCharactersPtr(JSStringRef string)
{
    UString::Rep* rep = toJS(string);
    return reinterpret_cast<const JSChar*>(rep->data());
}

size_t JSStringGetMaximumUTF8CStringSize(JSStringRef string)
{
    UString::Rep* rep = toJS(string);
    
    // Any UTF8 character > 3 bytes encodes as a UTF16 surrogate pair.
    return rep->size() * 3 + 1; // + 1 for terminating '\0'
}

size_t JSStringGetUTF8CString(JSStringRef string, char* buffer, size_t bufferSize)
{
    JSLock lock;
    UString::Rep* rep = toJS(string);
    CString cString = UString(rep).UTF8String();

    size_t length = std::min(bufferSize, cString.size() + 1); // + 1 for terminating '\0'
    memcpy(buffer, cString.c_str(), length);
    return length;
}

bool JSStringIsEqual(JSStringRef a, JSStringRef b)
{
    JSLock lock;

    UString::Rep* aRep = toJS(a);
    UString::Rep* bRep = toJS(b);
    
    return UString(aRep) == UString(bRep);
}

bool JSStringIsEqualToUTF8CString(JSStringRef a, const char* b)
{
    JSStringRef bBuf = JSStringCreateWithUTF8CString(b);
    bool result = JSStringIsEqual(a, bBuf);
    JSStringRelease(bBuf);
    
    return result;
}
