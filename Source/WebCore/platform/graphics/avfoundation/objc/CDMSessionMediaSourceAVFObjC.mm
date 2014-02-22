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

#if ENABLE(ENCRYPTED_MEDIA_V2) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090

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

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVStreamDataParser);
#define AVAssetResourceLoadingRequest getAVStreamDataParser()

@interface AVStreamDataParser : NSObject
- (void)processContentKeyResponseData:(NSData *)contentKeyResponseData forTrackID:(CMPersistentTrackID)trackID;
- (void)processContentKeyResponseError:(NSError *)error forTrackID:(CMPersistentTrackID)trackID;
- (void)renewExpiringContentKeyResponseDataForTrackID:(CMPersistentTrackID)trackID;
- (NSData *)streamingContentKeyRequestDataForApp:(NSData *)appIdentifier contentIdentifier:(NSData *)contentIdentifier trackID:(CMPersistentTrackID)trackID options:(NSDictionary *)options error:(NSError **)outError;
@end

namespace WebCore {

CDMSessionMediaSourceAVFObjC::CDMSessionMediaSourceAVFObjC(SourceBufferPrivateAVFObjC* parent)
    : m_parent(parent)
    , m_client(nullptr)
    , m_sessionId(createCanonicalUUIDString())
{
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
    String certificateString(ASCIILiteral("certificate"));
    return Uint8Array::create((uint8_t*)certificateString.getCharactersWithUpconvert<UChar>(), certificateString.length() * sizeof(UChar));
}

void CDMSessionMediaSourceAVFObjC::releaseKeys()
{
    LOG(Media, "CDMSessionMediaSourceAVFObjC::releaseKeys(%p)", this);
}

bool CDMSessionMediaSourceAVFObjC::update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, unsigned long& systemCode)
{
    if (!m_certificate) {
        LOG(Media, "CDMSessionMediaSourceAVFObjC::update(%p) - certificate data", this);

        m_certificate = key;

        RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:m_certificate->data() length:m_certificate->length()]);
        RetainPtr<NSData> initData = adoptNS([[NSData alloc] initWithBytes:m_initData->data() length:m_initData->length()]);

        NSError* error = nil;
        RetainPtr<NSData> request = adoptNS([m_parent->parser() streamingContentKeyRequestDataForApp:certificateData.get() contentIdentifier:initData.get() trackID:m_parent->protectedTrackID() options:nil error:&error]);

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

    LOG(Media, "CDMSessionMediaSourceAVFObjC::update(%p) - key data", this);
    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    RetainPtr<NSData> keyData = adoptNS([[NSData alloc] initWithBytes:key->data() length:key->length()]);
    [m_parent->parser() processContentKeyResponseData:keyData.get() forTrackID:m_parent->protectedTrackID()];
    return true;
}

}

#endif
