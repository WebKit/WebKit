/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "LegacyCDM.h"

#include "LegacyCDMPrivateClearKey.h"
#include "LegacyCDMPrivateMediaPlayer.h"
#include "LegacyCDMSession.h"
#include "MediaPlayer.h"
#include "WebKitMediaKeys.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

#if (HAVE(AVCONTENTKEYSESSION) || HAVE(AVSTREAMSESSION)) && ENABLE(MEDIA_SOURCE)
#include "CDMPrivateMediaSourceAVFObjC.h"
#endif

namespace WebCore {

struct CDMFactory {
    WTF_MAKE_NONCOPYABLE(CDMFactory); WTF_MAKE_FAST_ALLOCATED;
public:
    CDMFactory(CreateCDM&& constructor, CDMSupportsKeySystem supportsKeySystem, CDMSupportsKeySystemAndMimeType supportsKeySystemAndMimeType)
        : constructor(WTFMove(constructor))
        , supportsKeySystem(supportsKeySystem)
        , supportsKeySystemAndMimeType(supportsKeySystemAndMimeType)
    {
    }

    CreateCDM constructor;
    CDMSupportsKeySystem supportsKeySystem;
    CDMSupportsKeySystemAndMimeType supportsKeySystemAndMimeType;
};

static Vector<CDMFactory*>& installedCDMFactories()
{
    static auto cdms = makeNeverDestroyed(Vector<CDMFactory*> {
        new CDMFactory([](LegacyCDM* cdm) { return std::make_unique<LegacyCDMPrivateClearKey>(cdm); },
            LegacyCDMPrivateClearKey::supportsKeySystem, LegacyCDMPrivateClearKey::supportsKeySystemAndMimeType),

        // FIXME: initialize specific UA CDMs. http://webkit.org/b/109318, http://webkit.org/b/109320
        new CDMFactory([](LegacyCDM* cdm) { return std::make_unique<CDMPrivateMediaPlayer>(cdm); },
            CDMPrivateMediaPlayer::supportsKeySystem, CDMPrivateMediaPlayer::supportsKeySystemAndMimeType),

#if (HAVE(AVCONTENTKEYSESSION) || HAVE(AVSTREAMSESSION)) && ENABLE(MEDIA_SOURCE)
        new CDMFactory([](LegacyCDM* cdm) { return std::make_unique<CDMPrivateMediaSourceAVFObjC>(cdm); },
            CDMPrivateMediaSourceAVFObjC::supportsKeySystem, CDMPrivateMediaSourceAVFObjC::supportsKeySystemAndMimeType),
#endif
    });
    return cdms;
}

void LegacyCDM::registerCDMFactory(CreateCDM&& constructor, CDMSupportsKeySystem supportsKeySystem, CDMSupportsKeySystemAndMimeType supportsKeySystemAndMimeType)
{
    installedCDMFactories().append(new CDMFactory(WTFMove(constructor), supportsKeySystem, supportsKeySystemAndMimeType));
}

static CDMFactory* CDMFactoryForKeySystem(const String& keySystem)
{
    for (auto& factory : installedCDMFactories()) {
        if (factory->supportsKeySystem(keySystem))
            return factory;
    }
    return nullptr;
}

bool LegacyCDM::supportsKeySystem(const String& keySystem)
{
    return CDMFactoryForKeySystem(keySystem);
}

bool LegacyCDM::keySystemSupportsMimeType(const String& keySystem, const String& mimeType)
{
    if (CDMFactory* factory = CDMFactoryForKeySystem(keySystem))
        return factory->supportsKeySystemAndMimeType(keySystem, mimeType);
    return false;
}

std::unique_ptr<LegacyCDM> LegacyCDM::create(const String& keySystem)
{
    if (!supportsKeySystem(keySystem))
        return nullptr;

    return std::make_unique<LegacyCDM>(keySystem);
}

LegacyCDM::LegacyCDM(const String& keySystem)
    : m_keySystem(keySystem)
    , m_client(nullptr)
{
    m_private = CDMFactoryForKeySystem(keySystem)->constructor(this);
}

LegacyCDM::~LegacyCDM() = default;

bool LegacyCDM::supportsMIMEType(const String& mimeType) const
{
    return m_private->supportsMIMEType(mimeType);
}

std::unique_ptr<LegacyCDMSession> LegacyCDM::createSession(LegacyCDMSessionClient& client)
{
    auto session = m_private->createSession(&client);
    if (mediaPlayer())
        mediaPlayer()->setCDMSession(session.get());
    return session;
}

RefPtr<MediaPlayer> LegacyCDM::mediaPlayer() const
{
    if (!m_client)
        return nullptr;
    return m_client->cdmMediaPlayer(this);
}

}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)
