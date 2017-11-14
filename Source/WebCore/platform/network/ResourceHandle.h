/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "AuthenticationClient.h"
#include "StoredCredentialsPolicy.h"
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicString.h>

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
#include <wtf/RetainPtr.h>
#endif

#if USE(SOUP)
typedef struct _GTlsCertificate GTlsCertificate;
typedef struct _SoupSession SoupSession;
typedef struct _SoupRequest SoupRequest;
#endif

#if USE(CF)
typedef const struct __CFData * CFDataRef;
#endif

#if PLATFORM(COCOA)
OBJC_CLASS NSCachedURLResponse;
OBJC_CLASS NSData;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSError;
OBJC_CLASS NSURLConnection;
OBJC_CLASS NSURLRequest;
#ifndef __OBJC__
typedef struct objc_object *id;
#endif
#endif

#if USE(CFURLCONNECTION)
typedef const struct _CFCachedURLResponse* CFCachedURLResponseRef;
typedef struct _CFURLConnection* CFURLConnectionRef;
typedef int CFHTTPCookieStorageAcceptPolicy;
typedef struct OpaqueCFHTTPCookieStorage* CFHTTPCookieStorageRef;
#endif

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
#endif

namespace WTF {
class SchedulePair;
template<typename T> class MessageQueue;
}

namespace WebCore {

class AuthenticationChallenge;
class Credential;
class Frame;
class URL;
class NetworkingContext;
class ProtectionSpace;
class ResourceError;
class ResourceHandleClient;
class ResourceHandleInternal;
class NetworkLoadMetrics;
class ResourceRequest;
class ResourceResponse;
class SoupNetworkSession;
class SharedBuffer;
class Timer;

class ResourceHandle : public RefCounted<ResourceHandle>, public AuthenticationClient {
public:
    WEBCORE_EXPORT static RefPtr<ResourceHandle> create(NetworkingContext*, const ResourceRequest&, ResourceHandleClient*, bool defersLoading, bool shouldContentSniff, bool shouldContentEncodingSniff);
    WEBCORE_EXPORT static void loadResourceSynchronously(NetworkingContext*, const ResourceRequest&, StoredCredentialsPolicy, ResourceError&, ResourceResponse&, Vector<char>& data);

#if USE(SOUP)
    static RefPtr<ResourceHandle> create(SoupNetworkSession&, const ResourceRequest&, ResourceHandleClient*, bool defersLoading, bool shouldContentSniff, bool shouldContentEncodingSniff);
#endif

    WEBCORE_EXPORT virtual ~ResourceHandle();

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    void willSendRequest(ResourceRequest&&, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&&);
#endif

    void didReceiveResponse(ResourceResponse&&);

    bool shouldUseCredentialStorage();
    void didReceiveAuthenticationChallenge(const AuthenticationChallenge&);
    void receivedCredential(const AuthenticationChallenge&, const Credential&) override;
    void receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&) override;
    void receivedCancellation(const AuthenticationChallenge&) override;
    void receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&) override;
    void receivedChallengeRejection(const AuthenticationChallenge&) override;

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    bool tryHandlePasswordBasedAuthentication(const AuthenticationChallenge&);
#endif

#if PLATFORM(COCOA) && USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool canAuthenticateAgainstProtectionSpace(const ProtectionSpace&);
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT NSURLConnection *connection() const;
    id makeDelegate(bool, WTF::MessageQueue<WTF::Function<void()>>*);
    id delegate();
    void releaseDelegate();
#endif

#if PLATFORM(COCOA)
#if USE(CFURLCONNECTION)
    static void getConnectionTimingData(CFURLConnectionRef, NetworkLoadMetrics&);
#else
    static void getConnectionTimingData(NSURLConnection *, NetworkLoadMetrics&);
#endif
#endif

#if PLATFORM(COCOA)
    void schedule(WTF::SchedulePair&);
    void unschedule(WTF::SchedulePair&);
#endif

#if USE(CFURLCONNECTION)
    CFURLStorageSessionRef storageSession() const;
    CFURLConnectionRef connection() const;
    WEBCORE_EXPORT RetainPtr<CFURLConnectionRef> releaseConnectionForDownload();
    const ResourceRequest& currentRequest() const;
    static void setHostAllowsAnyHTTPSCertificate(const String&);
    static void setClientCertificate(const String& host, CFDataRef);
#endif

#if OS(WINDOWS) && USE(CURL)
    static void setHostAllowsAnyHTTPSCertificate(const String&);
    static void setClientCertificateInfo(const String&, const String&, const String&);
#endif

#if OS(WINDOWS) && USE(CURL) && USE(CF)
    static void setClientCertificate(const String& host, CFDataRef);
#endif

    bool shouldContentSniff() const;
    static bool shouldContentSniffURL(const URL&);

    bool shouldContentEncodingSniff() const;

    WEBCORE_EXPORT static void forceContentSniffing();

#if USE(CURL) || USE(SOUP)
    ResourceHandleInternal* getInternal() { return d.get(); }
#endif

#if USE(SOUP)
    RefPtr<ResourceHandle> releaseForDownload(ResourceHandleClient*);
    void continueDidReceiveAuthenticationChallenge(const Credential& credentialFromPersistentStorage);
    void sendPendingRequest();
    bool cancelledOrClientless();
    void ensureReadBuffer();
    size_t currentStreamPosition() const;
    void didStartRequest();
    MonotonicTime m_requestTime;
#endif

    bool hasAuthenticationChallenge() const;
    void clearAuthentication();
    WEBCORE_EXPORT virtual void cancel();

    // The client may be 0, in which case no callbacks will be made.
    WEBCORE_EXPORT ResourceHandleClient* client() const;
    WEBCORE_EXPORT void clearClient();

    // Called in response to ResourceHandleClient::didReceiveResponseAsync().
    WEBCORE_EXPORT virtual void continueDidReceiveResponse();

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    // Called in response to ResourceHandleClient::canAuthenticateAgainstProtectionSpaceAsync().
    WEBCORE_EXPORT void continueCanAuthenticateAgainstProtectionSpace(bool);
#endif

    // Called in response to ResourceHandleClient::willCacheResponseAsync().
#if USE(CFURLCONNECTION)
    WEBCORE_EXPORT void continueWillCacheResponse(CFCachedURLResponseRef);
#endif
#if PLATFORM(COCOA)
    WEBCORE_EXPORT void continueWillCacheResponse(NSCachedURLResponse *);
#endif

    WEBCORE_EXPORT void setDefersLoading(bool);

    WEBCORE_EXPORT ResourceRequest& firstRequest();
    const String& lastHTTPMethod() const;

    void failureTimerFired();

    NetworkingContext* context() const;

    using RefCounted<ResourceHandle>::ref;
    using RefCounted<ResourceHandle>::deref;

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    WEBCORE_EXPORT static CFStringRef synchronousLoadRunLoopMode();
#endif

    typedef Ref<ResourceHandle> (*BuiltinConstructor)(const ResourceRequest& request, ResourceHandleClient* client);
    static void registerBuiltinConstructor(const AtomicString& protocol, BuiltinConstructor);

    typedef void (*BuiltinSynchronousLoader)(NetworkingContext*, const ResourceRequest&, StoredCredentialsPolicy, ResourceError&, ResourceResponse&, Vector<char>& data);
    static void registerBuiltinSynchronousLoader(const AtomicString& protocol, BuiltinSynchronousLoader);

protected:
    ResourceHandle(NetworkingContext*, const ResourceRequest&, ResourceHandleClient*, bool defersLoading, bool shouldContentSniff, bool shouldContentEncodingSniff);

private:
    enum FailureType {
        NoFailure,
        BlockedFailure,
        InvalidURLFailure
    };

#if USE(SOUP)
    ResourceHandle(SoupNetworkSession&, const ResourceRequest&, ResourceHandleClient*, bool defersLoading, bool shouldContentSniff, bool shouldContentEncodingSniff);
#endif

    void platformSetDefersLoading(bool);

    void platformContinueSynchronousDidReceiveResponse();

    void scheduleFailure(FailureType);

    bool start();
    static void platformLoadResourceSynchronously(NetworkingContext*, const ResourceRequest&, StoredCredentialsPolicy, ResourceError&, ResourceResponse&, Vector<char>& data);

    void refAuthenticationClient() override { ref(); }
    void derefAuthenticationClient() override { deref(); }

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    enum class SchedulingBehavior { Asynchronous, Synchronous };
#endif

#if USE(CFURLCONNECTION)
    void createCFURLConnection(bool shouldUseCredentialStorage, bool shouldContentSniff, bool shouldContentEncodingSniff, WTF::MessageQueue<WTF::Function<void()>>*, CFDictionaryRef clientProperties);
#endif

#if PLATFORM(MAC)
    void createNSURLConnection(id delegate, bool shouldUseCredentialStorage, bool shouldContentSniff, bool shouldContentEncodingSniff, SchedulingBehavior);
#endif

#if PLATFORM(IOS)
    void createNSURLConnection(id delegate, bool shouldUseCredentialStorage, bool shouldContentSniff, bool shouldContentEncodingSniff, SchedulingBehavior, NSDictionary *connectionProperties);
#endif

#if PLATFORM(COCOA)
    void applySniffingPoliciesAndStoragePartitionIfNeeded(NSURLRequest*&, bool shouldContentSniff, bool shouldContentEncodingSniff);
#endif

#if USE(SOUP)
    void timeoutFired();
#endif

    friend class ResourceHandleInternal;
    std::unique_ptr<ResourceHandleInternal> d;
};

}
