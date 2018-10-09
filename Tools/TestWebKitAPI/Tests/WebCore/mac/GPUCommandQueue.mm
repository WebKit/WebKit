/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBMETAL)

#import "GPUTest.h"
#import "PlatformUtilities.h"
#import <Metal/Metal.h>
#import <WebCore/GPUCommandQueue.h>
#import <WebCore/GPUDevice.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST_F(GPU, CommandQueueCreate)
{
    GPUDevice device;

    // Not all hardware supports Metal, so it is possible
    // that we were unable to create the MTLDevice object.
    // In that case, the device should be null.
    if (!device)
        return;

    GPUCommandQueue commandQueue { device };
    EXPECT_WK_STREQ("", commandQueue.label());

    EXPECT_NOT_NULL(commandQueue.metal());

    // If you haven't set a label, we just use the prefix.
    EXPECT_STREQ("com.apple.WebKit", commandQueue.metal().label.UTF8String);

    commandQueue.setLabel("this.is.a.test"_s);

    // The WebKit API doesn't prefix.
    EXPECT_WK_STREQ("this.is.a.test", commandQueue.label());

    // But the underlying Metal object label will have one.
    EXPECT_STREQ("com.apple.WebKit.this.is.a.test", commandQueue.metal().label.UTF8String);
}

} // namespace TestWebKitAPI

#endif
