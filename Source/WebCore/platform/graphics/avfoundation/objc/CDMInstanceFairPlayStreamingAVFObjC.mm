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

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)

#import "CDMFairPlayStreaming.h"
#import "CDMKeySystemConfiguration.h"
#import "CDMMediaCapability.h"
#import "NotImplemented.h"
#import "SharedBuffer.h"
#import "TextDecoder.h"
#import <AVFoundation/AVContentKeySession.h>
#import <objc/runtime.h>
#import <pal/spi/mac/AVFoundationSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/StringHash.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVContentKeySession);
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVContentKeyResponse);
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVURLAsset);
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVContentKeySystemFairPlayStreaming, NSString*)

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVPersistableContentKeyRequest);
#endif

static const NSString *PlaybackSessionIdKey = @"PlaybackSessionID";

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

#if PLATFORM(IOS_FAMILY)
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

- (void)contentKeySession:(AVContentKeySession *)session contentKeyRequestDidSucceed:(AVContentKeyRequest *)keyRequest
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->requestDidSucceed(keyRequest);
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
#if PLATFORM(IOS_FAMILY)
    return getAVPersistableContentKeyRequestClass();
#else
    return false;
#endif
}

bool CDMInstanceFairPlayStreamingAVFObjC::supportsMediaCapability(const CDMMediaCapability& capability)
{
    if (![getAVURLAssetClass() isPlayableExtendedMIMEType:capability.contentType])
        return false;

    // FairPlay only supports 'cbcs' encryption:
    if (capability.encryptionScheme && capability.encryptionScheme.value() != CDMEncryptionScheme::cbcs)
        return false;

    return true;
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

RefPtr<CDMInstanceSession> CDMInstanceFairPlayStreamingAVFObjC::createSession()
{
    return adoptRef(new CDMInstanceSessionFairPlayStreamingAVFObjC(*this));
}

void CDMInstanceFairPlayStreamingAVFObjC::processContentKeyRequestForSession(CDMInstanceSessionFairPlayStreamingAVFObjC& session, RetainPtr<NSString>&& identifier, RetainPtr<NSData>&& initData)
{
    m_currentRequests.append({ makeWeakPtr(session), WTFMove(identifier), WTFMove(initData) });
    if (m_currentRequests.size() == 1)
        processNextContentKeyRequest();
}

void CDMInstanceFairPlayStreamingAVFObjC::processNextContentKeyRequest()
{
    if (m_currentRequests.isEmpty())
        return;

    auto& nextRequest = m_currentRequests.first();
    [m_session processContentKeyRequestWithIdentifier:nextRequest.identifier.get() initializationData:nextRequest.initData.get() options:nil];
}

const String& CDMInstanceFairPlayStreamingAVFObjC::keySystem() const
{
    static NeverDestroyed<String> keySystem { "com.apple.fps"_s };
    return keySystem;
}

void CDMInstanceFairPlayStreamingAVFObjC::didProvideRequest(AVContentKeyRequest *request)
{
    if (m_currentRequests.isEmpty())
        return;

    auto currentRequest = WTFMove(m_currentRequests.first());
    m_currentRequests.remove(0);

    if (!currentRequest.sessionInstance)
        return;

    m_requestMap.set(request, currentRequest.sessionInstance);
    currentRequest.sessionInstance->didProvideRequest(request);
    processNextContentKeyRequest();
}

void CDMInstanceFairPlayStreamingAVFObjC::didProvideRenewingRequest(AVContentKeyRequest *request)
{
    if (m_currentRequests.isEmpty())
        return;

    auto currentRequest = WTFMove(m_currentRequests.first());
    m_currentRequests.remove(0);

    if (!currentRequest.sessionInstance)
        return;

    m_requestMap.set(request, currentRequest.sessionInstance);
    currentRequest.sessionInstance->didProvideRenewingRequest(request);
    processNextContentKeyRequest();
}

void CDMInstanceFairPlayStreamingAVFObjC::didProvidePersistableRequest(AVContentKeyRequest *request)
{
    if (m_currentRequests.isEmpty())
        return;

    auto currentRequest = WTFMove(m_currentRequests.first());
    m_currentRequests.remove(0);

    if (!currentRequest.sessionInstance)
        return;

    m_requestMap.set(request, currentRequest.sessionInstance);
    currentRequest.sessionInstance->didProvidePersistableRequest(request);
    processNextContentKeyRequest();
}

void CDMInstanceFairPlayStreamingAVFObjC::didFailToProvideRequest(AVContentKeyRequest *request, NSError *error)
{
    if (auto sessionInterface = m_requestMap.get(request))
        sessionInterface->didFailToProvideRequest(request, error);
}

void CDMInstanceFairPlayStreamingAVFObjC::requestDidSucceed(AVContentKeyRequest *request)
{
    if (auto sessionInterface = m_requestMap.get(request))
        sessionInterface->requestDidSucceed(request);
}

bool CDMInstanceFairPlayStreamingAVFObjC::shouldRetryRequestForReason(AVContentKeyRequest *request, NSString *reason)
{
    if (auto sessionInterface = m_requestMap.get(request))
        return sessionInterface->shouldRetryRequestForReason(request, reason);
    return false;
}

void CDMInstanceFairPlayStreamingAVFObjC::sessionIdentifierChanged(NSData *sessionIdentifier)
{
    for (auto sessionInterface : m_requestMap.values()) {
        if (sessionInterface)
            sessionInterface->sessionIdentifierChanged(sessionIdentifier);
    }
}

void CDMInstanceFairPlayStreamingAVFObjC::outputObscuredDueToInsufficientExternalProtectionChanged(bool obscured)
{
    for (auto sessionInterface : m_requestMap.values()) {
        if (sessionInterface)
            sessionInterface->outputObscuredDueToInsufficientExternalProtectionChanged(obscured);
    }
}

CDMInstanceSessionFairPlayStreamingAVFObjC::CDMInstanceSessionFairPlayStreamingAVFObjC(Ref<CDMInstanceFairPlayStreamingAVFObjC>&& instance)
    : m_instance(WTFMove(instance))
{
}

Vector<Ref<SharedBuffer>> CDMInstanceSessionFairPlayStreamingAVFObjC::keyIDs()
{
    // FIXME(rdar://problem/35597141): use the future AVContentKeyRequest keyID property, rather than parsing it out of the init
    // data, to get the keyID.
    if ([m_request.get().identifier isKindOfClass:[NSString class]])
        return Vector<Ref<SharedBuffer>>::from(SharedBuffer::create([(NSString *)m_request.get().identifier dataUsingEncoding:NSUTF8StringEncoding]));
    if ([m_request.get().identifier isKindOfClass:[NSData class]])
        return Vector<Ref<SharedBuffer>>::from(SharedBuffer::create((NSData *)m_request.get().identifier));
    if (m_request.get().initializationData) {
        auto sinfKeyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsSinf(SharedBuffer::create(m_request.get().initializationData));
        if (!sinfKeyIDs)
            return { };
        return WTFMove(sinfKeyIDs.value());
    }
    return { };
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::requestLicense(LicenseType licenseType, const AtomicString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&& callback)
{
    if (!isLicenseTypeSupported(licenseType)) {
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    if (!m_instance->serverCertificate()) {
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    RetainPtr<NSString> identifier;
    RetainPtr<NSData> initializationData;

    if (initDataType == CDMPrivateFairPlayStreaming::sinfName())
        initializationData = initData->createNSData();
    else if (initDataType == CDMPrivateFairPlayStreaming::skdName())
        identifier = adoptNS([[NSString alloc] initWithData:initData->createNSData().get() encoding:NSUTF8StringEncoding]);
    else {
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    m_requestLicenseCallback = WTFMove(callback);
    m_instance->processContentKeyRequestForSession(*this, WTFMove(identifier), WTFMove(initializationData));
}

static bool isEqual(const SharedBuffer& data, const String& value)
{
    auto arrayBuffer = data.tryCreateArrayBuffer();
    if (!arrayBuffer)
        return false;

    auto exceptionOrDecoder = TextDecoder::create("utf8"_s, TextDecoder::Options());
    if (exceptionOrDecoder.hasException())
        return false;

    Ref<TextDecoder> decoder = exceptionOrDecoder.releaseReturnValue();
    auto stringOrException = decoder->decode(BufferSource::VariantType(WTFMove(arrayBuffer)), TextDecoder::DecodeOptions());
    if (stringOrException.hasException())
        return false;

    return stringOrException.returnValue() == value;
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::updateLicense(const String&, LicenseType, const SharedBuffer& responseData, LicenseUpdateCallback&& callback)
{
    if (!m_expiredSessions.isEmpty() && isEqual(responseData, "acknowledged"_s)) {
        auto expiredSessions = adoptNS([[NSMutableArray alloc] init]);
        for (auto& session : m_expiredSessions)
            [expiredSessions addObject:session.get()];

        auto* certificate = m_instance->serverCertificate();
        auto* storageDirectory = m_instance->storageDirectory();

        if (!certificate || !storageDirectory) {
            callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
            return;
        }

        RetainPtr<NSData> appIdentifier = certificate->createNSData();
        [getAVContentKeySessionClass() removePendingExpiredSessionReports:expiredSessions.get() withAppIdentifier:appIdentifier.get() storageDirectoryAtURL:storageDirectory];
        callback(false, { }, std::nullopt, std::nullopt, Succeeded);
        return;
    }

    if (!m_request) {
        callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        return;
    }
    Vector<Ref<SharedBuffer>> keyIDs = this->keyIDs();
    if (keyIDs.isEmpty()) {
        callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        return;
    }

    [m_request processContentKeyResponse:[getAVContentKeyResponseClass() contentKeyResponseWithFairPlayStreamingKeyResponseData:responseData.createNSData().get()]];

    // FIXME(rdar://problem/35592277): stash the callback and call it once AVContentKeyResponse supports a success callback.
    struct objc_method_description method = protocol_getMethodDescription(@protocol(AVContentKeySessionDelegate), @selector(contentKeySession:contentKeyRequestDidSucceed:), NO, YES);
    if (!method.name) {
        KeyStatusVector keyStatuses;
        keyStatuses.reserveInitialCapacity(1);
        keyStatuses.uncheckedAppend(std::make_pair(WTFMove(keyIDs.first()), KeyStatus::Usable));
        callback(false, std::make_optional(WTFMove(keyStatuses)), std::nullopt, std::nullopt, Succeeded);
        return;
    }

    m_updateLicenseCallback = WTFMove(callback);
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::loadSession(LicenseType licenseType, const String& sessionId, const String& origin, LoadSessionCallback&& callback)
{
    UNUSED_PARAM(origin);
    if (licenseType == LicenseType::PersistentUsageRecord) {
        auto* storageDirectory = m_instance->storageDirectory();
        if (!m_instance->persistentStateAllowed() || storageDirectory) {
            callback(std::nullopt, std::nullopt, std::nullopt, Failed, SessionLoadFailure::MismatchedSessionType);
            return;
        }
        auto* certificate = m_instance->serverCertificate();
        if (!certificate) {
            callback(std::nullopt, std::nullopt, std::nullopt, Failed, SessionLoadFailure::NoSessionData);
            return;
        }

        RetainPtr<NSData> appIdentifier = certificate->createNSData();
        KeyStatusVector changedKeys;
        for (NSData* expiredSessionData in [getAVContentKeySessionClass() pendingExpiredSessionReportsWithAppIdentifier:appIdentifier.get() storageDirectoryAtURL:storageDirectory]) {
            NSDictionary *expiredSession = [NSPropertyListSerialization propertyListWithData:expiredSessionData options:kCFPropertyListImmutable format:nullptr error:nullptr];
            NSString *playbackSessionIdValue = (NSString *)[expiredSession objectForKey:PlaybackSessionIdKey];
            if (![playbackSessionIdValue isKindOfClass:[NSString class]])
                continue;

            if (sessionId == String(playbackSessionIdValue)) {
                // FIXME(rdar://problem/35934922): use key values stored in expired session report once available
                changedKeys.append((KeyStatusVector::ValueType){ SharedBuffer::create(), KeyStatus::Released });
                m_expiredSessions.append(expiredSessionData);
            }
        }

        if (changedKeys.isEmpty()) {
            callback(std::nullopt, std::nullopt, std::nullopt, Failed, SessionLoadFailure::NoSessionData);
            return;
        }

        callback(WTFMove(changedKeys), std::nullopt, std::nullopt, Succeeded, SessionLoadFailure::None);
    }
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::closeSession(const String&, CloseSessionCallback&& callback)
{
    if (m_requestLicenseCallback) {
        m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
        m_requestLicenseCallback = nullptr;
    }
    if (m_updateLicenseCallback) {
        m_updateLicenseCallback(true, std::nullopt, std::nullopt, std::nullopt, Failed);
        m_updateLicenseCallback = nullptr;
    }
    if (m_removeSessionDataCallback) {
        m_removeSessionDataCallback({ }, std::nullopt, Failed);
        m_removeSessionDataCallback = nullptr;
    }
    m_request = nullptr;
    callback();
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::removeSessionData(const String& sessionId, LicenseType licenseType, RemoveSessionDataCallback&& callback)
{
    // FIXME: We should be able to expire individual AVContentKeyRequests rather than the entire AVContentKeySession.
    [m_instance->contentKeySession() expire];

    if (licenseType == LicenseType::PersistentUsageRecord) {
        auto* storageDirectory = m_instance->storageDirectory();
        auto* certificate = m_instance->serverCertificate();

        if (!m_instance->persistentStateAllowed() || !storageDirectory || !certificate) {
            callback({ }, std::nullopt, Failed);
            return;
        }

        RetainPtr<NSData> appIdentifier = certificate->createNSData();
        RetainPtr<NSMutableArray> expiredSessionsArray = adoptNS([[NSMutableArray alloc] init]);
        KeyStatusVector changedKeys;
        for (NSData* expiredSessionData in [getAVContentKeySessionClass() pendingExpiredSessionReportsWithAppIdentifier:appIdentifier.get() storageDirectoryAtURL:storageDirectory]) {
            NSDictionary *expiredSession = [NSPropertyListSerialization propertyListWithData:expiredSessionData options:kCFPropertyListImmutable format:nullptr error:nullptr];
            NSString *playbackSessionIdValue = (NSString *)[expiredSession objectForKey:PlaybackSessionIdKey];
            if (![playbackSessionIdValue isKindOfClass:[NSString class]])
                continue;

            if (sessionId == String(playbackSessionIdValue)) {
                // FIXME(rdar://problem/35934922): use key values stored in expired session report once available
                changedKeys.append((KeyStatusVector::ValueType){ SharedBuffer::create(), KeyStatus::Released });
                m_expiredSessions.append(expiredSessionData);
                [expiredSessionsArray addObject:expiredSession];
            }
        }

        RetainPtr<NSData> expiredSessionsData = [NSPropertyListSerialization dataWithPropertyList:expiredSessionsArray.get() format:NSPropertyListBinaryFormat_v1_0 options:kCFPropertyListImmutable error:nullptr];

        callback(WTFMove(changedKeys), SharedBuffer::create(expiredSessionsData.get()), Succeeded);
    }
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::storeRecordOfKeyUsage(const String&)
{
    // no-op; key usage data is stored automatically.
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::setClient(WeakPtr<CDMInstanceSessionClient>&& client)
{
    m_client = WTFMove(client);
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::clearClient()
{
    m_client = nullptr;
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::didProvideRequest(AVContentKeyRequest *request)
{
    m_request = request;
    if (!m_requestLicenseCallback)
        return;

    RetainPtr<NSData> appIdentifier;
    if (auto* certificate = m_instance->serverCertificate())
        appIdentifier = certificate->createNSData();
    Vector<Ref<SharedBuffer>> keyIDs = this->keyIDs();

    if (keyIDs.isEmpty()) {
        m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
        m_requestLicenseCallback = nullptr;
        return;
    }

    RetainPtr<NSData> contentIdentifier = keyIDs.first()->createNSData();
    [m_request makeStreamingContentKeyRequestDataForApp:appIdentifier.get() contentIdentifier:contentIdentifier.get() options:nil completionHandler:[this, weakThis = makeWeakPtr(*this)] (NSData *contentKeyRequestData, NSError *error) mutable {
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

void CDMInstanceSessionFairPlayStreamingAVFObjC::didProvideRenewingRequest(AVContentKeyRequest *request)
{
    UNUSED_PARAM(request);
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::didProvidePersistableRequest(AVContentKeyRequest *request)
{
    UNUSED_PARAM(request);
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::didFailToProvideRequest(AVContentKeyRequest *request, NSError *error)
{
    UNUSED_PARAM(request);
    UNUSED_PARAM(error);
    if (m_updateLicenseCallback)
        m_updateLicenseCallback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::requestDidSucceed(AVContentKeyRequest *request)
{
    UNUSED_PARAM(request);
    if (!m_updateLicenseCallback)
        return;

    Vector<Ref<SharedBuffer>> keyIDs = this->keyIDs();
    KeyStatusVector keyStatuses;
    keyStatuses.reserveInitialCapacity(1);
    keyStatuses.uncheckedAppend(std::make_pair(WTFMove(keyIDs.first()), KeyStatus::Usable));
    m_updateLicenseCallback(false, std::make_optional(WTFMove(keyStatuses)), std::nullopt, std::nullopt, Succeeded);
}

bool CDMInstanceSessionFairPlayStreamingAVFObjC::shouldRetryRequestForReason(AVContentKeyRequest *request, NSString *reason)
{
    UNUSED_PARAM(request);
    UNUSED_PARAM(reason);
    notImplemented();
    return false;
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::sessionIdentifierChanged(NSData *sessionIdentifier)
{
    if (!sessionIdentifier) {
        m_sessionId = emptyString();
        return;
    }

    auto sessionIdentifierString = adoptNS([[NSString alloc] initWithData:sessionIdentifier encoding:NSUTF8StringEncoding]);
    m_sessionId = sessionIdentifierString.get();
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::outputObscuredDueToInsufficientExternalProtectionChanged(bool obscured)
{
    if (!m_client || !m_request)
        return;

    CDMKeyStatus status;
    if (obscured)
        status = CDMKeyStatus::OutputRestricted;
    else {
        switch (m_request.get().status) {
        case AVContentKeyRequestStatusRequestingResponse:
        case AVContentKeyRequestStatusRetried:
            status = CDMKeyStatus::StatusPending;
            break;
        case AVContentKeyRequestStatusReceivedResponse:
        case AVContentKeyRequestStatusRenewed:
            status = CDMKeyStatus::Usable;
            break;
        case AVContentKeyRequestStatusCancelled:
            status = CDMKeyStatus::Released;
            break;
        case AVContentKeyRequestStatusFailed:
            status = CDMKeyStatus::InternalError;
            break;
        }
    }

    auto keyStatuses = keyIDs().map([status] (const Ref<SharedBuffer>& keyID) -> KeyStatusVector::ValueType {
        return { keyID.copyRef(), status };
    });
    m_client->updateKeyStatuses(WTFMove(keyStatuses));
}

bool CDMInstanceSessionFairPlayStreamingAVFObjC::isLicenseTypeSupported(LicenseType licenseType) const
{
    switch (licenseType) {
    case CDMSessionType::PersistentLicense:
        return m_instance->persistentStateAllowed() && m_instance->supportsPersistentKeys();
    case CDMSessionType::PersistentUsageRecord:
        return m_instance->persistentStateAllowed() && m_instance->supportsPersistableState();
    case CDMSessionType::Temporary:
        return true;
    }
}

}

#endif // ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
