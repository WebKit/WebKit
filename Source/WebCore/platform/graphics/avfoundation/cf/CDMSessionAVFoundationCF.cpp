/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
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

#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "LegacyCDM.h"
#include "LegacyCDMSession.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include <AVFoundationCF/AVFoundationCF.h>
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/Uint8Array.h>
#include <wtf/SoftLinking.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UUID.h>
#include <wtf/cf/CFURLExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CDMSessionAVFoundationCF);

CDMSessionAVFoundationCF::CDMSessionAVFoundationCF(MediaPlayerPrivateAVFoundationCF& parent, LegacyCDMSessionClient&)
    : m_parent(parent)
    , m_sessionId(createVersion4UUIDString())
{
}

RefPtr<Uint8Array> CDMSessionAVFoundationCF::generateKeyRequest(const String&, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode)
{
    String keyURI;
    String keyID;
    RefPtr<Uint8Array> certificate;
    if (!MediaPlayerPrivateAVFoundationCF::extractKeyURIKeyIDAndCertificateFromInitData(initData, keyURI, keyID, certificate)) {
        errorCode = MediaPlayer::InvalidPlayerState;
        return nullptr;
    }

    m_request = m_parent.takeRequestForKeyURI(keyURI);
    if (!m_request) {
        errorCode = MediaPlayer::InvalidPlayerState;
        return nullptr;
    }

    auto certificateData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, certificate->byteLength()));
    CFDataAppendBytes(certificateData.get(), certificate->data(), certificate->byteLength());

    RetainPtr assetID = bytesAsCFData(keyID.utf8().span());

    CFErrorRef cfError = nullptr;
    auto keyRequest = adoptCF(AVCFAssetResourceLoadingRequestCreateStreamingContentKeyRequestDataForApp(m_request.get(), certificateData.get(), assetID.get(), nullptr, &cfError));

    if (!keyRequest) {
        if (cfError) {
            if (auto userInfo = adoptCF(CFErrorCopyUserInfo(cfError))) {
                if (auto underlyingError = (CFErrorRef)CFDictionaryGetValue(userInfo.get(), kCFErrorUnderlyingErrorKey))
                    systemCode = CFErrorGetCode(underlyingError);
            }
            CFRelease(cfError);
        }
        return nullptr;
    }

    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    destinationURL = String();

    return Uint8Array::tryCreate(CFDataGetBytePtr(keyRequest.get()), CFDataGetLength(keyRequest.get()));
}

void CDMSessionAVFoundationCF::releaseKeys()
{
}

bool CDMSessionAVFoundationCF::update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode)
{
    auto keyData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, key->byteLength()));
    CFDataAppendBytes(keyData.get(), key->data(), key->byteLength());

    AVCFAssetResourceLoadingRequestFinishLoadingWithResponse(m_request.get(), nullptr, keyData.get(), nullptr);

    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    nextMessage = nullptr;
    return true;
}

RefPtr<ArrayBuffer> CDMSessionAVFoundationCF::cachedKeyForKeyID(const String&) const
{
    return nullptr;
}

}

#endif
