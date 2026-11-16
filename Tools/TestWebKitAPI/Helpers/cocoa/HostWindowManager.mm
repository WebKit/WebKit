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
#import "Helpers/cocoa/HostWindowManager.h"

#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

namespace TestWebKitAPI {

HostWindowManager& HostWindowManager::singleton()
{
    static HostWindowManager* singleton = new HostWindowManager();
    return *singleton;
}

HostWindowManager::HostWindowManager()
{
    testing::UnitTest::GetInstance()->listeners().Append(this);
}

void HostWindowManager::closeHostWindowOnTestEnd(Util::PlatformWindow *hostWindow)
{
    m_hostWindows.append(hostWindow);
}

void HostWindowManager::OnTestEnd(const testing::TestInfo&)
{
    for (RetainPtr hostWindow : std::exchange(m_hostWindows, { })) {
#if PLATFORM(MAC)
        [hostWindow close];
#else
        [hostWindow setHidden:YES];
        [hostWindow setWindowScene:nil];
#endif
    }
}

} // namespace TestWebKitAPI
