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
#include <wtf/SoftLinking.h>
#include <wtf/darwin/WeakLinking.h>

#if USE(APPLE_INTERNAL_SDK) && PLATFORM(VISION)
#include <CompositorServices/CompositorServices_Private.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebCore, CompositorServices)

SOFT_LINK_CLASS_FOR_HEADER(WebCore, CP_OBJECT_cp_proxy_process_rasterization_rate_map)
typedef CP_OBJECT_cp_proxy_process_rasterization_rate_map* cp_proxy_process_rasterization_rate_map_t;

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CompositorServices, cp_proxy_process_rasterization_rate_map_create, cp_proxy_process_rasterization_rate_map_t, (id<MTLDevice> device, cp_layer_renderer_layout layout, size_t view_count), (device, layout, view_count))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CompositorServices, cp_proxy_process_rasterization_rate_map_create, cp_proxy_process_rasterization_rate_map_t, (id<MTLDevice> device, cp_layer_renderer_layout layout, size_t view_count), (device, layout, view_count))
#define cp_proxy_process_rasterization_rate_map_create softLink_CompositorServices_cp_proxy_process_rasterization_rate_map_create


SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CompositorServices, cp_rasterization_rate_map_update_shared_from_layered_descriptor, void, (cp_proxy_process_rasterization_rate_map_t proxy_map, MTLRasterizationRateMapDescriptor* descriptor), (proxy_map, descriptor))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CompositorServices, cp_rasterization_rate_map_update_shared_from_layered_descriptor, void, (cp_proxy_process_rasterization_rate_map_t proxy_map, MTLRasterizationRateMapDescriptor* descriptor), (proxy_map, descriptor))
#define cp_rasterization_rate_map_update_shared_from_layered_descriptor softLink_CompositorServices_cp_rasterization_rate_map_update_shared_from_layered_descriptor


SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CompositorServices, cp_proxy_process_rasterization_rate_map_get_metal_maps, NSArray<id<MTLRasterizationRateMap>>*, (cp_proxy_process_rasterization_rate_map_t proxy_map), (proxy_map))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CompositorServices, cp_proxy_process_rasterization_rate_map_get_metal_maps, NSArray<id<MTLRasterizationRateMap>>*, (cp_proxy_process_rasterization_rate_map_t proxy_map), (proxy_map))
#define cp_proxy_process_rasterization_rate_map_get_metal_maps softLink_CompositorServices_cp_proxy_process_rasterization_rate_map_get_metal_maps


SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CompositorServices, cp_proxy_process_rasterization_rate_map_get_metal_descriptors, NSArray<MTLRasterizationRateMapDescriptor*>*, (cp_proxy_process_rasterization_rate_map_t proxy_map), (proxy_map))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CompositorServices, cp_rasterization_rate_map_update_from_descriptor, void, (cp_proxy_process_rasterization_rate_map_t proxy_map, __unsafe_unretained MTLRasterizationRateMapDescriptor* descriptors[2]), (proxy_map, descriptors))
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, CompositorServices, CP_OBJECT_cp_proxy_process_rasterization_rate_map)

SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CompositorServices, cp_drawable_get_layer_renderer_layout, cp_layer_renderer_layout_private, (cp_drawable_t drawable), (drawable))

SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CompositorServices, cp_proxy_process_rasterization_rate_map_get_metal_descriptors, NSArray<MTLRasterizationRateMapDescriptor*>*, (cp_proxy_process_rasterization_rate_map_t proxy_map), (proxy_map))

SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CompositorServices, cp_rasterization_rate_map_update_from_descriptor, void, (cp_proxy_process_rasterization_rate_map_t proxy_map, __unsafe_unretained MTLRasterizationRateMapDescriptor* descriptors[2]), (proxy_map, descriptors))

#define cp_proxy_process_rasterization_rate_map_get_metal_descriptors softLink_CompositorServices_cp_proxy_process_rasterization_rate_map_get_metal_descriptors
#define cp_proxy_process_rasterization_rate_map_get_metal_descriptors softLink_CompositorServices_cp_proxy_process_rasterization_rate_map_get_metal_descriptors
#define cp_rasterization_rate_map_update_from_descriptor softLink_CompositorServices_cp_rasterization_rate_map_update_from_descriptor
#define cp_drawable_get_layer_renderer_layout softLink_CompositorServices_cp_drawable_get_layer_renderer_layout

#endif

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

RetainPtr<MTLRasterizationRateMap> newRasterizationRateMap(GCGLDisplay display, IntSize physicalSizeLeft, IntSize physicalSizeRight, IntSize screenSize, std::span<const float> horizontalSamplesLeft, std::span<const float> verticalSamples, std::span<const float> horizontalSamplesRight)
{
    EGLDeviceEXT device = EGL_NO_DEVICE_EXT;
    if (!EGL_QueryDisplayAttribEXT(display, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&device)))
        return nullptr;

    id<MTLDevice> mtlDevice = nil;
    if (!EGL_QueryDeviceAttribEXT(device, EGL_METAL_DEVICE_ANGLE, reinterpret_cast<EGLAttrib*>(&mtlDevice)))
        return nullptr;

    UNUSED_PARAM(physicalSizeLeft);
    UNUSED_PARAM(physicalSizeRight);

#if USE(APPLE_INTERNAL_SDK) && PLATFORM(VISION)
    RetainPtr<MTLRasterizationRateMapDescriptor> descriptor = adoptNS([MTLRasterizationRateMapDescriptor new]);
    id<MTLRasterizationRateMapDescriptorSPI> descriptor_spi = (id<MTLRasterizationRateMapDescriptorSPI>)descriptor.get();
        descriptor_spi.skipSampleValidationAndApplySampleAtTileGranularity = YES;
    descriptor_spi.mutability = MTLMutabilityMutable;
    descriptor_spi.minFactor  = 0.01;

    constexpr MTLSize maxSampleCount { 256, 256, 1 };
    RetainPtr<MTLRasterizationRateLayerDescriptor> layerDescriptorLeft = adoptNS([[MTLRasterizationRateLayerDescriptor alloc] initWithSampleCount:maxSampleCount]);
    RetainPtr<MTLRasterizationRateLayerDescriptor> layerDescriptorRight = adoptNS([[MTLRasterizationRateLayerDescriptor alloc] initWithSampleCount:maxSampleCount]);

    if (horizontalSamplesLeft.size() > maxSampleCount.width || horizontalSamplesRight.size() > maxSampleCount.width || verticalSamples.size() > maxSampleCount.height || !layerDescriptorLeft.get() || !layerDescriptorRight.get())
        return nullptr;

    memcpy([layerDescriptorLeft horizontalSampleStorage], horizontalSamplesLeft.data(), horizontalSamplesLeft.size_bytes());
    memcpy([layerDescriptorLeft verticalSampleStorage], verticalSamples.data(), verticalSamples.size_bytes());
    [layerDescriptorLeft setSampleCount:MTLSizeMake(horizontalSamplesLeft.size(), verticalSamples.size(), 0)];

    memcpy([layerDescriptorRight horizontalSampleStorage], horizontalSamplesRight.data(), horizontalSamplesRight.size_bytes());
    memcpy([layerDescriptorRight verticalSampleStorage], verticalSamples.data(), verticalSamples.size_bytes());
    [layerDescriptorRight setSampleCount:MTLSizeMake(horizontalSamplesRight.size(), verticalSamples.size(), 0)];

    [descriptor setScreenSize:MTLSizeMake(screenSize.width(), screenSize.height(), 0)];
    [descriptor layers][0] = layerDescriptorLeft.get();
    [descriptor layers][1] = layerDescriptorRight.get();

    auto rateMap = cp_proxy_process_rasterization_rate_map_create(mtlDevice, cp_layer_renderer_layout_shared, 2);
    cp_rasterization_rate_map_update_shared_from_layered_descriptor(rateMap, descriptor.get());

    RetainPtr<MTLRasterizationRateMap> rasterizationRateMap = cp_proxy_process_rasterization_rate_map_get_metal_maps(rateMap).firstObject;
#else
    RetainPtr<MTLRasterizationRateMap> rasterizationRateMap;
    UNUSED_PARAM(display);
    UNUSED_PARAM(physicalSizeLeft);
    UNUSED_PARAM(physicalSizeRight);
    UNUSED_PARAM(screenSize);
    UNUSED_PARAM(horizontalSamplesLeft);
    UNUSED_PARAM(verticalSamples);
    UNUSED_PARAM(horizontalSamplesRight);
#endif
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
