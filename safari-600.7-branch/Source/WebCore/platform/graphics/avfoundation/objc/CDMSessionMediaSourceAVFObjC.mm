/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "CDMSessionMediaSourceAVFObjC.h"

#if ENABLE(ENCRYPTED_MEDIA_V2) && ENABLE(MEDIA_SOURCE)

#import "CDM.h"
#import "CDMSession.h"
#import "ExceptionCode.h"
#import "FileSystem.h"
#import "Logging.h"
#import "MediaPlayer.h"
#import "SourceBufferPrivateAVFObjC.h"
#import "SoftLinking.h"
#import "UUID.h"
#import <AVFoundation/AVError.h>
#import <CoreMedia/CMBase.h>
#import <objc/objc-runtime.h>
#import <wtf/NeverDestroyed.h>
#import <runtime/TypedArrayInlines.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVStreamDataParser);
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVStreamSession);
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVStreamDataParserContentKeyRequestProtocolVersionsKey, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVStreamSessionContentProtectionSessionIdentifierChangedNotification, NSString *)

@interface AVStreamDataParser : NSObject
- (void)processContentKeyResponseData:(NSData *)contentKeyResponseData forTrackID:(CMPersistentTrackID)trackID;
- (void)processContentKeyResponseError:(NSError *)error forTrackID:(CMPersistentTrackID)trackID;
- (void)renewExpiringContentKeyResponseDataForTrackID:(CMPersistentTrackID)trackID;
- (NSData *)streamingContentKeyRequestDataForApp:(NSData *)appIdentifier contentIdentifier:(NSData *)contentIdentifier trackID:(CMPersistentTrackID)trackID options:(NSDictionary *)options error:(NSError **)outError;
@end

@interface AVStreamSession : NSObject
- (void)addStreamDataParser:(AVStreamDataParser *)streamDataParser;
- (void)removeStreamDataParser:(AVStreamDataParser *)streamDataParser;
- (void)expire;
- (NSData *)contentProtectionSessionIdentifier;
+ (NSArray *)pendingExpiredSessionReportsWithAppIdentifier:(NSData *)appIdentifier storageDirectoryAtURL:(NSURL *)storageURL;
+ (void)removePendingExpiredSessionReports:(NSArray *)expiredSessionReports withAppIdentifier:(NSData *)appIdentifier storageDirectoryAtURL:(NSURL *)storageURL;
@end

@interface CDMSessionMediaSourceAVFObjCObserver : NSObject {
    WebCore::CDMSessionMediaSourceAVFObjC *m_parent;
}
@end

@implementation CDMSessionMediaSourceAVFObjCObserver
- (id)initWithParent:(WebCore::CDMSessionMediaSourceAVFObjC *)parent
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

static const NSString *PlaybackSessionIdKey = @"PlaybackSessionID";

namespace WebCore {

CDMSessionMediaSourceAVFObjC::CDMSessionMediaSourceAVFObjC(const Vector<int>& protocolVersions)
    : m_client(nullptr)
    , m_dataParserObserver(adoptNS([[CDMSessionMediaSourceAVFObjCObserver alloc] initWithParent:this]))
    , m_protocolVersions(protocolVersions)
    , m_mode(Normal)
{
}

CDMSessionMediaSourceAVFObjC::~CDMSessionMediaSourceAVFObjC()
{
    for (auto& sourceBuffer : m_sourceBuffers) {
        if (m_streamSession)
            [m_streamSession removeStreamDataParser:sourceBuffer->parser()];
    }

    setStreamSession(nullptr);
}

PassRefPtr<Uint8Array> CDMSessionMediaSourceAVFObjC::generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, unsigned long& systemCode)
{
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(destinationURL);
    ASSERT(initData);

    LOG(Media, "CDMSessionMediaSourceAVFObjC::generateKeyRequest(%p)", this);

    errorCode = MediaPlayer::NoError;
    systemCode = 0;

    m_initData = initData;

    if (equalIgnoringCase(mimeType, "keyrelease")) {
        m_mode = KeyRelease;
        return generateKeyReleaseMessage(errorCode, systemCode);
    }

    String certificateString(ASCIILiteral("certificate"));
    RefPtr<Uint8Array> array = Uint8Array::create(certificateString.length());
    for (unsigned i = 0, length = certificateString.length(); i < length; ++i)
        array->set(i, certificateString[i]);
    return array;
}

void CDMSessionMediaSourceAVFObjC::releaseKeys()
{
    if (m_streamSession) {
        m_stopped = true;
        for (auto& sourceBuffer : m_sourceBuffers)
            sourceBuffer->flush();

        LOG(Media, "CDMSessionMediaSourceAVFObjC::releaseKeys(%p) - expiring stream session", this);
        [m_streamSession expire];

        if (!m_certificate)
            return;

        if (![getAVStreamSessionClass() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)])
            return;

        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);
        NSArray* expiredSessions = [getAVStreamSessionClass() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath()]];
        for (NSData* expiredSessionData in expiredSessions) {
            NSDictionary *expiredSession = [NSPropertyListSerialization propertyListWithData:expiredSessionData options:kCFPropertyListImmutable format:nullptr error:nullptr];
            NSString *playbackSessionIdValue = (NSString *)[expiredSession objectForKey:PlaybackSessionIdKey];
            if (![playbackSessionIdValue isKindOfClass:[NSString class]])
                continue;

            if (m_sessionId == String(playbackSessionIdValue)) {
                LOG(Media, "CDMSessionMediaSourceAVFObjC::releaseKeys(%p) - found session, sending expiration message");
                m_expiredSession = expiredSessionData;
                m_client->sendMessage(Uint8Array::create(static_cast<const uint8_t*>([m_expiredSession bytes]), [m_expiredSession length]).get(), emptyString());
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

static NSInteger systemCodeForError(NSError *error)
{
    NSInteger code = [error code];
    if (code != AVErrorUnknown)
        return code;

    NSError* underlyingError = [error.userInfo valueForKey:NSUnderlyingErrorKey];
    if (!underlyingError || ![underlyingError isKindOfClass:[NSError class]])
        return code;
    
    return [underlyingError code];
}

bool CDMSessionMediaSourceAVFObjC::update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, unsigned long& systemCode)
{
    bool shouldGenerateKeyRequest = !m_certificate || isEqual(key, "renew");
    if (!m_certificate) {
        LOG(Media, "CDMSessionMediaSourceAVFObjC::update(%p) - certificate data", this);

        m_certificate = key;
    }

    if (isEqual(key, "acknowledged")) {
        LOG(Media, "CDMSessionMediaSourceAVFObjC::update(%p) - acknowleding secure stop message", this);

        if (!m_expiredSession) {
            errorCode = MediaPlayer::InvalidPlayerState;
            return false;
        }

        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

        if ([getAVStreamSessionClass() respondsToSelector:@selector(removePendingExpiredSessionReports:withAppIdentifier:storageDirectoryAtURL:)])
            [getAVStreamSessionClass() removePendingExpiredSessionReports:@[m_expiredSession.get()] withAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath()]];
        m_expiredSession = nullptr;
        return true;
    }

    if (m_mode == KeyRelease)
        return false;

    RefPtr<SourceBufferPrivateAVFObjC> protectedSourceBuffer;
    for (auto& sourceBuffer : m_sourceBuffers) {
        if (sourceBuffer->protectedTrackID() != -1) {
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
        if (!m_protocolVersions.isEmpty() && canLoadAVStreamDataParserContentKeyRequestProtocolVersionsKey()) {
            RetainPtr<NSMutableArray> protocolVersionsOption = adoptNS([[NSMutableArray alloc] init]);
            for (auto& version : m_protocolVersions) {
                if (!version)
                    continue;
                [protocolVersionsOption addObject:@(version)];
            }

            options = @{ getAVStreamDataParserContentKeyRequestProtocolVersionsKey(): protocolVersionsOption.get() };
        }

        NSError* error = nil;
        RetainPtr<NSData> request = [protectedSourceBuffer->parser() streamingContentKeyRequestDataForApp:certificateData.get() contentIdentifier:initData.get() trackID:protectedSourceBuffer->protectedTrackID() options:options.get() error:&error];

        if (![protectedSourceBuffer->parser() respondsToSelector:@selector(contentProtectionSessionIdentifier)])
            m_sessionId = createCanonicalUUIDString();

        if (error) {
            LOG(Media, "CDMSessionMediaSourceAVFObjC::update(%p) - error:%@", this, [error description]);
            errorCode = MediaPlayer::InvalidPlayerState;
            systemCode = abs(systemCodeForError(error));
            return false;
        }

        nextMessage = Uint8Array::create([request length]);
        [request getBytes:nextMessage->data() length:nextMessage->length()];
        return false;
    }

    ASSERT(!m_sourceBuffers.isEmpty());
    LOG(Media, "CDMSessionMediaSourceAVFObjC::update(%p) - key data", this);
    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    RetainPtr<NSData> keyData = adoptNS([[NSData alloc] initWithBytes:key->data() length:key->length()]);
    [protectedSourceBuffer->parser() processContentKeyResponseData:keyData.get() forTrackID:protectedSourceBuffer->protectedTrackID()];

    return true;
}

void CDMSessionMediaSourceAVFObjC::layerDidReceiveError(AVSampleBufferDisplayLayer *, NSError *error, bool& shouldIgnore)
{
    if (!m_client)
        return;

    unsigned long code = abs(systemCodeForError(error));

    // FIXME(142246): Remove the following once <rdar://problem/20027434> is resolved.
    shouldIgnore = m_stopped && code == 12785;
    if (!shouldIgnore)
        m_client->sendError(CDMSessionClient::MediaKeyErrorDomain, code);
}

void CDMSessionMediaSourceAVFObjC::rendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *error, bool& shouldIgnore)
{
    if (!m_client)
        return;

    unsigned long code = abs(systemCodeForError(error));

    // FIXME(142246): Remove the following once <rdar://problem/20027434> is resolved.
    shouldIgnore = m_stopped && code == 12785;
    if (!shouldIgnore)
        m_client->sendError(CDMSessionClient::MediaKeyErrorDomain, code);
}

void CDMSessionMediaSourceAVFObjC::setStreamSession(AVStreamSession *streamSession)
{
    if (m_streamSession && canLoadAVStreamSessionContentProtectionSessionIdentifierChangedNotification())
        [[NSNotificationCenter defaultCenter] removeObserver:m_dataParserObserver.get() name:getAVStreamSessionContentProtectionSessionIdentifierChangedNotification() object:m_streamSession.get()];

    m_streamSession = streamSession;

    if (!m_streamSession)
        return;

    if (canLoadAVStreamSessionContentProtectionSessionIdentifierChangedNotification())
        [[NSNotificationCenter defaultCenter] addObserver:m_dataParserObserver.get() selector:@selector(contentProtectionSessionIdentifierChanged:) name:getAVStreamSessionContentProtectionSessionIdentifierChangedNotification() object:m_streamSession.get()];

    NSData* identifier = [streamSession contentProtectionSessionIdentifier];
    RetainPtr<NSString> sessionIdentifierString = identifier ? adoptNS([[NSString alloc] initWithData:identifier encoding:(NSUTF8StringEncoding)]) : nil;
    setSessionId(sessionIdentifierString.get());
}

void CDMSessionMediaSourceAVFObjC::addSourceBuffer(SourceBufferPrivateAVFObjC* sourceBuffer)
{
    ASSERT(!m_sourceBuffers.contains(sourceBuffer));
    ASSERT(sourceBuffer);

    m_sourceBuffers.append(sourceBuffer);
    sourceBuffer->registerForErrorNotifications(this);
}

void CDMSessionMediaSourceAVFObjC::removeSourceBuffer(SourceBufferPrivateAVFObjC* sourceBuffer)
{
    ASSERT(m_sourceBuffers.contains(sourceBuffer));
    ASSERT(sourceBuffer);

    if (m_streamSession)
        [m_streamSession removeStreamDataParser:sourceBuffer->parser()];

    sourceBuffer->unregisterForErrorNotifications(this);
    m_sourceBuffers.remove(m_sourceBuffers.find(sourceBuffer));
}

String CDMSessionMediaSourceAVFObjC::storagePath() const
{
    return m_client ? pathByAppendingComponent(m_client->mediaKeysStorageDirectory(), "SecureStop.plist") : emptyString();
}

PassRefPtr<Uint8Array> CDMSessionMediaSourceAVFObjC::generateKeyReleaseMessage(unsigned short& errorCode, unsigned long& systemCode)
{
    ASSERT(m_mode == KeyRelease);
    m_certificate = m_initData;
    RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

    if (![getAVStreamSessionClass() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)]) {
        errorCode = MediaPlayer::KeySystemNotSupported;
        systemCode = '!mor';
        return nullptr;
    }

    NSArray* expiredSessions = [getAVStreamSessionClass() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:storagePath()]];
    if (![expiredSessions count]) {
        LOG(Media, "CDMSessionMediaSourceAVFObjC::generateKeyReleaseMessage(%p) - no expired sessions found", this);

        errorCode = MediaPlayer::KeySystemNotSupported;
        systemCode = '!mor';
        return nullptr;
    }

    LOG(Media, "CDMSessionMediaSourceAVFObjC::generateKeyReleaseMessage(%p) - found %d expired sessions", this, [expiredSessions count]);

    errorCode = 0;
    systemCode = 0;
    m_expiredSession = [expiredSessions firstObject];
    return Uint8Array::create(static_cast<const uint8_t*>([m_expiredSession bytes]), [m_expiredSession length]);
}

}

#endif
