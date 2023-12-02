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

#import "CDMPrivateMediaSourceAVFObjC.h"
#import "LegacyCDM.h"
#import "Logging.h"
#import "MediaPlayer.h"
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
#import <wtf/WorkQueue.h>
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
    WebCore::CDMSessionAVContentKeySession *m_parent;
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

    if (m_parent)
        m_parent->didProvideContentKeyRequest(keyRequest);
}

- (void)contentKeySessionContentProtectionSessionIdentifierDidChange:(AVContentKeySession *)session
{
    if (!m_parent)
        return;

    NSData* identifier = [session contentProtectionSessionIdentifier];
    RetainPtr<NSString> sessionIdentifierString = identifier ? adoptNS([[NSString alloc] initWithData:identifier encoding:NSUTF8StringEncoding]) : nil;
    callOnMainThread([self, protectedSelf = RetainPtr { self }, sessionIdentifierString = WTFMove(sessionIdentifierString)] {
        m_parent->setSessionId(sessionIdentifierString.get());
    });
}
@end

namespace WebCore {

constexpr Seconds kDidProvideContentKeyRequestTimeout { 5_s };

CDMSessionAVContentKeySession::CDMSessionAVContentKeySession(Vector<int>&& protocolVersions, int cdmVersion, CDMPrivateMediaSourceAVFObjC& cdm, LegacyCDMSessionClient& client)
    : CDMSessionMediaSourceAVFObjC(cdm, client)
    , m_contentKeySessionDelegate(adoptNS([[WebCDMSessionAVContentKeySessionDelegate alloc] initWithParent:this]))
    , m_delegateQueue(WorkQueue::create("CDMSessionAVContentKeySession delegate queue"))
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
        for (auto& sourceBuffer : m_sourceBuffers) {
            sourceBuffer->flush();
            removeParser(sourceBuffer->streamDataParser());
        }

        [contentKeySession() expire];
    }
}

bool CDMSessionAVContentKeySession::isAvailable()
{
    return PAL::getAVContentKeySessionClass();
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
        m_initData = SharedBuffer::create(initData->data(), initData->length());

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

        if (![PAL::getAVContentKeySessionClass() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)])
            return;

        auto storagePath = this->storagePath();
        if (storagePath.isEmpty())
            return;

        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);
        NSArray* expiredSessions = [PAL::getAVContentKeySessionClass() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]];
        for (NSData* expiredSessionData in expiredSessions) {
            static const NSString *PlaybackSessionIdKey = @"PlaybackSessionID";
            NSDictionary *expiredSession = [NSPropertyListSerialization propertyListWithData:expiredSessionData options:kCFPropertyListImmutable format:nullptr error:nullptr];
            NSString *playbackSessionIdValue = (NSString *)[expiredSession objectForKey:PlaybackSessionIdKey];
            if (![playbackSessionIdValue isKindOfClass:[NSString class]])
                continue;

            if (m_sessionId == String(playbackSessionIdValue)) {
                ALWAYS_LOG(LOGIDENTIFIER, "found session, sending expiration message");
                m_expiredSession = expiredSessionData;
                m_client->sendMessage(Uint8Array::create(static_cast<const uint8_t*>([m_expiredSession bytes]), [m_expiredSession length]).ptr(), emptyString());
                break;
            }
        }
    }
}

static bool isEqual(Uint8Array* data, const char* literal)
{
    ASSERT(data);
    ASSERT(literal);
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

    if (isEqual(key, "acknowledged")) {
        ALWAYS_LOG(LOGIDENTIFIER, "acknowleding secure stop message");

        String storagePath = this->storagePath();
        if (!m_expiredSession || storagePath.isEmpty()) {
            errorCode = MediaPlayer::InvalidPlayerState;
            return false;
        }

        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

        if ([PAL::getAVContentKeySessionClass() respondsToSelector:@selector(removePendingExpiredSessionReports:withAppIdentifier:storageDirectoryAtURL:)])
            [PAL::getAVContentKeySessionClass() removePendingExpiredSessionReports:@[m_expiredSession.get()] withAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]];
        m_expiredSession = nullptr;
        return true;
    }

    if (m_stopped) {
        errorCode = MediaPlayer::InvalidPlayerState;
        return false;
    }

    bool shouldGenerateKeyRequest = !m_certificate || isEqual(key, "renew");
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
        NSData* nsIdentifier = m_identifier ? [NSData dataWithBytes:m_identifier->data() length:m_identifier->length()] : nil;
        if ([contentKeySession() respondsToSelector:@selector(processContentKeyRequestWithIdentifier:initializationData:options:)])
            [contentKeySession() processContentKeyRequestWithIdentifier:nsIdentifier initializationData:nsInitData.get() options:nil];
        else
            [contentKeySession() processContentKeyRequestInitializationData:nsInitData.get() options:nil];
        m_hasKeyRequestSemaphore.waitFor(kDidProvideContentKeyRequestTimeout);
    }

    auto contentKeyRequest = this->contentKeyRequest();

    if (shouldGenerateKeyRequest) {
        ASSERT(contentKeyRequest);
        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

        RetainPtr<NSDictionary> options;
        if (!m_protocolVersions.isEmpty() && PAL::canLoad_AVFoundation_AVContentKeyRequestProtocolVersionsKey()) {
            options = @{ AVContentKeyRequestProtocolVersionsKey: createNSArray(m_protocolVersions, [] (int version) -> NSNumber * {
                return version ? @(version) : nil;
            }).get() };
        }

        errorCode = MediaPlayer::NoError;
        systemCode = 0;
        NSError* error = nil;
        NSData* nsIdentifier = m_identifier ? [NSData dataWithBytes:m_identifier->data() length:m_identifier->length()] : contentKeyRequest.get().identifier;

        NSData* requestData = [contentKeyRequest contentKeyRequestDataForApp:certificateData.get() contentIdentifier:nsIdentifier options:options.get() error:&error];
        if (error) {
            errorCode = LegacyCDM::DomainError;
            systemCode = mediaKeyErrorSystemCode(error);
            ERROR_LOG(LOGIDENTIFIER, "error: ", error);
            return false;
        }

        ALWAYS_LOG(LOGIDENTIFIER, "generated key request");
        nextMessage = Uint8Array::tryCreate(static_cast<const uint8_t*>([requestData bytes]), [requestData length]);
        return false;
    }

    ALWAYS_LOG(LOGIDENTIFIER, "processing key response");
    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    RetainPtr<NSData> keyData = adoptNS([[NSData alloc] initWithBytes:key->data() length:key->length()]);
    
    if ([contentKeyRequest respondsToSelector:@selector(processContentKeyResponse:)] && [PAL::getAVContentKeyResponseClass() respondsToSelector:@selector(contentKeyResponseWithFairPlayStreamingKeyResponseData:)])
        [contentKeyRequest processContentKeyResponse:[PAL::getAVContentKeyResponseClass() contentKeyResponseWithFairPlayStreamingKeyResponseData:keyData.get()]];
    else
        [contentKeyRequest processContentKeyResponseData:keyData.get()];

    return true;
}

RefPtr<ArrayBuffer> CDMSessionAVContentKeySession::cachedKeyForKeyID(const String&) const
{
    return nullptr;
}

void CDMSessionAVContentKeySession::addParser(AVStreamDataParser* parser)
{
    INFO_LOG(LOGIDENTIFIER);
    if ([contentKeySession() respondsToSelector:@selector(addContentKeyRecipient:)])
        [contentKeySession() addContentKeyRecipient:parser];
    else
        [contentKeySession() addStreamDataParser:parser];
}

void CDMSessionAVContentKeySession::removeParser(AVStreamDataParser* parser)
{
    INFO_LOG(LOGIDENTIFIER);
    if ([contentKeySession() respondsToSelector:@selector(removeContentKeyRecipient:)])
        [contentKeySession() removeContentKeyRecipient:parser];
    else
        [contentKeySession() removeStreamDataParser:parser];
}

RefPtr<Uint8Array> CDMSessionAVContentKeySession::generateKeyReleaseMessage(unsigned short& errorCode, uint32_t& systemCode)
{
    ASSERT(m_mode == KeyRelease);
    RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

    String storagePath = this->storagePath();
    if (storagePath.isEmpty() || ![PAL::getAVContentKeySessionClass() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)]) {
        errorCode = MediaPlayer::KeySystemNotSupported;
        systemCode = '!mor';
        return nullptr;
    }

    NSArray* expiredSessions = [PAL::getAVContentKeySessionClass() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]];
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
    return Uint8Array::tryCreate(static_cast<const uint8_t*>([m_expiredSession bytes]), [m_expiredSession length]);
}

bool CDMSessionAVContentKeySession::hasContentKeyRequest() const
{
    LockHolder holder { m_keyRequestLock };
    return m_keyRequest;
}

RetainPtr<AVContentKeyRequest> CDMSessionAVContentKeySession::contentKeyRequest()
{
    LockHolder holder { m_keyRequestLock };
    return RetainPtr { m_keyRequest.get() };
}

void CDMSessionAVContentKeySession::didProvideContentKeyRequest(AVContentKeyRequest *keyRequest)
{
    LockHolder holder { m_keyRequestLock };
    m_keyRequest = keyRequest;
    m_hasKeyRequestSemaphore.signal();
}

AVContentKeySession* CDMSessionAVContentKeySession::contentKeySession()
{
    if (m_contentKeySession)
        return m_contentKeySession.get();

    if (!PAL::canLoad_AVFoundation_AVContentKeySystemFairPlayStreaming())
        return nil;

    String storagePath = this->storagePath();
    NSURL* storageURL = nil;
    if (!storagePath.isEmpty()) {
        String storageDirectory = FileSystem::parentPath(storagePath);

        if (!FileSystem::fileExists(storageDirectory)) {
            if (!FileSystem::makeAllDirectories(storageDirectory))
                return nil;
        }

        storageURL = [NSURL fileURLWithPath:storagePath];
    }

#if HAVE(AVCONTENTKEYREQUEST_COMPATABILITIY_MODE) && HAVE(AVCONTENTKEYSPECIFIER)
    if (!MediaSessionManagerCocoa::sampleBufferContentKeySessionSupportEnabled() && [PAL::getAVContentKeySessionClass() respondsToSelector:@selector(contentKeySessionWithLegacyWebKitCompatibilityModeAndKeySystem:storageDirectoryAtURL:)]) {
        m_contentKeySession = [PAL::getAVContentKeySessionClass() contentKeySessionWithLegacyWebKitCompatibilityModeAndKeySystem:AVContentKeySystemFairPlayStreaming storageDirectoryAtURL:storageURL];
    } else
#endif
    if ([PAL::getAVContentKeySessionClass() respondsToSelector:@selector(contentKeySessionWithKeySystem:storageDirectoryAtURL:)])
        m_contentKeySession = [PAL::getAVContentKeySessionClass() contentKeySessionWithKeySystem:AVContentKeySystemFairPlayStreaming storageDirectoryAtURL:storageURL];
    else
        m_contentKeySession = adoptNS([PAL::allocAVContentKeySessionInstance() initWithStorageDirectoryAtURL:storageURL]);

#if HAVE(AVCONTENTKEYREQUEST_COMPATABILITIY_MODE) && HAVE(AVCONTENTKEYSPECIFIER)
    if (MediaSessionManagerCocoa::sampleBufferContentKeySessionSupportEnabled())
        [m_contentKeySession setDelegate:m_contentKeySessionDelegate.get() queue:m_delegateQueue->dispatchQueue()];
    else
#endif
        m_contentKeySession.get().delegate = m_contentKeySessionDelegate.get();
    return m_contentKeySession.get();
}

}

#endif
