/*
 * Copyright (C) 2009, 2015 Apple Inc. All rights reserved.
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

#ifndef WebKitCOMAPI_h
#define WebKitCOMAPI_h

#include <unknwn.h>

#ifndef WEBKIT_API
#ifdef WEBKIT_EXPORTS
#define WEBKIT_API __declspec(dllexport)
#else
#define WEBKIT_API __declspec(dllimport)
#endif
#endif

extern "C" {

// Callers should use this API instead of CoCreateInstance to instantiate WebKit COM classes.
// This bypasses CoCreateInstance entirely, so registry keys and isolated COM manifests aren't needed.
HRESULT WEBKIT_API WebKitCreateInstance(REFCLSID, IUnknown* pUnkOuter, REFIID, _COM_Outptr_ void** ppvObject);

#if USE(CF)
typedef struct __CFBundle* CFBundleRef;
CFBundleRef WEBKIT_API webKitBundle();
#endif

}

#endif // !defined(WebKitCOMAPI_h)
