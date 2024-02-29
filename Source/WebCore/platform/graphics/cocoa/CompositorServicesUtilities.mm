/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CompositorServicesUtilities.h"

#if ENABLE(WEBXR)

#import "ANGLEHeaders.h"
#import <Metal/Metal.h>
#import <pal/spi/cocoa/MetalSPI.h>

#if USE(APPLE_INTERNAL_SDK)

#import <CompositorServices/CompositorServices_Private.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebCore, CompositorServices)

SOFT_LINK_CLASS_FOR_HEADER(WebCore, CP_OBJECT_cp_proxy_process_rasterization_rate_map)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, CompositorServices, CP_OBJECT_cp_proxy_process_rasterization_rate_map)
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

#endif

namespace WebCore {

RetainPtr<MTLRasterizationRateMap> newRasterizationRateMapForSharedFixedFoveation(GCGLDisplay display, IntSize physicalSize, IntSize screenSize, std::span<const float> horizontalSamplesLeft, std::span<const float> horizontalSamplesRight, std::span<const float> verticalSamples)
{
#if USE(APPLE_INTERNAL_SDK) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
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

    // FIXME(djg): This is dubious!
    auto mtlScreenSize = MTLSizeMake(screenSize.width(), screenSize.height(), 0);
    if (horizontalSamplesLeft.size() > mtlScreenSize.width || horizontalSamplesRight.size() > mtlScreenSize.width || verticalSamples.size() > mtlScreenSize.height)
        return nullptr;

    RetainPtr<MTLRasterizationRateLayerDescriptor> layerDescriptorLeft = [[MTLRasterizationRateLayerDescriptor alloc] initWithSampleCount:mtlScreenSize];

    memcpy([layerDescriptorLeft horizontalSampleStorage], horizontalSamplesLeft.data(), horizontalSamplesLeft.size_bytes());
    memcpy([layerDescriptorLeft verticalSampleStorage], verticalSamples.data(), verticalSamples.size_bytes());
    [layerDescriptorLeft setSampleCount:MTLSizeMake(horizontalSamplesLeft.size(), verticalSamples.size(), 0)];

    RetainPtr<MTLRasterizationRateLayerDescriptor> layerDescriptorRight = [[MTLRasterizationRateLayerDescriptor alloc] initWithSampleCount:mtlScreenSize];

    memcpy([layerDescriptorRight horizontalSampleStorage], horizontalSamplesRight.data(), horizontalSamplesRight.size_bytes());
    memcpy([layerDescriptorRight verticalSampleStorage], verticalSamples.data(), verticalSamples.size_bytes());
    [layerDescriptorRight setSampleCount:MTLSizeMake(horizontalSamplesRight.size(), verticalSamples.size(), 0)];

    [descriptor setScreenSize:mtlScreenSize];
    [descriptor layers][0] = layerDescriptorLeft.get();
    [descriptor layers][1] = layerDescriptorRight.get();

    auto rateMap = cp_proxy_process_rasterization_rate_map_create(mtlDevice, cp_layer_renderer_layout_shared, 2);
    cp_rasterization_rate_map_update_shared_from_layered_descriptor(rateMap, descriptor.get());

    RetainPtr<MTLRasterizationRateMap> rasterizationRateMap = cp_proxy_process_rasterization_rate_map_get_metal_maps(rateMap).firstObject;

    MTLSize mtlPhysicalSizeLeft = [rasterizationRateMap physicalSizeForLayer:0];
    MTLSize mtlPhysicalSizeRight = [rasterizationRateMap physicalSizeForLayer:1];
    ASSERT_UNUSED(mtlPhysicalSizeLeft, (NSUInteger) physicalSize.width() == mtlPhysicalSizeLeft.width);
    ASSERT_UNUSED(mtlPhysicalSizeLeft, (NSUInteger) physicalSize.height() == mtlPhysicalSizeLeft.height);
    ASSERT_UNUSED(mtlPhysicalSizeRight, (NSUInteger) physicalSize.width() == mtlPhysicalSizeRight.width);
    ASSERT_UNUSED(mtlPhysicalSizeRight, (NSUInteger) physicalSize.height() == mtlPhysicalSizeRight.height);

    RELEASE_ASSERT(rasterizationRateMap.get());
    return rasterizationRateMap;
#else
    UNUSED_PARAM(display);
    UNUSED_PARAM(physicalSize);
    UNUSED_PARAM(screenSize);
    UNUSED_PARAM(horizontalSamples);
    UNUSED_PARAM(verticalSamples);
    return nullptr;
#endif
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
