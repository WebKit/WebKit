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
#include "ANGLEUtilitiesCocoa.h"

#if ENABLE(WEBGL)
#include "ANGLEHeaders.h"
#include "ANGLEUtilities.h"
#include "Logging.h"
#include <Metal/Metal.h>
#include <pal/spi/cocoa/MetalSPI.h>
#include <wtf/darwin/WeakLinking.h>

WTF_WEAK_LINK_FORCE_IMPORT(EGL_Initialize);

namespace WebCore {

bool platformIsANGLEAvailable()
{
    // The ANGLE is weak linked in full, and the EGL_Initialize is explicitly weak linked above
    // so that we can detect the case where ANGLE is not present.
    return EGL_Initialize != NULL; // NOLINT
}

void* createPbufferAndAttachIOSurface(GCGLDisplay display, GCGLConfig config, GCGLenum target, GCGLint usageHint, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLenum type, IOSurfaceRef surface, GCGLuint plane)
{
    auto eglTextureTarget = target == GL_TEXTURE_RECTANGLE_ANGLE ? EGL_TEXTURE_RECTANGLE_ANGLE : EGL_TEXTURE_2D;

    const EGLint surfaceAttributes[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_IOSURFACE_PLANE_ANGLE, static_cast<EGLint>(plane),
        EGL_TEXTURE_TARGET, static_cast<EGLint>(eglTextureTarget),
        EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, static_cast<EGLint>(internalFormat),
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TYPE_ANGLE, static_cast<EGLint>(type),
        // Only has an effect on the iOS Simulator.
        EGL_IOSURFACE_USAGE_HINT_ANGLE, usageHint,
        EGL_NONE, EGL_NONE
    };

    EGLSurface pbuffer = EGL_CreatePbufferFromClientBuffer(display, EGL_IOSURFACE_ANGLE, surface, config, surfaceAttributes);
    if (!pbuffer)
        return nullptr;

    if (!EGL_BindTexImage(display, pbuffer, EGL_BACK_BUFFER)) {
        EGL_DestroySurface(display, pbuffer);
        return nullptr;
    }

    return pbuffer;
}

void destroyPbufferAndDetachIOSurface(EGLDisplay display, void* handle)
{
    EGL_ReleaseTexImage(display, handle, EGL_BACK_BUFFER);
    EGL_DestroySurface(display, handle);
}

RetainPtr<MTLRasterizationRateMap> newRasterizationRateMap(GCGLDisplay display, IntSize physicalSize, IntSize screenSize, std::span<const float> horizontalSamples, std::span<const float> verticalSamples)
{
    EGLDeviceEXT device = EGL_NO_DEVICE_EXT;
    if (!EGL_QueryDisplayAttribEXT(display, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&device)))
        return nullptr;

    id<MTLDevice> mtlDevice = nil;
    if (!EGL_QueryDeviceAttribEXT(device, EGL_METAL_DEVICE_ANGLE, reinterpret_cast<EGLAttrib*>(&mtlDevice)))
        return nullptr;

    RetainPtr<MTLRasterizationRateMapDescriptor> descriptor = adoptNS([MTLRasterizationRateMapDescriptor new]);

    id<MTLRasterizationRateMapDescriptorSPI> descriptor_spi = (id<MTLRasterizationRateMapDescriptorSPI>)descriptor.get();
    descriptor_spi.skipSampleValidationAndApplySampleAtTileGranularity = YES;
    descriptor_spi.mutability = MTLMutabilityDefault;
    descriptor_spi.minFactor  = 0.01;

    auto mtlPhysicalSize = MTLSizeMake(physicalSize.width(), physicalSize.height(), 0);
    auto mtlScreenSize = MTLSizeMake(screenSize.width(), screenSize.height(), 0);
    RetainPtr<MTLRasterizationRateLayerDescriptor> layerDescriptor = [[MTLRasterizationRateLayerDescriptor alloc] initWithSampleCount:mtlScreenSize];

    memcpy([layerDescriptor horizontalSampleStorage], horizontalSamples.data(), horizontalSamples.size_bytes());
    memcpy([layerDescriptor verticalSampleStorage], verticalSamples.data(), verticalSamples.size_bytes());
    [layerDescriptor setSampleCount:MTLSizeMake(horizontalSamples.size(), verticalSamples.size(), 0)];

    [descriptor setScreenSize:mtlScreenSize];
    [descriptor layers][0] = layerDescriptor.get();

    for (unsigned i = 0; i < [layerDescriptor sampleCount].width; i++) {
        assert([layerDescriptor horizontalSampleStorage][i] > 0.01);
        assert([layerDescriptor horizontalSampleStorage][i] <= 1.0);
    }
    for (unsigned i = 0; i < [layerDescriptor sampleCount].height; i++) {
        assert([layerDescriptor verticalSampleStorage][i] > 0.01);
        assert([layerDescriptor verticalSampleStorage][i] <= 1.0);
    }

    RetainPtr rasterizationRateMap = [mtlDevice newRasterizationRateMapWithDescriptor:descriptor.get()];
    ASSERT([rasterizationRateMap screenSize].width == mtlScreenSize.width);
    ASSERT([rasterizationRateMap screenSize].height == mtlScreenSize.height);
    MTLSize physical_size = [rasterizationRateMap physicalSizeForLayer:0];
    ASSERT(physical_size.width == mtlPhysicalSize.width);
    ASSERT(physical_size.height == mtlPhysicalSize.height);

    return rasterizationRateMap;
}

RetainPtr<MTLSharedEvent> newSharedEventWithMachPort(GCGLDisplay display, mach_port_t machPort)
{
    // FIXME: Check for invalid mach_port_t
    EGLDeviceEXT device = EGL_NO_DEVICE_EXT;
    if (!EGL_QueryDisplayAttribEXT(display, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&device)))
        return nullptr;

    id<MTLDevice> mtlDevice = nil;
    if (!EGL_QueryDeviceAttribEXT(device, EGL_METAL_DEVICE_ANGLE, reinterpret_cast<EGLAttrib*>(&mtlDevice)))
        return nullptr;

    return adoptNS([(id<MTLDeviceSPI>)mtlDevice newSharedEventWithMachPort:machPort]);
}

RetainPtr<MTLSharedEvent> newSharedEvent(GCGLDisplay display)
{
    EGLDeviceEXT device = EGL_NO_DEVICE_EXT;
    if (!EGL_QueryDisplayAttribEXT(display, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&device)))
        return nullptr;

    id<MTLDevice> mtlDevice = nil;
    if (!EGL_QueryDeviceAttribEXT(device, EGL_METAL_DEVICE_ANGLE, reinterpret_cast<EGLAttrib*>(&mtlDevice)))
        return nullptr;

    return adoptNS([mtlDevice newSharedEvent]);
}

}

#endif
