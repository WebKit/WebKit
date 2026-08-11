//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceWgpuMetalLayer.cpp:
//    Defines the class interface for WindowSurfaceWgpuMetalLayer, implementing WindowSurfaceWgpu.
//

#include "libANGLE/renderer/wgpu/mac/WindowSurfaceWgpuMetalLayer.h"

#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

#include "libANGLE/Display.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{

WindowSurfaceWgpuMetalLayer::WindowSurfaceWgpuMetalLayer(const egl::SurfaceState &surfaceState,
                                                         EGLNativeWindowType window)
    : WindowSurfaceWgpu(surfaceState, window)
{}

egl::Error WindowSurfaceWgpuMetalLayer::initialize(const egl::Display *display)
{
    // TODO: Use the same Metal device as wgpu
    mMetalDevice = MTLCreateSystemDefaultDevice();

    return WindowSurfaceWgpu::initialize(display);
}

void WindowSurfaceWgpuMetalLayer::destroy(const egl::Display *display)
{
    WindowSurfaceWgpu::destroy(display);
    [mMetalDevice release];
    if (mMetalLayer)
    {
        [mMetalLayer removeFromSuperlayer];
        [mMetalLayer release];
    }
}

angle::Result WindowSurfaceWgpuMetalLayer::createWgpuSurface(const egl::Display *display,
                                                             webgpu::SurfaceHandle *outSurface)
    API_AVAILABLE(macosx(10.11))
{
    CALayer *layer = reinterpret_cast<CALayer *>(getNativeWindow());

    mMetalLayer        = [[CAMetalLayer alloc] init];
    mMetalLayer.frame  = CGRectMake(0, 0, layer.frame.size.width, layer.frame.size.height);
    mMetalLayer.device = mMetalDevice;
    mMetalLayer.drawableSize =
        CGSizeMake(mMetalLayer.bounds.size.width * mMetalLayer.contentsScale,
                   mMetalLayer.bounds.size.height * mMetalLayer.contentsScale);
    mMetalLayer.framebufferOnly  = NO;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    mMetalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
#endif
    mMetalLayer.contentsScale    = layer.contentsScale;

    [layer addSublayer:mMetalLayer];

    WGPUSurfaceSourceMetalLayer metalLayerDesc = WGPU_SURFACE_SOURCE_METAL_LAYER_INIT;
    metalLayerDesc.layer = mMetalLayer;

    WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
    surfaceDesc.nextInChain           = &metalLayerDesc.chain;

    DisplayWgpu *displayWgpu = webgpu::GetImpl(display);
    const DawnProcTable *wgpu       = displayWgpu->getProcs();
    webgpu::InstanceHandle instance = displayWgpu->getInstance();

    webgpu::SurfaceHandle surface = webgpu::SurfaceHandle::Acquire(
        wgpu, wgpu->instanceCreateSurface(instance.get(), &surfaceDesc));
    *outSurface           = surface;

    return angle::Result::Continue;
}

angle::Result WindowSurfaceWgpuMetalLayer::getCurrentWindowSize(const egl::Display *display,
                                                                gl::Extents *outSize)
    API_AVAILABLE(macosx(10.11))
{
    ASSERT(mMetalLayer != nullptr);

    mMetalLayer.drawableSize =
        CGSizeMake(mMetalLayer.bounds.size.width * mMetalLayer.contentsScale,
                   mMetalLayer.bounds.size.height * mMetalLayer.contentsScale);
    *outSize = gl::Extents(static_cast<int>(mMetalLayer.drawableSize.width),
                           static_cast<int>(mMetalLayer.drawableSize.height), 1);

    return angle::Result::Continue;
}

WindowSurfaceWgpu *CreateWgpuWindowSurface(const egl::SurfaceState &surfaceState,
                                           EGLNativeWindowType window)
{
    return new WindowSurfaceWgpuMetalLayer(surfaceState, window);
}
}  // namespace rx
