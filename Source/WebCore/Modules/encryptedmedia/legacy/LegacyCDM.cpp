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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

#if HAVE(AVCONTENTKEYSESSION) && ENABLE(MEDIA_SOURCE)
#include "CDMPrivateMediaSourceAVFObjC.h"
#endif

namespace WebCore {

struct LegacyCDMFactory {
    CreateCDM constructor;
    CDMSupportsKeySystem supportsKeySystem;
    CDMSupportsKeySystemAndMimeType supportsKeySystemAndMimeType;
};

static void platformRegisterFactories(Vector<LegacyCDMFactory>& factories)
{
    factories.append({ [](LegacyCDM& cdm) { return makeUniqueWithoutRefCountedCheck<LegacyCDMPrivateClearKey>(cdm); }, LegacyCDMPrivateClearKey::supportsKeySystem, LegacyCDMPrivateClearKey::supportsKeySystemAndMimeType });
    // FIXME: initialize specific UA CDMs. http://webkit.org/b/109318, http://webkit.org/b/109320
    factories.append({ [](LegacyCDM& cdm) { return makeUniqueWithoutRefCountedCheck<CDMPrivateMediaPlayer>(cdm); }, CDMPrivateMediaPlayer::supportsKeySystem, CDMPrivateMediaPlayer::supportsKeySystemAndMimeType });
#if HAVE(AVCONTENTKEYSESSION) && ENABLE(MEDIA_SOURCE)
    factories.append({ [](LegacyCDM& cdm) { return makeUniqueWithoutRefCountedCheck<CDMPrivateMediaSourceAVFObjC>(cdm); }, CDMPrivateMediaSourceAVFObjC::supportsKeySystem, CDMPrivateMediaSourceAVFObjC::supportsKeySystemAndMimeType });
#endif
}

static Vector<LegacyCDMFactory>& installedCDMFactories()
{
    static NeverDestroyed<Vector<LegacyCDMFactory>> cdms;
    static std::once_flag registerDefaults;
    std::call_once(registerDefaults, [&] {
        platformRegisterFactories(cdms);
    });
    return cdms;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(LegacyCDM);

void LegacyCDM::resetFactories()
{
    clearFactories();
    platformRegisterFactories(installedCDMFactories());
}

void LegacyCDM::clearFactories()
{
    installedCDMFactories().clear();
}

void LegacyCDM::registerCDMFactory(CreateCDM&& constructor, CDMSupportsKeySystem&& supportsKeySystem, CDMSupportsKeySystemAndMimeType&& supportsKeySystemAndMimeType)
{
    installedCDMFactories().append({ WTFMove(constructor), WTFMove(supportsKeySystem), WTFMove(supportsKeySystemAndMimeType) });
}

static LegacyCDMFactory* CDMFactoryForKeySystem(const String& keySystem)
{
    for (auto& factory : installedCDMFactories()) {
        if (factory.supportsKeySystem(keySystem))
            return &factory;
    }
    return nullptr;
}

bool LegacyCDM::supportsKeySystem(const String& keySystem)
{
    return CDMFactoryForKeySystem(keySystem);
}

bool LegacyCDM::keySystemSupportsMimeType(const String& keySystem, const String& mimeType)
{
    if (LegacyCDMFactory* factory = CDMFactoryForKeySystem(keySystem))
        return factory->supportsKeySystemAndMimeType(keySystem, mimeType);
    return false;
}

RefPtr<LegacyCDM> LegacyCDM::create(const String& keySystem)
{
    if (!supportsKeySystem(keySystem))
        return nullptr;

    return adoptRef(*new LegacyCDM(keySystem));
}

LegacyCDM::LegacyCDM(const String& keySystem)
    : m_keySystem(keySystem)
    , m_private(CDMFactoryForKeySystem(keySystem)->constructor(*this))
{
}

LegacyCDM::~LegacyCDM() = default;

bool LegacyCDM::supportsMIMEType(const String& mimeType) const
{
    return protectedCDMPrivate()->supportsMIMEType(mimeType);
}

RefPtr<CDMPrivateInterface> LegacyCDM::protectedCDMPrivate() const
{
    return cdmPrivate();
}

RefPtr<LegacyCDMSession> LegacyCDM::createSession(LegacyCDMSessionClient& client)
{
    RefPtr session = protectedCDMPrivate()->createSession(client);
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
