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
#include "AuthenticationClient.h"
#include "HTTPHeaderMap.h"
#include "ThreadableLoader.h"
#include <wtf/OwnPtr.h>

#if USE(SOUP)
typedef struct _SoupSession SoupSession;
#endif

#if PLATFORM(CF)
typedef const struct __CFData * CFDataRef;
#endif

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
#include <wtf/RetainPtr.h>
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
typedef int CFHTTPCookieStorageAcceptPolicy;
typedef struct OpaqueCFHTTPCookieStorage* CFHTTPCookieStorageRef;
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
class SchedulePair;
class SharedBuffer;

template <typename T> class Timer;

class ResourceHandle : public RefCounted<ResourceHandle>, public AuthenticationClient {
private:
    ResourceHandle(const ResourceRequest&, ResourceHandleClient*, bool defersLoading, bool shouldContentSniff, bool mightDownloadFromHandle);

    enum FailureType {
        BlockedFailure,
        InvalidURLFailure
    };

public:
    // FIXME: should not need the Frame
    static PassRefPtr<ResourceHandle> create(const ResourceRequest&, ResourceHandleClient*, Frame*, bool defersLoading, bool shouldContentSniff, bool mightDownloadFromHandle = false);

    static void loadResourceSynchronously(const ResourceRequest&, StoredCredentials, ResourceError&, ResourceResponse&, Vector<char>& data, Frame* frame);
    static bool willLoadFromCache(ResourceRequest&, Frame*);
#if PLATFORM(MAC)
    static bool didSendBodyDataDelegateExists();
#endif

    ~ResourceHandle();

#if PLATFORM(MAC) || USE(CFNETWORK)
    void willSendRequest(ResourceRequest&, const ResourceResponse& redirectResponse);
    bool shouldUseCredentialStorage();
#endif
#if PLATFORM(MAC) || USE(CFNETWORK) || USE(CURL)
    void didReceiveAuthenticationChallenge(const AuthenticationChallenge&);
    virtual void receivedCredential(const AuthenticationChallenge&, const Credential&);
    virtual void receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&);
    virtual void receivedCancellation(const AuthenticationChallenge&);
#endif

#if PLATFORM(MAC)
    void didCancelAuthenticationChallenge(const AuthenticationChallenge&);
    NSURLConnection *connection() const;
    WebCoreResourceHandleAsDelegate *delegate();
    void releaseDelegate();
    id releaseProxy();

    void schedule(SchedulePair*);
    void unschedule(SchedulePair*);
#elif USE(CFNETWORK)
    static CFRunLoopRef loaderRunLoop();
    CFURLConnectionRef connection() const;
    CFURLConnectionRef releaseConnectionForDownload();
    static void setHostAllowsAnyHTTPSCertificate(const String&);
    static void setClientCertificate(const String& host, CFDataRef);
#endif

#if PLATFORM(WIN) && USE(CURL)
    static void setHostAllowsAnyHTTPSCertificate(const String&);
#endif
#if PLATFORM(WIN) && USE(CURL) && PLATFORM(CF)
    static void setClientCertificate(const String& host, CFDataRef);
#endif

    PassRefPtr<SharedBuffer> bufferedData();
    static bool supportsBufferedData();

    bool shouldContentSniff() const;
    static bool shouldContentSniffURL(const KURL&);

    static void forceContentSniffing();

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

#if PLATFORM(QT) || USE(CURL) || USE(SOUP)
    ResourceHandleInternal* getInternal() { return d.get(); }
#endif

#if USE(SOUP)
    static SoupSession* defaultSession();
#endif

    // Used to work around the fact that you don't get any more NSURLConnection callbacks until you return from the one you're in.
    static bool loadsBlocked();    
    
    void clearAuthentication();
    void cancel();

    // The client may be 0, in which case no callbacks will be made.
    ResourceHandleClient* client() const;
    void setClient(ResourceHandleClient*);

    void setDefersLoading(bool);
      
    const ResourceRequest& request() const;

    void fireFailure(Timer<ResourceHandle>*);

    using RefCounted<ResourceHandle>::ref;
    using RefCounted<ResourceHandle>::deref;

private:
    void scheduleFailure(FailureType);

    bool start(Frame*);

    virtual void refAuthenticationClient() { ref(); }
    virtual void derefAuthenticationClient() { deref(); }

    friend class ResourceHandleInternal;
    OwnPtr<ResourceHandleInternal> d;
};

}

#endif // ResourceHandle_h
