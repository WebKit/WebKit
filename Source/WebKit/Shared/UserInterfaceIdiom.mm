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

#import "UIKitSPI.h"
#import <WebCore/Device.h>

namespace WebKit {

static std::optional<UserInterfaceIdiom> s_currentUserInterfaceIdiom;

bool currentUserInterfaceIdiomIsSmallScreen()
{
    if (!s_currentUserInterfaceIdiom)
        updateCurrentUserInterfaceIdiom();
    return s_currentUserInterfaceIdiom == UserInterfaceIdiom::SmallScreen;
}

bool currentUserInterfaceIdiomIsReality()
{
    if (!s_currentUserInterfaceIdiom)
        updateCurrentUserInterfaceIdiom();
    return s_currentUserInterfaceIdiom == UserInterfaceIdiom::Reality;
}

UserInterfaceIdiom currentUserInterfaceIdiom()
{
    if (!s_currentUserInterfaceIdiom)
        updateCurrentUserInterfaceIdiom();
    return s_currentUserInterfaceIdiom.value_or(UserInterfaceIdiom::Default);
}

void setCurrentUserInterfaceIdiom(UserInterfaceIdiom idiom)
{
    s_currentUserInterfaceIdiom = idiom;
}

bool updateCurrentUserInterfaceIdiom()
{
    UserInterfaceIdiom oldIdiom = s_currentUserInterfaceIdiom.value_or(UserInterfaceIdiom::Default);

    // If we are in a daemon, we cannot use UIDevice. Fall back to checking the hardware itself.
    // Since daemons don't ever run in an iPhone-app-on-iPad jail, this will be accurate in the daemon case,
    // but is not sufficient in the application case.
    UserInterfaceIdiom newIdiom = [&] {
        if (![UIApplication sharedApplication]) {
            if (WebCore::deviceClassIsSmallScreen())
                return UserInterfaceIdiom::SmallScreen;
            if (WebCore::deviceClassIsReality())
                return UserInterfaceIdiom::Reality;
        } else {
            auto idiom = [[UIDevice currentDevice] userInterfaceIdiom];
            if (idiom == UIUserInterfaceIdiomPhone || idiom == UIUserInterfaceIdiomWatch)
                return UserInterfaceIdiom::SmallScreen;
#if PLATFORM(VISION)
            if (idiom == UIUserInterfaceIdiomReality)
                return UserInterfaceIdiom::Reality;
#endif
        }

        return UserInterfaceIdiom::Default;
    }();

    if (oldIdiom == newIdiom)
        return false;

    setCurrentUserInterfaceIdiom(newIdiom);
    return true;
}

}

#endif
