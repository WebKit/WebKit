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
#import <JavaScriptCore/ArrayBuffer.h>
#import <JavaScriptCore/ArrayBufferView.h>
#import <JavaScriptCore/GenericTypedArrayViewInlines.h>
#import <JavaScriptCore/JSCInlines.h>
#import <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#import <JavaScriptCore/TypedArrays.h>
#import <Metal/Metal.h>
#import <WebCore/GPUBuffer.h>
#import <WebCore/GPUDevice.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST_F(GPU, BufferCreate)
{
    GPUDevice device;

    // Not all hardware supports Metal, so it is possible
    // that we were unable to create the MTLDevice object.
    // In that case, the device should be null.
    if (!device)
        return;

    EXPECT_NOT_NULL(device.metal());

    auto bufferView = JSC::Uint8Array::create(1024);

    auto data = bufferView->data();
    memset(data, 1, bufferView->byteLength());

    GPUBuffer buffer { device, bufferView.get() };
    EXPECT_EQ(1024U, buffer.length());

    auto contents = buffer.contents();
    EXPECT_NOT_NULL(contents);
    EXPECT_EQ(1024U, contents->byteLength());

    auto contentsData = static_cast<uint8_t*>(contents->data());
    EXPECT_NE(data, contentsData);
    EXPECT_EQ(1U, contentsData[0]);
    EXPECT_EQ(1U, contentsData[512]);
    EXPECT_EQ(1U, contentsData[1023]);

    EXPECT_NOT_NULL(buffer.metal());
}

} // namespace TestWebKitAPI

#endif
