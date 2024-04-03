/*
 * Copyright (C) 2019 Igalia S.L
 * Copyright (C) 2019 Metrological Group B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "MediaPlayerPrivateHolePunch.h"

#if USE(EXTERNAL_HOLEPUNCH)
#include "MediaPlayer.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxyGL.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const FloatSize s_holePunchDefaultFrameSize(1280, 720);

MediaPlayerPrivateHolePunch::MediaPlayerPrivateHolePunch(MediaPlayer* player)
    : m_player(player)
    , m_readyTimer(RunLoop::main(), this, &MediaPlayerPrivateHolePunch::notifyReadyState)
    , m_networkState(MediaPlayer::NetworkState::Empty)
#if USE(NICOSIA)
    , m_nicosiaLayer(Nicosia::ContentLayer::create(*this, adoptRef(*new WebCore::TextureMapperPlatformLayerProxyGL(true))))
#else
    , m_platformLayerProxy(adoptRef(new TextureMapperPlatformLayerProxyGL))
#endif
{
    pushNextHolePunchBuffer();

    // Delay the configuration of the HTMLMediaElement, as during this stage this is not
    // the MediaPlayer private yet and calls from HTMLMediaElement won't reach this.
    m_readyTimer.startOneShot(0_s);
}

MediaPlayerPrivateHolePunch::~MediaPlayerPrivateHolePunch()
{
#if USE(NICOSIA)
    m_nicosiaLayer->invalidateClient();
#endif
}

PlatformLayer* MediaPlayerPrivateHolePunch::platformLayer() const
{
#if USE(NICOSIA)
    return m_nicosiaLayer.ptr();
#else
    return const_cast<MediaPlayerPrivateHolePunch*>(this);
#endif
}

FloatSize MediaPlayerPrivateHolePunch::naturalSize() const
{
    // When using the holepuch we may not be able to get the video frames size, so we can't use
    // it. But we need to report some non empty naturalSize for the player's GraphicsLayer
    // to be properly created.
    return s_holePunchDefaultFrameSize;
}

void MediaPlayerPrivateHolePunch::pushNextHolePunchBuffer()
{
    auto proxyOperation =
        [this](TextureMapperPlatformLayerProxyGL& proxy)
        {
            Locker locker { proxy.lock() };
            std::unique_ptr<TextureMapperPlatformLayerBuffer> layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(0, m_size, TextureMapperFlags::ShouldNotBlend, GL_DONT_CARE);
            proxy.pushNextBuffer(WTFMove(layerBuffer));
        };

#if USE(NICOSIA)
    auto& proxy = m_nicosiaLayer->proxy();
    ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
    proxyOperation(downcast<TextureMapperPlatformLayerProxyGL>(proxy));
#else
    proxyOperation(*m_platformLayerProxy);
#endif
}

void MediaPlayerPrivateHolePunch::swapBuffersIfNeeded()
{
#if !USE(NICOSIA)
    pushNextHolePunchBuffer();
#endif
}

#if !USE(NICOSIA)
RefPtr<TextureMapperPlatformLayerProxy> MediaPlayerPrivateHolePunch::proxy() const
{
    return m_platformLayerProxy.copyRef();
}
#endif

static HashSet<String>& mimeTypeCache()
{
    static NeverDestroyed<HashSet<String>> cache;
    static bool typeListInitialized = false;

    if (typeListInitialized)
        return cache;

    const ASCIILiteral mimeTypes[] = {
        "video/holepunch"_s
    };

    for (unsigned i = 0; i < (sizeof(mimeTypes) / sizeof(*mimeTypes)); ++i)
        cache.get().add(mimeTypes[i]);

    typeListInitialized = true;

    return cache;
}

void MediaPlayerPrivateHolePunch::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivateHolePunch::supportsType(const MediaEngineSupportParameters& parameters)
{
    auto containerType = parameters.type.containerType();

    // Spec says we should not return "probably" if the codecs string is empty.
    if (!containerType.isEmpty() && mimeTypeCache().contains(containerType)) {
        if (parameters.type.codecs().isEmpty())
            return MediaPlayer::SupportsType::MayBeSupported;

        return MediaPlayer::SupportsType::IsSupported;
    }

    return MediaPlayer::SupportsType::IsNotSupported;
}

class MediaPlayerFactoryHolePunch final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::HolePunch; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return adoptRef(*new MediaPlayerPrivateHolePunch(player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        return MediaPlayerPrivateHolePunch::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateHolePunch::supportsType(parameters);
    }
};

void MediaPlayerPrivateHolePunch::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(makeUnique<MediaPlayerFactoryHolePunch>());
}

void MediaPlayerPrivateHolePunch::notifyReadyState()
{
    // Notify the ready state so the GraphicsLayer gets created.
    if (auto player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateHolePunch::setNetworkState(MediaPlayer::NetworkState networkState)
{
    m_networkState = networkState;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateHolePunch::load(const String&)
{
    auto player = m_player.get();
    if (!player)
        return;

    auto mimeType = player->contentMIMEType();
    if (mimeType.isEmpty() || !mimeTypeCache().contains(mimeType))
        setNetworkState(MediaPlayer::NetworkState::FormatError);
}

} // namespace WebCore
#endif // USE(EXTERNAL_HOLEPUNCH)
