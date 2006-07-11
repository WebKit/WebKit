// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "APICast.h"
#include "JSInternalStringRef.h"

#include <kjs/JSLock.h>
#include <kjs/JSType.h>
#include <kjs/internal.h>
#include <kjs/operations.h>
#include <kjs/ustring.h>
#include <kjs/value.h>

using namespace KJS;

JSValueRef JSStringMake(JSInternalStringRef string)
{
    JSLock lock;
    UString::Rep* rep = toJS(string);
    return toRef(jsString(UString(rep)));
}

JSInternalStringRef JSInternalStringCreate(const JSChar* chars, size_t numChars)
{
    JSLock lock;
    return toRef(UString(reinterpret_cast<const UChar*>(chars), numChars).rep()->ref());
}

JSInternalStringRef JSInternalStringCreateUTF8(const char* string)
{
    JSLock lock;
    // FIXME: Only works with ASCII
    // Use decodeUTF8Sequence or http://www.unicode.org/Public/PROGRAMS/CVTUTF/ instead
    return toRef(UString(string).rep()->ref());
}

JSInternalStringRef JSInternalStringRetain(JSInternalStringRef string)
{
    UString::Rep* rep = toJS(string);
    return toRef(rep->ref());
}

void JSInternalStringRelease(JSInternalStringRef string)
{
    JSLock lock;
    UString::Rep* rep = toJS(string);
    rep->deref();
}

JSInternalStringRef JSValueCopyStringValue(JSContextRef context, JSValueRef value)
{
    JSLock lock;
    JSValue* jsValue = toJS(value);
    ExecState* exec = toJS(context);

    JSInternalStringRef stringRef = toRef(jsValue->toString(exec).rep()->ref());
    if (exec->hadException())
        exec->clearException();
    return stringRef;
}

size_t JSInternalStringGetLength(JSInternalStringRef string)
{
    UString::Rep* rep = toJS(string);
    return rep->size();
}

const JSChar* JSInternalStringGetCharactersPtr(JSInternalStringRef string)
{
    UString::Rep* rep = toJS(string);
    return reinterpret_cast<const JSChar*>(rep->data());
}

void JSInternalStringGetCharacters(JSInternalStringRef string, JSChar* buffer, size_t numChars)
{
    UString::Rep* rep = toJS(string);
    const JSChar* data = reinterpret_cast<const JSChar*>(rep->data());
    
    memcpy(buffer, data, numChars * sizeof(JSChar));
}

size_t JSInternalStringGetMaxLengthUTF8(JSInternalStringRef string)
{
    UString::Rep* rep = toJS(string);
    
    // Any UTF8 character > 3 bytes encodes as a UTF16 surrogate pair.
    return rep->size() * 3 + 1; // + 1 for terminating '\0'
}

size_t JSInternalStringGetCharactersUTF8(JSInternalStringRef string, char* buffer, size_t bufferSize)
{
    JSLock lock;
    UString::Rep* rep = toJS(string);
    CString cString = UString(rep).UTF8String();

    size_t length = std::min(bufferSize, cString.size() + 1); // + 1 for terminating '\0'
    memcpy(buffer, cString.c_str(), length);
    return length;
}

bool JSInternalStringIsEqual(JSInternalStringRef a, JSInternalStringRef b)
{
    UString::Rep* aRep = toJS(a);
    UString::Rep* bRep = toJS(b);
    
    return UString(aRep) == UString(bRep);
}

bool JSInternalStringIsEqualUTF8(JSInternalStringRef a, const char* b)
{
    JSInternalStringRef bBuf = JSInternalStringCreateUTF8(b);
    bool result = JSInternalStringIsEqual(a, bBuf);
    JSInternalStringRelease(bBuf);
    
    return result;
}

#if defined(__APPLE__)
JSInternalStringRef JSInternalStringCreateCF(CFStringRef string)
{
    JSLock lock;
    CFIndex length = CFStringGetLength(string);
    
    // Optimized path for when CFString backing store is a UTF16 buffer
    if (const UniChar* buffer = CFStringGetCharactersPtr(string)) {
        UString::Rep* rep = UString(reinterpret_cast<const UChar*>(buffer), length).rep()->ref();
        return toRef(rep);
    }

    UniChar* buffer = static_cast<UniChar*>(fastMalloc(sizeof(UniChar) * length));
    CFStringGetCharacters(string, CFRangeMake(0, length), buffer);
    UString::Rep* rep = UString(reinterpret_cast<UChar*>(buffer), length, false).rep()->ref();
    return toRef(rep);
}

CFStringRef CFStringCreateWithJSInternalString(CFAllocatorRef alloc, JSInternalStringRef string)
{
    UString::Rep* rep = toJS(string);
    return CFStringCreateWithCharacters(alloc, reinterpret_cast<const JSChar*>(rep->data()), rep->size());
}

#endif // __APPLE__
