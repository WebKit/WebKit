/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#import "WebMetalLayer.h"

#if ENABLE(WEBMETAL)

#import "GPUDevice.h"
#import "GraphicsContextCG.h"
#import "GraphicsLayer.h"
#import <wtf/FastMalloc.h>
#import <wtf/RetainPtr.h>

@implementation WebMetalLayer

@synthesize context=_context;

- (id)initWithGPUDevice:(WebCore::GPUDevice*)context
{
    self = [super init];
    _context = context;

    // FIXME: WebMetal - handle retina correctly.
    _devicePixelRatio = 1;

#if PLATFORM(MAC)
    self.contentsScale = _devicePixelRatio;
    self.colorspace = WebCore::sRGBColorSpaceRef();
#endif
    return self;
}

- (CGImageRef)copyImageSnapshotWithColorSpace:(CGColorSpaceRef)colorSpace
{
    if (!_context)
        return nullptr;

    UNUSED_PARAM(colorSpace);
    return nullptr;
}

- (void)display
{
    // This is a no-op. We don't need to
    // call display - instead we present
    // a drawable from our context.
}

@end

#endif
