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

#include "AuthenticationChallenge.h"
#include "HTTPHeaderMap.h"
#include <wtf/OwnPtr.h>


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
#include "RetainPtr.h"
#ifdef __OBJC__
@class NSData;
@class NSError;
@class NSURLConnection;
@class WebCoreResourceHandleAsDelegate;
#else
class NSData;
class NSError;
class NSURLConnection;
class WebCoreResourceHandleAsDelegate;
typedef struct objc_object *id;
#endif
#endif

#if USE(CFNETWORK)
typedef struct _CFURLConnection* CFURLConnectionRef;
#endif

namespace WebCore {

class AuthenticationChallenge;
class Credential;
class FormData;
class Frame;
class KURL;
class ResourceError;
class ResourceHandleClient;
class ResourceHandleInternal;
class ResourceRequest;
class ResourceResponse;
class SharedBuffer;
class SubresourceLoader;
class SubresourceLoaderClient;

template <typename T> class Timer;

class ResourceHandle : public Shared<ResourceHandle> {
private:
    ResourceHandle(const ResourceRequest&, ResourceHandleClient*, bool defersLoading, bool mightDownloadFromHandle);

public:
    // FIXME: should not need the Frame
    static PassRefPtr<ResourceHandle> create(const ResourceRequest&, ResourceHandleClient*, Frame*, bool defersLoading, bool mightDownloadFromHandle = false);

    static void loadResourceSynchronously(const ResourceRequest&, ResourceError&, ResourceResponse&, Vector<char>& data);
    static bool willLoadFromCache(ResourceRequest&);
    
    ~ResourceHandle();

#if PLATFORM(MAC) || USE(CFNETWORK)
    void didReceiveAuthenticationChallenge(const AuthenticationChallenge&);
    void receivedCredential(const AuthenticationChallenge&, const Credential&);
    void receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&);
    void receivedCancellation(const AuthenticationChallenge&);
#endif
#if PLATFORM(MAC)
    void didCancelAuthenticationChallenge(const AuthenticationChallenge&);
    NSURLConnection *connection() const;
    WebCoreResourceHandleAsDelegate *delegate();
    void releaseDelegate();
#elif USE(CFNETWORK)
    static CFRunLoopRef loaderRunLoop();
    CFURLConnectionRef connection() const;
    CFURLConnectionRef releaseConnectionForDownload();
#endif
    PassRefPtr<SharedBuffer> bufferedData();
    static bool supportsBufferedData();
    
#if PLATFORM(MAC)
    id releaseProxy();
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
    ResourceHandleInternal* getInternal() { return d.get(); }
#endif

    // Used to work around the fact that you don't get any more NSURLConnection callbacks until you return from the one you're in.
    static bool loadsBlocked();    
    
    void clearAuthentication();
    void cancel();
    
    ResourceHandleClient* client() const;
    void setDefersLoading(bool);
      
    const HTTPHeaderMap& requestHeaders() const;
    const KURL& url() const;
    const String& method() const;
    PassRefPtr<FormData> postData() const;
    const ResourceRequest& request() const;

    void fireBlockedFailure(Timer<ResourceHandle>*);

private:
    static bool portAllowed(const ResourceRequest&);
    
    void scheduleBlockedFailure();

    bool start(Frame*);
        
    OwnPtr<ResourceHandleInternal> d;
};

}

#endif // ResourceHandle_h
