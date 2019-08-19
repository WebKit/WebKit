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

#pragma once

#if USE(EXTERNAL_HOLEPUNCH)

#include "MediaPlayerPrivate.h"
#include "PlatformLayer.h"
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

#if USE(NICOSIA)
#include "NicosiaContentLayerTextureMapperImpl.h"
#else
#include "TextureMapperPlatformLayerProxyProvider.h"
#endif

namespace WebCore {

class TextureMapperPlatformLayerProxy;

class MediaPlayerPrivateHolePunch : public MediaPlayerPrivateInterface, public CanMakeWeakPtr<MediaPlayerPrivateHolePunch>
#if USE(NICOSIA)
    , public Nicosia::ContentLayerTextureMapperImpl::Client
#else
    , public PlatformLayer
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    MediaPlayerPrivateHolePunch(MediaPlayer*);
    ~MediaPlayerPrivateHolePunch();

    static void registerMediaEngine(MediaEngineRegistrar);

    void load(const String&) final { };
#if ENABLE(MEDIA_SOURCE)
    void load(const String&, MediaSourcePrivateClient*) final { };
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final { };
#endif
    void cancelLoad() final { };

    void play() final { };
    void pause() final { };

    PlatformLayer* platformLayer() const final;

    FloatSize naturalSize() const final;

    bool hasVideo() const final { return false; };
    bool hasAudio() const final { return false; };

    void setVisible(bool) final { };

    bool seeking() const final { return false; }

    bool paused() const final { return false; };

    MediaPlayer::NetworkState networkState() const final { return MediaPlayer::Empty; };
    MediaPlayer::ReadyState readyState() const final { return MediaPlayer::HaveMetadata; };

    std::unique_ptr<PlatformTimeRanges> buffered() const final { return makeUnique<PlatformTimeRanges>(); };

    bool didLoadingProgress() const final { return false; };

    void setSize(const IntSize& size) final { m_size = size; };

    void paint(GraphicsContext&, const FloatRect&) final { };

    bool supportsAcceleratedRendering() const final { return true; }

    bool shouldIgnoreIntrinsicSize() final { return true; }

    void pushNextHolePunchBuffer();
    void swapBuffersIfNeeded() final;
#if !USE(NICOSIA)
    RefPtr<TextureMapperPlatformLayerProxy> proxy() const final;
#endif

private:
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    void notifyReadyState();

    MediaPlayer* m_player;
    IntSize m_size;
    RunLoop::Timer<MediaPlayerPrivateHolePunch> m_readyTimer;
#if USE(TEXTURE_MAPPER_GL)
#if USE(NICOSIA)
    Ref<Nicosia::ContentLayer> m_nicosiaLayer;
#else
    RefPtr<TextureMapperPlatformLayerProxy> m_platformLayerProxy;
#endif
#endif

};
}
#endif // USE(EXTERNAL_HOLEPUNCH)
