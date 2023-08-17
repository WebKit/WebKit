/*
* Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#import "UserInterfaceIdiom.h"

#if PLATFORM(IOS_FAMILY)

#import "Device.h"
#import <pal/spi/ios/UIKitSPI.h>

#if PLATFORM(VISION)
#import <xr/RuntimeSupport.h>
#endif

#import <pal/ios/UIKitSoftLink.h>

namespace PAL {

static std::atomic<std::optional<UserInterfaceIdiom>> s_currentUserInterfaceIdiom;

bool currentUserInterfaceIdiomIsSmallScreen()
{
    if (!s_currentUserInterfaceIdiom.load())
        updateCurrentUserInterfaceIdiom();
    return s_currentUserInterfaceIdiom.load() == UserInterfaceIdiom::SmallScreen;
}

bool currentUserInterfaceIdiomIsVisionOrVisionLegacy()
{
    if (!s_currentUserInterfaceIdiom.load())
        updateCurrentUserInterfaceIdiom();
    auto idiom = *s_currentUserInterfaceIdiom.load();
    return idiom == UserInterfaceIdiom::Vision || idiom == UserInterfaceIdiom::VisionLegacy;
}

bool currentUserInterfaceIdiomIsVisionLegacy()
{
    if (!s_currentUserInterfaceIdiom.load())
        updateCurrentUserInterfaceIdiom();
    return s_currentUserInterfaceIdiom.load() == UserInterfaceIdiom::VisionLegacy;
}

UserInterfaceIdiom currentUserInterfaceIdiom()
{
    if (!s_currentUserInterfaceIdiom.load())
        updateCurrentUserInterfaceIdiom();
    return s_currentUserInterfaceIdiom.load().value_or(UserInterfaceIdiom::Default);
}

void setCurrentUserInterfaceIdiom(UserInterfaceIdiom idiom)
{
    s_currentUserInterfaceIdiom = idiom;
}

static UserInterfaceIdiom determineVisionSubidiom()
{
#if PLATFORM(VISION)
    if (!_RSCurrentProcessUsesNewPointsPerMeter())
        return UserInterfaceIdiom::VisionLegacy;
#endif
    return UserInterfaceIdiom::Vision;
}

bool updateCurrentUserInterfaceIdiom()
{
    UserInterfaceIdiom oldIdiom = s_currentUserInterfaceIdiom.load().value_or(UserInterfaceIdiom::Default);

    // If we are in a daemon, we cannot use UIDevice. Fall back to checking the hardware itself.
    // Since daemons don't ever run in an iPhone-app-on-iPad jail, this will be accurate in the daemon case,
    // but is not sufficient in the application case.
    UserInterfaceIdiom newIdiom = [&] {
        if (![PAL::getUIApplicationClass() sharedApplication]) {
            if (PAL::deviceClassIsSmallScreen())
                return UserInterfaceIdiom::SmallScreen;
            if (PAL::deviceClassIsVision())
                return determineVisionSubidiom();
        } else {
            auto idiom = [[PAL::getUIDeviceClass() currentDevice] userInterfaceIdiom];
            if (idiom == UIUserInterfaceIdiomPhone || idiom == UIUserInterfaceIdiomWatch)
                return UserInterfaceIdiom::SmallScreen;
#if HAVE(UI_USER_INTERFACE_IDIOM_VISION)
            if (idiom == UIUserInterfaceIdiomVision)
                return determineVisionSubidiom();
#endif
        }

        return UserInterfaceIdiom::Default;
    }();

    if (s_currentUserInterfaceIdiom.load() && oldIdiom == newIdiom)
        return false;

    setCurrentUserInterfaceIdiom(newIdiom);
    return true;
}

} // namespace PAL

#endif
