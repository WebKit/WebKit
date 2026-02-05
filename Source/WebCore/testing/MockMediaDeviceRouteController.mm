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
#import "MockMediaDeviceRouteController.h"

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)

#import "MediaDeviceRouteController.h"
#import "MediaSessionHelperIOS.h"
#import "MockMediaDeviceRoute.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MockMediaDeviceRouteController);

Ref<MockMediaDeviceRouteController> MockMediaDeviceRouteController::create()
{
    return adoptRef(*new MockMediaDeviceRouteController());
}

MockMediaDeviceRouteController::MockMediaDeviceRouteController() = default;

bool MockMediaDeviceRouteController::enabled() const
{
    return mockMediaDeviceRouteControllerEnabled();
}

void MockMediaDeviceRouteController::setEnabled(bool enabled)
{
    MediaDeviceRouteController::singleton().setClient(enabled ? &MediaSessionHelper::sharedHelper() : nullptr);
    setMockMediaDeviceRouteControllerEnabled(enabled);
}

Ref<MockMediaDeviceRoute> MockMediaDeviceRouteController::createMockMediaDeviceRoute()
{
    return MockMediaDeviceRoute::create();
}

bool MockMediaDeviceRouteController::activateRoute(const MockMediaDeviceRoute& route)
{
    return MediaDeviceRouteController::singleton().activateRoute(route.platformRoute());
}

bool MockMediaDeviceRouteController::deactivateRoute(const MockMediaDeviceRoute& route)
{
    return MediaDeviceRouteController::singleton().deactivateRoute(route.platformRoute());
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
