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
#import "CDMSessionAVFoundationObjC.h"

#if ENABLE(ENCRYPTED_MEDIA_V2) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090

#import "CDM.h"
#import "CDMSession.h"
#import "ExceptionCode.h"
#import "MediaPlayer.h"
#import "MediaPlayerPrivateAVFoundationObjC.h"
#import "SoftLinking.h"
#import "UUID.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/objc-runtime.h>

namespace WebCore {

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVURLAsset)
SOFT_LINK_CLASS(AVFoundation, AVAssetResourceLoadingRequest)
#define AVURLAsset getAVURLAssetClass()
#define AVAssetResourceLoadingRequest getAVAssetResourceLoadingRequest()

CDMSessionAVFoundationObjC::CDMSessionAVFoundationObjC(MediaPlayerPrivateAVFoundationObjC* parent)
    : m_parent(parent)
    , m_client(nullptr)
    , m_sessionId(createCanonicalUUIDString())
{
}

PassRefPtr<Uint8Array> CDMSessionAVFoundationObjC::generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, unsigned long& systemCode)
{
    UNUSED_PARAM(mimeType);

    String keyURI;
    String keyID;
    RefPtr<Uint8Array> certificate;
    if (!MediaPlayerPrivateAVFoundationObjC::extractKeyURIKeyIDAndCertificateFromInitData(initData, keyURI, keyID, certificate)) {
        errorCode = MediaPlayer::InvalidPlayerState;
        return nullptr;
    }

    m_request = m_parent->takeRequestForKeyURI(keyURI);
    if (!m_request) {
        errorCode = MediaPlayer::InvalidPlayerState;
        return nullptr;
    }

    RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:certificate->baseAddress() length:certificate->byteLength()]);
    NSString* assetStr = keyID;
    RetainPtr<NSData> assetID = [NSData dataWithBytes: [assetStr cStringUsingEncoding:NSUTF8StringEncoding] length:[assetStr lengthOfBytesUsingEncoding:NSUTF8StringEncoding]];
    NSError* nsError = 0;
    RetainPtr<NSData> keyRequest = [m_request streamingContentKeyRequestDataForApp:certificateData.get() contentIdentifier:assetID.get() options:nil error:&nsError];

    if (!keyRequest) {
        NSError* underlyingError = [[nsError userInfo] objectForKey:NSUnderlyingErrorKey];
        systemCode = [underlyingError code];
        return nullptr;
    }

    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    destinationURL = String();

    RefPtr<ArrayBuffer> keyRequestBuffer = ArrayBuffer::create([keyRequest.get() bytes], [keyRequest.get() length]);
    return Uint8Array::create(keyRequestBuffer, 0, keyRequestBuffer->byteLength());
}

void CDMSessionAVFoundationObjC::releaseKeys()
{
}

bool CDMSessionAVFoundationObjC::update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, unsigned long& systemCode)
{
    RetainPtr<NSData> keyData = adoptNS([[NSData alloc] initWithBytes:key->baseAddress() length:key->byteLength()]);
    [[m_request dataRequest] respondWithData:keyData.get()];
    [m_request finishLoading];
    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    nextMessage = nullptr;

    return true;
}

}

#endif
