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

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/UserInterfaceIdiomAdditionsBefore.mm>
#endif

#import <pal/ios/UIKitSoftLink.h>

namespace PAL {

static std::atomic<std::optional<UserInterfaceIdiom>> s_currentUserInterfaceIdiom;

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/UserInterfaceIdiomAdditionsAfter.mm>
#else
static bool shouldForceUserInterfaceIdiomSmallScreen(std::optional<UIUserInterfaceIdiom> = std::nullopt)
{
    return false;
}
#endif

bool currentUserInterfaceIdiomIsDesktop()
{
    if (!s_currentUserInterfaceIdiom.load())
        updateCurrentUserInterfaceIdiom();
    auto idiom = *s_currentUserInterfaceIdiom.load();
    return idiom == UserInterfaceIdiom::Desktop;
}

bool currentUserInterfaceIdiomIsSmallScreen()
{
    if (!s_currentUserInterfaceIdiom.load())
        updateCurrentUserInterfaceIdiom();
    return s_currentUserInterfaceIdiom.load() == UserInterfaceIdiom::SmallScreen;
}

bool currentUserInterfaceIdiomIsVision()
{
    if (!s_currentUserInterfaceIdiom.load())
        updateCurrentUserInterfaceIdiom();
    auto idiom = *s_currentUserInterfaceIdiom.load();
    return idiom == UserInterfaceIdiom::Vision;
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

bool updateCurrentUserInterfaceIdiom()
{
    UserInterfaceIdiom oldIdiom = s_currentUserInterfaceIdiom.load().value_or(UserInterfaceIdiom::Default);

    // If we are in a daemon, we cannot use UIDevice. Fall back to checking the hardware itself.
    // Since daemons don't ever run in an iPhone-app-on-iPad jail, this will be accurate in the daemon case,
    // but is not sufficient in the application case.
    UserInterfaceIdiom newIdiom = [&] {
        if (![PAL::getUIApplicationClassSingleton() sharedApplication]) {
            if (PAL::deviceClassIsDesktop())
                return UserInterfaceIdiom::Desktop;
            if (PAL::deviceClassIsSmallScreen() || shouldForceUserInterfaceIdiomSmallScreen())
                return UserInterfaceIdiom::SmallScreen;
            if (PAL::deviceClassIsVision())
                return UserInterfaceIdiom::Vision;
        } else {
            auto idiom = [[PAL::getUIDeviceClassSingleton() currentDevice] userInterfaceIdiom];
            if (idiom == UIUserInterfaceIdiomPad || idiom == UIUserInterfaceIdiomMac)
                return UserInterfaceIdiom::Desktop;
            if (idiom == UIUserInterfaceIdiomPhone || idiom == UIUserInterfaceIdiomWatch || shouldForceUserInterfaceIdiomSmallScreen(idiom))
                return UserInterfaceIdiom::SmallScreen;
#if HAVE(UI_USER_INTERFACE_IDIOM_VISION)
            if (idiom == UIUserInterfaceIdiomVision)
                return UserInterfaceIdiom::Vision;
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
