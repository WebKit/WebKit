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

#import "GPULegacyTest.h"
#import "PlatformUtilities.h"
#import <Metal/Metal.h>
#import <WebCore/GPULegacyDevice.h>
#import <WebCore/GPULegacyFunction.h>
#import <WebCore/GPULegacyLibrary.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST_F(GPULegacy, FunctionAccessByName)
{
    GPULegacyDevice device;

    // Not all hardware supports Metal, so it is possible
    // that we were unable to create the MTLDevice object.
    // In that case, the device should be null.
    if (!device)
        return;

    EXPECT_NOT_NULL(device.metal());

    GPULegacyLibrary library { device, librarySourceCode() };

    GPULegacyFunction vertexFunction { library, "vertex_main"_s };
    EXPECT_NOT_NULL(vertexFunction.metal());
    EXPECT_WK_STREQ("vertex_main", vertexFunction.name());

    GPULegacyFunction fragmentFunction { library, "fragment_main"_s };
    EXPECT_NOT_NULL(fragmentFunction.metal());
    EXPECT_WK_STREQ("fragment_main", fragmentFunction.name());

    GPULegacyFunction nonexistentFunction { library, "name_that_is_not_in_library"_s };
    EXPECT_NULL(nonexistentFunction.metal());
}

} // namespace TestWebKitAPI

#endif
