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

#include "config.h"
#include "CDMSessionAVFoundationCF.h"

#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && ENABLE(ENCRYPTED_MEDIA_V2)

#include "CDM.h"
#include "CDMSession.h"
#include "ExceptionCode.h"
#include "MediaPlayer.h"
#include "MediaPlayerPrivateAVFoundationCF.h"
#include "NotImplemented.h"
#include "SoftLinking.h"
#include "UUID.h"
#include <AVFoundationCF/AVFoundationCF.h>
#include <wtf/text/CString.h>

// The softlink header files must be included after the AVCF and CoreMedia header files.
#include "AVFoundationCFSoftLinking.h"

namespace WebCore {

CDMSessionAVFoundationCF::CDMSessionAVFoundationCF(MediaPlayerPrivateAVFoundationCF* parent)
    : m_parent(parent)
    , m_client(nullptr)
    , m_sessionId(createCanonicalUUIDString())
{
}

PassRefPtr<Uint8Array> CDMSessionAVFoundationCF::generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, unsigned long& systemCode)
{
    UNUSED_PARAM(mimeType);

    String keyURI;
    String keyID;
    RefPtr<Uint8Array> certificate;
    if (!MediaPlayerPrivateAVFoundationCF::extractKeyURIKeyIDAndCertificateFromInitData(initData, keyURI, keyID, certificate)) {
        errorCode = MediaPlayer::InvalidPlayerState;
        return nullptr;
    }

    m_request = m_parent->takeRequestForKeyURI(keyURI);
    if (!m_request) {
        errorCode = MediaPlayer::InvalidPlayerState;
        return nullptr;
    }

    RetainPtr<CFMutableDataRef> certificateData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, certificate->byteLength()));
    CFDataAppendBytes(certificateData.get(), reinterpret_cast<const UInt8*>(certificate->baseAddress()), certificate->byteLength());

    CString assetStr = keyID.utf8();
    RetainPtr<CFMutableDataRef> assetID = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, assetStr.length()));
    CFDataAppendBytes(assetID.get(), reinterpret_cast<const UInt8*>(assetStr.data()), assetStr.length());

    CFErrorRef cfError = nullptr;
    RetainPtr<CFDataRef> keyRequest = AVCFAssetResourceLoadingRequestCreateStreamingContentKeyRequestDataForApp(m_request.get(), certificateData.get(), assetID.get(), nullptr, &cfError);

    if (!keyRequest) {
        RetainPtr<CFDictionaryRef> userInfo;
        if (cfError) {
            userInfo = adoptCF(CFErrorCopyUserInfo(cfError));

            if (userInfo) {
                if (CFErrorRef underlyingError = (CFErrorRef)CFDictionaryGetValue(userInfo.get(), kCFErrorUnderlyingErrorKey))
                    systemCode = CFErrorGetCode(underlyingError);
            }

            CFRelease(cfError);
        }

        return nullptr;
    }

    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    destinationURL = String();

    RefPtr<ArrayBuffer> keyRequestBuffer = ArrayBuffer::create(CFDataGetBytePtr(keyRequest.get()), CFDataGetLength(keyRequest.get()));
    return Uint8Array::create(keyRequestBuffer, 0, keyRequestBuffer->byteLength());
}

void CDMSessionAVFoundationCF::releaseKeys()
{
}

bool CDMSessionAVFoundationCF::update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, unsigned long& systemCode)
{
    RetainPtr<CFMutableDataRef> keyData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, key->byteLength()));
    CFDataAppendBytes(keyData.get(), reinterpret_cast<const UInt8*>(key->baseAddress()), key->byteLength());

    AVCFAssetResourceLoadingRequestFinishLoadingWithResponse(m_request.get(), nullptr, keyData.get(), nullptr);

    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    nextMessage = nullptr;

    return true;
}

}

#endif
