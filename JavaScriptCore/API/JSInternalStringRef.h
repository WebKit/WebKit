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

#ifndef JSInternalStringRef_h
#define JSInternalStringRef_h

#include "JSValueRef.h"
#ifdef __cplusplus
extern "C" {
#endif

/*!
@typedef JSChar
@abstract A Unicode character.
*/
#if !defined(WIN32) && !defined(_WIN32)
    typedef unsigned short JSChar;
#else
    typedef wchar_t JSChar;
#endif

/*!
@function
@abstract         Creates a JavaScript string from a buffer of Unicode characters.
@param chars      The buffer of Unicode characters to copy into the new JSInternalString.
@param numChars   The number of characters to copy from the buffer pointed to by chars.
@result           A JSInternalString containing chars. Ownership follows the create rule.
*/
JSInternalStringRef JSInternalStringCreate(const JSChar* chars, size_t numChars);
/*!
@function
@abstract         Creates a JavaScript string from a null-terminated UTF8 string.
@param string     The null-terminated UTF8 string to copy into the new JSInternalString.
@result           A JSInternalString containing string. Ownership follows the create rule.
*/
JSInternalStringRef JSInternalStringCreateUTF8(const char* string);

/*!
@function
@abstract         Retains a JavaScript string.
@param buffer     The JSInternalString to retain.
@result           A JSInternalString that is the same as buffer.
*/
JSInternalStringRef JSInternalStringRetain(JSInternalStringRef buffer);
/*!
@function
@abstract         Releases a JavaScript string.
@param buffer     The JSInternalString to release.
*/
void JSInternalStringRelease(JSInternalStringRef buffer);

/*!
@function
@abstract         Returns the number of Unicode characters in a JavaScript string.
@param buffer     The JSInternalString whose length (in Unicode characters) you want to know.
@result           The number of Unicode characters stored in buffer.
*/
size_t JSInternalStringGetLength(JSInternalStringRef buffer);
/*!
@function
@abstract         Quickly obtains a pointer to the Unicode character buffer that 
 serves as the backing store for a JavaScript string.
@param buffer     The JSInternalString whose backing store you want to access.
@result           A pointer to the Unicode character buffer that serves as buffer's 
 backing store, which will be deallocated when buffer is deallocated.
*/
const JSChar* JSInternalStringGetCharactersPtr(JSInternalStringRef buffer);
/*!
@function
@abstract         Copies a JavaScript string's Unicode characters into an 
 external character buffer.
@param inBuffer   The source JSInternalString.
@param outBuffer  The destination JSChar buffer into which to copy inBuffer's 
 characters. On return, outBuffer contains the requested Unicode characters.
@param numChars   The number of Unicode characters to copy. This number must not 
 exceed the length of the string.
*/
void JSInternalStringGetCharacters(JSInternalStringRef inBuffer, JSChar* outBuffer, size_t numChars);

/*!
@function
@abstract         Returns the maximum number of bytes required to encode the 
 contents of a JavaScript string as a null-terminated UTF8 string.
@param buffer     The JSInternalString whose maximum encoded length (in bytes) you 
 want to know.
@result           The maximum number of bytes required to encode buffer's contents 
 as a null-terminated UTF8 string.
*/
size_t JSInternalStringGetMaxLengthUTF8(JSInternalStringRef buffer);
/*!
@function
@abstract         Converts a JavaScript string's contents into a 
 null-terminated UTF8 string, and copies the result into an external byte buffer.
@param inBuffer   The source JSInternalString.
@param outBuffer  The destination byte buffer into which to copy a UTF8 string 
 representation of inBuffer. The buffer must be at least bufferSize bytes in length. 
 On return, outBuffer contains a UTF8 string representation of inBuffer. 
 If bufferSize is too small, outBuffer will contain only partial results.
@param bufferSize The length of the external buffer in bytes.
@result           The number of bytes written into outBuffer (including the null-terminator byte).
*/
size_t JSInternalStringGetCharactersUTF8(JSInternalStringRef inBuffer, char* outBuffer, size_t bufferSize);

/*!
@function
@abstract     Tests whether the characters in two JavaScript strings match.
@param a      The first JSInternalString to test.
@param b      The second JSInternalString to test.
@result       true if the characters in the two buffers match, otherwise false.
*/
bool JSInternalStringIsEqual(JSInternalStringRef a, JSInternalStringRef b);
/*!
@function
@abstract     Tests whether the characters in a JavaScript string match 
 the characters in a null-terminated UTF8 string.
@param a      The JSInternalString to test.
@param b      The null-terminated UTF8 string to test.
@result       true if the characters in the two buffers match, otherwise false.
*/
bool JSInternalStringIsEqualUTF8(JSInternalStringRef a, const char* b);

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
// CFString convenience methods
/*!
@function
@abstract         Creates a JavaScript string from a CFString.
@discussion       This function is optimized to take advantage of cases when 
 CFStringGetCharactersPtr returns a valid pointer.
@param string     The CFString to copy into the new JSInternalString.
@result           A JSInternalString containing string. Ownership follows the create rule.
*/
JSInternalStringRef JSInternalStringCreateCF(CFStringRef string);
/*!
@function
@abstract         Creates a CFString form a JavaScript string.
@param alloc      The alloc parameter to pass to CFStringCreate.
@param buffer     The JSInternalString to copy into the new CFString.
@result           A CFString containing buffer. Ownership follows the create rule.
*/
CFStringRef CFStringCreateWithJSInternalString(CFAllocatorRef alloc, JSInternalStringRef buffer);
#endif // __APPLE__
    
#ifdef __cplusplus
}
#endif

#endif // JSInternalStringRef_h
