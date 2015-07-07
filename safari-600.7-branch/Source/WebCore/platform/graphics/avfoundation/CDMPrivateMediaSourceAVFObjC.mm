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
#import "CDMPrivateMediaSourceAVFObjC.h"

#if ENABLE(ENCRYPTED_MEDIA_V2) && ENABLE(MEDIA_SOURCE)

#import "CDM.h"
#import "CDMSessionMediaSourceAVFObjC.h"
#import "ContentType.h"
#import "ExceptionCode.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"
#import "WebCoreSystemInterface.h"
#import <wtf/NeverDestroyed.h>
#import <yarr/RegularExpression.h>

using JSC::Yarr::RegularExpression;

namespace WebCore {

static RegularExpression& validKeySystemRE()
{
    static NeverDestroyed<RegularExpression> keySystemRE("^com\\.apple\\.fps\\.2_\\d+(?:,\\d+)*$", TextCaseInsensitive);
    return keySystemRE;
}

bool CDMPrivateMediaSourceAVFObjC::supportsKeySystem(const String& keySystem)
{
    if (!wkQueryDecoderAvailability())
        return false;

    if (!keySystem.isEmpty() && validKeySystemRE().match(keySystem) < 0)
        return false;

    return true;
}

bool CDMPrivateMediaSourceAVFObjC::supportsKeySystemAndMimeType(const String& keySystem, const String& mimeType)
{
    if (!supportsKeySystem(keySystem))
        return false;

    if (!mimeType.isEmpty()) {
        if (equalIgnoringCase(mimeType, "keyrelease"))
            return true;

        MediaEngineSupportParameters parameters;
        parameters.isMediaSource = true;
        parameters.type = mimeType;

        return MediaPlayerPrivateMediaSourceAVFObjC::supportsType(parameters) != MediaPlayer::IsNotSupported;
    }

    return true;
}

bool CDMPrivateMediaSourceAVFObjC::supportsMIMEType(const String& mimeType)
{
    if (mimeType == "keyrelease")
        return true;

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = mimeType;

    return MediaPlayerPrivateMediaSourceAVFObjC::supportsType(parameters) != MediaPlayer::IsNotSupported;
}

std::unique_ptr<CDMSession> CDMPrivateMediaSourceAVFObjC::createSession()
{
    String keySystem = m_cdm->keySystem();
    ASSERT(validKeySystemRE().match(keySystem) >= 0);

    Vector<String> protocolVersionsStrings;
    keySystem.substring(16).split(',', false, protocolVersionsStrings);

    Vector<int> protocolVersions;
    for (auto& protocolVersionString : protocolVersionsStrings)
        protocolVersions.append(protocolVersionString.toInt());

    return std::make_unique<CDMSessionMediaSourceAVFObjC>(protocolVersions);
}

}

#endif // ENABLE(ENCRYPTED_MEDIA_V2) && ENABLE(MEDIA_SOURCE)
