/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
#import "PresentationContextIOSurface.h"

#import "APIConversions.h"
#import "Texture.h"
#import "TextureView.h"
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/IOTypesSPI.h>

namespace WebGPU {

Ref<PresentationContextIOSurface> PresentationContextIOSurface::create(const WGPUSurfaceDescriptor& surfaceDescriptor, const Instance& instance)
{
    auto presentationContextIOSurface = adoptRef(*new PresentationContextIOSurface(surfaceDescriptor, instance));

    ASSERT(surfaceDescriptor.nextInChain);
    ASSERT(surfaceDescriptor.nextInChain->sType == static_cast<WGPUSType>(WGPUSTypeExtended_SurfaceDescriptorCocoaSurfaceBacking));
    const auto& descriptor = *reinterpret_cast<const WGPUSurfaceDescriptorCocoaCustomSurface*>(surfaceDescriptor.nextInChain);
    descriptor.compositorIntegrationRegister([presentationContext = presentationContextIOSurface.copyRef()](CFArrayRef ioSurfaces) {
        presentationContext->renderBuffersWereRecreated(bridge_cast(ioSurfaces));
    }, [presentationContext = presentationContextIOSurface.copyRef()](WGPUWorkItem workItem) {
        presentationContext->onSubmittedWorkScheduled(makeBlockPtr(WTFMove(workItem)));
    });

    return presentationContextIOSurface;
}

PresentationContextIOSurface::PresentationContextIOSurface(const WGPUSurfaceDescriptor&, const Instance& instance)
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY) && HAVE(TASK_IDENTITY_TOKEN)
    : m_webProcessID(instance.webProcessID())
#endif
{
#if !(HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY) && HAVE(TASK_IDENTITY_TOKEN))
    UNUSED_PARAM(instance);
#endif
}

PresentationContextIOSurface::~PresentationContextIOSurface() = default;

void PresentationContextIOSurface::renderBuffersWereRecreated(NSArray<IOSurface *> *ioSurfaces)
{
    m_ioSurfaces = ioSurfaces;
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY) && HAVE(TASK_IDENTITY_TOKEN)
    if (m_webProcessID) {
        mach_port_t webProcessID = m_webProcessID->sendRight();
        if (webProcessID) {
            for (IOSurface *surface in ioSurfaces)
                IOSurfaceSetOwnershipIdentity(bridge_cast(surface), webProcessID, kIOSurfaceMemoryLedgerTagGraphics, 0);
        }
    }
#endif
    m_renderBuffers.clear();
}

void PresentationContextIOSurface::onSubmittedWorkScheduled(Function<void()>&& completionHandler)
{
    if (m_device)
        m_device->getQueue().onSubmittedWorkScheduled(WTFMove(completionHandler));
    else
        completionHandler();
}

static void generateAValidationError(Device& device, NSString* message, bool generateValidationError)
{
    if (generateValidationError)
        device.generateAValidationError(message);
}

void PresentationContextIOSurface::configure(Device& device, const WGPUSwapChainDescriptor& descriptor)
{
    m_renderBuffers.clear();
    m_currentIndex = 0;
    m_invalidTexture = Texture::createInvalid(device);

    if (descriptor.nextInChain)
        return;

    bool reportValidationErrors = descriptor.reportValidationErrors;
    m_device = &device;
    auto allowedFormat = ^(WGPUTextureFormat format) {
        return format == WGPUTextureFormat_BGRA8Unorm || format == WGPUTextureFormat_RGBA8Unorm || format == WGPUTextureFormat_RGBA16Float;
    };

    auto allowedViewFormat = ^(WGPUTextureFormat format) {
        return allowedFormat(Texture::removeSRGBSuffix(format));
    };

    auto& limits = device.limits();
    auto width = std::min<uint32_t>(limits.maxTextureDimension2D, descriptor.width);
    auto height = std::min<uint32_t>(limits.maxTextureDimension2D, descriptor.height);
    auto effectiveFormat = allowedFormat(descriptor.format) ? descriptor.format : WGPUTextureFormat_BGRA8Unorm;
    WGPUTextureDescriptor wgpuTextureDescriptor = {
        nullptr,
        descriptor.label,
        descriptor.usage,
        WGPUTextureDimension_2D, {
            width,
            height,
            1,
        },
        effectiveFormat,
        1,
        1,
        descriptor.viewFormats.size() ?: 1,
        descriptor.viewFormats.size() ? &descriptor.viewFormats[0] : &effectiveFormat,
    };
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Texture::pixelFormat(effectiveFormat) width:width height:height mipmapped:NO];
    textureDescriptor.usage = Texture::usage(descriptor.usage, effectiveFormat);
    bool needsLuminanceClampFunction = false;
    for (IOSurface *iosurface in m_ioSurfaces) {
        if (iosurface.height != static_cast<NSInteger>(height) || iosurface.width != static_cast<NSInteger>(width))
            return generateAValidationError(device, [NSString stringWithFormat:@"Invalid surface size. Backing surface has size (%d, %d) but attempting to configure a size of (%u, %u)", static_cast<int>(iosurface.width), static_cast<int>(iosurface.height), width, height], reportValidationErrors);
    }

    if (!allowedFormat(descriptor.format)) {
        generateAValidationError(device, [NSString stringWithFormat:@"Requested texture format %s is not a valid context format", Texture::formatToString(descriptor.format)], reportValidationErrors);
        return;
    }

    if (!width || !height) {
        generateAValidationError(device, @"Width or height is zero", reportValidationErrors);
        return;
    }

    if (descriptor.width > limits.maxTextureDimension2D || descriptor.height > limits.maxTextureDimension2D) {
        generateAValidationError(device, @"Requested canvas width and/or height are too large", reportValidationErrors);
        return;
    }

    for (auto viewFormat : descriptor.viewFormats) {
        if (!allowedViewFormat(viewFormat)) {
            generateAValidationError(device, @"Requested texture view format BGRA8UnormStorage is not enabled", reportValidationErrors);
            return;
        }
    }

    Vector viewFormats(std::span { wgpuTextureDescriptor.viewFormats, wgpuTextureDescriptor.viewFormatCount });
    if (NSString *error = device.errorValidatingTextureCreation(wgpuTextureDescriptor, viewFormats)) {
        generateAValidationError(device, error, reportValidationErrors);
        return;
    }

    if (descriptor.format == WGPUTextureFormat_BGRA8Unorm && (descriptor.usage & WGPUTextureUsage_StorageBinding) && !device.hasFeature(WGPUFeatureName_BGRA8UnormStorage)) {
        generateAValidationError(device, @"Requested storage format but BGRA8UnormStorage is not enabled", reportValidationErrors);
        return;
    }

    for (IOSurface *iosurface in m_ioSurfaces) {
        RefPtr<Texture> parentLuminanceClampTexture;
        if (textureDescriptor.pixelFormat == MTLPixelFormatRGBA16Float) {
            auto existingUsage = textureDescriptor.usage;
            textureDescriptor.usage |= MTLTextureUsageShaderRead;
            id<MTLTexture> luminanceClampTexture = device.newTextureWithDescriptor(textureDescriptor);
            luminanceClampTexture.label = fromAPI(descriptor.label);
            auto viewFormats = descriptor.viewFormats;
            parentLuminanceClampTexture = Texture::create(luminanceClampTexture, wgpuTextureDescriptor, WTFMove(viewFormats), device);
            parentLuminanceClampTexture->makeCanvasBacking();
            textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
            wgpuTextureDescriptor.format = WGPUTextureFormat_BGRA8Unorm;
            textureDescriptor.usage = existingUsage | MTLTextureUsageShaderWrite;
            needsLuminanceClampFunction = true;
        }

        id<MTLTexture> texture = device.newTextureWithDescriptor(textureDescriptor, bridge_cast(iosurface));
        texture.label = fromAPI(descriptor.label);
        auto viewFormats = descriptor.viewFormats;
        auto parentTexture = Texture::create(texture, wgpuTextureDescriptor, WTFMove(viewFormats), device);
        parentTexture->makeCanvasBacking();
        m_renderBuffers.append({ parentTexture, parentLuminanceClampTexture });
    }
    ASSERT(m_ioSurfaces.count == m_renderBuffers.size());

    if (needsLuminanceClampFunction) {
        NSError *error = nil;
        MTLCompileOptions* options = [MTLCompileOptions new];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        options.fastMathEnabled = YES;
        ALLOW_DEPRECATED_DECLARATIONS_END
        id<MTLDevice> device = m_device->device();
        /* NOLINT */ id<MTLLibrary> library = [device newLibraryWithSource:@R"(
    using namespace metal;
    constant float3x3 rgbToYCbCr = float3x3(
        float3(0.2126, 0.7152, 0.0722),
        float3(-0.1146, -0.3854, 0.5),
        float3(0.5, -0.4542, -0.0458));
    constant float3x3 yCbCrToRGB = float3x3(
        float3(1, 0, 1.5748),
        float3(1, -0.1873, -0.4681),
        float3(1, 1.8556, 0));
    kernel void luminanceClamp(texture2d<float, access::read>  inTexture  [[texture(0)]],
        texture2d<float, access::write> outTexture [[texture(1)]],
        uint2 gid [[thread_position_in_grid]])
    {
        if (gid.x >= outTexture.get_width() || gid.y >= outTexture.get_height())
            return;

        float4 inColor  = inTexture.read(gid);
        float3 yCbCr = rgbToYCbCr * inColor.rgb;
        yCbCr.x = clamp(yCbCr.x, 0., 1.);
        float3 outColor = yCbCrToRGB * yCbCr;
        outTexture.write(float4(outColor, 1), gid);
    })" /* NOLINT */ options:options error:&error];
        if (error) {
            WTFLogAlways("%@", error);
            return;
        }
        m_luminanceClampFunction = [library newFunctionWithName:@"luminanceClamp"];
        m_computePipelineState = [device newComputePipelineStateWithFunction:m_luminanceClampFunction error:&error];
    }
}

void PresentationContextIOSurface::unconfigure()
{
    m_ioSurfaces = nil;
    m_renderBuffers.clear();
    m_currentIndex = 0;
    m_device = nullptr;
}

void PresentationContextIOSurface::present()
{
    if (m_ioSurfaces.count != m_renderBuffers.size())
        return;

    auto& textureRefPtr = m_renderBuffers[m_currentIndex].luminanceClampTexture;
    if (Texture* texturePtr = textureRefPtr.get(); texturePtr && m_computePipelineState) {
        MTLCommandBufferDescriptor *descriptor = [MTLCommandBufferDescriptor new];
        descriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
        id<MTLCommandBuffer> commandBuffer = m_device->getQueue().commandBufferWithDescriptor(descriptor);
        MTLComputePassDescriptor* computeDescriptor = [MTLComputePassDescriptor new];
        computeDescriptor.dispatchType = MTLDispatchTypeSerial;

        id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoderWithDescriptor:computeDescriptor];
        [computeEncoder setComputePipelineState:m_computePipelineState];
        id<MTLTexture> inputTexture = texturePtr->texture();
        [computeEncoder setTexture:inputTexture atIndex:0];
        [computeEncoder setTexture:m_renderBuffers[m_currentIndex].texture->texture() atIndex:1];
        auto threadgroupSize = MTLSizeMake(16, 16, 1);

        MTLSize threadgroupCount;
        threadgroupCount.width  = (inputTexture.width  + threadgroupSize.width -  1) / threadgroupSize.width;
        threadgroupCount.height = (inputTexture.height + threadgroupSize.height - 1) / threadgroupSize.height;
        threadgroupCount.depth = 1;
        [computeEncoder dispatchThreadgroups:threadgroupCount threadsPerThreadgroup:threadgroupSize];
        [computeEncoder endEncoding];
        m_device->getQueue().commitMTLCommandBuffer(commandBuffer);
    }

    m_currentIndex = (m_currentIndex + 1) % m_renderBuffers.size();
}

Texture* PresentationContextIOSurface::getCurrentTexture()
{
    if (m_ioSurfaces.count != m_renderBuffers.size() || m_renderBuffers.size() <= m_currentIndex) {
        if (m_device.get())
            m_device->generateAValidationError("GPUCanvasContext is not configured correctly"_s);
        return m_invalidTexture.get();
    }

    auto& texturePtr = m_renderBuffers[m_currentIndex].luminanceClampTexture;
    if (texturePtr.get()) {
        texturePtr->recreateIfNeeded();
        return texturePtr.get();
    }
    auto& texture = m_renderBuffers[m_currentIndex].texture;
    texture->recreateIfNeeded();
    texture->setPreviouslyCleared(0, 0, false);
    return texture.ptr();
}

TextureView* PresentationContextIOSurface::getCurrentTextureView()
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebGPU

#pragma mark WGPU Stubs
