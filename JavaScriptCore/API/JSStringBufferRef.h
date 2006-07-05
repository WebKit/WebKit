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

#ifndef JSStringBufferRef_h
#define JSStringBufferRef_h

#include "JSValueRef.h"
#ifdef __cplusplus
extern "C" {
#endif

#if defined(WIN32) || defined(_WIN32)
    typedef wchar_t JSChar;
#else
    typedef unsigned short JSChar;
#endif
    
JSStringBufferRef JSStringBufferCreate(const JSChar* chars, size_t numChars);
JSStringBufferRef JSStringBufferCreateUTF8(const char* string);

JSStringBufferRef JSStringBufferRetain(JSStringBufferRef buffer);
void JSStringBufferRelease(JSStringBufferRef buffer);

size_t JSStringBufferGetLength(JSStringBufferRef buffer);
const JSChar* JSStringBufferGetCharactersPtr(JSStringBufferRef buffer);
void JSStringBufferGetCharacters(JSStringBufferRef inBuffer, JSChar* outBuffer, size_t numChars);

size_t JSStringBufferGetMaxLengthUTF8(JSStringBufferRef buffer);
// Returns the number of bytes written into outBuffer, including the trailing '\0'
size_t JSStringBufferGetCharactersUTF8(JSStringBufferRef inBuffer, char* outBuffer, size_t bufferSize);

bool JSStringBufferIsEqual(JSStringBufferRef a, JSStringBufferRef b);
bool JSStringBufferIsEqualUTF8(JSStringBufferRef a, const char* b);

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
// CFString convenience methods
JSStringBufferRef JSStringBufferCreateWithCFString(CFStringRef string);
CFStringRef CFStringCreateWithJSStringBuffer(CFAllocatorRef alloc, JSStringBufferRef buffer);
#endif // __APPLE__
    
#ifdef __cplusplus
}
#endif

#endif // JSStringBufferRef_h
