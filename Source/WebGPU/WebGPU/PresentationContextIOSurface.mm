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
#import <CoreVideo/CVPixelBuffer.h>
#import <wtf/MonotonicTime.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/IOTypesSPI.h>

namespace WebGPU::Metal {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PresentationContextIOSurface);

Ref<PresentationContextIOSurface> PresentationContextIOSurface::create(const WebGPU::PresentationContextDescriptor& descriptor, const Instance& instance)
{
    Ref presentationContextIOSurface = adoptRef(*new PresentationContextIOSurface(instance));
    if (!descriptor.registerCompositorIntegration)
        return presentationContextIOSurface;

    descriptor.registerCompositorIntegration([presentationContext = presentationContextIOSurface.copyRef()](std::span<const IOSurfaceRef> ioSurfaces) {
        RetainPtr array = adoptNS([[NSMutableArray alloc] initWithCapacity:ioSurfaces.size()]);
        for (auto& ioSurface : ioSurfaces)
            [array addObject:(__bridge IOSurface *)ioSurface];
        presentationContext->renderBuffersWereRecreated(array.get());
    }, [presentationContext = presentationContextIOSurface.copyRef()](CompletionHandler<void()>&& completionHandler) {
        presentationContext->onSubmittedWorkScheduled([completionHandler = WTF::move(completionHandler)] mutable {
            completionHandler();
        });
    }, [presentationContext = presentationContextIOSurface.copyRef()](uint32_t index, IOSurfaceRef ioSurface) {
        presentationContext->replaceRenderBuffer(index, (__bridge IOSurface *)ioSurface);
    });

    return presentationContextIOSurface;
}

PresentationContextIOSurface::PresentationContextIOSurface(const Instance& instance)
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
    m_existingRenderBuffers = WTF::move(m_renderBuffers);
    m_ioSurfaces = ioSurfaces;
    for (IOSurface *surface in ioSurfaces)
        setOwnershipIdentity(surface);
    m_renderBuffers.clear();
}

void PresentationContextIOSurface::replaceRenderBuffer(uint32_t index, IOSurface *ioSurface)
{
    if (index >= m_ioSurfaces.count)
        return;

    RetainPtr ioSurfaces = adoptNS([m_ioSurfaces mutableCopy]);
    [ioSurfaces replaceObjectAtIndex:index withObject:ioSurface];
    m_ioSurfaces = ioSurfaces.get();
    setOwnershipIdentity(ioSurface);

    RefPtr device = m_device;
    if (!device || index >= m_renderBuffers.size())
        return;

    Ref previousTexture = m_renderBuffers[index].texture;
    id<MTLTexture> previousMTLTexture = previousTexture->texture();
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:previousMTLTexture.pixelFormat width:previousMTLTexture.width height:previousMTLTexture.height mipmapped:NO];
    textureDescriptor.usage = previousMTLTexture.usage;
    textureDescriptor.storageMode = previousMTLTexture.storageMode;
    WebGPU::TextureDescriptor apiTextureDescriptor {
        .label = { },
        .usage = previousTexture->usage(),
        .dimension = previousTexture->dimension(),
        .size = {
            .width = previousTexture->width(),
            .height = previousTexture->height(),
            .depthOrArrayLayers = previousTexture->depthOrArrayLayers(),
        },
        .format = previousTexture->format(),
        .mipLevelCount = previousTexture->mipLevelCount(),
        .sampleCount = previousTexture->sampleCount(),
        .viewFormats = m_textureViewFormats.span(),
    };
    auto viewFormats = m_textureViewFormats;
    Ref texture = Texture::create(device->newTextureWithDescriptor(textureDescriptor, bridge_cast(ioSurface)), apiTextureDescriptor, WTF::move(viewFormats), *device);
    texture->makeCanvasBacking();
    m_renderBuffers[index].texture = WTF::move(texture);
}

void PresentationContextIOSurface::setOwnershipIdentity(IOSurface *surface)
{
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY) && HAVE(TASK_IDENTITY_TOKEN)
    if (!m_webProcessID)
        return;
    if (mach_port_t webProcessID = m_webProcessID->sendRight())
        IOSurfaceSetOwnershipIdentity(bridge_cast(surface), webProcessID, kIOSurfaceMemoryLedgerTagGraphics, 0);
#else
    UNUSED_PARAM(surface);
#endif
}

void PresentationContextIOSurface::onSubmittedWorkScheduled(Function<void()>&& completionHandler)
{
    if (m_device)
        m_device->getQueue()->onSubmittedWorkScheduled(WTF::move(completionHandler));
    else
        completionHandler();
}

bool PresentationContextIOSurface::copyRenderBuffer(uint32_t bufferIndex, IOSurfaceRef destination, WebGPU::RenderBufferCopyColorSpace copyColorSpace)
{
    RefPtr device = m_device;
    if (!device || !destination || m_ioSurfaces.count != m_renderBuffers.size() || bufferIndex >= m_renderBuffers.size())
        return false;

    // The render buffer texture is the IOSurface that is displayed.
    id<MTLTexture> source = m_renderBuffers[bufferIndex].texture->texture();
    RetainPtr<IOSurfaceRef> sourceSurface = source.iosurface;
    if (!source || !sourceSurface || IOSurfaceGetWidth(destination) != source.width || IOSurfaceGetHeight(destination) != source.height)
        return false;

    // A copy to sRGB converts the contents when they are in another color space, or in the extended
    // range of a 16-bit float render buffer.
    bool convertsToSRGB = copyColorSpace == WebGPU::RenderBufferCopyColorSpace::SRGB && (source.pixelFormat == MTLPixelFormatRGBA16Float || m_colorSpace != WebGPU::PredefinedColorSpace::SRGB);
    if (convertsToSRGB && (IOSurfaceGetPixelFormat(destination) != kCVPixelFormatType_32BGRA || !convertToSRGBComputePipelineState()))
        return false;

    // A blit copies the bytes, so it needs a destination with the layout of the source. Otherwise,
    // such as for an RGBA render buffer copied to a BGRA destination, a compute pass converts them.
    bool canBlit = !convertsToSRGB && IOSurfaceGetPixelFormat(destination) == IOSurfaceGetPixelFormat(sourceSurface.get());
    MTLPixelFormat destinationPixelFormat = source.pixelFormat;
    if (!canBlit) {
        switch (IOSurfaceGetPixelFormat(destination)) {
        case kCVPixelFormatType_32BGRA:
            destinationPixelFormat = MTLPixelFormatBGRA8Unorm;
            break;
        default:
            return false;
        }
    }
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:destinationPixelFormat width:source.width height:source.height mipmapped:NO];
    textureDescriptor.usage = canBlit ? MTLTextureUsageShaderRead : MTLTextureUsageShaderWrite;
    id<MTLTexture> destinationTexture = device->newTextureWithDescriptor(textureDescriptor, destination);
    if (!destinationTexture)
        return false;

    Ref deviceQueue = device->getQueue();
    deviceQueue->resolveProvisionalTextureClears();
    id<MTLCommandBuffer> commandBuffer = deviceQueue->commandBufferWithDescriptor([MTLCommandBufferDescriptor new]);
    if (!commandBuffer)
        return false;
    if (canBlit) {
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoderWithDescriptor:[MTLBlitPassDescriptor new]];
        deviceQueue->setEncoderForBuffer(commandBuffer, blitEncoder);
        [blitEncoder copyFromTexture:source toTexture:destinationTexture];
        deviceQueue->endEncoding(blitEncoder, commandBuffer);
    } else if (convertsToSRGB)
        convertTextureToSRGB(destinationTexture, source, commandBuffer);
    else
        copyTextureToTexture(destinationTexture, source, commandBuffer);
    deviceQueue->commitMTLCommandBuffer(commandBuffer);
    // An invalid queue, such as the one of a lost device, does not commit.
    if (commandBuffer.status < MTLCommandBufferStatusCommitted)
        return false;
    // The copy is read by others once it is scheduled.
    [commandBuffer waitUntilScheduled];
    return true;
}

static void generateAValidationError(Device& device, NSString* message, bool generateValidationError)
{
    if (generateValidationError)
        device.generateAValidationError(message);
}

id<MTLComputePipelineState> PresentationContextIOSurface::resizeComputePipelineState()
{
    if (m_resizeComputePipelineState)
        return m_resizeComputePipelineState;

    NSError *error = nil;
    MTLCompileOptions* options = [MTLCompileOptions new];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    options.fastMathEnabled = YES;
    ALLOW_DEPRECATED_DECLARATIONS_END
    id<MTLDevice> device = m_device->device();
    /* NOLINT */ id<MTLLibrary> library = [device newLibraryWithSource:@R"(
using namespace metal;
kernel void resize(texture2d<float, access::read>  inTexture  [[texture(0)]],
    texture2d<float, access::write> outTexture [[texture(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= outTexture.get_width() || gid.y >= outTexture.get_height())
        return;

    float2 remappedGid = float2(static_cast<float>(gid.x), static_cast<float>(gid.y)) * float2(static_cast<float>(inTexture.get_width()) / outTexture.get_width(), static_cast<float>(inTexture.get_height()) / outTexture.get_height());
    float4 color = inTexture.read(uint2((uint)remappedGid.x, (uint)remappedGid.y));
    outTexture.write(color, gid);
})" /* NOLINT */ options:options error:&error];
    if (error) {
        WTFLogAlways("%@", error); // NOLINT
        return nil;
    }
    id<MTLFunction> function = [library newFunctionWithName:@"resize"];
    m_resizeComputePipelineState = [device newComputePipelineStateWithFunction:function error:&error];
    if (error) {
        WTFLogAlways("%@", error); // NOLINT
        return nil;
    }

    return m_resizeComputePipelineState;
}

void PresentationContextIOSurface::copyTextureToTexture(id<MTLTexture> destination, id<MTLTexture> source, id<MTLCommandBuffer> commandBuffer)
{
    if (!source || !destination)
        return;

    id<MTLComputePipelineState> pso = resizeComputePipelineState();
    if (!pso)
        return;

    RefPtr device = m_device;
    Ref deviceQueue = device->getQueue();

    MTLComputePassDescriptor* computeDescriptor = [MTLComputePassDescriptor new];
    computeDescriptor.dispatchType = MTLDispatchTypeSerial;

    id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoderWithDescriptor:computeDescriptor];
    deviceQueue->setEncoderForBuffer(commandBuffer, computeEncoder);
    [computeEncoder setComputePipelineState:pso];

    [computeEncoder setTexture:source atIndex:0];
    [computeEncoder setTexture:destination atIndex:1];
    auto threadgroupSize = MTLSizeMake(16, 16, 1);

    MTLSize threadgroupCount;
    threadgroupCount.width  = (destination.width  + threadgroupSize.width -  1) / threadgroupSize.width;
    threadgroupCount.height = (destination.height + threadgroupSize.height - 1) / threadgroupSize.height;
    threadgroupCount.depth = 1;
    [computeEncoder dispatchThreadgroups:threadgroupCount threadsPerThreadgroup:threadgroupSize];
    deviceQueue->endEncoding(computeEncoder, commandBuffer);
}

id<MTLComputePipelineState> PresentationContextIOSurface::convertToSRGBComputePipelineState()
{
    if (m_convertToSRGBComputePipelineState)
        return m_convertToSRGBComputePipelineState;

    NSError *error = nil;
    MTLCompileOptions* options = [MTLCompileOptions new];
    // The transfer functions are evaluated precisely, so that the 8-bit result matches a conversion
    // on the CPU.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    options.fastMathEnabled = NO;
    ALLOW_DEPRECATED_DECLARATIONS_END
    id<MTLDevice> device = m_device->device();
    /* NOLINT */ id<MTLLibrary> library = [device newLibraryWithSource:@R"(
using namespace metal;
struct ConvertToSRGBParameters {
    uint isPremultiplied;
    uint isOpaque;
    uint isLinear;
    uint isDisplayP3;
};
// A float3x3 is built from its columns, so each float3 below is one column of the linear Display P3
// (D65) to linear sRGB matrix.
constant float3x3 linearDisplayP3ToLinearSRGB = float3x3(
    float3(1.2249402, -0.0420570, -0.0196376),
    float3(-0.2249402, 1.0420570, -0.0786360),
    float3(0, 0, 1.0982736));
// The extended sRGB transfer function, which mirrors the negative values.
static float3 sRGBToLinear(float3 color)
{
    float3 magnitude = abs(color);
    float3 linear = select(pow((magnitude + 0.055) / 1.055, 2.4), magnitude / 12.92, magnitude <= 0.04045);
    return copysign(linear, color);
}
static float3 linearToSRGB(float3 color)
{
    return select(1.055 * pow(color, 1.0 / 2.4) - 0.055, color * 12.92, color <= 0.0031308);
}
kernel void convertToSRGB(texture2d<float, access::read> inTexture [[texture(0)]],
    texture2d<float, access::write> outTexture [[texture(1)]],
    constant ConvertToSRGBParameters& parameters [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= outTexture.get_width() || gid.y >= outTexture.get_height())
        return;

    float4 color = inTexture.read(gid);
    float alpha = parameters.isOpaque ? 1 : saturate(color.a);
    float3 rgb = color.rgb;
    if (parameters.isPremultiplied)
        rgb = alpha > 0 ? rgb / alpha : float3(0);
    if (!parameters.isLinear)
        rgb = sRGBToLinear(rgb);
    if (parameters.isDisplayP3)
        rgb = linearDisplayP3ToLinearSRGB * rgb;
    rgb = linearToSRGB(saturate(rgb));
    if (parameters.isPremultiplied)
        rgb *= alpha;
    outTexture.write(float4(rgb, alpha), gid);
})" /* NOLINT */ options:options error:&error];
    if (error) {
        WTFLogAlways("%@", error); // NOLINT
        return nil;
    }
    id<MTLFunction> function = [library newFunctionWithName:@"convertToSRGB"];
    m_convertToSRGBComputePipelineState = [device newComputePipelineStateWithFunction:function error:&error];
    if (error) {
        WTFLogAlways("%@", error); // NOLINT
        return nil;
    }

    return m_convertToSRGBComputePipelineState;
}

// Converts the contents of the source, which may be in the extended range of a 16-bit float texture,
// to sRGB in the standard range. The colors are converted before they are clamped, so the colors
// outside of the sRGB range are clamped as they would be by a conversion on the CPU.
void PresentationContextIOSurface::convertTextureToSRGB(id<MTLTexture> destination, id<MTLTexture> source, id<MTLCommandBuffer> commandBuffer)
{
    if (!source || !destination || source.width != destination.width || source.height != destination.height)
        return;

    id<MTLComputePipelineState> pso = convertToSRGBComputePipelineState();
    if (!pso)
        return;

    struct {
        uint32_t isPremultiplied;
        uint32_t isOpaque;
        uint32_t isLinear;
        uint32_t isDisplayP3;
    } parameters {
        .isPremultiplied = m_alphaMode == WebGPU::CanvasAlphaMode::Premultiplied,
        .isOpaque = m_alphaMode == WebGPU::CanvasAlphaMode::Opaque,
        .isLinear = m_colorSpace == WebGPU::PredefinedColorSpace::SRGBLinear || m_colorSpace == WebGPU::PredefinedColorSpace::DisplayP3Linear,
        .isDisplayP3 = m_colorSpace == WebGPU::PredefinedColorSpace::DisplayP3 || m_colorSpace == WebGPU::PredefinedColorSpace::DisplayP3Linear,
    };

    RefPtr device = m_device;
    Ref deviceQueue = device->getQueue();

    MTLComputePassDescriptor* computeDescriptor = [MTLComputePassDescriptor new];
    computeDescriptor.dispatchType = MTLDispatchTypeSerial;

    id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoderWithDescriptor:computeDescriptor];
    deviceQueue->setEncoderForBuffer(commandBuffer, computeEncoder);
    [computeEncoder setComputePipelineState:pso];
    [computeEncoder setTexture:source atIndex:0];
    [computeEncoder setTexture:destination atIndex:1];
    [computeEncoder setBytes:&parameters length:sizeof(parameters) atIndex:0];
    auto threadgroupSize = MTLSizeMake(16, 16, 1);

    MTLSize threadgroupCount;
    threadgroupCount.width  = (destination.width  + threadgroupSize.width -  1) / threadgroupSize.width;
    threadgroupCount.height = (destination.height + threadgroupSize.height - 1) / threadgroupSize.height;
    threadgroupCount.depth = 1;
    [computeEncoder dispatchThreadgroups:threadgroupCount threadsPerThreadgroup:threadgroupSize];
    deviceQueue->endEncoding(computeEncoder, commandBuffer);
}

void PresentationContextIOSurface::configure(const WebGPU::CanvasConfiguration& configuration)
{
    Ref device = metal(configuration.device.get());
    m_renderBuffers.clear();
    m_inFlightFrames.clear();
    m_maximumInFlightFrames = 0;
    m_lastDrainedFrameGPUCost = 0_s;
    m_lastFramePresentStall = 0_s;
    m_invalidTexture = Texture::createInvalid(device);

    bool reportValidationErrors = configuration.reportValidationErrors;
    m_device = device.copyRef();
    auto allowedFormat = ^(WebGPU::TextureFormat format) {
        return format == WebGPU::TextureFormat::Bgra8unorm || format == WebGPU::TextureFormat::Rgba8unorm || format == WebGPU::TextureFormat::Rgba16float;
    };

    auto allowedViewFormat = ^(WebGPU::TextureFormat format) {
        return allowedFormat(Texture::removeSRGBSuffix(format));
    };

    auto format = configuration.format;
    auto& limits = device->limits();
    auto width = std::min<uint32_t>(limits.maxTextureDimension2D, configuration.width);
    auto height = std::min<uint32_t>(limits.maxTextureDimension2D, configuration.height);
    auto effectiveFormat = allowedFormat(format) ? format : WebGPU::TextureFormat::Bgra8unorm;
    // Without view formats in the configuration, the validation takes the format of the textures as
    // their only view format.
    std::array effectiveViewFormats { effectiveFormat };
    WebGPU::TextureDescriptor apiTextureDescriptor {
        .label = { },
        .usage = configuration.usage,
        .dimension = WebGPU::TextureDimension::_2d,
        .size = {
            .width = width,
            .height = height,
            .depthOrArrayLayers = 1,
        },
        .format = effectiveViewFormats[0],
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormats = configuration.viewFormats.empty() ? std::span<const WebGPU::TextureFormat> { effectiveViewFormats } : configuration.viewFormats,
    };
    m_colorSpace = configuration.colorSpace;
    m_toneMappingMode = configuration.toneMappingMode;
    m_alphaMode = configuration.compositingAlphaMode;

    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Texture::pixelFormat(effectiveFormat) width:width height:height mipmapped:NO];
    textureDescriptor.usage = Texture::usage(apiTextureDescriptor.usage, effectiveFormat);
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#if ENABLE(WEBGPU_BY_DEFAULT)
    textureDescriptor.storageMode = device->hasUnifiedMemory() ? MTLStorageModeShared : MTLStorageModeManaged;
#else
    textureDescriptor.storageMode = MTLStorageModeManaged;
#endif
    ALLOW_DEPRECATED_DECLARATIONS_END
#else
    textureDescriptor.storageMode = MTLStorageModeShared;
#endif
    for (IOSurface *iosurface in m_ioSurfaces) {
        if (iosurface.height != static_cast<NSInteger>(height) || iosurface.width != static_cast<NSInteger>(width))
            return generateAValidationError(device, [NSString stringWithFormat:@"Invalid surface size. Backing surface has size (%d, %d) but attempting to configure a size of (%u, %u)", static_cast<int>(iosurface.width), static_cast<int>(iosurface.height), width, height], reportValidationErrors);
    }

    if (!allowedFormat(format)) {
        generateAValidationError(device, [NSString stringWithFormat:@"Requested texture format %s is not a valid context format", Texture::formatToString(format).characters()], reportValidationErrors);
        return;
    }

    if (!width || !height) {
        generateAValidationError(device, @"Width or height is zero", reportValidationErrors);
        return;
    }

    if (configuration.width > limits.maxTextureDimension2D || configuration.height > limits.maxTextureDimension2D) {
        generateAValidationError(device, @"Requested canvas width and/or height are too large", reportValidationErrors);
        return;
    }

    for (auto viewFormat : configuration.viewFormats) {
        if (!allowedViewFormat(viewFormat)) {
            generateAValidationError(device, @"Requested texture view format BGRA8UnormStorage is not enabled", reportValidationErrors);
            return;
        }
    }

    if (NSString *error = device->errorValidatingTextureCreation(apiTextureDescriptor)) {
        generateAValidationError(device, error, reportValidationErrors);
        return;
    }

    // The textures allow views only in the formats that the configuration lists.
    Vector<WebGPU::TextureFormat> textureViewFormats { configuration.viewFormats };
    m_textureViewFormats = textureViewFormats;

    if (format == WebGPU::TextureFormat::Bgra8unorm && configuration.usage.contains(WebGPU::TextureUsage::StorageBinding) && !device->hasFeature(WebGPU::FeatureName::Bgra8unormStorage)) {
        generateAValidationError(device, @"Requested storage format but BGRA8UnormStorage is not enabled", reportValidationErrors);
        return;
    }

    textureDescriptor.usage |= (MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);
    bool needsLuminanceClampFunction = textureDescriptor.pixelFormat == MTLPixelFormatRGBA16Float && m_toneMappingMode == WebGPU::CanvasToneMappingMode::Standard;
    auto existingUsage = textureDescriptor.usage;
    Ref deviceQueue = device->getQueue();
    id<MTLCommandBuffer> resizeCommandBuffer = nil;
    for (IOSurface *iosurface in m_ioSurfaces) {
        RefPtr<Texture> parentLuminanceClampTexture;
        MTLTextureDescriptor* luminanceClampDescriptor = nil;
        if (needsLuminanceClampFunction) {
            textureDescriptor.pixelFormat = MTLPixelFormatRGBA16Float;
            apiTextureDescriptor.format = WebGPU::TextureFormat::Rgba16float;
            textureDescriptor.usage = existingUsage;
            textureDescriptor.usage |= MTLTextureUsageShaderRead;
            luminanceClampDescriptor = textureDescriptor;
            id<MTLTexture> luminanceClampTexture = device->newTextureWithDescriptor(textureDescriptor);
            auto viewFormats = textureViewFormats;
            parentLuminanceClampTexture = Texture::create(luminanceClampTexture, apiTextureDescriptor, WTF::move(viewFormats), device);
            parentLuminanceClampTexture->makeCanvasBacking();
            textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
            apiTextureDescriptor.format = WebGPU::TextureFormat::Bgra8unorm;
            textureDescriptor.usage = existingUsage | MTLTextureUsageShaderWrite;
        }

        id<MTLTexture> texture = device->newTextureWithDescriptor(textureDescriptor, bridge_cast(iosurface));
        auto viewFormats = textureViewFormats;
        auto parentTexture = Texture::create(texture, apiTextureDescriptor, WTF::move(viewFormats), device);
        parentTexture->makeCanvasBacking();
        m_renderBuffers.append({ parentTexture, parentLuminanceClampTexture });
        if (m_existingRenderBuffers.size() >= m_renderBuffers.size()) {
            if (!resizeCommandBuffer)
                resizeCommandBuffer = deviceQueue->commandBufferWithDescriptor([MTLCommandBufferDescriptor new]);

            auto existingBufferIndex = m_renderBuffers.size() - 1;
            auto& existingRenderBuffer = m_existingRenderBuffers[existingBufferIndex];
            textureDescriptor.usage |= MTLTextureUsageShaderWrite;
            RetainPtr<IOSurfaceRef> iosurface = parentTexture->texture().iosurface;
            id<MTLTexture> destinationTexture = device->newTextureWithDescriptor(textureDescriptor, iosurface.get());
            copyTextureToTexture(destinationTexture, existingRenderBuffer.texture->texture(), resizeCommandBuffer);
            if (parentLuminanceClampTexture && existingRenderBuffer.luminanceClampTexture) {
                luminanceClampDescriptor.usage |= MTLTextureUsageShaderWrite;
                iosurface = parentLuminanceClampTexture->texture().iosurface;
                destinationTexture = device->newTextureWithDescriptor(luminanceClampDescriptor, iosurface.get());
                copyTextureToTexture(destinationTexture, existingRenderBuffer.luminanceClampTexture->texture(), resizeCommandBuffer);
            }
        }
    }
    ASSERT(m_ioSurfaces.count == m_renderBuffers.size());
    m_maximumInFlightFrames = m_renderBuffers.size() > 1 ? m_renderBuffers.size() - 1 : 1;
    if (resizeCommandBuffer) {
        deviceQueue->commitMTLCommandBuffer(resizeCommandBuffer);
        m_existingRenderBuffers.clear();
    }

    if (needsLuminanceClampFunction) {
        NSError *error = nil;
        MTLCompileOptions* options = [MTLCompileOptions new];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        options.fastMathEnabled = YES;
        ALLOW_DEPRECATED_DECLARATIONS_END
        id<MTLDevice> device = m_device->device();
        /* NOLINT */ id<MTLLibrary> library = [device newLibraryWithSource:@R"(
    using namespace metal;
    // A float3x3 is built from its columns, so each float3 below is one column of the BT.709
    // matrix: the luma coefficients run down the first column, not across the first row. Writing
    // these as rows transposes both matrices, and while a transposed pair still round trips
    // exactly, the value clamped in between is then no longer luma, so any color whose first
    // component goes negative comes back a different color.
    constant float3x3 rgbToYCbCr = float3x3(
        float3(0.2126, -0.1146, 0.5),
        float3(0.7152, -0.3854, -0.4542),
        float3(0.0722, 0.5, -0.0458));
    constant float3x3 yCbCrToRGB = float3x3(
        float3(1, 1, 1),
        float3(0, -0.1873, 1.8556),
        float3(1.5748, -0.4681, 0));
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
        // Clamping luminance says nothing about how opaque the frame is, so the alpha the canvas
        // was drawn with has to survive. An opaque canvas is presented through a surface with no
        // alpha channel at all, which is what makes it opaque.
        outTexture.write(float4(outColor, inColor.a), gid);
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
    m_inFlightFrames.clear();
    m_maximumInFlightFrames = 0;
    m_lastDrainedFrameGPUCost = 0_s;
    m_lastFramePresentStall = 0_s;
    m_device = nullptr;
}

void PresentationContextIOSurface::waitForInFlightFrameSlot()
{
    if (!m_maximumInFlightFrames)
        return;

    Seconds stall;
    while (m_inFlightFrames.size() >= m_maximumInFlightFrames) {
        Ref<Texture> oldestFrame = m_inFlightFrames.takeFirst();
        // Blocking here is what tells the frame pacer the canvas is presenting faster than the GPU retires frames.
        auto waitStartTime = MonotonicTime::now();
        bool completed = oldestFrame->waitForCommandBufferCompletion();
        stall += MonotonicTime::now() - waitStartTime;
        m_lastDrainedFrameGPUCost = completed ? oldestFrame->gpuFrameCost() : 0_s;
    }
    m_lastFramePresentStall = stall;
}

void PresentationContextIOSurface::present(uint32_t currentIndex)
{
    RefPtr device = m_device;
    if (m_ioSurfaces.count != m_renderBuffers.size() || currentIndex >= m_renderBuffers.size() || !device)
        return;

    waitForInFlightFrameSlot();

    Ref deviceQueue = device->getQueue();

    if (auto texture = m_renderBuffers[currentIndex].luminanceClampTexture; texture && m_computePipelineState) {
        deviceQueue->resolveProvisionalTextureClears();
        MTLCommandBufferDescriptor *descriptor = [MTLCommandBufferDescriptor new];
        id<MTLCommandBuffer> commandBuffer = deviceQueue->commandBufferWithDescriptor(descriptor);
        MTLComputePassDescriptor* computeDescriptor = [MTLComputePassDescriptor new];
        computeDescriptor.dispatchType = MTLDispatchTypeSerial;

        id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoderWithDescriptor:computeDescriptor];
        deviceQueue->setEncoderForBuffer(commandBuffer, computeEncoder);
        [computeEncoder setComputePipelineState:m_computePipelineState];
        id<MTLTexture> inputTexture = texture->texture();
        [computeEncoder setTexture:inputTexture atIndex:0];
        [computeEncoder setTexture:m_renderBuffers[currentIndex].texture->texture() atIndex:1];
        auto threadgroupSize = MTLSizeMake(16, 16, 1);

        MTLSize threadgroupCount;
        threadgroupCount.width  = (inputTexture.width  + threadgroupSize.width -  1) / threadgroupSize.width;
        threadgroupCount.height = (inputTexture.height + threadgroupSize.height - 1) / threadgroupSize.height;
        threadgroupCount.depth = 1;
        [computeEncoder dispatchThreadgroups:threadgroupCount threadsPerThreadgroup:threadgroupSize];
        deviceQueue->endEncoding(computeEncoder, commandBuffer);
        deviceQueue->commitMTLCommandBuffer(commandBuffer);
    }

    auto& presentedBuffer = m_renderBuffers[currentIndex];
    Ref<Texture> inFlightTexture = presentedBuffer.luminanceClampTexture ? *presentedBuffer.luminanceClampTexture : presentedBuffer.texture.get();
    m_inFlightFrames.append(inFlightTexture);
    RELEASE_ASSERT(m_inFlightFrames.size() <= m_maximumInFlightFrames);
}

Texture* PresentationContextIOSurface::currentTexture(uint32_t currentIndex)
{
    if (m_ioSurfaces.count != m_renderBuffers.size() || m_renderBuffers.size() <= currentIndex) {
        if (RefPtr device = m_device)
            device->generateAValidationError("GPUCanvasContext is not configured correctly"_s);
        return m_invalidTexture.get();
    }

    auto& texturePtr = m_renderBuffers[currentIndex].luminanceClampTexture;
    if (texturePtr.get()) {
        texturePtr->recreateIfNeeded();
        return texturePtr.get();
    }
    auto& texture = m_renderBuffers[currentIndex].texture;
    texture->recreateIfNeeded();
    return texture.ptr();
}

TextureView* PresentationContextIOSurface::getCurrentTextureView()
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebGPU::Metal

#pragma mark WGPU Stubs
