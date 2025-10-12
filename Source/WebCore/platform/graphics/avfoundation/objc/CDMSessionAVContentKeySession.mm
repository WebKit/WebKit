/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#import "CDMSessionAVContentKeySession.h"

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)

#import "CDMFairPlayStreaming.h"
#import "CDMInstanceFairPlayStreamingAVFObjC.h"
#import "CDMPrivateMediaSourceAVFObjC.h"
#import "LegacyCDM.h"
#import "Logging.h"
#import "MediaPlayer.h"
#import "MediaSampleAVFObjC.h"
#import "MediaSessionManagerCocoa.h"
#import "SharedBuffer.h"
#import "SourceBufferPrivateAVFObjC.h"
#import "WebCoreNSErrorExtras.h"
#import <AVFoundation/AVError.h>
#import <CoreMedia/CMBase.h>
#import <JavaScriptCore/HeapInlines.h>
#import <JavaScriptCore/JSCellInlines.h>
#import <JavaScriptCore/JSGlobalObjectInlines.h>
#import <JavaScriptCore/TypedArrayInlines.h>
#import <objc/objc-runtime.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

typedef NSString *AVContentKeySystem;

@interface AVContentKeySession (WebCorePrivate)
- (instancetype)initWithStorageDirectoryAtURL:(NSURL *)storageURL;
@property (assign) id delegate;
- (void)addStreamDataParser:(AVStreamDataParser *)streamDataParser;
- (void)removeStreamDataParser:(AVStreamDataParser *)streamDataParser;
- (void)processContentKeyRequestInitializationData:(NSData *)initializationData options:(NSDictionary *)options;
@end

@interface AVContentKeyRequest (WebCorePrivate)
- (NSData *)contentKeyRequestDataForApp:(NSData *)appIdentifier contentIdentifier:(NSData *)contentIdentifier options:(NSDictionary *)options error:(NSError **)outError;
- (void)processContentKeyResponseData:(NSData *)contentKeyResponseData;
- (void)renewExpiringContentKeyResponseData;
@end

@interface WebCDMSessionAVContentKeySessionDelegate : NSObject<AVContentKeySessionDelegate> {
    ThreadSafeWeakPtr<WebCore::CDMSessionAVContentKeySession> m_parent;
}
- (void)invalidate;
@end

@implementation WebCDMSessionAVContentKeySessionDelegate
- (id)initWithParent:(WebCore::CDMSessionAVContentKeySession *)parent
{
    if ((self = [super init]))
        m_parent = parent;
    return self;
}


- (void)invalidate
{
    m_parent = nullptr;
}

- (void)contentKeySession:(AVContentKeySession *)session didProvideContentKeyRequest:(AVContentKeyRequest *)keyRequest
{
    UNUSED_PARAM(session);

    if (RefPtr parent = m_parent.get())
        parent->didProvideContentKeyRequest(keyRequest);
}

- (void)contentKeySessionContentProtectionSessionIdentifierDidChange:(AVContentKeySession *)session
{
    RefPtr parent = m_parent.get();
    if (!parent)
        return;

    NSData* identifier = [session contentProtectionSessionIdentifier];
    RetainPtr<NSString> sessionIdentifierString = identifier ? adoptNS([[NSString alloc] initWithData:identifier encoding:NSUTF8StringEncoding]) : nil;
    callOnMainThread([self, protectedSelf = RetainPtr { self }, sessionIdentifierString = WTFMove(sessionIdentifierString)] {
        RefPtr parent = m_parent.get();
        if (!parent)
            return;

        parent->setSessionId(sessionIdentifierString.get());
    });
}
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CDMSessionAVContentKeySession);

constexpr Seconds kDidProvideContentKeyRequestTimeout { 5_s };

CDMSessionAVContentKeySession::CDMSessionAVContentKeySession(Vector<int>&& protocolVersions, int cdmVersion, CDMPrivateMediaSourceAVFObjC& cdm, LegacyCDMSessionClient& client)
    : CDMSessionMediaSourceAVFObjC(cdm, client)
    , m_contentKeySessionDelegate(adoptNS([[WebCDMSessionAVContentKeySessionDelegate alloc] initWithParent:this]))
    , m_delegateQueue(WorkQueue::create("CDMSessionAVContentKeySession delegate queue"_s))
    , m_hasKeyRequestSemaphore(0)
    , m_protocolVersions(WTFMove(protocolVersions))
    , m_cdmVersion(cdmVersion)
    , m_mode(Normal)
#if !RELEASE_LOG_DISABLED
    , m_logger(client.logger())
    , m_logIdentifier(client.logIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

CDMSessionAVContentKeySession::~CDMSessionAVContentKeySession()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    [m_contentKeySessionDelegate invalidate];

    if (hasContentKeySession()) {
        for (auto& sourceBuffer : m_sourceBuffers)
            sourceBuffer->flush();

        [contentKeySession() expire];
    }
}

bool CDMSessionAVContentKeySession::isAvailable()
{
    return PAL::getAVContentKeySessionClassSingleton();
}

RefPtr<Uint8Array> CDMSessionAVContentKeySession::generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode)
{
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(destinationURL);
    ASSERT(initData);

    ALWAYS_LOG(LOGIDENTIFIER, "mimeType: ", mimeType);

    errorCode = MediaPlayer::NoError;
    systemCode = 0;

    if (equalLettersIgnoringASCIICase(mimeType, "keyrelease"_s)) {
        m_mode = KeyRelease;
        m_certificate = initData;
        return generateKeyReleaseMessage(errorCode, systemCode);
    }

    if (m_cdmVersion == 2)
        m_identifier = initData;
    else
        m_initData = SharedBuffer::create(initData->span());

    ASSERT(!m_certificate);
    String certificateString("certificate"_s);
    auto array = Uint8Array::create(certificateString.length());
    for (unsigned i = 0, length = certificateString.length(); i < length; ++i)
        array->set(i, certificateString[i]);
    return WTFMove(array);
}

void CDMSessionAVContentKeySession::releaseKeys()
{
    if (hasContentKeySession()) {
        m_stopped = true;
        for (auto& sourceBuffer : m_sourceBuffers)
            sourceBuffer->flush();

        ALWAYS_LOG(LOGIDENTIFIER, "expiring stream session");
        [contentKeySession() expire];

        if (!m_certificate)
            return;

        if (![PAL::getAVContentKeySessionClassSingleton() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)])
            return;

        auto storagePath = this->storagePath();
        if (storagePath.isEmpty())
            return;

        RetainPtr certificateData = toNSData(m_certificate->span());
        NSArray* expiredSessions = [PAL::getAVContentKeySessionClassSingleton() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath.createNSString().get()]];
        for (NSData* expiredSessionData in expiredSessions) {
            static const NSString *PlaybackSessionIdKey = @"PlaybackSessionID";
            NSDictionary *expiredSession = [NSPropertyListSerialization propertyListWithData:expiredSessionData options:kCFPropertyListImmutable format:nullptr error:nullptr];
            RetainPtr playbackSessionIdValue = dynamic_objc_cast<NSString>([expiredSession objectForKey:PlaybackSessionIdKey]);
            if (!playbackSessionIdValue)
                continue;

            if (m_sessionId == String(playbackSessionIdValue.get())) {
                ALWAYS_LOG(LOGIDENTIFIER, "found session, sending expiration message");
                m_expiredSession = expiredSessionData;
                m_client->sendMessage(Uint8Array::create(span(m_expiredSession.get())).ptr(), emptyString());
                break;
            }
        }
    }
}

static bool isEqual(Uint8Array* data, ASCIILiteral literal)
{
    ASSERT(data);
    ASSERT(!literal.isNull());
    unsigned length = data->length();

    for (unsigned i = 0; i < length; ++i) {
        if (!literal[i])
            return false;

        if (data->item(i) != static_cast<uint8_t>(literal[i]))
            return false;
    }
    return !literal[length];
}

bool CDMSessionAVContentKeySession::update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode)
{
    UNUSED_PARAM(nextMessage);

    if (isEqual(key, "acknowledged"_s)) {
        ALWAYS_LOG(LOGIDENTIFIER, "acknowleding secure stop message");

        String storagePath = this->storagePath();
        if (!m_expiredSession || storagePath.isEmpty()) {
            errorCode = MediaPlayer::InvalidPlayerState;
            return false;
        }

        RetainPtr certificateData = toNSData(m_certificate->span());

        if ([PAL::getAVContentKeySessionClassSingleton() respondsToSelector:@selector(removePendingExpiredSessionReports:withAppIdentifier:storageDirectoryAtURL:)])
            [PAL::getAVContentKeySessionClassSingleton() removePendingExpiredSessionReports:@[m_expiredSession.get()] withAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath.createNSString().get()]];
        m_expiredSession = nullptr;
        return true;
    }

    if (m_stopped) {
        errorCode = MediaPlayer::InvalidPlayerState;
        return false;
    }

    bool shouldGenerateKeyRequest = !m_certificate || isEqual(key, "renew"_s);
    if (!m_certificate) {
        ALWAYS_LOG(LOGIDENTIFIER, "certificate data");

        m_certificate = key;
    }

    if (m_mode == KeyRelease)
        return false;
    
    if (m_cdmVersion == 2) {
        // In the com.apple.fps.2_0 communication protocol, the client must first attach the
        // session to the protected SourceBuffer in order to get access to the initialization
        // data.
        RefPtr<SourceBufferPrivateAVFObjC> protectedSourceBuffer;
        for (auto& sourceBuffer : m_sourceBuffers) {
            if (sourceBuffer->protectedTrackID()) {
                protectedSourceBuffer = sourceBuffer;
                break;
            }
        }

        if (!protectedSourceBuffer) {
            errorCode = MediaPlayer::InvalidPlayerState;
            return false;
        }
        
        m_initData = protectedSourceBuffer->initData();
    }

    if (!hasContentKeyRequest()) {
        RetainPtr<NSData> nsInitData = m_initData ? m_initData->createNSData() : nil;
        RetainPtr nsIdentifier = m_identifier ? toNSData(m_identifier->span()) : nil;
        if ([contentKeySession() respondsToSelector:@selector(processContentKeyRequestWithIdentifier:initializationData:options:)])
            [contentKeySession() processContentKeyRequestWithIdentifier:nsIdentifier.get() initializationData:nsInitData.get() options:nil];
        else
            [contentKeySession() processContentKeyRequestInitializationData:nsInitData.get() options:nil];
        m_hasKeyRequestSemaphore.waitFor(kDidProvideContentKeyRequestTimeout);
    }

    auto contentKeyRequest = this->contentKeyRequest();

    if (shouldGenerateKeyRequest) {
        ASSERT(contentKeyRequest);
        RetainPtr certificateData = toNSData(m_certificate->span());

        RetainPtr options = CDMInstanceSessionFairPlayStreamingAVFObjC::optionsForKeyRequestWithHashSalt(m_client->mediaKeysHashSalt());

        if (!m_protocolVersions.isEmpty() && PAL::canLoad_AVFoundation_AVContentKeyRequestProtocolVersionsKey()) {
            RetainPtr mutableOptions = adoptNS([[NSMutableDictionary alloc] init]);
            [mutableOptions addEntriesFromDictionary:options.get()];
            [mutableOptions setValue:createNSArray(m_protocolVersions, [] (int version) -> NSNumber * {
                return version ? @(version) : nil;
            }).get() forKey:AVContentKeyRequestProtocolVersionsKey];
            options = WTFMove(mutableOptions);
        }

        errorCode = MediaPlayer::NoError;
        systemCode = 0;
        RetainPtr<id> nsIdentifier;
        if (m_identifier)
            nsIdentifier = toNSData(m_identifier->span());
        else
            nsIdentifier = contentKeyRequest.get().identifier;

        RetainPtr<NSError> error;
        RetainPtr<NSData> requestData;
        Semaphore keyRequestCompleted { 0 };
        [contentKeyRequest makeStreamingContentKeyRequestDataForApp:certificateData.get() contentIdentifier:nsIdentifier.get() options:options.get() completionHandler:[&] (NSData *outRequestData, NSError *outError) {
            error = outError;
            requestData = outRequestData;
            keyRequestCompleted.signal();
        }];
        keyRequestCompleted.wait();

        if (error) {
            errorCode = LegacyCDM::DomainError;
            systemCode = mediaKeyErrorSystemCode(error.get());
            ERROR_LOG(LOGIDENTIFIER, "error: ", error.get());
            return false;
        }

        ALWAYS_LOG(LOGIDENTIFIER, "generated key request");
        nextMessage = Uint8Array::tryCreate(span(requestData.get()));
        return false;
    }

    ALWAYS_LOG(LOGIDENTIFIER, "processing key response");
    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    RetainPtr keyData = toNSData(key->span());
    
    if ([contentKeyRequest respondsToSelector:@selector(processContentKeyResponse:)] && [PAL::getAVContentKeyResponseClassSingleton() respondsToSelector:@selector(contentKeyResponseWithFairPlayStreamingKeyResponseData:)])
        [contentKeyRequest processContentKeyResponse:[PAL::getAVContentKeyResponseClassSingleton() contentKeyResponseWithFairPlayStreamingKeyResponseData:keyData.get()]];
    else
        [contentKeyRequest processContentKeyResponseData:keyData.get()];

    return true;
}

RefPtr<ArrayBuffer> CDMSessionAVContentKeySession::cachedKeyForKeyID(const String&) const
{
    return nullptr;
}

bool CDMSessionAVContentKeySession::isAnyKeyUsable(const Keys& keys) const
{
    auto requestKeys = CDMPrivateFairPlayStreaming::keyIDsForRequest(contentKeyRequest().get());
    for (auto& requestKey : requestKeys) {
        if (keys.containsIf([&](auto& key) { return arePointingToEqualData(key, requestKey); }))
            return true;
    }
    return false;
}

void CDMSessionAVContentKeySession::attachContentKeyToSample(const MediaSampleAVFObjC& sample)
{
    AVContentKey *contentKey = [contentKeyRequest() contentKey];
    ASSERT(contentKey);

    NSError *error = nil;
    if (!AVSampleBufferAttachContentKey(sample.platformSample().cmSampleBuffer(), contentKey, &error))
        ERROR_LOG(LOGIDENTIFIER, "Failed to attach content key with error: %{public}@", error);
}

RefPtr<Uint8Array> CDMSessionAVContentKeySession::generateKeyReleaseMessage(unsigned short& errorCode, uint32_t& systemCode)
{
    ASSERT(m_mode == KeyRelease);
    RetainPtr certificateData = toNSData(m_certificate->span());

    String storagePath = this->storagePath();
    if (storagePath.isEmpty() || ![PAL::getAVContentKeySessionClassSingleton() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)]) {
        errorCode = MediaPlayer::KeySystemNotSupported;
        systemCode = '!mor';
        return nullptr;
    }

    NSArray* expiredSessions = [PAL::getAVContentKeySessionClassSingleton() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath.createNSString().get()]];
    if (![expiredSessions count]) {
        ALWAYS_LOG(LOGIDENTIFIER, "no expired sessions found");

        errorCode = MediaPlayer::KeySystemNotSupported;
        systemCode = '!mor';
        return nullptr;
    }

    ALWAYS_LOG(LOGIDENTIFIER, "found ", [expiredSessions count], " expired sessions");

    errorCode = 0;
    systemCode = 0;
    m_expiredSession = [expiredSessions firstObject];
    return Uint8Array::tryCreate(span(m_expiredSession.get()));
}

bool CDMSessionAVContentKeySession::hasContentKeyRequest() const
{
    Locker holder { m_keyRequestLock };
    return !!m_keyRequest;
}

RetainPtr<AVContentKeyRequest> CDMSessionAVContentKeySession::contentKeyRequest() const
{
    Locker holder { m_keyRequestLock };
    return RetainPtr { m_keyRequest.get() };
}

void CDMSessionAVContentKeySession::didProvideContentKeyRequest(AVContentKeyRequest *keyRequest)
{
    Locker holder { m_keyRequestLock };
    m_keyRequest = keyRequest;
    m_hasKeyRequestSemaphore.signal();
}

RetainPtr<AVContentKeySession> CDMSessionAVContentKeySession::createContentKeySession(NSURL *storageURL)
{
    if ([PAL::getAVContentKeySessionClassSingleton() respondsToSelector:@selector(contentKeySessionWithKeySystem:storageDirectoryAtURL:)])
        return [PAL::getAVContentKeySessionClassSingleton() contentKeySessionWithKeySystem:AVContentKeySystemFairPlayStreaming storageDirectoryAtURL:storageURL];
    return adoptNS([PAL::allocAVContentKeySessionInstance() initWithStorageDirectoryAtURL:storageURL]);
}

AVContentKeySession* CDMSessionAVContentKeySession::contentKeySession()
{
    if (m_contentKeySession)
        return m_contentKeySession.get();

    if (!PAL::canLoad_AVFoundation_AVContentKeySystemFairPlayStreaming())
        return nil;

    String storagePath = this->storagePath();
    NSURL *storageURL = nil;
    if (!storagePath.isEmpty()) {
        String storageDirectory = FileSystem::parentPath(storagePath);

        if (!FileSystem::fileExists(storageDirectory)) {
            if (!FileSystem::makeAllDirectories(storageDirectory))
                return nil;
        }

        storageURL = [NSURL fileURLWithPath:storagePath.createNSString().get()];
    }

    lazyInitialize(m_contentKeySession, createContentKeySession(storageURL));

    [m_contentKeySession setDelegate:m_contentKeySessionDelegate.get() queue:m_delegateQueue->dispatchQueue()];
    return m_contentKeySession.get();
}

}

#endif
