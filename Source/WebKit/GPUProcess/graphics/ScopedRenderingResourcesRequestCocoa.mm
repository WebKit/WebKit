/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScopedRenderingResourcesRequest.h"

#if PLATFORM(COCOA)

#import <Metal/Metal.h>
#import <objc/NSObjCRuntime.h>
#import <pal/spi/cocoa/MetalSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Seconds.h>

OBJC_PROTOCOL(MTLDevice);
OBJC_PROTOCOL(MTLDeviceSPI);

namespace WebKit {

static constexpr Seconds freeRenderingResourcesTimeout = 1_s;
static bool didScheduleFreeRenderingResources;

void ScopedRenderingResourcesRequest::scheduleFreeRenderingResources()
{
    if (didScheduleFreeRenderingResources)
        return;
    RunLoop::main().dispatchAfter(freeRenderingResourcesTimeout, freeRenderingResources);
    didScheduleFreeRenderingResources = true;
}

void ScopedRenderingResourcesRequest::freeRenderingResources()
{
    didScheduleFreeRenderingResources = false;
    if (s_requests)
        return;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
#if PLATFORM(MAC)
    auto devices = adoptNS(MTLCopyAllDevices());
    for (id <MTLDevice> device : devices.get())
        [(_MTLDevice *)device _purgeDevice];
#else
    RetainPtr<MTLDevice> devicePtr = adoptNS(MTLCreateSystemDefaultDevice());
    [(_MTLDevice *)devicePtr.get() _purgeDevice];
#endif
    END_BLOCK_OBJC_EXCEPTIONS
}

}

#endif
