/*
 * Copyright (C) 2006-2017 Apple Inc.  All rights reserved.
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

#ifndef WebKitDLL_H
#define WebKitDLL_H

#ifndef _HAS_EXCEPTIONS
#define _HAS_EXCEPTIONS 0
#endif

#include <winsock2.h>
#include <windows.h>
#include <wtf/HashCountedSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#ifndef WEBKIT_API
#ifdef WEBKIT_EXPORTS
#define WEBKIT_API __declspec(dllexport)
#else
#define WEBKIT_API __declspec(dllimport)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern ULONG gLockCount;
extern ULONG gClassCount;
extern HashCountedSet<WTF::String>& gClassNameCount();
extern HINSTANCE gInstance;
extern CLSID gRegCLSIDs[];

WEBKIT_API void shutDownWebKit();

#if defined(DEPRECATED_EXPORT_SYMBOLS)

#include <JavaScriptCore/JSObjectRef.h>

// Force symbols to be included so we can export them for legacy clients.
// DEPRECATED! People should get these symbols from JavaScriptCore.dll, not WebKit.dll!
typedef struct OpaqueJSClass* JSClassRef;
typedef const struct OpaqueJSContext* JSContextRef;
typedef const struct OpaqueJSValue* JSValueRef;
typedef struct OpaqueJSString* JSStringRef;
typedef wchar_t JSChar;
typedef unsigned JSPropertyAttributes;

WEBKIT_API JSClassRef JSClassCreate(const JSClassDefinition*);
WEBKIT_API void* JSObjectGetPrivate(JSObjectRef);
WEBKIT_API JSObjectRef JSObjectMake(JSContextRef, JSClassRef, void*);
WEBKIT_API void JSObjectSetProperty(JSContextRef, JSObjectRef, JSStringRef propertyName, JSValueRef, JSPropertyAttributes, JSValueRef* exception);
WEBKIT_API JSStringRef JSStringCreateWithCFString(CFStringRef);
WEBKIT_API JSStringRef JSStringCreateWithUTF8CString(const char*);
WEBKIT_API const JSChar* JSStringGetCharactersPtr(JSStringRef);
WEBKIT_API size_t JSStringGetLength(JSStringRef);
WEBKIT_API void JSStringRelease(JSStringRef);
WEBKIT_API bool JSValueIsNumber(JSContextRef, JSValueRef);
WEBKIT_API bool JSValueIsString(JSContextRef, JSValueRef);
WEBKIT_API JSValueRef JSValueMakeString(JSContextRef, JSStringRef);
WEBKIT_API JSValueRef JSValueMakeUndefined(JSContextRef ctx);
WEBKIT_API double JSValueToNumber(JSContextRef, JSValueRef, JSValueRef*);
WEBKIT_API JSStringRef JSValueToStringCopy(JSContextRef, JSValueRef, JSValueRef* exception);
// End
#endif

#ifdef __cplusplus
}
#endif

#endif // WebKitDLL_H
