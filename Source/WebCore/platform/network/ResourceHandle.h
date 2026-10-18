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

#include <WebCore/AuthenticationClient.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <wtf/Box.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Platform.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
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
#endif

#if PLATFORM(COCOA)
typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
#endif

namespace WTF {
class SchedulePair;
template<typename T> class MessageQueue;
}

namespace WebCore {

class AuthenticationChallenge;
class Credential;
class LocalFrame;
class NetworkingContext;
class ProtectionSpace;
class ResourceError;
class ResourceHandleClient;
class ResourceHandleInternal;
class NetworkLoadMetrics;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;
class FragmentedSharedBuffer;
class SynchronousLoaderMessageQueue;
class Timer;

class ResourceHandle : public RefCounted<ResourceHandle>, public AuthenticationClient {
public:
    WEBCORE_EXPORT static RefPtr<ResourceHandle> create(NetworkingContext*, const ResourceRequest&, ResourceHandleClient*, bool defersLoading, bool shouldContentSniff, ContentEncodingSniffingPolicy, RefPtr<SecurityOrigin>&& sourceOrigin, bool isMainFrameNavigation);
    WEBCORE_EXPORT static void loadResourceSynchronously(NetworkingContext*, const ResourceRequest&, StoredCredentialsPolicy, SecurityOrigin*, ResourceError&, ResourceResponse&, Vector<uint8_t>& data);
    WEBCORE_EXPORT virtual ~ResourceHandle();

#if PLATFORM(COCOA)
    void willSendRequest(ResourceRequest&&, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&&);
#endif

    void didReceiveResponse(ResourceResponse&&, CompletionHandler<void()>&&);

    bool shouldUseCredentialStorage();
    void didReceiveAuthenticationChallenge(const AuthenticationChallenge&);
    void receivedCredential(const AuthenticationChallenge&, const Credential&) override;
    void receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&) override;
    void receivedCancellation(const AuthenticationChallenge&) override;
    void receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&) override;
    void receivedChallengeRejection(const AuthenticationChallenge&) override;

    virtual bool isBlobResourceHandle() const { return false; }

#if PLATFORM(COCOA)
    bool tryHandlePasswordBasedAuthentication(const AuthenticationChallenge&);
#endif

#if PLATFORM(COCOA) && USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void canAuthenticateAgainstProtectionSpace(const ProtectionSpace&, CompletionHandler<void(bool)>&&);
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT NSURLConnection *LEGACY_NODELETE connection() const;
    id makeDelegate(bool, RefPtr<SynchronousLoaderMessageQueue>&&);
    id delegate();
    void releaseDelegate();
#endif

#if PLATFORM(COCOA)
    void schedule(WTF::SchedulePair&);
    void unschedule(WTF::SchedulePair&);
#endif

    bool LEGACY_NODELETE shouldContentSniff() const;
    static bool shouldContentSniffURL(const URL&);

    ContentEncodingSniffingPolicy LEGACY_NODELETE contentEncodingSniffingPolicy() const;

    WEBCORE_EXPORT static void LEGACY_NODELETE forceContentSniffing();

    bool LEGACY_NODELETE hasAuthenticationChallenge() const;
    void clearAuthentication();
    WEBCORE_EXPORT virtual void cancel();

    NetworkLoadMetrics* LEGACY_NODELETE networkLoadMetrics();
    void setNetworkLoadMetrics(Box<NetworkLoadMetrics>&&);

    MonotonicTime LEGACY_NODELETE startTimeBeforeRedirects() const;
    bool LEGACY_NODELETE failsTAOCheck() const;
    void checkTAO(const ResourceResponse&);
    bool LEGACY_NODELETE hasCrossOriginRedirect() const;
    void LEGACY_NODELETE markAsHavingCrossOriginRedirect();
    uint16_t LEGACY_NODELETE redirectCount() const;
    void LEGACY_NODELETE incrementRedirectCount();

    // The client may be 0, in which case no callbacks will be made.
    WEBCORE_EXPORT ResourceHandleClient* LEGACY_NODELETE client() const;
    WEBCORE_EXPORT void LEGACY_NODELETE clearClient();

    WEBCORE_EXPORT void setDefersLoading(bool);

    WEBCORE_EXPORT ResourceRequest& LEGACY_NODELETE firstRequest();
    WEBCORE_EXPORT const ResourceRequest& LEGACY_NODELETE firstRequest() const;
    const String& LEGACY_NODELETE lastHTTPMethod() const;

    void failureTimerFired();

    NetworkingContext* LEGACY_NODELETE context() const;

    // AuthenticationClient.
    void ref() const override { RefCounted::ref(); }
    void deref() const override { RefCounted::deref(); }

#if PLATFORM(COCOA)
    WEBCORE_EXPORT static CFStringRef LEGACY_NODELETE synchronousLoadRunLoopMode();
#endif

    typedef Ref<ResourceHandle> (*BuiltinConstructor)(const ResourceRequest& request, ResourceHandleClient* client);
    static void registerBuiltinConstructor(const AtomString& protocol, BuiltinConstructor);

    typedef void (*BuiltinSynchronousLoader)(NetworkingContext*, const ResourceRequest&, StoredCredentialsPolicy, ResourceError&, ResourceResponse&, Vector<uint8_t>& data);
    static void registerBuiltinSynchronousLoader(const AtomString& protocol, BuiltinSynchronousLoader);

protected:
    ResourceHandle(NetworkingContext*, const ResourceRequest&, ResourceHandleClient*, bool defersLoading, bool shouldContentSniff, ContentEncodingSniffingPolicy, RefPtr<SecurityOrigin>&& sourceOrigin, bool isMainFrameNavigation);

private:
    enum FailureType {
        NoFailure,
        BlockedFailure,
        InvalidURLFailure
    };

    void platformSetDefersLoading(bool);

    void scheduleFailure(FailureType);

    bool start();
    static void platformLoadResourceSynchronously(NetworkingContext*, const ResourceRequest&, StoredCredentialsPolicy, SecurityOrigin*, ResourceError&, ResourceResponse&, Vector<uint8_t>& data);

#if PLATFORM(COCOA)
    enum class SchedulingBehavior { Asynchronous, Synchronous };
#endif

#if PLATFORM(MAC)
    void createNSURLConnection(id delegate, bool shouldUseCredentialStorage, bool shouldContentSniff, ContentEncodingSniffingPolicy, SchedulingBehavior);
#endif

#if PLATFORM(IOS_FAMILY)
    void createNSURLConnection(id delegate, bool shouldUseCredentialStorage, bool shouldContentSniff, ContentEncodingSniffingPolicy, SchedulingBehavior, NSDictionary *connectionProperties);
#endif

#if PLATFORM(COCOA)
    NSURLRequest *applySniffingPoliciesIfNeeded(NSURLRequest *, bool shouldContentSniff, ContentEncodingSniffingPolicy);
#endif

    friend class ResourceHandleInternal;
    const std::unique_ptr<ResourceHandleInternal> d;
};

}
