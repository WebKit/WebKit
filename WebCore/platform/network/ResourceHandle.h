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

#ifndef ResourceHandle_h
#define ResourceHandle_h

#include "HTTPHeaderMap.h"
#include "ResourceHandleClient.h" // for PlatformResponse

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
@class NSURLRequest;
@class NSURLResponse;
@class NSError;
#else
class NSURLRequest;
class NSURLResponse;
class NSError;
#endif
#endif

namespace WebCore {

class DocLoader;
class FormData;
class KURL;
class ResourceHandleInternal;
class SubresourceLoader;
class SubresourceLoaderClient;

struct ResourceRequest;

template <typename T> class Timer;

class ResourceHandle : public Shared<ResourceHandle> {
private:
    ResourceHandle(const ResourceRequest&, ResourceHandleClient*, SubresourceLoaderClient*);

public:
    // FIXME: should not need the DocLoader
    static PassRefPtr<ResourceHandle> create(const ResourceRequest&, ResourceHandleClient*, DocLoader*, SubresourceLoaderClient* = 0);

    ~ResourceHandle();

#if PLATFORM(MAC)
    NSURLRequest *willSendRequest(NSURLRequest *, NSURLResponse *);
    void didReceiveResponse(NSURLResponse *);  
    void addData(NSData *);
    void finishJobAndHandle(NSData *);
    void reportError(NSError* error);
    SubresourceLoader* loader() const;
#endif

#if USE(WININET)
    void setHasReceivedResponse(bool = true);
    bool hasReceivedResponse() const;
    void fileLoadTimer(Timer<ResourceHandle>*);
    void onHandleCreated(LPARAM);
    void onRequestRedirected(LPARAM);
    void onRequestComplete(LPARAM);
    friend void __stdcall transferJobStatusCallback(HINTERNET, DWORD_PTR, DWORD, LPVOID, DWORD);
    friend LRESULT __stdcall ResourceHandleWndProc(HWND, unsigned message, WPARAM, LPARAM);
#endif

#if PLATFORM(GDK) || PLATFORM(QT)
    ResourceHandleInternal* getInternal() { return d; }
#endif

#if PLATFORM(QT)
    QString extractCharsetFromHeaders(QString headers) const;
#endif

    void cancel();
    
    ResourceHandleClient* client() const;
      
    const HTTPHeaderMap& requestHeaders() const;
    const KURL& url() const;
    const String& method() const;
    PassRefPtr<FormData> postData() const;

private:
    bool start(DocLoader*);

    ResourceHandleInternal* d;
};

}

#endif // ResourceHandle_h
