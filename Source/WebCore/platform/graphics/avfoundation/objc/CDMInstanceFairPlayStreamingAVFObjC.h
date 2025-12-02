/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)

#include "CDMInstance.h"
#include "CDMInstanceSession.h"
#include "ContentKeyGroupDataSource.h"
#include <wtf/Function.h>
#include <wtf/Observer.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakHashSet.h>

OBJC_CLASS AVContentKey;
OBJC_CLASS AVContentKeyReportGroup;
OBJC_CLASS AVContentKeyRequest;
OBJC_CLASS AVContentKeySession;
OBJC_CLASS NSData;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSError;
OBJC_CLASS NSURL;
OBJC_CLASS WebCoreFPSContentKeySessionDelegate;

OBJC_PROTOCOL(WebAVContentKeyGrouping);

#if !RELEASE_LOG_DISABLED
namespace WTF {
class Logger;
}
#endif

namespace WebCore {
class AVContentKeySessionDelegateClient;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::AVContentKeySessionDelegateClient> : std::true_type { };
}

namespace WebCore {

class CDMInstanceSessionFairPlayStreamingAVFObjC;
class CDMPrivateFairPlayStreaming;
class MediaSampleAVFObjC;
struct CDMMediaCapability;

class AVContentKeySessionDelegateClient : public CanMakeWeakPtr<AVContentKeySessionDelegateClient> {
public:
    virtual ~AVContentKeySessionDelegateClient() = default;
    virtual void didProvideRequest(AVContentKeyRequest*) = 0;
    virtual void didProvideRequests(Vector<RetainPtr<AVContentKeyRequest>>&&) = 0;
    virtual void didProvideRenewingRequest(AVContentKeyRequest*) = 0;
    virtual void didProvidePersistableRequest(AVContentKeyRequest*) = 0;
    virtual void didFailToProvideRequest(AVContentKeyRequest*, NSError*) = 0;
    virtual void requestDidSucceed(AVContentKeyRequest*) = 0;
    virtual bool shouldRetryRequestForReason(AVContentKeyRequest*, NSString*) = 0;
    virtual void sessionIdentifierChanged(NSData*) = 0;
    virtual void groupSessionIdentifierChanged(AVContentKeyReportGroup*, NSData*) = 0;
    virtual void outputObscuredDueToInsufficientExternalProtectionChanged(bool) = 0;
    virtual void externalProtectionStatusDidChangeForContentKey(AVContentKey *) = 0;
    virtual void externalProtectionStatusDidChangeForContentKeyRequest(AVContentKeyRequest*) = 0;

#if !RELEASE_LOG_DISABLED
    virtual const Logger& logger() const = 0;
    virtual uint64_t logIdentifier() const = 0;
#endif
};

class CDMInstanceFairPlayStreamingAVFObjC final : public CDMInstance, public AVContentKeySessionDelegateClient, public CanMakeWeakPtr<CDMInstanceFairPlayStreamingAVFObjC> {
public:
    USING_CAN_MAKE_WEAKPTR(CanMakeWeakPtr<CDMInstanceFairPlayStreamingAVFObjC>);

    CDMInstanceFairPlayStreamingAVFObjC(const CDMPrivateFairPlayStreaming&);
    virtual ~CDMInstanceFairPlayStreamingAVFObjC() = default;

    static bool supportsPersistableState();
    static bool supportsPersistentKeys();
    static bool supportsMediaCapability(const CDMMediaCapability&);
    static bool mimeTypeIsPlayable(const String&);

    ImplementationType implementationType() const final { return ImplementationType::FairPlayStreaming; }

    void initializeWithConfiguration(const CDMKeySystemConfiguration&, AllowDistinctiveIdentifiers, AllowPersistentState, SuccessCallback&&) final;
    void setServerCertificate(Ref<SharedBuffer>&&, SuccessCallback&&) final;
    void setStorageDirectory(const String&) final;
    RefPtr<CDMInstanceSession> createSession() final;
    void setClient(WeakPtr<CDMInstanceClient>&&) final;
    void clearClient() final;

    const String& keySystem() const final;

    NSURL *storageURL() const { return m_storageURL.get(); }
    bool persistentStateAllowed() const { return m_persistentStateAllowed; }
    SharedBuffer* serverCertificate() const { return m_serverCertificate.get(); }
    AVContentKeySession *contentKeySession();

    RetainPtr<AVContentKeyRequest> takeUnexpectedKeyRequestForInitializationData(const String& initDataType, SharedBuffer& initData);

    // AVContentKeySessionDelegateClient
    void didProvideRequest(AVContentKeyRequest*) final;
    void didProvideRequests(Vector<RetainPtr<AVContentKeyRequest>>&&) final;
    void didProvideRenewingRequest(AVContentKeyRequest*) final;
    void didProvidePersistableRequest(AVContentKeyRequest*) final;
    void didFailToProvideRequest(AVContentKeyRequest*, NSError*) final;
    void requestDidSucceed(AVContentKeyRequest*) final;
    bool shouldRetryRequestForReason(AVContentKeyRequest*, NSString*) final;
    void sessionIdentifierChanged(NSData*) final;
    void groupSessionIdentifierChanged(AVContentKeyReportGroup*, NSData*) final;
    void outputObscuredDueToInsufficientExternalProtectionChanged(bool) final;
    void externalProtectionStatusDidChangeForContentKey(AVContentKey *) final;
    void externalProtectionStatusDidChangeForContentKeyRequest(AVContentKeyRequest*) final;

    using Keys = Vector<Ref<SharedBuffer>>;
    CDMInstanceSessionFairPlayStreamingAVFObjC* sessionForKeyIDs(const Keys&) const;
    CDMInstanceSessionFairPlayStreamingAVFObjC* sessionForGroup(WebAVContentKeyGrouping *) const;
    CDMInstanceSessionFairPlayStreamingAVFObjC* sessionForKey(AVContentKey *) const;
    CDMInstanceSessionFairPlayStreamingAVFObjC* sessionForRequest(AVContentKeyRequest *) const;

    bool isAnyKeyUsable(const Keys&) const;

    using KeyStatusesChangedObserver = Observer<void()>;
    void addKeyStatusesChangedObserver(const KeyStatusesChangedObserver&);
    void removeKeyStatusesChangedObserver(const KeyStatusesChangedObserver&);

    void sessionKeyStatusesChanged(const CDMInstanceSessionFairPlayStreamingAVFObjC&);

    void attachContentKeyToSample(const MediaSampleAVFObjC&);

#if !RELEASE_LOG_DISABLED
    void setLogIdentifier(uint64_t logIdentifier) final { m_logIdentifier = logIdentifier; }
    const Logger& logger() const { return m_logger; };
    uint64_t logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "CDMInstanceFairPlayStreamingAVFObjC"_s; }
#endif

    const String& mediaKeysHashSalt() const { return m_mediaKeysHashSalt; }

private:
    void handleUnexpectedRequests(Vector<RetainPtr<AVContentKeyRequest>>&&);

    WeakPtr<CDMInstanceClient> m_client;
    RetainPtr<AVContentKeySession> m_session;
    RetainPtr<WebCoreFPSContentKeySessionDelegate> m_delegate;
    RefPtr<SharedBuffer> m_serverCertificate;
    bool m_persistentStateAllowed { true };
    RetainPtr<NSURL> m_storageURL;
    Vector<WeakPtr<CDMInstanceSessionFairPlayStreamingAVFObjC>> m_sessions;
    HashSet<RetainPtr<AVContentKeyRequest>> m_unexpectedKeyRequests;
    WeakHashSet<KeyStatusesChangedObserver> m_keyStatusChangedObservers;
    String m_mediaKeysHashSalt;

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    uint64_t m_logIdentifier { 0 };
#endif
};

class CDMInstanceSessionFairPlayStreamingAVFObjC final
    : public CDMInstanceSession
    , public AVContentKeySessionDelegateClient
    , private ContentKeyGroupDataSource {
public:
    USING_CAN_MAKE_WEAKPTR(AVContentKeySessionDelegateClient);

    static Ref<CDMInstanceSessionFairPlayStreamingAVFObjC> create(Ref<CDMInstanceFairPlayStreamingAVFObjC>&&);
    virtual ~CDMInstanceSessionFairPlayStreamingAVFObjC();

    // ContentKeyGroupDataSource.
    void ref() const final { CDMInstanceSession::ref(); }
    void deref() const final { CDMInstanceSession::deref(); }

    // CDMInstanceSession
    void requestLicense(LicenseType, KeyGroupingStrategy, const String& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&&) final;
    void updateLicense(const String&, LicenseType, Ref<SharedBuffer>&&, LicenseUpdateCallback&&) final;
    void loadSession(LicenseType, const String&, const String&, LoadSessionCallback&&) final;
    void closeSession(const String&, CloseSessionCallback&&) final;
    void removeSessionData(const String&, LicenseType, RemoveSessionDataCallback&&) final;
    void storeRecordOfKeyUsage(const String&) final;
    void displayChanged(PlatformDisplayID) final;
    void setClient(WeakPtr<CDMInstanceSessionClient>&&) final;
    void clearClient() final;

    // AVContentKeySessionDelegateClient
    void didProvideRequest(AVContentKeyRequest*) final;
    void didProvideRequests(Vector<RetainPtr<AVContentKeyRequest>>&&) final;
    void didProvideRenewingRequest(AVContentKeyRequest*) final;
    void didProvidePersistableRequest(AVContentKeyRequest*) final;
    void didFailToProvideRequest(AVContentKeyRequest*, NSError*) final;
    void requestDidSucceed(AVContentKeyRequest*) final;
    bool shouldRetryRequestForReason(AVContentKeyRequest*, NSString*) final;
    void sessionIdentifierChanged(NSData*) final;
    void groupSessionIdentifierChanged(AVContentKeyReportGroup*, NSData*) final;
    void outputObscuredDueToInsufficientExternalProtectionChanged(bool) final;
    void externalProtectionStatusDidChangeForContentKey(AVContentKey *) final;
    void externalProtectionStatusDidChangeForContentKeyRequest(AVContentKeyRequest*) final;

    using Keys = CDMInstanceFairPlayStreamingAVFObjC::Keys;
    Keys keyIDs();
    AVContentKeySession *contentKeySession() { return m_session ? m_session.get() : m_instance->contentKeySession(); }
    WebAVContentKeyGrouping *contentKeyReportGroup() { return m_group.get(); }

    struct Request {
        String initType;
        Vector<RetainPtr<AVContentKeyRequest>> requests;
        friend bool operator==(const Request&, const Request&) = default;
    };

    bool hasKey(AVContentKey *) const;
    bool hasRequest(AVContentKeyRequest*) const;
    bool isAnyKeyUsable(const Keys&) const;

    const KeyStatusVector& keyStatuses() const { return m_keyStatuses; }
    KeyStatusVector copyKeyStatuses() const;

    void attachContentKeyToSample(const MediaSampleAVFObjC&);

    static RetainPtr<NSDictionary> optionsForKeyRequestWithHashSalt(const String&);

private:
    explicit CDMInstanceSessionFairPlayStreamingAVFObjC(Ref<CDMInstanceFairPlayStreamingAVFObjC>&&);

    bool ensureSessionOrGroup(KeyGroupingStrategy);
    bool isLicenseTypeSupported(LicenseType) const;

    void updateKeyStatuses();
    void nextRequest();

    AVContentKeyRequest* lastKeyRequest() const;
    Vector<RetainPtr<AVContentKey>> contentKeys() const;
    Vector<RetainPtr<AVContentKeyRequest>> contentKeyRequests() const;

    std::optional<CDMKeyStatus> protectionStatusForRequest(AVContentKeyRequest *) const;
    void updateProtectionStatus();

    AVContentKey *contentKeyForSample(const MediaSampleAVFObjC&);

    bool requestMatchesRenewingRequest(AVContentKeyRequest *);

#if !RELEASE_LOG_DISABLED
    void setLogIdentifier(uint64_t logIdentifier) final { m_logIdentifier = logIdentifier; }
    const Logger& logger() const { return m_logger; };
    uint64_t logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "CDMInstanceSessionFairPlayStreamingAVFObjC"_s; }
#endif

    // ContentKeyGroupDataSource
    Vector<RetainPtr<AVContentKey>> contentKeyGroupDataSourceKeys() const final;
#if !RELEASE_LOG_DISABLED
    uint64_t contentKeyGroupDataSourceLogIdentifier() const final;
    const Logger& contentKeyGroupDataSourceLogger() const final;
    WTFLogChannel& contentKeyGroupDataSourceLogChannel() const final;
#endif // !RELEASE_LOG_DISABLED

    const Ref<CDMInstanceFairPlayStreamingAVFObjC> m_instance;
    const RetainPtr<WebAVContentKeyGrouping> m_group;
    RetainPtr<AVContentKeySession> m_session;
    std::optional<Request> m_currentRequest;
    const RetainPtr<WebCoreFPSContentKeySessionDelegate> m_delegate;
    Vector<RetainPtr<NSData>> m_expiredSessions;
    WeakPtr<CDMInstanceSessionClient> m_client;
    String m_sessionId;
    bool m_outputObscured { false };

    class UpdateResponseCollector;
    std::unique_ptr<UpdateResponseCollector> m_updateResponseCollector;
    KeyStatusVector m_keyStatuses;

    Vector<Request> m_pendingRequests;
    Vector<Request> m_requests;
    std::optional<Request> m_renewingRequest;

    LicenseCallback m_requestLicenseCallback;
    LicenseUpdateCallback m_updateLicenseCallback;
    CloseSessionCallback m_closeSessionCallback;
    RemoveSessionDataCallback m_removeSessionDataCallback;

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    uint64_t m_logIdentifier { 0 };
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CDM_INSTANCE(WebCore::CDMInstanceFairPlayStreamingAVFObjC, WebCore::CDMInstance::ImplementationType::FairPlayStreaming)

#endif // ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
