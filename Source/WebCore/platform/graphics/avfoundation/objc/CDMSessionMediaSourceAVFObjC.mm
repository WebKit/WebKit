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
#import "Logging.h"
#import "MediaPlayer.h"
#import "SourceBufferPrivateAVFObjC.h"
#import "SoftLinking.h"
#import "UUID.h"
#import <CoreMedia/CMBase.h>
#import <objc/objc-runtime.h>
#import <wtf/NeverDestroyed.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVStreamDataParser);
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVStreamSession);

@interface AVStreamDataParser : NSObject
- (void)processContentKeyResponseData:(NSData *)contentKeyResponseData forTrackID:(CMPersistentTrackID)trackID;
- (void)processContentKeyResponseError:(NSError *)error forTrackID:(CMPersistentTrackID)trackID;
- (void)renewExpiringContentKeyResponseDataForTrackID:(CMPersistentTrackID)trackID;
- (NSData *)streamingContentKeyRequestDataForApp:(NSData *)appIdentifier contentIdentifier:(NSData *)contentIdentifier trackID:(CMPersistentTrackID)trackID options:(NSDictionary *)options error:(NSError **)outError;
- (NSData *)sessionIdentifier;
@end

@interface AVStreamSession : NSObject
- (instancetype)initWithAppIdentifier:(NSData *)appIdentifier storageDirectoryAtURL:(NSURL *)storageURL error:(NSError **)outError;
- (void)addStreamDataParser:(AVStreamDataParser *)streamDataParser;
- (void)removeStreamDataParser:(AVStreamDataParser *)streamDataParser;
- (void)expire;
+ (NSArray *)pendingExpiredSessionReportsWithAppIdentifier:(NSData *)appIdentifier;
+ (void)removePendingExpiredSessionReports:(NSArray *)expiredSessionReports withAppIdentifier:(NSData *)appIdentifier;
@end

@interface CDMSessionMediaSourceAVFObjCObserver : NSObject {
    WebCore::CDMSessionMediaSourceAVFObjC *m_parent;
    HashSet<RetainPtr<AVStreamDataParser>> m_parsers;
}
@end

@implementation CDMSessionMediaSourceAVFObjCObserver
- (id)initWithParent:(WebCore::CDMSessionMediaSourceAVFObjC *)parent
{
    if ((self = [super init]))
        m_parent = parent;
    return self;
}

- (void)dealloc
{
    [self invalidate];
    [super dealloc];
}

- (void)beginObserving:(AVStreamDataParser *)parser
{
    ASSERT(!m_parsers.contains(parser));
    m_parsers.add(parser);
    [parser addObserver:self forKeyPath:@"sessionIdentifier" options:(NSKeyValueObservingOptionNew|NSKeyValueObservingOptionInitial) context:nullptr];
}

- (void)stopObserving:(AVStreamDataParser *)parser
{
    ASSERT(m_parsers.contains(parser));
    m_parsers.remove(parser);
    [parser removeObserver:self forKeyPath:@"sessionIdentifier" context:nullptr];
}

- (void)invalidate
{
    m_parent = nullptr;
    for (auto& parser : m_parsers)
        [parser removeObserver:self forKeyPath:@"sessionIdentifier" context:nullptr];
    m_parsers.clear();
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void*)context
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(change);
    UNUSED_PARAM(context);

    if ([keyPath isEqual:@"sessionIdentifier"]) {
        NSData* identifier = [change valueForKey:NSKeyValueChangeNewKey];
        if ([identifier isKindOfClass:[NSNull class]])
            return;

        RetainPtr<NSString> sessionIdentifierString = adoptNS([[NSString alloc] initWithData:identifier encoding:(NSUTF8StringEncoding)]);
        if (m_parent)
            m_parent->setSessionId(sessionIdentifierString.get());
        return;
    }

    ASSERT_NOT_REACHED();
}
@end

namespace WebCore {

CDMSessionMediaSourceAVFObjC::CDMSessionMediaSourceAVFObjC()
    : m_client(nullptr)
    , m_dataParserObserver(adoptNS([[CDMSessionMediaSourceAVFObjCObserver alloc] initWithParent:this]))
    , m_mode(Normal)
{
}

CDMSessionMediaSourceAVFObjC::~CDMSessionMediaSourceAVFObjC()
{
    for (auto& sourceBuffer : m_sourceBuffers) {
        if (m_streamSession)
            [m_streamSession removeStreamDataParser:sourceBuffer->parser()];

        [sourceBuffer->parser() removeObserver:m_dataParserObserver.get() forKeyPath:@"sessionIdentifier" context:nullptr];
    }

    m_streamSession = nil;
    [m_dataParserObserver invalidate];
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
        LOG(Media, "CDMSessionMediaSourceAVFObjC::releaseKeys(%p) - expiring stream session", this);
        [m_streamSession expire];

        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);
        NSArray* expiredSessions = [getAVStreamSessionClass() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get()];
        for (NSData* expiredSessionData in expiredSessions) {
            NSDictionary *expiredSession = [NSPropertyListSerialization propertyListWithData:expiredSessionData options:kCFPropertyListImmutable format:nullptr error:nullptr];
            if ([expiredSession objectForKey:m_sessionId]) {
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

static const String& sessionStorageDirectory()
{
    static NeverDestroyed<String> sessionDirectoryPath;

    if (sessionDirectoryPath.get().isEmpty()) {
        char cacheDirectoryPath[PATH_MAX];
        if (!confstr(_CS_DARWIN_USER_CACHE_DIR, cacheDirectoryPath, PATH_MAX))
            return WTF::emptyString();

        sessionDirectoryPath.get().append(String(cacheDirectoryPath, strlen(cacheDirectoryPath)));
        sessionDirectoryPath.get().append(ASCIILiteral("AVStreamSession/"));
    }

    return sessionDirectoryPath.get();
}

bool CDMSessionMediaSourceAVFObjC::update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, unsigned long& systemCode)
{
    if (m_mode == KeyRelease)
        return false;

    bool shouldGenerateKeyRequest = !m_certificate || isEqual(key, "renew");
    if (!m_certificate) {
        LOG(Media, "CDMSessionMediaSourceAVFObjC::update(%p) - certificate data", this);

        m_certificate = key;
    }

    if (isEqual(key, "acknowledged")) {
        if (!m_expiredSession) {
            errorCode = MediaPlayer::InvalidPlayerState;
            return false;
        }

        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);
        [getAVStreamSessionClass() removePendingExpiredSessionReports:@[m_expiredSession.get()] withAppIdentifier:certificateData.get()];
        m_expiredSession = nullptr;
    }

    RefPtr<SourceBufferPrivateAVFObjC> protectedSourceBuffer;
    for (auto& sourceBuffer : m_sourceBuffers) {
        if (sourceBuffer->protectedTrackID() != -1) {
            protectedSourceBuffer = sourceBuffer;
            break;
        }
    }

    if (shouldGenerateKeyRequest) {
        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);
        if (getAVStreamSessionClass() && [getAVStreamSessionClass() instancesRespondToSelector:@selector(initWithAppIdentifier:storageDirectoryAtURL:error:)]) {
            m_streamSession = adoptNS([[getAVStreamSessionClass() alloc] initWithAppIdentifier:certificateData.get() storageDirectoryAtURL:[NSURL fileURLWithPath:sessionStorageDirectory()] error:nil]);
            for (auto& sourceBuffer : m_sourceBuffers)
                [m_streamSession addStreamDataParser:sourceBuffer->parser()];
            LOG(Media, "CDMSessionMediaSourceAVFObjC::update(%p) - created stream session %p", this, m_streamSession.get());
        }

        if (m_sourceBuffers.isEmpty())
            return true;

        if (!protectedSourceBuffer)
            return true;

        RetainPtr<NSData> initData = adoptNS([[NSData alloc] initWithBytes:m_initData->data() length:m_initData->length()]);

        NSError* error = nil;
        RetainPtr<NSData> request = [protectedSourceBuffer->parser() streamingContentKeyRequestDataForApp:certificateData.get() contentIdentifier:initData.get() trackID:protectedSourceBuffer->protectedTrackID() options:nil error:&error];

        if (![protectedSourceBuffer->parser() respondsToSelector:@selector(sessionIdentifier)])
            m_sessionId = createCanonicalUUIDString();

        if (error) {
            LOG(Media, "CDMSessionMediaSourceAVFObjC::update(%p) - error:%@", this, [error description]);
            errorCode = MediaPlayer::InvalidPlayerState;
            systemCode = [error code];
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

void CDMSessionMediaSourceAVFObjC::layerDidReceiveError(AVSampleBufferDisplayLayer *, NSError *error)
{
    if (!m_client)
        return;

    m_client->sendError(CDMSessionClient::MediaKeyErrorDomain, abs([error code]));
}

void CDMSessionMediaSourceAVFObjC::rendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *error)
{
    if (!m_client)
        return;

    m_client->sendError(CDMSessionClient::MediaKeyErrorDomain, abs([error code]));
}

void CDMSessionMediaSourceAVFObjC::addSourceBuffer(SourceBufferPrivateAVFObjC* sourceBuffer)
{
    ASSERT(!m_sourceBuffers.contains(sourceBuffer));
    ASSERT(sourceBuffer);

    m_sourceBuffers.append(sourceBuffer);
    sourceBuffer->registerForErrorNotifications(this);

    if (m_streamSession)
        [m_streamSession addStreamDataParser:sourceBuffer->parser()];

    [m_dataParserObserver beginObserving:sourceBuffer->parser()];
}

void CDMSessionMediaSourceAVFObjC::removeSourceBuffer(SourceBufferPrivateAVFObjC* sourceBuffer)
{
    ASSERT(m_sourceBuffers.contains(sourceBuffer));
    ASSERT(sourceBuffer);

    if (m_streamSession)
        [m_streamSession removeStreamDataParser:sourceBuffer->parser()];

    sourceBuffer->unregisterForErrorNotifications(this);
    m_sourceBuffers.remove(m_sourceBuffers.find(sourceBuffer));

    [m_dataParserObserver stopObserving:sourceBuffer->parser()];
}

PassRefPtr<Uint8Array> CDMSessionMediaSourceAVFObjC::generateKeyReleaseMessage(unsigned short& errorCode, unsigned long& systemCode)
{
    ASSERT(m_mode == KeyRelease);
    m_certificate = m_initData;
    RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);

    NSArray* expiredSessions = [getAVStreamSessionClass() pendingExpiredSessionReportsWithAppIdentifier:certificateData.get()];
    if (![expiredSessions count]) {
        errorCode = MediaPlayer::KeySystemNotSupported;
        return nullptr;
    }

    errorCode = 0;
    systemCode = 0;
    m_expiredSession = [expiredSessions firstObject];
    return Uint8Array::create(static_cast<const uint8_t*>([m_expiredSession bytes]), [m_expiredSession length]);
}

}

#endif
