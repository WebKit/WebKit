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

#import "config.h"
#import "CDMSessionAVFoundationObjC.h"

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

#import "LegacyCDM.h"
#import "LegacyCDMSession.h"
#import "Logging.h"
#import "MediaPlayer.h"
#import "MediaPlayerPrivateAVFoundationObjC.h"
#import "WebCoreNSErrorExtras.h"
#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVAssetResourceLoader.h>
#import <JavaScriptCore/TypedArrayInlines.h>
#import <objc/objc-runtime.h>
#import <wtf/LoggerHelper.h>
#import <wtf/MainThread.h>
#import <wtf/SoftLinking.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/UUID.h>
#import <wtf/cocoa/SpanCocoa.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CDMSessionAVFoundationObjC);

CDMSessionAVFoundationObjC::CDMSessionAVFoundationObjC(MediaPlayerPrivateAVFoundationObjC* parent, LegacyCDMSessionClient& client)
    : m_parent(*parent)
    , m_client(client)
    , m_sessionId(createVersion4UUIDString())
#if !RELEASE_LOG_DISABLED
    , m_logger(client.logger())
    , m_logIdentifier(client.logIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

CDMSessionAVFoundationObjC::~CDMSessionAVFoundationObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

RefPtr<Uint8Array> CDMSessionAVFoundationObjC::generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode)
{
    UNUSED_PARAM(mimeType);

    RefPtr parent = m_parent.get();
    if (!parent) {
        ERROR_LOG(LOGIDENTIFIER, "error: !parent");
        errorCode = LegacyCDM::UnknownError;
        return nullptr;
    }

    String keyURI;
    String keyID;
    RefPtr<Uint8Array> certificate;
    if (!MediaPlayerPrivateAVFoundationObjC::extractKeyURIKeyIDAndCertificateFromInitData(initData, keyURI, keyID, certificate)) {
        ERROR_LOG(LOGIDENTIFIER, "error: could not extract key info");
        errorCode = LegacyCDM::UnknownError;
        return nullptr;
    }

    m_request = parent->takeRequestForKeyURI(keyURI);
    if (!m_request) {
        ERROR_LOG(LOGIDENTIFIER, "error: could not find request for key URI");
        errorCode = LegacyCDM::UnknownError;
        return nullptr;
    }

    RetainPtr certificateData = toNSData(certificate->span());
    RetainPtr assetString = keyID.createNSString();
    RetainPtr<NSData> assetID = [assetString dataUsingEncoding:NSUTF8StringEncoding];
    NSError* nsError = 0;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<NSData> keyRequest = [m_request streamingContentKeyRequestDataForApp:certificateData.get() contentIdentifier:assetID.get() options:nil error:&nsError];
ALLOW_DEPRECATED_DECLARATIONS_END

    if (!keyRequest) {
        ERROR_LOG(LOGIDENTIFIER, "failed to generate key request with error: ", nsError);
        errorCode = LegacyCDM::DomainError;
        systemCode = mediaKeyErrorSystemCode(nsError);
        return nullptr;
    }

    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    destinationURL = String();

    ALWAYS_LOG(LOGIDENTIFIER);

    return Uint8Array::create(ArrayBuffer::create(span(keyRequest.get())));
}

void CDMSessionAVFoundationObjC::releaseKeys()
{
}

bool CDMSessionAVFoundationObjC::update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode)
{
    RetainPtr keyData = toNSData(key->span());
    [retainPtr([m_request dataRequest]) respondWithData:keyData.get()];
    [m_request finishLoading];
    errorCode = MediaPlayer::NoError;
    systemCode = 0;
    nextMessage = nullptr;

    ALWAYS_LOG(LOGIDENTIFIER);

    return true;
}

RefPtr<ArrayBuffer> CDMSessionAVFoundationObjC::cachedKeyForKeyID(const String&) const
{
    return nullptr;
}

void CDMSessionAVFoundationObjC::playerDidReceiveError(NSError *error)
{
    RefPtr client = m_client.get();
    if (!client)
        return;

    ERROR_LOG(LOGIDENTIFIER, error);

    unsigned long code = mediaKeyErrorSystemCode(error);
    client->sendError(LegacyCDMSessionClient::MediaKeyErrorDomain, code);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& CDMSessionAVFoundationObjC::logChannel() const
{
    return LogEME;
}
#endif

}

#endif
