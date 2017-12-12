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

#import "config.h"
#import "CDMInstanceFairPlayStreamingAVFObjC.h"

#if HAVE(AVCONTENTKEYSESSION)

#import "CDMFairPlayStreaming.h"
#import "CDMKeySystemConfiguration.h"
#import "NotImplemented.h"
#import "SharedBuffer.h"
#import <AVFoundation/AVContentKeySession.h>
#import <pal/spi/mac/AVFoundationSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/StringHash.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVContentKeySession);
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVContentKeyResponse);
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVURLAsset);
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVContentKeySystemFairPlayStreaming, NSString*)

#if PLATFORM(IOS)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVPersistableContentKeyRequest);
#endif

@interface WebCoreFPSContentKeySessionDelegate : NSObject<AVContentKeySessionDelegate> {
    WebCore::CDMInstanceFairPlayStreamingAVFObjC* _parent;
}
@end

@implementation WebCoreFPSContentKeySessionDelegate
- (id)initWithParent:(WebCore::CDMInstanceFairPlayStreamingAVFObjC *)parent
{
    if (!(self = [super init]))
        return nil;

    _parent = parent;
    return self;
}

- (void)invalidate
{
    _parent = nil;
}

- (void)contentKeySession:(AVContentKeySession *)session didProvideContentKeyRequest:(AVContentKeyRequest *)keyRequest
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->didProvideRequest(keyRequest);
}

- (void)contentKeySession:(AVContentKeySession *)session didProvideRenewingContentKeyRequest:(AVContentKeyRequest *)keyRequest
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->didProvideRenewingRequest(keyRequest);
}

#if PLATFORM(IOS)
- (void)contentKeySession:(AVContentKeySession *)session didProvidePersistableContentKeyRequest:(AVPersistableContentKeyRequest *)keyRequest
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->didProvidePersistableRequest(keyRequest);
}

- (void)contentKeySession:(AVContentKeySession *)session didUpdatePersistableContentKey:(NSData *)persistableContentKey forContentKeyIdentifier:(id)keyIdentifier
{
    UNUSED_PARAM(session);
    UNUSED_PARAM(persistableContentKey);
    UNUSED_PARAM(keyIdentifier);
    notImplemented();
}
#endif

- (void)contentKeySession:(AVContentKeySession *)session contentKeyRequest:(AVContentKeyRequest *)keyRequest didFailWithError:(NSError *)err
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->didFailToProvideRequest(keyRequest, err);
}

- (BOOL)contentKeySession:(AVContentKeySession *)session shouldRetryContentKeyRequest:(AVContentKeyRequest *)keyRequest reason:(AVContentKeyRequestRetryReason)retryReason
{
    UNUSED_PARAM(session);
    return _parent ? _parent->shouldRetryRequestForReason(keyRequest, retryReason) : false;
}

- (void)contentKeySessionContentProtectionSessionIdentifierDidChange:(AVContentKeySession *)session
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->sessionIdentifierChanged(session.contentProtectionSessionIdentifier);
}

@end

namespace WebCore {

CDMInstanceFairPlayStreamingAVFObjC::CDMInstanceFairPlayStreamingAVFObjC()
    : CDMInstance()
    , m_delegate([[WebCoreFPSContentKeySessionDelegate alloc] initWithParent:this])
{
}

CDMInstanceFairPlayStreamingAVFObjC::~CDMInstanceFairPlayStreamingAVFObjC()
{
    [m_delegate invalidate];
}

bool CDMInstanceFairPlayStreamingAVFObjC::supportsPersistableState()
{
    return [getAVContentKeySessionClass() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)];
}

bool CDMInstanceFairPlayStreamingAVFObjC::supportsPersistentKeys()
{
#if PLATFORM(IOS)
    return getAVPersistableContentKeyRequestClass();
#else
    return false;
#endif
}

bool CDMInstanceFairPlayStreamingAVFObjC::mimeTypeIsPlayable(const String& contentType)
{
    return [getAVURLAssetClass() isPlayableExtendedMIMEType:contentType];
}

CDMInstance::SuccessValue CDMInstanceFairPlayStreamingAVFObjC::initializeWithConfiguration(const CDMKeySystemConfiguration& configuration)
{
    // FIXME: verify that FairPlayStreaming does not (and cannot) expose a distinctive identifier to the client
    if (configuration.distinctiveIdentifier == CDMRequirement::Required)
        return Failed;

    if (configuration.persistentState != CDMRequirement::Required && (configuration.sessionTypes.contains(CDMSessionType::PersistentUsageRecord) || configuration.sessionTypes.contains(CDMSessionType::PersistentLicense)))
        return Failed;

    if (configuration.persistentState == CDMRequirement::Required && !m_storageDirectory)
        return Failed;

    if (configuration.sessionTypes.contains(CDMSessionType::PersistentLicense) && !supportsPersistentKeys())
        return Failed;

    if (!canLoadAVContentKeySystemFairPlayStreaming())
        return Failed;

    if (configuration.persistentState == CDMRequirement::NotAllowed || !m_storageDirectory)
        m_session = [getAVContentKeySessionClass() contentKeySessionWithKeySystem:getAVContentKeySystemFairPlayStreaming()];
    else
        m_session = [getAVContentKeySessionClass() contentKeySessionWithKeySystem:getAVContentKeySystemFairPlayStreaming() storageDirectoryAtURL:m_storageDirectory.get()];

    if (!m_session)
        return Failed;

    [m_session setDelegate:m_delegate.get() queue:dispatch_get_main_queue()];

    return Succeeded;
}

CDMInstance::SuccessValue CDMInstanceFairPlayStreamingAVFObjC::setDistinctiveIdentifiersAllowed(bool)
{
    // FIXME: verify that FairPlayStreaming does not (and cannot) expose a distinctive identifier to the client
    return Succeeded;
}

CDMInstance::SuccessValue CDMInstanceFairPlayStreamingAVFObjC::setPersistentStateAllowed(bool persistentStateAllowed)
{
    m_persistentStateAllowed = persistentStateAllowed;
    return Succeeded;
}

CDMInstance::SuccessValue CDMInstanceFairPlayStreamingAVFObjC::setServerCertificate(Ref<SharedBuffer>&& serverCertificate)
{
    m_serverCertificate = WTFMove(serverCertificate);
    return Succeeded;
}

CDMInstance::SuccessValue CDMInstanceFairPlayStreamingAVFObjC::setStorageDirectory(const String& storageDirectory)
{
    if (storageDirectory.isEmpty())
        m_storageDirectory = nil;
    else
        m_storageDirectory = adoptNS([[NSURL alloc] initFileURLWithPath:storageDirectory isDirectory:YES]);
    return Succeeded;
}

bool CDMInstanceFairPlayStreamingAVFObjC::isLicenseTypeSupported(LicenseType licenseType) const
{
    switch (licenseType) {
    case CDMSessionType::PersistentLicense:
        return m_persistentStateAllowed && supportsPersistentKeys();
    case CDMSessionType::PersistentUsageRecord:
        return m_persistentStateAllowed && supportsPersistableState();
    case CDMSessionType::Temporary:
        return true;
    }
}

void CDMInstanceFairPlayStreamingAVFObjC::requestLicense(LicenseType licenseType, const AtomicString&, Ref<SharedBuffer>&& initData, LicenseCallback callback)
{
    if (!isLicenseTypeSupported(licenseType)) {
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    if (!m_serverCertificate) {
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    m_requestLicenseCallback = WTFMove(callback);

    [m_session processContentKeyRequestWithIdentifier:nil initializationData:initData->createNSData().get() options:nil];
}

void CDMInstanceFairPlayStreamingAVFObjC::updateLicense(const String&, LicenseType, const SharedBuffer& responseData, LicenseUpdateCallback callback)
{
    if (!m_request) {
        callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        return;
    }
    // FIXME(rdar://problem/35597141): use the future AVContentKeyRequest keyID property, rather than parsing it out of the init
    // data, to get the keyID.
    Vector<Ref<SharedBuffer>> keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsSinf(SharedBuffer::create(m_request.get().initializationData));
    if (keyIDs.isEmpty()) {
        callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        return;
    }

    [m_request processContentKeyResponse:[getAVContentKeyResponseClass() contentKeyResponseWithFairPlayStreamingKeyResponseData:responseData.createNSData().get()]];

    // FIXME(rdar://problem/35592277): stash the callback and call it once AVContentKeyResponse supports a success callback.
    KeyStatusVector keyStatuses;
    keyStatuses.reserveInitialCapacity(1);
    keyStatuses.uncheckedAppend(std::make_pair(WTFMove(keyIDs.first()), KeyStatus::Usable));
    callback(false, std::make_optional(WTFMove(keyStatuses)), std::nullopt, std::nullopt, Succeeded);
}

void CDMInstanceFairPlayStreamingAVFObjC::loadSession(LicenseType, const String&, const String&, LoadSessionCallback)
{
    notImplemented();
}

void CDMInstanceFairPlayStreamingAVFObjC::closeSession(const String&, CloseSessionCallback)
{
    notImplemented();
}

void CDMInstanceFairPlayStreamingAVFObjC::removeSessionData(const String&, LicenseType, RemoveSessionDataCallback)
{
    notImplemented();
}

void CDMInstanceFairPlayStreamingAVFObjC::storeRecordOfKeyUsage(const String&)
{
    notImplemented();
}

const String& CDMInstanceFairPlayStreamingAVFObjC::keySystem() const
{
    static NeverDestroyed<String> s_keySystem { ASCIILiteral("com.apple.fps") };
    return s_keySystem;
}

void CDMInstanceFairPlayStreamingAVFObjC::didProvideRequest(AVContentKeyRequest *request)
{
    m_request = request;
    if (!m_requestLicenseCallback)
        return;

    RetainPtr<NSData> appIdentifier = m_serverCertificate ? m_serverCertificate->createNSData() : nullptr;
    Vector<Ref<SharedBuffer>> keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsSinf(SharedBuffer::create(request.initializationData));
    if (keyIDs.isEmpty()) {
        m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
        m_requestLicenseCallback = nullptr;
        return;
    }

    RetainPtr<NSData> contentIdentifier = keyIDs.first()->createNSData();
    [m_request makeStreamingContentKeyRequestDataForApp:appIdentifier.get() contentIdentifier:contentIdentifier.get() options:nil completionHandler:[this, weakThis = createWeakPtr()] (NSData *contentKeyRequestData, NSError *error) mutable {
        callOnMainThread([this, weakThis = WTFMove(weakThis), error = retainPtr(error), contentKeyRequestData = retainPtr(contentKeyRequestData)] {
            if (!weakThis || !m_requestLicenseCallback)
                return;

            if (error)
                m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
            else
                m_requestLicenseCallback(SharedBuffer::create(contentKeyRequestData.get()), m_sessionId, false, Succeeded);
            m_requestLicenseCallback = nullptr;
        });
    }];
}

void CDMInstanceFairPlayStreamingAVFObjC::didProvideRenewingRequest(AVContentKeyRequest *request)
{
    UNUSED_PARAM(request);
}

void CDMInstanceFairPlayStreamingAVFObjC::didProvidePersistableRequest(AVContentKeyRequest *request)
{
    UNUSED_PARAM(request);
}

void CDMInstanceFairPlayStreamingAVFObjC::didFailToProvideRequest(AVContentKeyRequest *request, NSError *error)
{
    UNUSED_PARAM(request);
    UNUSED_PARAM(error);
    if (m_requestLicenseCallback)
        m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
}

bool CDMInstanceFairPlayStreamingAVFObjC::shouldRetryRequestForReason(AVContentKeyRequest *request, NSString *reason)
{
    UNUSED_PARAM(request);
    UNUSED_PARAM(reason);
    notImplemented();
    return false;
}

void CDMInstanceFairPlayStreamingAVFObjC::sessionIdentifierChanged(NSData *sessionIdentifier)
{
    if (!sessionIdentifier) {
        m_sessionId = emptyString();
        return;
    }

    auto sessionIdentifierString = adoptNS([[NSString alloc] initWithData:sessionIdentifier encoding:NSUTF8StringEncoding]);
    m_sessionId = sessionIdentifierString.get();
}   

}

#endif
