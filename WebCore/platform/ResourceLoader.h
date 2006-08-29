/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef ResourceLoader_h
#define ResourceLoader_h

#include "ResourceLoaderClient.h" // for PlatformResponse
#include "StringHash.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/Platform.h>

#if PLATFORM(WIN)
typedef unsigned long DWORD;
typedef unsigned long DWORD_PTR;
typedef void* LPVOID;
typedef LPVOID HINTERNET;
typedef unsigned WPARAM;
typedef long LPARAM;
typedef struct HWND__* HWND;
typedef _W64 long LONG_PTR;
typedef LONG_PTR LRESULT;
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class WebCoreResourceLoaderImp;
#else
class WebCoreResourceLoaderImp;
#endif
#endif

namespace WebCore {

class DocLoader;
class FormData;
class KURL;
class ResourceLoaderInternal;

class ResourceLoader {
public:
    ResourceLoader(ResourceLoaderClient*, const String& method, const KURL&);
    ResourceLoader(ResourceLoaderClient*, const String& method, const KURL&, const FormData& postData);
    ~ResourceLoader();

    bool start(DocLoader*);

    int error() const;
    void setError(int);
    String errorText() const;
    bool isErrorPage() const;
    String queryMetaData(const String&) const;
    void addMetaData(const String& key, const String& value);
    void addMetaData(const HashMap<String, String>&);
    void kill();

    KURL url() const;
    String method() const;
    FormData postData() const;

#if PLATFORM(MAC)
    void setLoader(WebCoreResourceLoaderImp*);
#endif

#if USE(WININET)
    void fileLoadTimer(Timer<ResourceLoader>* timer);
    friend void __stdcall transferJobStatusCallback(HINTERNET, DWORD_PTR, DWORD, LPVOID, DWORD);
    friend LRESULT __stdcall ResourceLoaderWndProc(HWND, unsigned message, WPARAM, LPARAM);
#endif

#if PLATFORM(GDK) || PLATFORM(QT)
    ResourceLoaderInternal* getInternal() { return d; }
#endif

    void cancel();
    
    ResourceLoaderClient* client() const;

    void receivedResponse(PlatformResponse);

private:
    void assembleResponseHeaders() const;
    void retrieveCharset() const;

    ResourceLoaderInternal* d;
};

}

#endif // ResourceLoader_h
