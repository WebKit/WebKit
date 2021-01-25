/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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

enum class UserInterfaceIdiomState : uint8_t {
    IsPadOrMac,
    IsNotPadOrMac,
    Unknown,
};

static UserInterfaceIdiomState userInterfaceIdiomIsPadState = UserInterfaceIdiomState::Unknown;

bool currentUserInterfaceIdiomIsPadOrMac()
{
    // FIXME: We should get rid of this function and have callers make explicit decisions for all of iPhone/iPad/macOS.

    if (userInterfaceIdiomIsPadState == UserInterfaceIdiomState::Unknown)
        updateCurrentUserInterfaceIdiom();

    return userInterfaceIdiomIsPadState == UserInterfaceIdiomState::IsPadOrMac;
}

void setCurrentUserInterfaceIdiomIsPadOrMac(bool isPadOrMac)
{
    userInterfaceIdiomIsPadState = isPadOrMac ? UserInterfaceIdiomState::IsPadOrMac : UserInterfaceIdiomState::IsNotPadOrMac;
}

bool updateCurrentUserInterfaceIdiom()
{
    bool wasPadOrMac = userInterfaceIdiomIsPadState == UserInterfaceIdiomState::IsPadOrMac;
    bool isPadOrMac = false;

    // If we are in a daemon, we cannot use UIDevice. Fall back to checking the hardware itself.
    // Since daemons don't ever run in an iPhone-app-on-iPad jail, this will be accurate in the daemon case,
    // but is not sufficient in the application case.
    if (![UIApplication sharedApplication]) {
        auto deviceClass = WebCore::deviceClass();
        isPadOrMac = deviceClass == MGDeviceClassiPad || deviceClass == MGDeviceClassMac;
    } else {
        auto idiom = [[UIDevice currentDevice] userInterfaceIdiom];
        isPadOrMac = idiom == UIUserInterfaceIdiomPad || idiom == UIUserInterfaceIdiomMac;
    }

    if (wasPadOrMac == isPadOrMac)
        return false;

    setCurrentUserInterfaceIdiomIsPadOrMac(isPadOrMac);
    return true;
}

}

#endif
