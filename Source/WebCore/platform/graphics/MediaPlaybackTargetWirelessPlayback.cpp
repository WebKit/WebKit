/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "MediaPlaybackTargetWirelessPlayback.h"

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)

#include "MediaDeviceRoute.h"
#include "MediaDeviceRouteController.h"
#include <wtf/CompletionHandler.h>
#include <wtf/UUID.h>

namespace WebCore {

Ref<MediaPlaybackTargetWirelessPlayback> MediaPlaybackTargetWirelessPlayback::create(std::optional<WTF::UUID> identifier, bool hasActiveRoute)
{
    return adoptRef(*new MediaPlaybackTargetWirelessPlayback(MediaDeviceRouteController::singleton().routeForIdentifier(identifier), hasActiveRoute));
}

Ref<MediaPlaybackTargetWirelessPlayback> MediaPlaybackTargetWirelessPlayback::create(MediaDeviceRoute& route)
{
    return adoptRef(*new MediaPlaybackTargetWirelessPlayback(route, true));
}

MediaPlaybackTargetWirelessPlayback::MediaPlaybackTargetWirelessPlayback(RefPtr<MediaDeviceRoute>&& route, bool hasActiveRoute)
    : MediaPlaybackTarget { Type::WirelessPlayback }
    , m_route { WTF::move(route) }
    , m_hasActiveRoute { hasActiveRoute }
{
}

MediaPlaybackTargetWirelessPlayback::~MediaPlaybackTargetWirelessPlayback() = default;

std::optional<WTF::UUID> MediaPlaybackTargetWirelessPlayback::identifier() const
{
    if (RefPtr route = m_route)
        return m_route->identifier();
    return std::nullopt;
}

MediaDeviceRoute* MediaPlaybackTargetWirelessPlayback::route() const
{
    return m_route.get();
}

String MediaPlaybackTargetWirelessPlayback::deviceName() const
{
    // FIXME: provide a real device name
    if (auto identifier = this->identifier())
        return identifier->toString();
    return { };
}

void MediaPlaybackTargetWirelessPlayback::loadURL(const URL& url, CompletionHandler<void(const MediaDeviceRouteLoadURLResult&)>&& completionHandler)
{
    RefPtr route = m_route;
    if (!route)
        return completionHandler(makeUnexpected(MediaDeviceRouteLoadURLError::NoRoute));

    route->loadURL(url, WTF::move(completionHandler));
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
