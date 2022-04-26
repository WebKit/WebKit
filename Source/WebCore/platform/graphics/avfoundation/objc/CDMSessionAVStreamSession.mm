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
#import "CDMSessionAVStreamSession.h"

#if HAVE(AVSTREAMSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)

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
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/LoggerHelper.h>
#import <wtf/UUID.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVStreamSession : NSObject
- (void)addStreamDataParser:(AVStreamDataParser *)streamDataParser;
- (void)removeStreamDataParser:(AVStreamDataParser *)streamDataParser;
- (void)expire;
- (NSData *)contentProtectionSessionIdentifier;
+ (NSArray *)pendingExpiredSessionReportsWithAppIdentifier:(NSData *)appIdentifier storageDirectoryAtURL:(NSURL *)storageURL;
+ (void)removePendingExpiredSessionReports:(NSArray *)expiredSessionReports withAppIdentifier:(NSData *)appIdentifier storageDirectoryAtURL:(NSURL *)storageURL;
@end

@interface WebCDMSessionAVStreamSessionObserver : NSObject {
    WebCore::CDMSessionAVStreamSession *m_parent;
}
@end

@implementation WebCDMSessionAVStreamSessionObserver
- (id)initWithParent:(WebCore::CDMSessionAVStreamSession *)parent
{
    if ((self = [super init]))
        m_parent = parent;
    return self;
}

- (void)contentProtectionSessionIdentifierChanged:(NSNotification *)notification
{
    AVStreamSession* streamSession = (AVStreamSession*)[notification object];

    NSData* identifier = [streamSession contentProtectionSessionIdentifier];
    RetainPtr<NSString> sessionIdentifierString = identifier ? adoptNS([[NSString alloc] initWithData:identifier encoding:NSUTF8StringEncoding]) : nil;

    if (m_parent)
        m_parent->setSessionId(sessionIdentifierString.get());
}
@end

namespace WebCore {

CDMSessionAVStreamSession::CDMSessionAVStreamSession(Vector<int>&& protocolVersions, CDMPrivateMediaSourceAVFObjC& cdm, LegacyCDMSessionClient& client)
    : CDMSessionMediaSourceAVFObjC(cdm, client)
    , m_dataParserObserver(adoptNS([[WebCDMSessionAVStreamSessionObserver alloc] initWithParent:this]))
    , m_protocolVersions(WTFMove(protocolVersions))
    , m_mode(Normal)
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

CDMSessionAVStreamSession::~CDMSessionAVStreamSession()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    setStreamSession(nullptr);

    for (auto& sourceBuffer : m_sourceBuffers)
        removeParser(sourceBuffer->streamDataParser());
}

RefPtr<Uint8Array> CDMSessionAVStreamSession::generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode)
{
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(destinationURL);
    ASSERT(initData);

    ALWAYS_LOG(LOGIDENTIFIER);

    errorCode = MediaPlayer::NoError;
    systemCode = 0;

    m_initData = initData;

    if (equalLettersIgnoringASCIICase(mimeType, "keyrelease"_s)) {
        m_mode = KeyRelease;
        return generateKeyReleaseMessage(errorCode, systemCode);
    }

    String certificateString("certificate"_s);
    auto array = Uint8Array::create(certificateString.length());
    for (unsigned i = 0, length = certificateString.length(); i < length; ++i)
        array->set(i, certificateString[i]);
    return WTFMove(array);
}

void CDMSessionAVStreamSession::releaseKeys()
{
    if (m_streamSession) {
        m_stopped = true;
        for (auto& sourceBuffer : m_sourceBuffers)
            sourceBuffer->flush();

        ALWAYS_LOG(LOGIDENTIFIER, "expiring stream session");
        [m_streamSession expire];

        if (!m_certificate)
            return;

        String storagePath = this->storagePath();
        if (storagePath.isEmpty() || ![PAL::getAVStreamSessionClass() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)])
            return;

        // FIXME: This code is repeated in three places.
        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);
        NSArray* expiredSessions = [PAL::getAVStreamSessionClass() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]];
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

static bool isEqual2(Uint8Array* data, const char* literal)
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

bool CDMSessionAVStreamSession::update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode)
{
    bool shouldGenerateKeyRequest = !m_certificate || isEqual2(key, "renew");
    if (!m_certificate) {
        ALWAYS_LOG(LOGIDENTIFIER, "certificate data");

        m_certificate = key;
    }

    if (isEqual2(key, "acknowledged")) {
        ALWAYS_LOG(LOGIDENTIFIER, "acknowleding secure stop message");

        if (!m_expiredSession) {
            errorCode = MediaPlayer::InvalidPlayerState;
            return false;
        }

        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

        IGNORE_WARNINGS_BEGIN("objc-literal-conversion")
        String storagePath = this->storagePath();
        if (!storagePath.isEmpty() && [PAL::getAVStreamSessionClass() respondsToSelector:@selector(removePendingExpiredSessionReports:withAppIdentifier:storageDirectoryAtURL:)])
            [PAL::getAVStreamSessionClass() removePendingExpiredSessionReports:@[m_expiredSession.get()] withAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]];
        IGNORE_WARNINGS_END
        m_expiredSession = nullptr;
        return true;
    }

    if (m_mode == KeyRelease)
        return false;

    RefPtr<SourceBufferPrivateAVFObjC> protectedSourceBuffer;
    for (auto& sourceBuffer : m_sourceBuffers) {
        if (sourceBuffer->protectedTrackID() != notFound) {
            protectedSourceBuffer = sourceBuffer;
            break;
        }
    }

    if (shouldGenerateKeyRequest) {
        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

        if (m_sourceBuffers.isEmpty())
            return true;

        if (!protectedSourceBuffer)
            return true;

        RetainPtr<NSData> initData = adoptNS([[NSData alloc] initWithBytes:m_initData->data() length:m_initData->length()]);

        RetainPtr<NSDictionary> options;
        if (!m_protocolVersions.isEmpty()) {
            options = @{ PAL::get_AVFoundation_AVStreamDataParserContentKeyRequestProtocolVersionsKey(): createNSArray(m_protocolVersions, [] (int version) -> NSNumber * {
                return version ? @(version) : nil;
            }).get() };
        }

        NSError* error = nil;
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        RetainPtr<NSData> request = [protectedSourceBuffer->streamDataParser() streamingContentKeyRequestDataForApp:certificateData.get() contentIdentifier:initData.get() trackID:protectedSourceBuffer->protectedTrackID() options:options.get() error:&error];
        ALLOW_DEPRECATED_DECLARATIONS_END

        if (![protectedSourceBuffer->streamDataParser() respondsToSelector:@selector(contentProtectionSessionIdentifier)])
            m_sessionId = createVersion4UUIDString();

        if (error) {
            ERROR_LOG(LOGIDENTIFIER, "error generating key request: ", String(error.description));
            errorCode = MediaPlayer::InvalidPlayerState;
            systemCode = mediaKeyErrorSystemCode(error);
            return false;
        }

        ALWAYS_LOG(LOGIDENTIFIER, "generated key request");
        nextMessage = Uint8Array::create([request length]);
        [request getBytes:nextMessage->data() length:nextMessage->length()];
        return false;
    }

    if (!protectedSourceBuffer) {
        ERROR_LOG(LOGIDENTIFIER, "error: !protectedSourceBuffer");
        errorCode = MediaPlayer::InvalidPlayerState;
        return false;
    }

    ASSERT(!m_sourceBuffers.isEmpty());
    ALWAYS_LOG(LOGIDENTIFIER, "processing key response");
    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    RetainPtr<NSData> keyData = adoptNS([[NSData alloc] initWithBytes:key->data() length:key->length()]);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [protectedSourceBuffer->streamDataParser() processContentKeyResponseData:keyData.get() forTrackID:protectedSourceBuffer->protectedTrackID()];
    ALLOW_DEPRECATED_DECLARATIONS_END

    return true;
}

RefPtr<ArrayBuffer> CDMSessionAVStreamSession::cachedKeyForKeyID(const String&) const
{
    return nullptr;
}

void CDMSessionAVStreamSession::setStreamSession(AVStreamSession *streamSession)
{
    if (m_streamSession)
        [[NSNotificationCenter defaultCenter] removeObserver:m_dataParserObserver.get() name:AVStreamSessionContentProtectionSessionIdentifierChangedNotification object:m_streamSession.get()];

    m_streamSession = streamSession;

    if (!m_streamSession)
        return;

    [[NSNotificationCenter defaultCenter] addObserver:m_dataParserObserver.get() selector:@selector(contentProtectionSessionIdentifierChanged:) name:AVStreamSessionContentProtectionSessionIdentifierChangedNotification object:m_streamSession.get()];

    NSData* identifier = [streamSession contentProtectionSessionIdentifier];
    RetainPtr<NSString> sessionIdentifierString = identifier ? adoptNS([[NSString alloc] initWithData:identifier encoding:(NSUTF8StringEncoding)]) : nil;
    setSessionId(sessionIdentifierString.get());
}

void CDMSessionAVStreamSession::addParser(AVStreamDataParser* parser)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_streamSession)
        [m_streamSession addStreamDataParser:parser];
}

void CDMSessionAVStreamSession::removeParser(AVStreamDataParser* parser)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_streamSession)
        [m_streamSession removeStreamDataParser:parser];
}

RefPtr<Uint8Array> CDMSessionAVStreamSession::generateKeyReleaseMessage(unsigned short& errorCode, uint32_t& systemCode)
{
    ASSERT(m_mode == KeyRelease);
    m_certificate = m_initData;
    RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

    String storagePath = this->storagePath();
    if (storagePath.isEmpty() || ![PAL::getAVStreamSessionClass() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)]) {
        errorCode = MediaPlayer::KeySystemNotSupported;
        systemCode = '!mor';
        return nullptr;
    }

    NSArray* expiredSessions = [PAL::getAVStreamSessionClass() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]];
    if (![expiredSessions count]) {
        ALWAYS_LOG("no expired sessions found");

        errorCode = MediaPlayer::KeySystemNotSupported;
        systemCode = '!mor';
        return nullptr;
    }

    ALWAYS_LOG("found ", [expiredSessions count], " expired sessions");

    errorCode = 0;
    systemCode = 0;
    m_expiredSession = [expiredSessions firstObject];
    return Uint8Array::tryCreate(static_cast<const uint8_t*>([m_expiredSession bytes]), [m_expiredSession length]);
}

}

#endif
