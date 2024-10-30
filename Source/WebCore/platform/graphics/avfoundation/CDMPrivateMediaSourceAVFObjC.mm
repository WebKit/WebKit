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
#import "ContentType.h"
#import "LegacyCDM.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"
#import <JavaScriptCore/RegularExpression.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/StringToIntegerConversion.h>
#import <wtf/text/StringView.h>

#import "VideoToolboxSoftLink.h"

using JSC::Yarr::RegularExpression;

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CDMPrivateMediaSourceAVFObjC);

auto CDMPrivateMediaSourceAVFObjC::parseKeySystem(const String& keySystem) -> std::optional<KeySystemParameters>
{
    static NeverDestroyed<RegularExpression> keySystemRE("^com\\.apple\\.fps\\.[23]_\\d+(?:,\\d+)*$"_s, OptionSet<JSC::Yarr::Flags> { JSC::Yarr::Flags::IgnoreCase });

    if (keySystemRE.get().match(keySystem) < 0)
        return std::nullopt;

    StringView keySystemView { keySystem };

    int cdmVersion = parseInteger<int>(keySystemView.substring(14, 1)).value();
    Vector<int> protocolVersions;
    for (auto protocolVersionString : keySystemView.substring(16).split(','))
        protocolVersions.append(parseInteger<int>(protocolVersionString).value());
    return { { cdmVersion, WTFMove(protocolVersions) } };
}

CDMPrivateMediaSourceAVFObjC::~CDMPrivateMediaSourceAVFObjC()
{
    for (auto& session : m_sessions)
        session->invalidateCDM();
}

static bool queryDecoderAvailability()
{
    if (!canLoad_VideoToolbox_VTGetGVADecoderAvailability()) {
        return true;
    }
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
    if (equalLettersIgnoringASCIICase(mimeType, "keyrelease"_s))
        return true;

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = ContentType(mimeType);

    return MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(parameters) != MediaPlayer::SupportsType::IsNotSupported;
}

bool CDMPrivateMediaSourceAVFObjC::supportsMIMEType(const String& mimeType) const
{
    // FIXME: Why is this checking case since the check in supportsKeySystemAndMimeType is ignoring case?
    if (mimeType == "keyrelease"_s)
        return true;

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = ContentType(mimeType);

    return MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(parameters) != MediaPlayer::SupportsType::IsNotSupported;
}

RefPtr<LegacyCDMSession> CDMPrivateMediaSourceAVFObjC::createSession(LegacyCDMSessionClient& client)
{
    String keySystem = m_cdm->keySystem(); // Local copy for StringView usage
    auto parameters = parseKeySystem(m_cdm->keySystem());
    ASSERT(parameters);
    if (!parameters)
        return nullptr;

    RefPtr session = CDMSessionAVContentKeySession::create(WTFMove(parameters.value().protocols), parameters.value().version, *this, client);

    m_sessions.append(session.get());
    return WTFMove(session);
}

void CDMPrivateMediaSourceAVFObjC::invalidateSession(CDMSessionMediaSourceAVFObjC* session)
{
    ASSERT(m_sessions.contains(session));
    m_sessions.removeAll(session);
}

void CDMPrivateMediaSourceAVFObjC::ref() const
{
    m_cdm->ref();
}

void CDMPrivateMediaSourceAVFObjC::deref() const
{
    m_cdm->deref();
}

}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)
