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
    auto device = GPUDevice::create();
    // Not all hardware supports Metal, so it is possible
    // that we were unable to create the MTLDevice object.
    // In that case, the device should be null.
    if (!device)
        return;

    id<MTLDevice> mtlDevice = (id<MTLDevice>)device->platformDevice();
    EXPECT_NOT_NULL(mtlDevice);

    auto bufferView = JSC::Uint8Array::create(1024);

    uint8_t* data = bufferView->data();
    memset(data, 1, bufferView->byteLength());

    auto buffer = device->createBufferFromData(bufferView.get());
    EXPECT_NOT_NULL(buffer);
    EXPECT_EQ(buffer->length(), static_cast<unsigned long>(1024));

    auto contents = buffer->contents();
    EXPECT_NOT_NULL(contents);
    EXPECT_EQ(contents->byteLength(), static_cast<unsigned long>(1024));

    uint8_t* contentsData = static_cast<uint8_t*>(contents->data());
    EXPECT_NE(contentsData, data);
    EXPECT_EQ(contentsData[0], 1);
    EXPECT_EQ(contentsData[512], 1);
    EXPECT_EQ(contentsData[1023], 1);

    MTLBuffer *mtlBuffer = buffer->platformBuffer();
    EXPECT_NOT_NULL(mtlBuffer);


}

} // namespace TestWebKitAPI

#endif
