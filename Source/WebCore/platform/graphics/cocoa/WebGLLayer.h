/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

#import "IOSurface.h"
#import <QuartzCore/QuartzCore.h>
#import <wtf/NakedPtr.h>

namespace WebCore {
struct WebGLLayerBuffer {
    std::unique_ptr<WebCore::IOSurface> surface; // The actual contents.
    void* handle { nullptr }; // Client specific metadata handle (such as EGLSurface).
};
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

// A layer class implementing front buffer management of a 3-buffering swap
// chain of IOSurfaces.
// The layer will own the IOSurfaces it uses for display.
// The client may attach metadata to each IOSurface and will receive the metadata
// back once the IOSurface has been displayed. However, the client may not neccessarily
// be able to obtain the IOSurface itself for reuse.
// Example use of the metadata is to use EGLSurface binding as the metadata. This way
// when the WebGLLayer is done with the IOSurface display, the client can continue using
// the existing binding obtained through the buffer recycle logic.
@interface WebGLLayer : CALayer

- (id)initWithDevicePixelRatio:(float)devicePixelRatio contentsOpaque:(bool)contentsOpaque;

- (CGImageRef)copyImageSnapshotWithColorSpace:(CGColorSpaceRef)colorSpace;

// Returns the metadata handle of last unused contents buffer.
// Client may recieve back also the ownership of the contents surface, in case it is available at the
// time of the call.
// Returns either:
// - Empty buffer if no buffer has been submitted.
// - Buffer with empty surface and non-empty metadata handle if the recycled buffer was available
//   but the surface is still in use.
// - Surface and handle.
- (WebCore::WebGLLayerBuffer)recycleBuffer;

// Prepares the layer for display with a contents buffer.
// Client transfers the ownership of the IOSurface surface in the `buffer`.
- (void)prepareForDisplayWithContents:(WebCore::WebGLLayerBuffer)buffer;

// Detaches the client and returns the current contents buffer metadata handle.
// The if multiple buffers have been submitted, recycleBuffer must have been called before calling
// this.
- (void*)detachClient;

@end

ALLOW_DEPRECATED_DECLARATIONS_END
