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

OBJC_CLASS AVContentKeyRequest;
OBJC_CLASS AVContentKeySession;
OBJC_CLASS NSData;
OBJC_CLASS NSError;
OBJC_CLASS NSURL;
OBJC_CLASS WebCoreFPSContentKeySessionDelegate;

namespace WebCore {

class CDMInstanceSessionFairPlayStreamingAVFObjC;
struct CDMMediaCapability;

class CDMInstanceFairPlayStreamingAVFObjC final : public CDMInstance, public CanMakeWeakPtr<CDMInstanceFairPlayStreamingAVFObjC> {
public:
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

    const String& keySystem() const final;

    NSURL *storageDirectory() const { return m_storageDirectory.get(); }
    bool persistentStateAllowed() const { return m_persistentStateAllowed; }
    SharedBuffer* serverCertificate() const { return m_serverCertificate.get(); }

    void outputObscuredDueToInsufficientExternalProtectionChanged(bool);
    CDMInstanceSessionFairPlayStreamingAVFObjC* sessionForKeyIDs(const Vector<Ref<SharedBuffer>>&) const;

private:
    RefPtr<SharedBuffer> m_serverCertificate;
    bool m_persistentStateAllowed { true };
    RetainPtr<NSURL> m_storageDirectory;
    Vector<WeakPtr<CDMInstanceSessionFairPlayStreamingAVFObjC>> m_sessions;
};

class CDMInstanceSessionFairPlayStreamingAVFObjC final : public CDMInstanceSession, public CanMakeWeakPtr<CDMInstanceSessionFairPlayStreamingAVFObjC> {
public:
    CDMInstanceSessionFairPlayStreamingAVFObjC(Ref<CDMInstanceFairPlayStreamingAVFObjC>&&);
    virtual ~CDMInstanceSessionFairPlayStreamingAVFObjC();

    // CDMInstanceSession
    void requestLicense(LicenseType, const AtomicString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&&) final;
    void updateLicense(const String&, LicenseType, const SharedBuffer&, LicenseUpdateCallback&&) final;
    void loadSession(LicenseType, const String&, const String&, LoadSessionCallback&&) final;
    void closeSession(const String&, CloseSessionCallback&&) final;
    void removeSessionData(const String&, LicenseType, RemoveSessionDataCallback&&) final;
    void storeRecordOfKeyUsage(const String&) final;
    void setClient(WeakPtr<CDMInstanceSessionClient>&&) final;
    void clearClient() final;

    void didProvideRequest(AVContentKeyRequest*);
    void didProvideRenewingRequest(AVContentKeyRequest*);
    void didProvidePersistableRequest(AVContentKeyRequest*);
    void didFailToProvideRequest(AVContentKeyRequest*, NSError*);
    void requestDidSucceed(AVContentKeyRequest*);
    bool shouldRetryRequestForReason(AVContentKeyRequest*, NSString*);
    void sessionIdentifierChanged(NSData*);
    void outputObscuredDueToInsufficientExternalProtectionChanged(bool);

    Vector<Ref<SharedBuffer>> keyIDs();
    AVContentKeySession* contentKeySession() { return m_session.get(); }

private:
    AVContentKeySession* ensureSession();
    bool isLicenseTypeSupported(LicenseType) const;

    KeyStatusVector keyStatuses() const;
    void nextRequest();

    Ref<CDMInstanceFairPlayStreamingAVFObjC> m_instance;
    RetainPtr<AVContentKeySession> m_session;
    RetainPtr<AVContentKeyRequest> m_currentRequest;
    RetainPtr<WebCoreFPSContentKeySessionDelegate> m_delegate;
    Vector<RetainPtr<NSData>> m_expiredSessions;
    WeakPtr<CDMInstanceSessionClient> m_client;
    String m_sessionId;
    bool m_outputObscured { false };

    Vector<RetainPtr<AVContentKeyRequest>> m_pendingRequests;
    Vector<RetainPtr<AVContentKeyRequest>> m_requests;

    LicenseCallback m_requestLicenseCallback;
    LicenseUpdateCallback m_updateLicenseCallback;
    CloseSessionCallback m_closeSessionCallback;
    RemoveSessionDataCallback m_removeSessionDataCallback;
};

}

SPECIALIZE_TYPE_TRAITS_CDM_INSTANCE(WebCore::CDMInstanceFairPlayStreamingAVFObjC, WebCore::CDMInstance::ImplementationType::FairPlayStreaming)

#endif // ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
