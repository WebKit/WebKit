/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)

#import "CDMSessionAVContentKeySession.h"
#import "CDMSessionAVStreamSession.h"
#import "ContentType.h"
#import "LegacyCDM.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"
#import <JavaScriptCore/RegularExpression.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/StringView.h>

#import "VideoToolboxSoftLink.h"

using JSC::Yarr::RegularExpression;

namespace WebCore {

auto CDMPrivateMediaSourceAVFObjC::parseKeySystem(const String& keySystem) -> Optional<KeySystemParameters>
{
    static NeverDestroyed<RegularExpression> keySystemRE("^com\\.apple\\.fps\\.[23]_\\d+(?:,\\d+)*$", JSC::Yarr::TextCaseInsensitive);

    if (keySystem.isEmpty())
        return WTF::nullopt;
    
    if (keySystemRE.get().match(keySystem) < 0)
        return WTF::nullopt;
    
    StringView keySystemView { keySystem };

    int cdmVersion = keySystemView.substring(14, 1).toInt();
    
    Vector<int> protocolVersions;
    for (StringView protocolVersionString : keySystemView.substring(16).split(','))
        protocolVersions.append(protocolVersionString.toInt());
    
    return {{ cdmVersion, WTFMove(protocolVersions) }};
}

CDMPrivateMediaSourceAVFObjC::~CDMPrivateMediaSourceAVFObjC()
{
    for (auto& session : m_sessions)
        session->invalidateCDM();
}

static bool queryDecoderAvailability()
{
    if (!canLoad_VideoToolbox_VTGetGVADecoderAvailability())
#if HAVE(AVSTREAMSESSION)
        return false;
#else
        return true;
#endif
    uint32_t totalInstanceCount = 0;
    OSStatus status = VTGetGVADecoderAvailability(&totalInstanceCount, nullptr);
    return status == noErr && totalInstanceCount;
}

bool CDMPrivateMediaSourceAVFObjC::supportsKeySystem(const String& keySystem)
{
    if (!queryDecoderAvailability())
        return false;

    auto parameters = parseKeySystem(keySystem);
    if (!parameters)
        return false;

    if (parameters.value().version == 3 && !CDMSessionAVContentKeySession::isAvailable())
        return false;

    return true;
}

bool CDMPrivateMediaSourceAVFObjC::supportsKeySystemAndMimeType(const String& keySystem, const String& mimeType)
{
    if (!supportsKeySystem(keySystem))
        return false;

    if (mimeType.isEmpty())
        return true;

    // FIXME: Why is this ignoring case since the check in supportsMIMEType is checking case?
    if (equalLettersIgnoringASCIICase(mimeType, "keyrelease"))
        return true;

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = ContentType(mimeType);

    return MediaPlayerPrivateMediaSourceAVFObjC::supportsType(parameters) != MediaPlayer::IsNotSupported;
}

bool CDMPrivateMediaSourceAVFObjC::supportsMIMEType(const String& mimeType)
{
    // FIXME: Why is this checking case since the check in supportsKeySystemAndMimeType is ignoring case?
    if (mimeType == "keyrelease")
        return true;

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = ContentType(mimeType);

    return MediaPlayerPrivateMediaSourceAVFObjC::supportsType(parameters) != MediaPlayer::IsNotSupported;
}

std::unique_ptr<LegacyCDMSession> CDMPrivateMediaSourceAVFObjC::createSession(LegacyCDMSessionClient* client)
{
    String keySystem = m_cdm->keySystem(); // Local copy for StringView usage
    auto parameters = parseKeySystem(m_cdm->keySystem());
    ASSERT(parameters);
    if (!parameters)
        return nullptr;

    std::unique_ptr<CDMSessionMediaSourceAVFObjC> session;
    
#if HAVE(AVSTREAMSESSION)
    bool shouldUseAVContentKeySession = parameters.value().version == 3;
#else
    bool shouldUseAVContentKeySession = true;
#endif
    
    if (shouldUseAVContentKeySession && CDMSessionAVContentKeySession::isAvailable())
        session = std::make_unique<CDMSessionAVContentKeySession>(WTFMove(parameters.value().protocols), parameters.value().version, *this, client);
    else
#if HAVE(AVSTREAMSESSION)
        session = std::make_unique<CDMSessionAVStreamSession>(WTFMove(parameters.value().protocols), *this, client);
#else
        return nullptr;
#endif

    m_sessions.append(session.get());
    return WTFMove(session);
}

void CDMPrivateMediaSourceAVFObjC::invalidateSession(CDMSessionMediaSourceAVFObjC* session)
{
    ASSERT(m_sessions.contains(session));
    m_sessions.removeAll(session);
}

}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)
