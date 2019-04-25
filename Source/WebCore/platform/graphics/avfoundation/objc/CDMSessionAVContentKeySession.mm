/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "SourceBufferPrivateAVFObjC.h"
#import "WebCoreNSErrorExtras.h"
#import <AVFoundation/AVError.h>
#import <CoreMedia/CMBase.h>
#import <JavaScriptCore/TypedArrayInlines.h>
#import <objc/objc-runtime.h>
#import <pal/spi/mac/AVFoundationSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVStreamDataParser);
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVContentKeySession);
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVContentKeyResponse);
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVContentKeyRequestProtocolVersionsKey, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVContentKeySystemFairPlayStreaming, NSString *)

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
    m_parent->setSessionId(sessionIdentifierString.get());
}
@end

static const NSString *PlaybackSessionIdKey = @"PlaybackSessionID";

namespace WebCore {

CDMSessionAVContentKeySession::CDMSessionAVContentKeySession(Vector<int>&& protocolVersions, int cdmVersion, CDMPrivateMediaSourceAVFObjC& cdm, LegacyCDMSessionClient* client)
    : CDMSessionMediaSourceAVFObjC(cdm, client)
    , m_contentKeySessionDelegate(adoptNS([[WebCDMSessionAVContentKeySessionDelegate alloc] initWithParent:this]))
    , m_protocolVersions(WTFMove(protocolVersions))
    , m_cdmVersion(cdmVersion)
    , m_mode(Normal)
{
}

CDMSessionAVContentKeySession::~CDMSessionAVContentKeySession()
{
    [m_contentKeySessionDelegate invalidate];

    for (auto& sourceBuffer : m_sourceBuffers)
        removeParser(sourceBuffer->parser());
}

bool CDMSessionAVContentKeySession::isAvailable()
{
    return getAVContentKeySessionClass();
}

RefPtr<Uint8Array> CDMSessionAVContentKeySession::generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode)
{
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(destinationURL);
    ASSERT(initData);

    LOG(Media, "CDMSessionAVContentKeySession::generateKeyRequest(%p)", this);

    errorCode = MediaPlayer::NoError;
    systemCode = 0;

    if (equalLettersIgnoringASCIICase(mimeType, "keyrelease")) {
        m_mode = KeyRelease;
        m_certificate = initData;
        return generateKeyReleaseMessage(errorCode, systemCode);
    }

    if (m_cdmVersion == 2)
        m_identifier = initData;
    else
        m_initData = initData;

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

        LOG(Media, "CDMSessionAVContentKeySession::releaseKeys(%p) - expiring stream session", this);
        [contentKeySession() expire];

        if (!m_certificate)
            return;

        if (![getAVContentKeySessionClass() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)])
            return;

        auto storagePath = this->storagePath();
        if (storagePath.isEmpty())
            return;

        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);
        NSArray* expiredSessions = [getAVContentKeySessionClass() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]];
        for (NSData* expiredSessionData in expiredSessions) {
            NSDictionary *expiredSession = [NSPropertyListSerialization propertyListWithData:expiredSessionData options:kCFPropertyListImmutable format:nullptr error:nullptr];
            NSString *playbackSessionIdValue = (NSString *)[expiredSession objectForKey:PlaybackSessionIdKey];
            if (![playbackSessionIdValue isKindOfClass:[NSString class]])
                continue;

            if (m_sessionId == String(playbackSessionIdValue)) {
                LOG(Media, "CDMSessionAVContentKeySession::releaseKeys(%p) - found session, sending expiration message");
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
        LOG(Media, "CDMSessionAVContentKeySession::update(%p) - acknowleding secure stop message", this);

        String storagePath = this->storagePath();
        if (!m_expiredSession || storagePath.isEmpty()) {
            errorCode = MediaPlayer::InvalidPlayerState;
            return false;
        }

        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

        if ([getAVContentKeySessionClass() respondsToSelector:@selector(removePendingExpiredSessionReports:withAppIdentifier:storageDirectoryAtURL:)])
            [getAVContentKeySessionClass() removePendingExpiredSessionReports:@[m_expiredSession.get()] withAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]];
        m_expiredSession = nullptr;
        return true;
    }

    if (m_stopped) {
        errorCode = MediaPlayer::InvalidPlayerState;
        return false;
    }

    bool shouldGenerateKeyRequest = !m_certificate || isEqual(key, "renew");
    if (!m_certificate) {
        LOG(Media, "CDMSessionAVContentKeySession::update(%p) - certificate data", this);

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
            if (sourceBuffer->protectedTrackID() != -1) {
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

    if (!m_keyRequest) {
        NSData* nsInitData = m_initData ? [NSData dataWithBytes:m_initData->data() length:m_initData->length()] : nil;
        NSData* nsIdentifier = m_identifier ? [NSData dataWithBytes:m_identifier->data() length:m_identifier->length()] : nil;
        if ([contentKeySession() respondsToSelector:@selector(processContentKeyRequestWithIdentifier:initializationData:options:)])
            [contentKeySession() processContentKeyRequestWithIdentifier:nsIdentifier initializationData:nsInitData options:nil];
        else
            [contentKeySession() processContentKeyRequestInitializationData:nsInitData options:nil];
    }

    if (shouldGenerateKeyRequest) {
        ASSERT(m_keyRequest);
        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

        RetainPtr<NSDictionary> options;
        if (!m_protocolVersions.isEmpty() && canLoadAVContentKeyRequestProtocolVersionsKey()) {
            RetainPtr<NSMutableArray> protocolVersionsOption = adoptNS([[NSMutableArray alloc] init]);
            for (auto& version : m_protocolVersions) {
                if (!version)
                    continue;
                [protocolVersionsOption addObject:@(version)];
            }

            options = @{ getAVContentKeyRequestProtocolVersionsKey(): protocolVersionsOption.get() };
        }

        errorCode = MediaPlayer::NoError;
        systemCode = 0;
        NSError* error = nil;
        NSData* nsIdentifier = m_identifier ? [NSData dataWithBytes:m_identifier->data() length:m_identifier->length()] : m_keyRequest.get().identifier;

        NSData* requestData = [m_keyRequest contentKeyRequestDataForApp:certificateData.get() contentIdentifier:nsIdentifier options:options.get() error:&error];
        if (error) {
            errorCode = LegacyCDM::DomainError;
            systemCode = mediaKeyErrorSystemCode(error);
            return false;
        }

        nextMessage = Uint8Array::tryCreate(static_cast<const uint8_t*>([requestData bytes]), [requestData length]);
        return false;
    }

    LOG(Media, "CDMSessionAVContentKeySession::update(%p) - key data", this);
    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    RetainPtr<NSData> keyData = adoptNS([[NSData alloc] initWithBytes:key->data() length:key->length()]);
    
    if ([m_keyRequest respondsToSelector:@selector(processContentKeyResponse:)] && [getAVContentKeyResponseClass() respondsToSelector:@selector(contentKeyResponseWithFairPlayStreamingKeyResponseData:)])
        [m_keyRequest processContentKeyResponse:[getAVContentKeyResponseClass() contentKeyResponseWithFairPlayStreamingKeyResponseData:keyData.get()]];
    else
        [m_keyRequest processContentKeyResponseData:keyData.get()];

    return true;
}

void CDMSessionAVContentKeySession::addParser(AVStreamDataParser* parser)
{
    if ([contentKeySession() respondsToSelector:@selector(addContentKeyRecipient:)])
        [contentKeySession() addContentKeyRecipient:parser];
    else
        [contentKeySession() addStreamDataParser:parser];
}

void CDMSessionAVContentKeySession::removeParser(AVStreamDataParser* parser)
{
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
    if (storagePath.isEmpty() || ![getAVContentKeySessionClass() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)]) {
        errorCode = MediaPlayer::KeySystemNotSupported;
        systemCode = '!mor';
        return nullptr;
    }

    NSArray* expiredSessions = [getAVContentKeySessionClass() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]];
    if (![expiredSessions count]) {
        LOG(Media, "CDMSessionAVContentKeySession::generateKeyReleaseMessage(%p) - no expired sessions found", this);

        errorCode = MediaPlayer::KeySystemNotSupported;
        systemCode = '!mor';
        return nullptr;
    }

    LOG(Media, "CDMSessionAVContentKeySession::generateKeyReleaseMessage(%p) - found %d expired sessions", this, [expiredSessions count]);

    errorCode = 0;
    systemCode = 0;
    m_expiredSession = [expiredSessions firstObject];
    return Uint8Array::tryCreate(static_cast<const uint8_t*>([m_expiredSession bytes]), [m_expiredSession length]);
}

void CDMSessionAVContentKeySession::didProvideContentKeyRequest(AVContentKeyRequest *keyRequest)
{
    m_keyRequest = keyRequest;
}

AVContentKeySession* CDMSessionAVContentKeySession::contentKeySession()
{
    if (m_contentKeySession)
        return m_contentKeySession.get();

    String storagePath = this->storagePath();
    if (storagePath.isEmpty()) {
        if (![getAVContentKeySessionClass() respondsToSelector:@selector(contentKeySessionWithKeySystem:)] || !canLoadAVContentKeySystemFairPlayStreaming())
            return nil;

        m_contentKeySession = [getAVContentKeySessionClass() contentKeySessionWithKeySystem:getAVContentKeySystemFairPlayStreaming()];
    } else {
        String storageDirectory = FileSystem::directoryName(storagePath);

        if (!FileSystem::fileExists(storageDirectory)) {
            if (!FileSystem::makeAllDirectories(storageDirectory))
                return nil;
        }

        auto url = [NSURL fileURLWithPath:storagePath];
        if ([getAVContentKeySessionClass() respondsToSelector:@selector(contentKeySessionWithKeySystem:storageDirectoryAtURL:)] && canLoadAVContentKeySystemFairPlayStreaming())
            m_contentKeySession = [getAVContentKeySessionClass() contentKeySessionWithKeySystem:getAVContentKeySystemFairPlayStreaming() storageDirectoryAtURL:url];
        else
            m_contentKeySession = adoptNS([allocAVContentKeySessionInstance() initWithStorageDirectoryAtURL:url]);
    }

    m_contentKeySession.get().delegate = m_contentKeySessionDelegate.get();
    return m_contentKeySession.get();
}

}

#endif
