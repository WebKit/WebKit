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

#if ENABLE(WEBGPU)

#import "GPUTest.h"
#import "PlatformUtilities.h"
#import <Metal/Metal.h>
#import <WebCore/GPUCommandQueue.h>
#import <WebCore/GPUDevice.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST_F(GPU, CommandQueueCreate)
{
    auto device = GPUDevice::create();
    // Not all hardware supports Metal, so it is possible
    // that we were unable to create the MTLDevice object.
    // In that case, the device should be null.
    if (!device)
        return;

    auto commandQueue = device->createCommandQueue();
    EXPECT_NOT_NULL(commandQueue);
    EXPECT_WK_STREQ(commandQueue->label(), emptyString());

    id<MTLCommandQueue> mtlCommandQueue = (id<MTLCommandQueue>)commandQueue->platformCommandQueue();
    EXPECT_NOT_NULL(mtlCommandQueue);

    // If you haven't set a label, we just use the prefix.
    EXPECT_STREQ("com.apple.WebKit", [mtlCommandQueue.label UTF8String]);

    String testLabel("this.is.a.test"_s);
    commandQueue->setLabel(testLabel);

    // The WebKit API doesn't prefix.
    EXPECT_WK_STREQ(commandQueue->label(), testLabel);

    // But the underlying Metal object label will have one.
    EXPECT_STREQ("com.apple.WebKit.this.is.a.test", [mtlCommandQueue.label UTF8String]);
}

} // namespace TestWebKitAPI

#endif
