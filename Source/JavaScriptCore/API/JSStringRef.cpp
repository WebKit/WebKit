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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "JSStringRefPrivate.h"

#include "InitializeThreading.h"
#include "OpaqueJSString.h"
#include <wtf/unicode/UTF8Conversion.h>

using namespace JSC;
using namespace WTF::Unicode;

JSStringRef JSStringCreateWithCharacters(const JSChar* chars, size_t numChars)
{
    JSC::initialize();
    return &OpaqueJSString::create(reinterpret_cast<const UChar*>(chars), numChars).leakRef();
}

JSStringRef JSStringCreateWithUTF8CString(const char* string)
{
    JSC::initialize();
    if (string) {
        size_t length = strlen(string);
        Vector<UChar, 1024> buffer(length);
        UChar* p = buffer.data();
        bool sourceIsAllASCII;
        const LChar* stringStart = reinterpret_cast<const LChar*>(string);
        if (convertUTF8ToUTF16(string, string + length, &p, p + length, &sourceIsAllASCII)) {
            if (sourceIsAllASCII)
                return &OpaqueJSString::create(stringStart, length).leakRef();
            return &OpaqueJSString::create(buffer.data(), p - buffer.data()).leakRef();
        }
    }

    return &OpaqueJSString::create().leakRef();
}

JSStringRef JSStringCreate(const char* string, size_t length)
{
    JSC::initialize();
    return &OpaqueJSString::create((reinterpret_cast<const LChar*>(string)), length).leakRef();
}

JSStringRef JSStringCreateStatic(const char* string, size_t length)
{
    JSC::initialize();

    Ref<ExternalStringImpl> impl = ExternalStringImpl::createStatic(reinterpret_cast<const LChar*>(string), length);
    String str = *new String(impl.get());
    return OpaqueJSString::tryCreate(str).leakRef();
}

JSStringRef JSStringCreateExternal(const char* string, size_t length, void* finalize_ptr, const ExternalStringFinalizer finalizer)
{
    JSC::initialize();

    Ref<ExternalStringImpl> impl = ExternalStringImpl::create(reinterpret_cast<const LChar*>(string), length, finalize_ptr, [finalizer](void * finalize_ptr, void * buffer, unsigned bufferSize) -> void {
        if (finalizer != nullptr) finalizer(finalize_ptr, buffer, bufferSize);
	});
    String str = *new String(impl.get());
    return OpaqueJSString::tryCreate(str).leakRef();
}

JSStringRef JSStringCreateWithCharactersNoCopy(const JSChar* chars, size_t numChars)
{
    JSC::initialize();
    Ref<ExternalStringImpl> impl = ExternalStringImpl::createStatic(reinterpret_cast<const UChar*>(chars), numChars);
    String str = *new String(impl.get());
    return OpaqueJSString::tryCreate(str).leakRef();
}

JSStringRef JSStringRetain(JSStringRef string)
{
    string->ref();
    return string;
}

void JSStringRelease(JSStringRef string)
{
    string->deref();
}

size_t JSStringGetLength(JSStringRef string)
{
    if (!string)
        return 0;
    return string->length();
}

const JSChar* JSStringGetCharactersPtr(JSStringRef string)
{
    if (!string)
        return nullptr;
    return reinterpret_cast<const JSChar*>(string->characters());
}

const char* JSStringGetCharacters8Ptr(JSStringRef string)
{
    if (!string)
        return nullptr;
    return reinterpret_cast<const char*>(string->characters8());
}

size_t JSStringGetMaximumUTF8CStringSize(JSStringRef string)
{
    // Any UTF8 character > 3 bytes encodes as a UTF16 surrogate pair.
    return string->length() * 3 + 1; // + 1 for terminating '\0'
}

size_t JSStringGetUTF8CString(JSStringRef string, char* buffer, size_t bufferSize)
{
    if (!string || !buffer || !bufferSize)
        return 0;

    char* destination = buffer;
    bool failed = false;
    if (string->is8Bit()) {
        const LChar* source = string->characters8();
        convertLatin1ToUTF8(&source, source + string->length(), &destination, destination + bufferSize - 1);
    } else {
        const UChar* source = string->characters16();
        auto result = convertUTF16ToUTF8(&source, source + string->length(), &destination, destination + bufferSize - 1);
        failed = result != ConversionResult::Success && result != ConversionResult::TargetExhausted;
    }

    *destination++ = '\0';
    return failed ? 0 : destination - buffer;
}

char JSStringEncoding(JSStringRef string)
{
    if (!string || string->isEmpty()) {
        return 0;
    }

    return string->is8Bit() ? 8 : 16;
}

bool JSStringIsEqual(JSStringRef a, JSStringRef b)
{
    return OpaqueJSString::equal(a, b);
}

bool JSStringIsEqualToUTF8CString(JSStringRef a, const char* b)
{
    return JSStringIsEqual(a, adoptRef(JSStringCreateWithUTF8CString(b)).get());
}

bool JSStringIsEqualToString(JSStringRef a, const char* b, size_t length)
{
    if (a->is8Bit()) {
        return length == a->length() && a->rawString() == StringView(b, length);
    } else {
        return length == a->length() && a->rawString() == StringView(b, length);
    }
}

bool JSStringIsStatic(JSStringRef string)
{
    return string->isStatic();
}

bool JSStringIsExternal(JSStringRef string)
{
    return string->isExternal();
}

// char JSStringMetadata(JSStringRef string)
// {
//     if (string->isEmpty()) {
//         return 0;
//     }

//     char metadata = (string->isStatic());
//     metadata |= (string->isExternal()) << 1;
//     metadata |= (string->is()) << 2;
// }
