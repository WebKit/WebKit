/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS AVContentKeyReportGroup;
OBJC_CLASS AVContentKeyRequest;
OBJC_CLASS AVContentKeySession;
OBJC_CLASS NSData;
OBJC_CLASS NSError;
OBJC_CLASS NSURL;
OBJC_CLASS WebCoreFPSContentKeySessionDelegate;

namespace WebCore {

class CDMInstanceSessionFairPlayStreamingAVFObjC;
struct CDMMediaCapability;

class AVContentKeySessionDelegateClient {
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
};

class CDMInstanceFairPlayStreamingAVFObjC final : public CDMInstance, public AVContentKeySessionDelegateClient, public CanMakeWeakPtr<CDMInstanceFairPlayStreamingAVFObjC> {
public:
    CDMInstanceFairPlayStreamingAVFObjC();
    virtual ~CDMInstanceFairPlayStreamingAVFObjC() = default;

    static bool supportsPersistableState();
    static bool supportsPersistentKeys();
    static bool supportsMediaCapability(const CDMMediaCapability&);
    static bool mimeTypeIsPlayable(const String&);

    ImplementationType implementationType() const final { return ImplementationType::FairPlayStreaming; }

    SuccessValue initializeWithConfiguration(const CDMKeySystemConfiguration&) final;
    SuccessValue setDistinctiveIdentifiersAllowed(bool) final;
    SuccessValue setPersistentStateAllowed(bool) final;
    SuccessValue setServerCertificate(Ref<SharedBuffer>&&) final;
    SuccessValue setStorageDirectory(const String&) final;
    RefPtr<CDMInstanceSession> createSession() final;
    RefPtr<ProxyCDM> proxyCDM() const final { return nullptr; }

    const String& keySystem() const final;

    NSURL *storageURL() const { return m_storageURL.get(); }
    bool persistentStateAllowed() const { return m_persistentStateAllowed; }
    SharedBuffer* serverCertificate() const { return m_serverCertificate.get(); }
    AVContentKeySession* contentKeySession();

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

    using Keys = Vector<Ref<SharedBuffer>>;
    CDMInstanceSessionFairPlayStreamingAVFObjC* sessionForKeyIDs(const Keys&) const;
    CDMInstanceSessionFairPlayStreamingAVFObjC* sessionForGroup(AVContentKeyReportGroup*) const;

private:
    RetainPtr<AVContentKeySession> m_session;
    RetainPtr<WebCoreFPSContentKeySessionDelegate> m_delegate;
    RefPtr<SharedBuffer> m_serverCertificate;
    bool m_persistentStateAllowed { true };
    RetainPtr<NSURL> m_storageURL;
    Vector<WeakPtr<CDMInstanceSessionFairPlayStreamingAVFObjC>> m_sessions;
};

class CDMInstanceSessionFairPlayStreamingAVFObjC final : public CDMInstanceSession, public AVContentKeySessionDelegateClient, public CanMakeWeakPtr<CDMInstanceSessionFairPlayStreamingAVFObjC> {
public:
    CDMInstanceSessionFairPlayStreamingAVFObjC(Ref<CDMInstanceFairPlayStreamingAVFObjC>&&);
    virtual ~CDMInstanceSessionFairPlayStreamingAVFObjC();

    // CDMInstanceSession
    void requestLicense(LicenseType, const AtomString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&&) final;
    void updateLicense(const String&, LicenseType, const SharedBuffer&, LicenseUpdateCallback&&) final;
    void loadSession(LicenseType, const String&, const String&, LoadSessionCallback&&) final;
    void closeSession(const String&, CloseSessionCallback&&) final;
    void removeSessionData(const String&, LicenseType, RemoveSessionDataCallback&&) final;
    void storeRecordOfKeyUsage(const String&) final;
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

    using Keys = CDMInstanceFairPlayStreamingAVFObjC::Keys;
    Keys keyIDs();
    AVContentKeySession* contentKeySession() { return m_session ? m_session.get() : m_instance->contentKeySession(); }
    AVContentKeyReportGroup* contentKeyReportGroup() { return m_group.get(); }

    struct Request {
        AtomString initType;
        Vector<RetainPtr<AVContentKeyRequest>> requests;
        bool operator==(const Request& other) const { return initType == other.initType && requests == other.requests; }
    };

private:
    bool ensureSessionOrGroup();
    bool isLicenseTypeSupported(LicenseType) const;

    KeyStatusVector keyStatuses() const;
    void nextRequest();
    AVContentKeyRequest* lastKeyRequest() const;

    Ref<CDMInstanceFairPlayStreamingAVFObjC> m_instance;
    RetainPtr<AVContentKeyReportGroup> m_group;
    RetainPtr<AVContentKeySession> m_session;
    Optional<Request> m_currentRequest;
    RetainPtr<WebCoreFPSContentKeySessionDelegate> m_delegate;
    Vector<RetainPtr<NSData>> m_expiredSessions;
    WeakPtr<CDMInstanceSessionClient> m_client;
    String m_sessionId;
    bool m_outputObscured { false };

    class UpdateResponseCollector;
    std::unique_ptr<UpdateResponseCollector> m_updateResponseCollector;

    Vector<Request> m_pendingRequests;
    Vector<Request> m_requests;

    LicenseCallback m_requestLicenseCallback;
    LicenseUpdateCallback m_updateLicenseCallback;
    CloseSessionCallback m_closeSessionCallback;
    RemoveSessionDataCallback m_removeSessionDataCallback;
};

}

SPECIALIZE_TYPE_TRAITS_CDM_INSTANCE(WebCore::CDMInstanceFairPlayStreamingAVFObjC, WebCore::CDMInstance::ImplementationType::FairPlayStreaming)

#endif // ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
