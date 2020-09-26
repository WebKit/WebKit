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
    IsPad,
    IsNotPad,
    Unknown,
};

static UserInterfaceIdiomState userInterfaceIdiomIsPadState = UserInterfaceIdiomState::Unknown;

static inline bool userInterfaceIdiomIsPad()
{
#if PLATFORM(MACCATALYST)
    // UIKit varies the UIUserInterfaceIdiom between iPad and macOS in macCatalyst, depending on various settings,
    // but for the purposes of WebKit we always want to use iPad behavior (vs. iPhone) in macCatalyst.
    // FIXME: We should get rid of this function and have callers make explicit decisions for all of iPhone/iPad/macOS.
    return true;
#else
    // If we are in a daemon, we cannot use UIDevice. Fall back to checking the hardware itself.
    // Since daemons don't ever run in an iPhone-app-on-iPad jail, this will be accurate in the daemon case,
    // but is not sufficient in the application case.
    if (![UIApplication sharedApplication])
        return WebCore::deviceClass() == MGDeviceClassiPad;

    // This inline function exists to thwart unreachable code
    // detection on platforms where UICurrentUserInterfaceIdiomIsPad
    // is defined directly to false.
#if USE(APPLE_INTERNAL_SDK)
    return UICurrentUserInterfaceIdiomIsPad();
#else
    return [[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad;
#endif
#endif
}

bool currentUserInterfaceIdiomIsPadOrMac()
{
    if (userInterfaceIdiomIsPadState == UserInterfaceIdiomState::Unknown)
        setCurrentUserInterfaceIdiomIsPadOrMac(userInterfaceIdiomIsPad());

    return userInterfaceIdiomIsPadState == UserInterfaceIdiomState::IsPad;
}

void setCurrentUserInterfaceIdiomIsPadOrMac(bool isPad)
{
    userInterfaceIdiomIsPadState = isPad ? UserInterfaceIdiomState::IsPad : UserInterfaceIdiomState::IsNotPad;
}

}

#endif
