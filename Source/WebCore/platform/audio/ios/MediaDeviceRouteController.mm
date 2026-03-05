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

#import "config.h"
#import "MediaDeviceRouteController.h"

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)

#import "MediaDeviceRoute.h"
#import "MediaSessionHelperIOS.h"
#import "MediaStrategy.h"
#import "PlatformStrategies.h"
#import <WebKitAdditions/MediaDeviceRouteControllerAdditions.mm>

#import <pal/ios/AVRoutingSoftLink.h>

namespace WebCore {

MediaDeviceRouteController& MediaDeviceRouteController::singleton()
{
    static NeverDestroyed<MediaDeviceRouteController> controller;
    return controller;
}

MediaDeviceRouteController::MediaDeviceRouteController()
#if HAVE(AVROUTING_FRAMEWORK)
    : m_routeObserver { adoptNS([[WebMediaDeviceRouteObserver alloc] init]) }
    , m_platformController { [WebMediaDevicePlatformRouteControllerClass sharedController] }
#endif
{
#if HAVE(AVROUTING_FRAMEWORK)
    [m_platformController addObserver:m_routeObserver.get()];
#endif
}

RefPtr<MediaDeviceRoute> MediaDeviceRouteController::mostRecentActiveRoute() const
{
    if (m_activeRoutes.isEmpty())
        return nullptr;

    return m_activeRoutes.last().get();
}

RefPtr<MediaDeviceRoute> MediaDeviceRouteController::routeForIdentifier(const std::optional<WTF::UUID>& identifier) const
{
    auto index = m_activeRoutes.findIf([&](auto& route) {
        return route->identifier() == identifier;
    });

    if (index != notFound)
        return m_activeRoutes[index].get();

    return nullptr;
}

bool MediaDeviceRouteController::activateRoute(WebMediaDevicePlatformRoute *platformRoute)
{
    m_activeRoutes.removeAllMatching([&](auto& route) { return route->platformRoute() == platformRoute; });
    m_activeRoutes.append(MediaDeviceRoute::create(platformRoute));

    if (RefPtr client = m_client.get())
        client->activeRoutesDidChange(*this);

    return true;
}

bool MediaDeviceRouteController::deactivateRoute(WebMediaDevicePlatformRoute *platformRoute)
{
    if (!m_activeRoutes.removeAllMatching([&](auto& route) { return route->platformRoute() == platformRoute; }))
        return false;

    if (RefPtr client = m_client.get())
        client->activeRoutesDidChange(*this);

    return true;
}

void MediaDeviceRouteController::deactivateAllRoutes()
{
    m_activeRoutes.clear();

    if (RefPtr client = m_client.get())
        client->activeRoutesDidChange(*this);
}

static bool& mockMediaDeviceRouteControllerEnabledValue()
{
    static bool mockMediaDeviceRouteControllerEnabled;
    return mockMediaDeviceRouteControllerEnabled;
}

void setMockMediaDeviceRouteControllerEnabled(bool isEnabled)
{
    MediaDeviceRouteController::singleton().deactivateAllRoutes();
    MediaDeviceRouteController::singleton().setClient(isEnabled ? &MediaSessionHelper::sharedHelper() : nullptr);

    if (mockMediaDeviceRouteControllerEnabledValue() == isEnabled)
        return;

    mockMediaDeviceRouteControllerEnabledValue() = isEnabled;
    platformStrategies()->mediaStrategy()->resetMediaEngines();
}

bool mockMediaDeviceRouteControllerEnabled()
{
    return mockMediaDeviceRouteControllerEnabledValue();
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
