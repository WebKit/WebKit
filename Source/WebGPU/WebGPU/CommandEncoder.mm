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
#import "CommandEncoder.h"

#import "APIConversions.h"
#import "Buffer.h"
#import "CommandBuffer.h"
#import "ComputePassEncoder.h"
#import "Device.h"
#import "IsValidToUseWith.h"
#import "QuerySet.h"
#import "RenderPassEncoder.h"
#import "Texture.h"
#import <wtf/CheckedArithmetic.h>

@implementation TextureAndClearColor
- (instancetype)initWithTexture:(id<MTLTexture>)texture
{
    if (!(self = [super init]))
        return nil;

    _texture = texture;
    _clearColor = MTLClearColorMake(0, 0, 0, 0);

    return self;
}
@end

namespace WebGPU {

#define GENERATE_INVALID_ENCODER_STATE_ERROR() \
if (m_state == EncoderState::Ended) \
    m_device->generateAValidationError([NSString stringWithFormat:@"%s: encoder state is %@", __PRETTY_FUNCTION__, encoderStateName()]); \
else \
    makeInvalid(@"Encoder state is locked");

static MTLLoadAction loadAction(WGPULoadOp loadOp, WGPUStoreOp storeOp)
{
    switch (loadOp) {
    case WGPULoadOp_Load:
        return storeOp == WGPUStoreOp_Discard ? MTLLoadActionClear : MTLLoadActionLoad;
    case WGPULoadOp_Clear:
        return MTLLoadActionClear;
    case WGPULoadOp_Undefined:
        return MTLLoadActionDontCare;
    case WGPULoadOp_Force32:
        ASSERT_NOT_REACHED();
        return MTLLoadActionDontCare;
    }
}

static MTLStoreAction storeAction(WGPUStoreOp storeOp, WGPULoadOp loadOp)
{
    switch (storeOp) {
    case WGPUStoreOp_Store:
        return MTLStoreActionStore;
    case WGPUStoreOp_Discard:
        return loadOp == WGPULoadOp_Load ? MTLStoreActionStore : MTLStoreActionDontCare;
    case WGPUStoreOp_Undefined:
        return MTLStoreActionDontCare;
    case WGPUStoreOp_Force32:
        ASSERT_NOT_REACHED();
        return MTLStoreActionDontCare;
    }
}

Ref<CommandEncoder> Device::createCommandEncoder(const WGPUCommandEncoderDescriptor& descriptor)
{
    if (descriptor.nextInChain || !isValid())
        return CommandEncoder::createInvalid(*this);

    captureFrameIfNeeded();
    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createcommandencoder

    auto *commandBufferDescriptor = [MTLCommandBufferDescriptor new];
    commandBufferDescriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    id<MTLCommandBuffer> commandBuffer = getQueue().commandBufferWithDescriptor(commandBufferDescriptor);
    if (!commandBuffer)
        return CommandEncoder::createInvalid(*this);

    commandBuffer.label = fromAPI(descriptor.label);

    return CommandEncoder::create(commandBuffer, *this);
}

CommandEncoder::CommandEncoder(id<MTLCommandBuffer> commandBuffer, Device& device)
    : m_commandBuffer(commandBuffer)
    , m_device(device)
{
}

CommandEncoder::CommandEncoder(Device& device)
    : m_device(device)
{
}

CommandEncoder::~CommandEncoder()
{
    finalizeBlitCommandEncoder();
    m_device->getQueue().commitMTLCommandBuffer(m_commandBuffer);
}

id<MTLBlitCommandEncoder> CommandEncoder::ensureBlitCommandEncoder()
{
    if (m_blitCommandEncoder && m_pendingTimestampWrites.isEmpty())
        return m_blitCommandEncoder;

    auto pendingTimestampWrites = std::exchange(m_pendingTimestampWrites, { });
    if (m_blitCommandEncoder && !pendingTimestampWrites.isEmpty())
        finalizeBlitCommandEncoder();

    MTLBlitPassDescriptor *descriptor = [MTLBlitPassDescriptor new];
    ASSERT(pendingTimestampWrites.isEmpty() || m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary);
    for (size_t i = 0; i < pendingTimestampWrites.size(); ++i) {
        const auto& pendingTimestampWrite = pendingTimestampWrites[i];
        descriptor.sampleBufferAttachments[i].sampleBuffer = pendingTimestampWrite.querySet->counterSampleBuffer();
        descriptor.sampleBufferAttachments[i].startOfEncoderSampleIndex = i;
    }
    m_blitCommandEncoder = [m_commandBuffer blitCommandEncoderWithDescriptor:descriptor];
    setExistingEncoder(m_blitCommandEncoder);

    return m_blitCommandEncoder;
}

void CommandEncoder::finalizeBlitCommandEncoder()
{
    endEncoding(m_blitCommandEncoder);
    m_blitCommandEncoder = nil;
    setExistingEncoder(nil);

    if (!m_pendingTimestampWrites.isEmpty()) {
        ASSERT(m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary);
        ensureBlitCommandEncoder();
        finalizeBlitCommandEncoder();
    }
}

static auto timestampWriteIndex(auto writeIndex)
{
    return writeIndex == WGPU_QUERY_SET_INDEX_UNDEFINED ? 0 : writeIndex;
}

static NSString* errorValidatingTimestampWrites(const auto& timestampWrites, const CommandEncoder& commandEncoder)
{
    if (timestampWrites) {
        if (!commandEncoder.device().hasFeature(WGPUFeatureName_TimestampQuery))
            return @"device does not have timestamp query feature";

        const auto& timestampWrite = *timestampWrites;
        auto& querySet = fromAPI(timestampWrite.querySet);
        if (querySet.type() != WGPUQueryType_Timestamp)
            return [NSString stringWithFormat:@"query type is not timestamp but %d", querySet.type()];

        if (!isValidToUseWith(querySet, commandEncoder))
            return @"device mismatch";

        auto querySetCount = querySet.count();
        auto beginningOfPassWriteIndex = timestampWriteIndex(timestampWrite.beginningOfPassWriteIndex);
        auto endOfPassWriteIndex = timestampWriteIndex(timestampWrite.endOfPassWriteIndex);
        if (beginningOfPassWriteIndex >= querySetCount || endOfPassWriteIndex >= querySetCount || timestampWrite.beginningOfPassWriteIndex == timestampWrite.endOfPassWriteIndex)
            return [NSString stringWithFormat:@"writeIndices mismatch: beginningOfPassWriteIndex(%u) >= querySetCount(%u) || endOfPassWriteIndex(%u) >= querySetCount(%u) || timestampWrite.beginningOfPassWriteIndex(%u) == timestampWrite.endOfPassWriteIndex(%u)", beginningOfPassWriteIndex, querySetCount, endOfPassWriteIndex, querySetCount, timestampWrite.beginningOfPassWriteIndex, timestampWrite.endOfPassWriteIndex];
    }

    return nil;
}

NSString* CommandEncoder::errorValidatingComputePassDescriptor(const WGPUComputePassDescriptor& descriptor) const
{
    return errorValidatingTimestampWrites(descriptor.timestampWrites, *this);
}

Ref<ComputePassEncoder> CommandEncoder::beginComputePass(const WGPUComputePassDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return ComputePassEncoder::createInvalid(*this, m_device, @"descriptor is corrupted");

    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return ComputePassEncoder::createInvalid(*this, m_device, @"encoder state is invalid");
    }

    if (NSString* error = errorValidatingComputePassDescriptor(descriptor))
        return ComputePassEncoder::createInvalid(*this, m_device, error);

    finalizeBlitCommandEncoder();

    MTLComputePassDescriptor* computePassDescriptor = [MTLComputePassDescriptor new];
    computePassDescriptor.dispatchType = MTLDispatchTypeSerial;

    if (m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary
        && descriptor.timestampWrites) {
        // rdar://91371495 is about how we can't just naively transform descriptor.timestampWrites into computePassDescriptor.sampleBufferAttachments.
        // Instead, we can resolve all the information to a dummy counter sample buffer, and then internally remember that the data
        // is in a different place than where it's supposed to be. Later, when we resolve the data, we can resolve it from our dummy
        // buffer instead of from where it's supposed to be.
        //
        // When rdar://91371495 is fixed, we can delete this indirection, and put the data directly where it's supposed to go.

        MTLCounterSampleBufferDescriptor *counterSampleBufferDescriptor = [MTLCounterSampleBufferDescriptor new];
        counterSampleBufferDescriptor.counterSet = m_device->baseCapabilities().timestampCounterSet;
        counterSampleBufferDescriptor.label = @"Dummy compute pass timestamp counter sample buffer";
        counterSampleBufferDescriptor.storageMode = MTLStorageModePrivate;
        counterSampleBufferDescriptor.sampleCount = 2;
        auto counterSampleBuffer = [m_device->device() newCounterSampleBufferWithDescriptor:counterSampleBufferDescriptor error:nil];
        // FIXME: We should probably do something sensible if the counter sample buffer failed to be created.
        auto dummyQuerySet = QuerySet::create(counterSampleBuffer, 2, WGPUQueryType_Timestamp, m_device);

        const auto startIndex = 0;
        const auto endIndex = 1;

        computePassDescriptor.sampleBufferAttachments[0].sampleBuffer = counterSampleBuffer;
        computePassDescriptor.sampleBufferAttachments[0].startOfEncoderSampleIndex = startIndex;
        computePassDescriptor.sampleBufferAttachments[0].endOfEncoderSampleIndex = endIndex;

        auto& timestampWrite = *descriptor.timestampWrites;
        auto& querySet = fromAPI(timestampWrite.querySet);
        querySet.setCommandEncoder(*this);
        if (querySet.isValid())
            querySet.setOverrideLocation(dummyQuerySet, timestampWriteIndex(timestampWrite.beginningOfPassWriteIndex), timestampWriteIndex(timestampWrite.endOfPassWriteIndex));
    }

    id<MTLComputeCommandEncoder> computeCommandEncoder = [m_commandBuffer computeCommandEncoderWithDescriptor:computePassDescriptor];
    setExistingEncoder(computeCommandEncoder);
    computeCommandEncoder.label = fromAPI(descriptor.label);

    return ComputePassEncoder::create(computeCommandEncoder, descriptor, *this, m_device);
}

void CommandEncoder::setExistingEncoder(id<MTLCommandEncoder> encoder)
{
    ASSERT(!m_existingCommandEncoder || !encoder);
    m_existingCommandEncoder = encoder;
    m_device->getQueue().setEncoderForBuffer(m_commandBuffer, encoder);
}

void CommandEncoder::endEncoding(id<MTLCommandEncoder> encoder)
{
    id<MTLCommandEncoder> existingEncoder = m_device->getQueue().encoderForBuffer(m_commandBuffer);
    if (existingEncoder != encoder) {
        [existingEncoder endEncoding];
        setExistingEncoder(nil);
        return;
    }

    ASSERT(encoder == m_existingCommandEncoder || !m_existingCommandEncoder || !encoder);
    ASSERT(!m_blitCommandEncoder || m_blitCommandEncoder == encoder);
    [m_existingCommandEncoder endEncoding];
    setExistingEncoder(nil);
    m_blitCommandEncoder = nil;
}

NSString* CommandEncoder::errorValidatingRenderPassDescriptor(const WGPURenderPassDescriptor& descriptor) const
{
    if (auto* wgpuOcclusionQuery = descriptor.occlusionQuerySet) {
        const auto& occlusionQuery = fromAPI(wgpuOcclusionQuery);
        if (!isValidToUseWith(occlusionQuery, *this))
            return @"occlusion query does not match the device";
        if (occlusionQuery.type() != WGPUQueryType_Occlusion)
            return @"occlusion query type is not occlusion";
    }

    return errorValidatingTimestampWrites(descriptor.timestampWrites, *this);
}

bool Device::isStencilOnlyFormat(MTLPixelFormat format)
{
    switch (format) {
    case MTLPixelFormatStencil8:
    case MTLPixelFormatX32_Stencil8:
        return true;
    default:
        return false;
    }
}

static std::pair<id<MTLRenderPipelineState>, id<MTLDepthStencilState>> createSimplePso(NSMutableDictionary<NSNumber*, TextureAndClearColor*> *attachmentsToClear, id<MTLTexture> depthStencilAttachmentToClear, bool depthAttachmentToClear, bool stencilAttachmentToClear, id<MTLDevice> device)
{
    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];

    NSUInteger sampleCount = 0;
    MTLDepthStencilDescriptor *depthStencilDescriptor = nil;
    for (NSNumber *key in attachmentsToClear) {
        int i = key.intValue;
        TextureAndClearColor *textureAndClearColor = [attachmentsToClear objectForKey:key];
        id<MTLTexture> t = textureAndClearColor.texture;
        sampleCount = t.sampleCount;

        const auto& mtlColorAttachment = mtlRenderPipelineDescriptor.colorAttachments[i];
        mtlColorAttachment.pixelFormat = t.pixelFormat;
        mtlColorAttachment.blendingEnabled = NO;
    }

    if (depthStencilAttachmentToClear) {
        depthStencilDescriptor = [MTLDepthStencilDescriptor new];
        id<MTLTexture> t = depthStencilAttachmentToClear;
        sampleCount = t.sampleCount;
        mtlRenderPipelineDescriptor.depthAttachmentPixelFormat = (!depthAttachmentToClear || Device::isStencilOnlyFormat(t.pixelFormat)) ? MTLPixelFormatInvalid : t.pixelFormat;
        depthStencilDescriptor.depthWriteEnabled = NO;

        if (stencilAttachmentToClear && (t.pixelFormat == MTLPixelFormatDepth32Float_Stencil8 || t.pixelFormat == MTLPixelFormatStencil8 || t.pixelFormat == MTLPixelFormatX32_Stencil8))
            mtlRenderPipelineDescriptor.stencilAttachmentPixelFormat = t.pixelFormat;
    }

    static id<MTLFunction> function = nil;
    NSError *error = nil;
    if (!function) {
        MTLCompileOptions* options = [MTLCompileOptions new];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        options.fastMathEnabled = YES;
        ALLOW_DEPRECATED_DECLARATIONS_END
        id<MTLLibrary> library = [device newLibraryWithSource:@"[[vertex]] float4 vs() { return (float4)0; }" options:options error:&error];
        ASSERT_UNUSED(error, !error);
        function = [library newFunctionWithName:@"vs"];
    }

    mtlRenderPipelineDescriptor.vertexFunction = function;
    mtlRenderPipelineDescriptor.fragmentFunction = nil;

    RELEASE_ASSERT(sampleCount);
    mtlRenderPipelineDescriptor.rasterSampleCount = sampleCount;
    mtlRenderPipelineDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;

    id<MTLRenderPipelineState> pso = [device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    id<MTLDepthStencilState> depthStencil = depthStencilDescriptor ? [device newDepthStencilStateWithDescriptor:depthStencilDescriptor] : nil;
    RELEASE_ASSERT(!error);
    ASSERT_UNUSED(error, !error);

    return std::make_pair(pso, depthStencil);
}

void CommandEncoder::runClearEncoder(NSMutableDictionary<NSNumber*, TextureAndClearColor*> *attachmentsToClear, id<MTLTexture> depthStencilAttachmentToClear, bool depthAttachmentToClear, bool stencilAttachmentToClear, float depthClearValue, uint32_t stencilClearValue, id<MTLRenderCommandEncoder> existingEncoder)
{
    if (!attachmentsToClear.count && !depthAttachmentToClear && !stencilAttachmentToClear) {
        endEncoding(existingEncoder);
        return;
    }

    if (!stencilAttachmentToClear && !depthAttachmentToClear)
        depthStencilAttachmentToClear = nil;

    id<MTLDevice> device = m_device->device();
    id<MTLRenderCommandEncoder> clearRenderCommandEncoder = existingEncoder;
    if (!clearRenderCommandEncoder) {
        MTLRenderPassDescriptor* clearDescriptor = [MTLRenderPassDescriptor new];
        if (depthAttachmentToClear) {
            clearDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
            clearDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
            clearDescriptor.depthAttachment.clearDepth = depthClearValue;
            clearDescriptor.depthAttachment.texture = depthStencilAttachmentToClear;
        }

        if (stencilAttachmentToClear) {
            clearDescriptor.stencilAttachment.loadAction = MTLLoadActionClear;
            clearDescriptor.stencilAttachment.storeAction = MTLStoreActionStore;
            clearDescriptor.stencilAttachment.clearStencil = stencilClearValue;
            clearDescriptor.stencilAttachment.texture = depthStencilAttachmentToClear;
        }

        if (!attachmentsToClear.count) {
            RELEASE_ASSERT(depthStencilAttachmentToClear);
            clearDescriptor.defaultRasterSampleCount = depthStencilAttachmentToClear.sampleCount;
            clearDescriptor.renderTargetWidth = depthStencilAttachmentToClear.width;
            clearDescriptor.renderTargetHeight = depthStencilAttachmentToClear.height;
        }

        for (NSNumber *key in attachmentsToClear) {
            int i = key.intValue;
            TextureAndClearColor *textureAndClearColor = [attachmentsToClear objectForKey:key];
            id<MTLTexture> t = textureAndClearColor.texture;
            const auto& mtlAttachment = clearDescriptor.colorAttachments[i];
            mtlAttachment.loadAction = MTLLoadActionClear;
            mtlAttachment.storeAction = MTLStoreActionStore;
            mtlAttachment.clearColor = textureAndClearColor.clearColor;
            mtlAttachment.texture = t;
            mtlAttachment.level = 0;
            mtlAttachment.slice = 0;
            mtlAttachment.depthPlane = 0;
        }
        clearRenderCommandEncoder = [m_commandBuffer renderCommandEncoderWithDescriptor:clearDescriptor];
    }
    auto [pso, depthStencil] = createSimplePso(attachmentsToClear, depthStencilAttachmentToClear, depthAttachmentToClear, stencilAttachmentToClear, device);
    [clearRenderCommandEncoder setRenderPipelineState:pso];
    if (depthStencil)
        [clearRenderCommandEncoder setDepthStencilState:depthStencil];
    [clearRenderCommandEncoder setCullMode:MTLCullModeNone];
    [clearRenderCommandEncoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:1 instanceCount:1 baseInstance:0];
    [clearRenderCommandEncoder endEncoding];
}

static bool isMultisampleTexture(id<MTLTexture> texture)
{
    return texture.textureType == MTLTextureType2DMultisample || texture.textureType == MTLTextureType2DMultisampleArray;
}

static bool isRenderableTextureView(const TextureView& texture)
{
    auto textureDimension = texture.dimension();

    return (texture.usage() & WGPUTextureUsage_RenderAttachment) && (textureDimension == WGPUTextureViewDimension_2D || textureDimension == WGPUTextureViewDimension_2DArray || textureDimension == WGPUTextureViewDimension_3D) && texture.mipLevelCount() == 1 && texture.arrayLayerCount() <= 1;
}

Ref<RenderPassEncoder> CommandEncoder::beginRenderPass(const WGPURenderPassDescriptor& descriptor)
{
    auto maxDrawCount = UINT64_MAX;
    if (descriptor.nextInChain) {
        if (descriptor.nextInChain->sType != WGPUSType_RenderPassDescriptorMaxDrawCount)
            return RenderPassEncoder::createInvalid(*this, m_device, @"descriptor is corrupted");
        auto* maxDrawCountStruct = reinterpret_cast<const WGPURenderPassDescriptorMaxDrawCount*>(descriptor.nextInChain);
        maxDrawCount = maxDrawCountStruct->maxDrawCount;
    }

    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return RenderPassEncoder::createInvalid(*this, m_device, @"encoder state is not valid");
    }

    if (NSString* error = errorValidatingRenderPassDescriptor(descriptor))
        return RenderPassEncoder::createInvalid(*this, m_device, error);

    MTLRenderPassDescriptor* mtlDescriptor = [MTLRenderPassDescriptor new];

    if (descriptor.colorAttachmentCount > 8)
        return RenderPassEncoder::createInvalid(*this, m_device, @"color attachment count is > 8");

    finalizeBlitCommandEncoder();
    NSMutableDictionary<NSNumber*, TextureAndClearColor*> *attachmentsToClear = [NSMutableDictionary dictionary];
    bool zeroColorTargets = true;
    uint32_t bytesPerSample = 0;
    const auto maxColorAttachmentBytesPerSample = m_device->limits().maxColorAttachmentBytesPerSample;
    uint32_t textureWidth = 0, textureHeight = 0, sampleCount = 0;
    using SliceSet = HashSet<uint64_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>;
    HashMap<void*, SliceSet> depthSlices;
    for (uint32_t i = 0; i < descriptor.colorAttachmentCount; ++i) {
        const auto& attachment = descriptor.colorAttachments[i];
        if (!attachment.view)
            continue;

        const auto& mtlAttachment = mtlDescriptor.colorAttachments[i];

        mtlAttachment.clearColor = MTLClearColorMake(attachment.clearValue.r,
            attachment.clearValue.g,
            attachment.clearValue.b,
            attachment.clearValue.a);

        auto& texture = fromAPI(attachment.view);
        if (!isValidToUseWith(texture, *this))
            return RenderPassEncoder::createInvalid(*this, m_device, @"device mismatch");
        if (textureWidth && (texture.width() != textureWidth || texture.height() != textureHeight || sampleCount != texture.sampleCount()))
            return RenderPassEncoder::createInvalid(*this, m_device, @"texture size does not match");
        textureWidth = texture.width();
        textureHeight = texture.height();
        sampleCount = texture.sampleCount();
        auto textureFormat = texture.format();
        bytesPerSample = roundUpToMultipleOfNonPowerOfTwo(Texture::renderTargetPixelByteAlignment(textureFormat), bytesPerSample);
        bytesPerSample += Texture::renderTargetPixelByteCost(textureFormat);
        if (bytesPerSample > maxColorAttachmentBytesPerSample)
            return RenderPassEncoder::createInvalid(*this, m_device, @"total bytes per sample exceeds limit");

        bool textureIsDestroyed = texture.isDestroyed();
        if (!textureIsDestroyed) {
            if (!(texture.usage() & WGPUTextureUsage_RenderAttachment) || !Texture::isColorRenderableFormat(textureFormat, m_device))
                return RenderPassEncoder::createInvalid(*this, m_device, @"color attachment is not renderable");

            if (!isRenderableTextureView(texture))
                return RenderPassEncoder::createInvalid(*this, m_device, @"texture view is not renderable");
        }
        texture.setCommandEncoder(*this);

        id<MTLTexture> mtlTexture = texture.texture();
        mtlAttachment.texture = mtlTexture;
        if (!mtlAttachment.texture) {
            if (!textureIsDestroyed)
                return RenderPassEncoder::createInvalid(*this, m_device, @"color attachment's texture is nil");
            continue;
        }
        mtlAttachment.level = 0;
        mtlAttachment.slice = 0;
        uint64_t depthSliceOrArrayLayer = 0;
        auto textureDimension = texture.dimension();
        if (attachment.depthSlice) {
            if (textureDimension != WGPUTextureViewDimension_3D)
                return RenderPassEncoder::createInvalid(*this, m_device, @"depthSlice specified on 2D texture");
            depthSliceOrArrayLayer = *attachment.depthSlice;
            if (depthSliceOrArrayLayer >= texture.depthOrArrayLayers())
                return RenderPassEncoder::createInvalid(*this, m_device, @"depthSlice is greater than texture's depth or array layers");

        } else {
            if (textureDimension == WGPUTextureViewDimension_3D)
                return RenderPassEncoder::createInvalid(*this, m_device, @"textureDimension is 3D and no depth slice is specified");
            depthSliceOrArrayLayer = texture.baseArrayLayer();
        }

        auto* bridgedTexture = (__bridge void*)texture.parentTexture();
        uint64_t depthAndMipLevel = depthSliceOrArrayLayer | (static_cast<uint64_t>(texture.baseMipLevel()) << 32);
        if (auto it = depthSlices.find(bridgedTexture); it != depthSlices.end()) {
            auto addResult = it->value.add(depthAndMipLevel);
            if (!addResult.isNewEntry)
                return RenderPassEncoder::createInvalid(*this, m_device, @"attempting to render to overlapping color attachment");
        } else
            depthSlices.set(bridgedTexture, SliceSet { depthAndMipLevel });

        mtlAttachment.depthPlane = textureDimension == WGPUTextureViewDimension_3D ? depthSliceOrArrayLayer : 0;
        mtlAttachment.slice = 0;
        mtlAttachment.loadAction = loadAction(attachment.loadOp, attachment.storeOp);
        mtlAttachment.storeAction = attachment.resolveTarget ? MTLStoreActionStoreAndMultisampleResolve : storeAction(attachment.storeOp, attachment.loadOp);

        zeroColorTargets = false;
        id<MTLTexture> textureToClear = nil;
        if (mtlAttachment.loadAction == MTLLoadActionLoad && !texture.previouslyCleared())
            textureToClear = mtlAttachment.texture;

        if (attachment.resolveTarget) {
            auto& resolveTarget = fromAPI(attachment.resolveTarget);
            if (!isValidToUseWith(resolveTarget, *this))
                return RenderPassEncoder::createInvalid(*this, m_device, @"resolve target created from different device");
            resolveTarget.setCommandEncoder(*this);
            id<MTLTexture> resolveTexture = resolveTarget.texture();
            if (mtlTexture.sampleCount == 1 || resolveTexture.sampleCount != 1 || isMultisampleTexture(resolveTexture) || !isMultisampleTexture(mtlTexture) || !isRenderableTextureView(resolveTarget) || mtlTexture.pixelFormat != resolveTexture.pixelFormat || !Texture::supportsResolve(resolveTarget.format(), m_device))
                return RenderPassEncoder::createInvalid(*this, m_device, @"resolve target is invalid");

            mtlAttachment.resolveTexture = resolveTexture;
            mtlAttachment.resolveLevel = 0;
            mtlAttachment.resolveSlice = 0;
            mtlAttachment.resolveDepthPlane = 0;
            if (resolveTarget.width() != texture.width() || resolveTarget.height() != texture.height())
                return RenderPassEncoder::createInvalid(*this, m_device, @"resolve target dimensions are invalid");
        }
        if (textureToClear) {
            TextureAndClearColor *textureWithResolve = [[TextureAndClearColor alloc] initWithTexture:textureToClear];
            [attachmentsToClear setObject:textureWithResolve forKey:@(i)];
            if (textureToClear)
                texture.setPreviouslyCleared();
            if (attachment.resolveTarget)
                fromAPI(attachment.resolveTarget).setPreviouslyCleared();
        }
    }

    bool depthReadOnly = false, stencilReadOnly = false;
    bool hasStencilComponent = false;
    id<MTLTexture> depthStencilAttachmentToClear = nil;
    bool depthAttachmentToClear = false;
    if (const auto* attachment = descriptor.depthStencilAttachment) {
        auto& textureView = fromAPI(attachment->view);
        if (!isValidToUseWith(textureView, *this))
            return RenderPassEncoder::createInvalid(*this, m_device, @"depth stencil texture device mismatch");
        id<MTLTexture> metalDepthStencilTexture = textureView.texture();
        hasStencilComponent = Texture::stencilOnlyAspectMetalFormat(textureView.descriptor().format).has_value();
        bool hasDepthComponent = !Device::isStencilOnlyFormat(metalDepthStencilTexture.pixelFormat);
        bool isDestroyed = textureView.isDestroyed();
        if (!isDestroyed) {
            if (textureWidth && (textureView.width() != textureWidth || textureView.height() != textureHeight || sampleCount != textureView.sampleCount()))
                return RenderPassEncoder::createInvalid(*this, m_device, @"depth stencil texture dimensions mismatch");
            if (textureView.arrayLayerCount() > 1 || textureView.mipLevelCount() > 1)
                return RenderPassEncoder::createInvalid(*this, m_device, @"depth stencil texture has more than one array layer or mip level");

            if (!Texture::isDepthStencilRenderableFormat(textureView.format(), m_device) || !isRenderableTextureView(textureView))
                return RenderPassEncoder::createInvalid(*this, m_device, @"depth stencil texture is not renderable");
        }

        depthReadOnly = attachment->depthReadOnly;
        if (hasDepthComponent) {
            const auto& mtlAttachment = mtlDescriptor.depthAttachment;
            auto clearDepth = std::clamp(RenderPassEncoder::quantizedDepthValue(attachment->depthClearValue, textureView.format()), 0., 1.);
            mtlAttachment.clearDepth = clearDepth;
            mtlAttachment.texture = metalDepthStencilTexture;
            mtlAttachment.level = 0;
            mtlAttachment.loadAction = loadAction(attachment->depthLoadOp, attachment->depthStoreOp);
            mtlAttachment.storeAction = storeAction(attachment->depthStoreOp, attachment->depthLoadOp);

            if (mtlAttachment.loadAction == MTLLoadActionLoad && mtlAttachment.storeAction == MTLStoreActionDontCare && !textureView.previouslyCleared()) {
                depthStencilAttachmentToClear = mtlAttachment.texture;
                depthAttachmentToClear = !!mtlAttachment.texture;
            }
        }

        if (!isDestroyed) {
            if (hasDepthComponent && !depthReadOnly) {
                if (attachment->depthLoadOp == WGPULoadOp_Undefined || attachment->depthStoreOp == WGPUStoreOp_Undefined)
                    return RenderPassEncoder::createInvalid(*this, m_device, @"depth load and store op were not specified");
            } else if (attachment->depthLoadOp != WGPULoadOp_Undefined || attachment->depthStoreOp != WGPUStoreOp_Undefined)
                return RenderPassEncoder::createInvalid(*this, m_device, @"depth load and store op were specified");
        }

        if (attachment->depthLoadOp == WGPULoadOp_Clear && (attachment->depthClearValue < 0 || attachment->depthClearValue > 1))
            return RenderPassEncoder::createInvalid(*this, m_device, @"depth clear value is invalid");

        if (zeroColorTargets) {
            mtlDescriptor.defaultRasterSampleCount = textureView.sampleCount();
            mtlDescriptor.renderTargetWidth = textureView.width();
            mtlDescriptor.renderTargetHeight = textureView.height();
        }
    }

    bool stencilAttachmentToClear = false;
    if (const auto* attachment = descriptor.depthStencilAttachment) {
        const auto& mtlAttachment = mtlDescriptor.stencilAttachment;
        stencilReadOnly = attachment->stencilReadOnly;
        auto& textureView = fromAPI(attachment->view);
        if (hasStencilComponent)
            mtlAttachment.texture = textureView.texture();
        mtlAttachment.clearStencil = attachment->stencilClearValue;
        mtlAttachment.loadAction = loadAction(attachment->stencilLoadOp, attachment->stencilStoreOp);
        mtlAttachment.storeAction = storeAction(attachment->stencilStoreOp, attachment->stencilLoadOp);

        bool isDestroyed = textureView.isDestroyed();
        if (!isDestroyed) {
            if (hasStencilComponent && !stencilReadOnly) {
                if (attachment->stencilLoadOp == WGPULoadOp_Undefined || attachment->stencilStoreOp == WGPUStoreOp_Undefined)
                    return RenderPassEncoder::createInvalid(*this, m_device, @"stencil load and store op were not specified");
            } else if (attachment->stencilLoadOp != WGPULoadOp_Undefined || attachment->stencilStoreOp != WGPUStoreOp_Undefined)
                return RenderPassEncoder::createInvalid(*this, m_device, @"stencil load and store op were specified");
        }

        textureView.setCommandEncoder(*this);

        if (mtlAttachment.loadAction == MTLLoadActionLoad && mtlAttachment.storeAction == MTLStoreActionDontCare && !textureView.previouslyCleared()) {
            depthStencilAttachmentToClear = mtlAttachment.texture;
            stencilAttachmentToClear = !!mtlAttachment.texture;
        }
    }

    if (zeroColorTargets && !mtlDescriptor.renderTargetWidth)
        return RenderPassEncoder::createInvalid(*this, m_device, @"zero color and depth targets");

    size_t visibilityResultBufferSize = 0;
    id<MTLBuffer> visibilityResultBuffer = nil;
    if (auto* wgpuOcclusionQuery = descriptor.occlusionQuerySet) {
        const auto& occlusionQuery = fromAPI(wgpuOcclusionQuery);
        occlusionQuery.setCommandEncoder(*this);
        if (occlusionQuery.type() != WGPUQueryType_Occlusion)
            return RenderPassEncoder::createInvalid(*this, m_device, @"querySet for occlusion query was not of type occlusion");
        mtlDescriptor.visibilityResultBuffer = occlusionQuery.visibilityBuffer();
        visibilityResultBuffer = mtlDescriptor.visibilityResultBuffer;
        visibilityResultBufferSize = occlusionQuery.isDestroyed() ? NSUIntegerMax : occlusionQuery.visibilityBuffer().length;
    }

    if (m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary
        && descriptor.timestampWrites) {
        // rdar://91371495 is about how we can't just naively transform descriptor.timestampWrites into computePassDescriptor.sampleBufferAttachments.
        // Instead, we can resolve all the information to a dummy counter sample buffer, and then internally remember that the data
        // is in a different place than where it's supposed to be. Later, when we resolve the data, we can resolve it from our dummy
        // buffer instead of from where it's supposed to be.
        //
        // When rdar://91371495 is fixed, we can delete this indirection, and put the data directly where it's supposed to go.

        MTLCounterSampleBufferDescriptor *counterSampleBufferDescriptor = [MTLCounterSampleBufferDescriptor new];
        counterSampleBufferDescriptor.counterSet = m_device->baseCapabilities().timestampCounterSet;
        counterSampleBufferDescriptor.label = @"Dummy render pass timestamp counter sample buffer";
        counterSampleBufferDescriptor.storageMode = MTLStorageModePrivate;
        counterSampleBufferDescriptor.sampleCount = 4;
        auto counterSampleBuffer = [m_device->device() newCounterSampleBufferWithDescriptor:counterSampleBufferDescriptor error:nil];
        // FIXME: We should probably do something sensible if the counter sample buffer failed to be created.
        auto dummyQuerySet = QuerySet::create(counterSampleBuffer, 4, WGPUQueryType_Timestamp, m_device);

        const auto startVertexIndex = 0;
        const auto endVertexIndex = 1;
        const auto startFragmentIndex = 2;
        const auto endFragmentIndex = 3;

        mtlDescriptor.sampleBufferAttachments[0].sampleBuffer = counterSampleBuffer;
        mtlDescriptor.sampleBufferAttachments[0].startOfVertexSampleIndex = startVertexIndex;
        mtlDescriptor.sampleBufferAttachments[0].endOfVertexSampleIndex = endVertexIndex;
        mtlDescriptor.sampleBufferAttachments[0].startOfFragmentSampleIndex = startFragmentIndex;
        mtlDescriptor.sampleBufferAttachments[0].endOfFragmentSampleIndex = endFragmentIndex;

        auto& timestampWrite = *descriptor.timestampWrites;
        auto& querySet = fromAPI(timestampWrite.querySet);
        querySet.setCommandEncoder(*this);
        if (querySet.isValid())
            querySet.setOverrideLocation(dummyQuerySet, timestampWriteIndex(timestampWrite.beginningOfPassWriteIndex), timestampWriteIndex(timestampWrite.endOfPassWriteIndex));
    }

    if (attachmentsToClear.count || depthStencilAttachmentToClear) {
        if (const auto* attachment = descriptor.depthStencilAttachment; depthStencilAttachmentToClear)
            fromAPI(attachment->view).setPreviouslyCleared();

        runClearEncoder(attachmentsToClear, depthStencilAttachmentToClear, depthAttachmentToClear, stencilAttachmentToClear);
    }

    auto mtlRenderCommandEncoder = [m_commandBuffer renderCommandEncoderWithDescriptor:mtlDescriptor];
    ASSERT(!m_existingCommandEncoder);
    setExistingEncoder(mtlRenderCommandEncoder);
    return RenderPassEncoder::create(mtlRenderCommandEncoder, descriptor, visibilityResultBufferSize, depthReadOnly, stencilReadOnly, *this, visibilityResultBuffer, maxDrawCount, m_device);
}

NSString* CommandEncoder::errorValidatingCopyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size)
{
#define ERROR_STRING(x) (@"GPUCommandEncoder.copyBufferToBuffer: " x)
    if (!source.isDestroyed() && !isValidToUseWith(source, *this))
        return ERROR_STRING(@"source buffer is not valid");

    if (!destination.isDestroyed() && !isValidToUseWith(destination, *this))
        return ERROR_STRING(@"destination buffer is not valid");

    if (!(source.usage() & WGPUBufferUsage_CopySrc))
        return ERROR_STRING(@"source usage does not have COPY_SRC");

    if (!(destination.usage() & WGPUBufferUsage_CopyDst))
        return ERROR_STRING(@"destination usage does not have COPY_DST");

    if (destination.state() == Buffer::State::MappingPending || source.state() == Buffer::State::MappingPending)
        return ERROR_STRING(@"destination state is not unmapped or source state is not unmapped");

    if (size % 4)
        return ERROR_STRING(@"size is not a multiple of 4");

    if (sourceOffset % 4)
        return ERROR_STRING(@"source offset is not a multiple of 4");

    if (destinationOffset % 4)
        return ERROR_STRING(@"destination offset is not a multiple of 4");

    auto sourceEnd = checkedSum<uint64_t>(sourceOffset, size);
    if (sourceEnd.hasOverflowed())
        return ERROR_STRING(@"source size + offset overflows");

    auto destinationEnd = checkedSum<uint64_t>(destinationOffset, size);
    if (destinationEnd.hasOverflowed())
        return ERROR_STRING(@"destination size + offset overflows");

    if (source.initialSize() < sourceEnd.value())
        return ERROR_STRING(@"source size + offset overflows");

    if (destination.initialSize() < destinationEnd.value())
        return ERROR_STRING(@"destination size + offset overflows");

    if (&source == &destination)
        return ERROR_STRING(@"source equals destination not valid");

#undef ERROR_STRING
    return nil;
}

void CommandEncoder::incrementBufferMapCount()
{
    ++m_bufferMapCount;
    if (m_cachedCommandBuffer)
        m_cachedCommandBuffer->setBufferMapCount(m_bufferMapCount);
}

void CommandEncoder::decrementBufferMapCount()
{
    --m_bufferMapCount;
    if (m_cachedCommandBuffer)
        m_cachedCommandBuffer->setBufferMapCount(m_bufferMapCount);
}

void CommandEncoder::copyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copybuffertobuffer
    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return;
    }

    if (NSString* error = errorValidatingCopyBufferToBuffer(source, sourceOffset, destination, destinationOffset, size)) {
        makeInvalid(error);
        return;
    }

    source.setCommandEncoder(*this);
    destination.setCommandEncoder(*this);
    if (!size || source.isDestroyed() || destination.isDestroyed())
        return;

    ensureBlitCommandEncoder();

    [m_blitCommandEncoder copyFromBuffer:source.buffer() sourceOffset:static_cast<NSUInteger>(sourceOffset) toBuffer:destination.buffer() destinationOffset:static_cast<NSUInteger>(destinationOffset) size:static_cast<NSUInteger>(size)];
}

NSString* CommandEncoder::errorValidatingImageCopyBuffer(const WGPUImageCopyBuffer& imageCopyBuffer) const
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpuimagecopybuffer
    const auto& buffer = fromAPI(imageCopyBuffer.buffer);
    if (!isValidToUseWith(buffer, *this))
        return @"buffer is not valid";

    if (!buffer.isDestroyed()) {
        if (buffer.state() != Buffer::State::Unmapped)
            return @"buffer state != Unmapped";
    }

    if (imageCopyBuffer.layout.bytesPerRow != WGPU_COPY_STRIDE_UNDEFINED && (imageCopyBuffer.layout.bytesPerRow % 256))
        return @"imageCopyBuffer.layout.bytesPerRow is not a multiple of 256";

    return nil;
}

static bool refersToAllAspects(WGPUTextureFormat format, WGPUTextureAspect aspect)
{
    switch (aspect) {
    case WGPUTextureAspect_All:
        return true;
    case WGPUTextureAspect_StencilOnly:
        return Texture::containsStencilAspect(format) && !Texture::containsDepthAspect(format);
    case WGPUTextureAspect_DepthOnly:
        return Texture::containsDepthAspect(format) && !Texture::containsStencilAspect(format);
    case WGPUTextureAspect_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

NSString* CommandEncoder::errorValidatingCopyBufferToTexture(const WGPUImageCopyBuffer& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize) const
{
#define ERROR_STRING(x) [NSString stringWithFormat:@"GPUCommandEncoder.copyBufferToTexture: %@", x]
    const auto& destinationTexture = fromAPI(destination.texture);
    const auto& sourceBuffer = fromAPI(source.buffer);

    if (NSString* error = errorValidatingImageCopyBuffer(source))
        return ERROR_STRING(error);

    if (!(sourceBuffer.usage() & WGPUBufferUsage_CopySrc))
        return ERROR_STRING(@"source usage does not contain CopySrc");

    if (!isValidToUseWith(destinationTexture, *this))
        return ERROR_STRING(@"destination texture is not valid to use with this GPUCommandEncoder");

    if (NSString* error = Texture::errorValidatingImageCopyTexture(destination, copySize))
        return ERROR_STRING(error);

    if (!(destinationTexture.usage() & WGPUTextureUsage_CopyDst))
        return ERROR_STRING(@"destination usage does not contain CopyDst");

    if (destinationTexture.sampleCount() != 1)
        return ERROR_STRING(@"destination sample count is not one");

    WGPUTextureFormat aspectSpecificFormat = destinationTexture.format();

    if (Texture::isDepthOrStencilFormat(destinationTexture.format())) {
        if (!Texture::refersToSingleAspect(destinationTexture.format(), destination.aspect))
            return ERROR_STRING(@"destination aspect refers to more than one asepct");

        if (!Texture::isValidDepthStencilCopyDestination(destinationTexture.format(), destination.aspect))
            return ERROR_STRING(@"destination is not valid depthStencilCopyDestination");

        aspectSpecificFormat = Texture::aspectSpecificFormat(destinationTexture.format(), destination.aspect);
    }

    if (NSString* error = Texture::errorValidatingTextureCopyRange(destination, copySize))
        return ERROR_STRING(error);

    if (!Texture::isDepthOrStencilFormat(destinationTexture.format())) {
        auto texelBlockSize = Texture::texelBlockSize(destinationTexture.format());
        if (source.layout.offset % texelBlockSize)
            return ERROR_STRING(@"source.layout.offset is not a multiple of texelBlockSize");
    }

    if (Texture::isDepthOrStencilFormat(destinationTexture.format())) {
        if (source.layout.offset % 4)
            return ERROR_STRING(@"source.layout.offset is not a multiple of four for depth stencil format");
    }

    if (!Texture::validateLinearTextureData(source.layout, fromAPI(source.buffer).initialSize(), aspectSpecificFormat, copySize))
        return ERROR_STRING(@"source.layout.offset is not a multiple of four for depth stencil format");
#undef ERROR_STRING
    return nil;
}

void CommandEncoder::copyBufferToTexture(const WGPUImageCopyBuffer& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize)
{
    if (source.nextInChain || source.layout.nextInChain || destination.nextInChain)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copybuffertotexture

    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return;
    }

    auto& destinationTexture = fromAPI(destination.texture);
    if (NSString* error = errorValidatingCopyBufferToTexture(source, destination, copySize)) {
        makeInvalid(error);
        return;
    }

    auto& apiBuffer = fromAPI(source.buffer);
    apiBuffer.setCommandEncoder(*this);
    destinationTexture.setCommandEncoder(*this);

    if (!copySize.width && !copySize.height && !copySize.depthOrArrayLayers)
        return;

    if (apiBuffer.isDestroyed() || destinationTexture.isDestroyed())
        return;

    ensureBlitCommandEncoder();

    NSUInteger sourceBytesPerRow = source.layout.bytesPerRow;
    id<MTLBuffer> sourceBuffer = apiBuffer.buffer();
    RELEASE_ASSERT(sourceBuffer);
    if (sourceBytesPerRow == WGPU_COPY_STRIDE_UNDEFINED)
        sourceBytesPerRow = sourceBuffer.length;

    auto aspectSpecificFormat = Texture::aspectSpecificFormat(destinationTexture.format(), destination.aspect);
    auto blockSize = Texture::texelBlockSize(aspectSpecificFormat);
    switch (destinationTexture.dimension()) {
    case WGPUTextureDimension_1D:
        sourceBytesPerRow = std::min<uint32_t>(sourceBytesPerRow, blockSize * m_device->limits().maxTextureDimension1D);
        break;
    case WGPUTextureDimension_2D:
    case WGPUTextureDimension_3D:
        sourceBytesPerRow = std::min<uint32_t>(sourceBytesPerRow, blockSize * m_device->limits().maxTextureDimension2D);
        break;
    case WGPUTextureDimension_Force32:
        break;
    }


    MTLBlitOption options = MTLBlitOptionNone;
    switch (destination.aspect) {
    case WGPUTextureAspect_All:
        options = MTLBlitOptionNone;
        break;
    case WGPUTextureAspect_StencilOnly:
        options = MTLBlitOptionStencilFromDepthStencil;
        break;
    case WGPUTextureAspect_DepthOnly:
        options = MTLBlitOptionDepthFromDepthStencil;
        break;
    case WGPUTextureAspect_Force32:
        ASSERT_NOT_REACHED();
        return;
    }

    auto logicalSize = fromAPI(destination.texture).logicalMiplevelSpecificTextureExtent(destination.mipLevel);
    auto widthForMetal = logicalSize.width < destination.origin.x ? 0 : std::min(copySize.width, logicalSize.width - destination.origin.x);
    auto heightForMetal = logicalSize.height < destination.origin.y ? 0 : std::min(copySize.height, logicalSize.height - destination.origin.y);
    auto depthForMetal = logicalSize.depthOrArrayLayers < destination.origin.z ? 0 : std::min(copySize.depthOrArrayLayers, logicalSize.depthOrArrayLayers - destination.origin.z);

    auto rowsPerImage = source.layout.rowsPerImage;
    if (rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED)
        rowsPerImage = heightForMetal ?: 1;

    NSUInteger sourceBytesPerImage = rowsPerImage * sourceBytesPerRow;

    id<MTLTexture> mtlDestinationTexture = destinationTexture.texture();

    auto textureDimension = destinationTexture.dimension();
    uint32_t sliceCount = textureDimension == WGPUTextureDimension_3D ? 1 : copySize.depthOrArrayLayers;
    for (uint32_t layer = 0; layer < sliceCount; ++layer) {
        NSUInteger destinationSlice = destinationTexture.dimension() == WGPUTextureDimension_3D ? 0 : (destination.origin.z + layer);
        RELEASE_ASSERT(mtlDestinationTexture.parentTexture == nil);
        if (Queue::writeWillCompletelyClear(textureDimension, widthForMetal, logicalSize.width, heightForMetal, logicalSize.height, depthForMetal, logicalSize.depthOrArrayLayers))
            destinationTexture.setPreviouslyCleared(destination.mipLevel, destinationSlice);
        else
            clearTextureIfNeeded(destination, destinationSlice);
    }

    NSUInteger maxSourceBytesPerRow = textureDimension == WGPUTextureDimension_3D ? (2048 * blockSize) : sourceBytesPerRow;

    if (textureDimension == WGPUTextureDimension_3D && copySize.depthOrArrayLayers <= 1 && copySize.height <= 1)
        sourceBytesPerRow = 0;

    if (sourceBytesPerRow > maxSourceBytesPerRow) {
        for (uint32_t z = 0; z < copySize.depthOrArrayLayers; ++z) {
            for (uint32_t y = 0; y < copySize.height; ++y) {
                WGPUImageCopyBuffer newSource {
                    .nextInChain = nullptr,
                    .layout = WGPUTextureDataLayout {
                        .nextInChain = nullptr,
                        .offset = z * sourceBytesPerImage + y * sourceBytesPerRow + source.layout.offset,
                        .bytesPerRow = WGPU_COPY_STRIDE_UNDEFINED,
                        .rowsPerImage = WGPU_COPY_STRIDE_UNDEFINED,
                    },
                    .buffer = source.buffer
                };
                WGPUImageCopyTexture newDestination {
                    .nextInChain = nullptr,
                    .texture = destination.texture,
                    .mipLevel = destination.mipLevel,
                    .origin = { .x = destination.origin.x, .y = destination.origin.y + y, .z = destination.origin.z + z },
                    .aspect = destination.aspect
                };

                copyBufferToTexture(newSource, newDestination, {
                    .width = copySize.width,
                    .height = 1,
                    .depthOrArrayLayers = 1
                });
            }
        }
        return;
    }

    if (sourceBuffer.length < Texture::bytesPerRow(aspectSpecificFormat, widthForMetal, destinationTexture.sampleCount()))
        return;

    switch (destinationTexture.dimension()) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, 1, 1);
        if (!widthForMetal)
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, 0, 0);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            auto sourceOffset = static_cast<NSUInteger>(source.layout.offset + layer * sourceBytesPerImage);
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromBuffer:sourceBuffer
                sourceOffset:sourceOffset
                sourceBytesPerRow:sourceBytesPerRow
                sourceBytesPerImage:sourceBytesPerImage
                sourceSize:sourceSize
                toTexture:mtlDestinationTexture
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_2D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
        // "When you copy to a 2D texture, depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, 1);
        if (!widthForMetal || !heightForMetal)
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, 0);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            auto sourceOffset = static_cast<NSUInteger>(source.layout.offset + layer * sourceBytesPerImage);

            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromBuffer:sourceBuffer
                sourceOffset:sourceOffset
                sourceBytesPerRow:sourceBytesPerRow
                sourceBytesPerImage:sourceBytesPerImage
                sourceSize:sourceSize
                toTexture:mtlDestinationTexture
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_3D: {
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, depthForMetal);
        if (!widthForMetal || !heightForMetal || !depthForMetal)
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, destination.origin.z);
        auto sourceOffset = static_cast<NSUInteger>(source.layout.offset);
        [m_blitCommandEncoder
            copyFromBuffer:sourceBuffer
            sourceOffset:sourceOffset
            sourceBytesPerRow:sourceBytesPerRow
            sourceBytesPerImage:sourceBytesPerImage
            sourceSize:sourceSize
            toTexture:mtlDestinationTexture
            destinationSlice:0
            destinationLevel:destination.mipLevel
            destinationOrigin:destinationOrigin
            options:options];
        break;
    }
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return;
    }
}

NSString* CommandEncoder::errorValidatingCopyTextureToBuffer(const WGPUImageCopyTexture& source, const WGPUImageCopyBuffer& destination, const WGPUExtent3D& copySize) const
{
#define ERROR_STRING(x) [NSString stringWithFormat:@"GPUCommandEncoder.copyTextureToBuffer: %@", x]
    const auto& sourceTexture = fromAPI(source.texture);

    if (!isValidToUseWith(sourceTexture, *this))
        return ERROR_STRING(@"source texture is not valid to use with this GPUCommandEncoder");

    if (NSString* error = Texture::errorValidatingImageCopyTexture(source, copySize))
        return ERROR_STRING(error);

    if (!(sourceTexture.usage() & WGPUTextureUsage_CopySrc))
        return ERROR_STRING(@"sourceTexture usage does not contain CopySrc");

    if (sourceTexture.sampleCount() != 1)
        return ERROR_STRING(@"sourceTexture sample count != 1");

    WGPUTextureFormat aspectSpecificFormat = sourceTexture.format();

    if (Texture::isDepthOrStencilFormat(sourceTexture.format())) {
        if (!Texture::refersToSingleAspect(sourceTexture.format(), source.aspect))
            return ERROR_STRING(@"copying to depth stencil texture with more than one aspect");

        if (!Texture::isValidDepthStencilCopySource(sourceTexture.format(), source.aspect))
            return ERROR_STRING(@"copying to depth stencil texture, validDepthStencilCopySource fails");

        aspectSpecificFormat = Texture::aspectSpecificFormat(sourceTexture.format(), source.aspect);
    }

    if (NSString* error = errorValidatingImageCopyBuffer(destination))
        return ERROR_STRING(error);

    if (!(fromAPI(destination.buffer).usage() & WGPUBufferUsage_CopyDst))
        return ERROR_STRING(@"destination buffer usage does not contain CopyDst");

    if (NSString* error = Texture::errorValidatingTextureCopyRange(source, copySize))
        return ERROR_STRING(error);

    if (!Texture::isDepthOrStencilFormat(sourceTexture.format())) {
        auto texelBlockSize = Texture::texelBlockSize(sourceTexture.format());
        if (destination.layout.offset % texelBlockSize)
            return ERROR_STRING(@"destination.layout.offset is not a multiple of texelBlockSize");
    }

    if (Texture::isDepthOrStencilFormat(sourceTexture.format())) {
        if (destination.layout.offset % 4)
            return ERROR_STRING(@"destination.layout.offset is not a multiple of 4");
    }

    if (!Texture::validateLinearTextureData(destination.layout, fromAPI(destination.buffer).initialSize(), aspectSpecificFormat, copySize))
        return ERROR_STRING(@"validateLinearTextureData fails");
#undef ERROR_STRING
    return nil;
}

void CommandEncoder::clearTextureIfNeeded(const WGPUImageCopyTexture& destination, NSUInteger slice)
{
    clearTextureIfNeeded(destination, slice, m_device->device(), m_blitCommandEncoder);
}

void CommandEncoder::clearTextureIfNeeded(const WGPUImageCopyTexture& destination, NSUInteger slice, id<MTLDevice> device, id<MTLBlitCommandEncoder> blitCommandEncoder)
{
    auto& texture = fromAPI(destination.texture);
    NSUInteger mipLevel = destination.mipLevel;
    CommandEncoder::clearTextureIfNeeded(texture, mipLevel, slice, device, blitCommandEncoder);
}

void CommandEncoder::clearTextureIfNeeded(Texture& texture, NSUInteger mipLevel, NSUInteger slice, id<MTLDevice> device, id<MTLBlitCommandEncoder> blitCommandEncoder)
{
    if (!blitCommandEncoder || texture.previouslyCleared(mipLevel, slice))
        return;

    texture.setPreviouslyCleared(mipLevel, slice);
    auto logicalSize = texture.logicalMiplevelSpecificTextureExtent(mipLevel);
    if (!logicalSize.width)
        return;
    if (texture.dimension() != WGPUTextureDimension_1D && !logicalSize.height)
        return;
    if (texture.dimension() == WGPUTextureDimension_3D && !logicalSize.depthOrArrayLayers)
        return;

    auto depth = texture.dimension() == WGPUTextureDimension_3D ? logicalSize.depthOrArrayLayers : 1;
    id<MTLTexture> mtlTexture = texture.texture();
    if (!mtlTexture)
        return;

    NSUInteger sourceBytesPerRow = 0;
    auto textureFormat = texture.format();
    if (mtlTexture.pixelFormat == MTLPixelFormatDepth32Float_Stencil8 || mtlTexture.pixelFormat == MTLPixelFormatX32_Stencil8) {
        textureFormat = WGPUTextureFormat_Depth32Float;
        sourceBytesPerRow = Texture::bytesPerRow(WGPUTextureFormat_Depth32Float, logicalSize.width, texture.sampleCount());
    } else
        sourceBytesPerRow = Texture::bytesPerRow(textureFormat, logicalSize.width, texture.sampleCount());
    auto blockSize = Texture::texelBlockSize(textureFormat);
    sourceBytesPerRow = std::max<NSUInteger>(blockSize * 12 * (sourceBytesPerRow / logicalSize.width), sourceBytesPerRow);
    if (Texture::isCompressedFormat(textureFormat))
        sourceBytesPerRow = roundUpToMultipleOf(blockSize, sourceBytesPerRow);

    NSUInteger sourceBytesPerImage = sourceBytesPerRow * logicalSize.height;
    NSUInteger bufferLength = sourceBytesPerImage * depth;
    if (!bufferLength)
        return;
    id<MTLBuffer> temporaryBuffer = [device newBufferWithLength:bufferLength options:MTLResourceStorageModeShared];
    if (!temporaryBuffer)
        return;

    MTLSize sourceSize;
    switch (texture.dimension()) {
    case WGPUTextureDimension_1D:
        sourceSize = MTLSizeMake(logicalSize.width, 1, 1);
        break;
    case WGPUTextureDimension_2D:
        sourceSize = MTLSizeMake(logicalSize.width, logicalSize.height, 1);
        break;
    case WGPUTextureDimension_3D:
        sourceSize = MTLSizeMake(logicalSize.width, logicalSize.height, logicalSize.depthOrArrayLayers);
        break;
    default:
    case WGPUTextureDimension_Force32:
        RELEASE_ASSERT_NOT_REACHED();
    }

    MTLBlitOption options = MTLBlitOptionNone;
    if (mtlTexture.pixelFormat == MTLPixelFormatDepth32Float_Stencil8)
        options = MTLBlitOptionDepthFromDepthStencil;

    auto& destinationTexture = texture;
    if (destinationTexture.dimension() == WGPUTextureDimension_3D)
        slice = 0;

    [blitCommandEncoder
        copyFromBuffer:temporaryBuffer
        sourceOffset:0
        sourceBytesPerRow:sourceBytesPerRow
        sourceBytesPerImage:sourceBytesPerImage
        sourceSize:sourceSize
        toTexture:mtlTexture
        destinationSlice:slice
        destinationLevel:mipLevel
        destinationOrigin:MTLOriginMake(0, 0, 0)
        options:options];

    if (options != MTLBlitOptionNone) {
        sourceBytesPerRow /= sizeof(float);
        sourceBytesPerImage = sourceBytesPerRow * logicalSize.height;
        [blitCommandEncoder
            copyFromBuffer:temporaryBuffer
            sourceOffset:0
            sourceBytesPerRow:sourceBytesPerRow
            sourceBytesPerImage:sourceBytesPerImage
            sourceSize:sourceSize
            toTexture:mtlTexture
            destinationSlice:slice
            destinationLevel:mipLevel
            destinationOrigin:MTLOriginMake(0, 0, 0)
            options:MTLBlitOptionStencilFromDepthStencil];
    }
}

void CommandEncoder::makeInvalid(NSString* errorString)
{
    endEncoding(m_existingCommandEncoder);
    m_blitCommandEncoder = nil;
    m_device->getQueue().commitMTLCommandBuffer(m_commandBuffer);

    m_commandBuffer = nil;
    m_lastErrorString = errorString;
    if (m_cachedCommandBuffer)
        m_cachedCommandBuffer.get()->makeInvalid(errorString);
}

void CommandEncoder::makeSubmitInvalid(NSString* errorString)
{
    m_makeSubmitInvalid = true;
    if (m_cachedCommandBuffer)
        m_cachedCommandBuffer->makeInvalid(errorString ?: m_lastErrorString);
}

void CommandEncoder::copyTextureToBuffer(const WGPUImageCopyTexture& source, const WGPUImageCopyBuffer& destination, const WGPUExtent3D& copySize)
{
    if (source.nextInChain || destination.nextInChain || destination.layout.nextInChain)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copytexturetobuffer

    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return;
    }

    auto& sourceTexture = fromAPI(source.texture);
    if (NSString* error = errorValidatingCopyTextureToBuffer(source, destination, copySize)) {
        makeInvalid(error);
        return;
    }

    auto& apiDestinationBuffer = fromAPI(destination.buffer);
    sourceTexture.setCommandEncoder(*this);
    apiDestinationBuffer.setCommandEncoder(*this);
    if (sourceTexture.isDestroyed() || apiDestinationBuffer.isDestroyed())
        return;

    MTLBlitOption options = MTLBlitOptionNone;
    switch (source.aspect) {
    case WGPUTextureAspect_All:
        options = MTLBlitOptionNone;
        break;
    case WGPUTextureAspect_StencilOnly:
        options = MTLBlitOptionStencilFromDepthStencil;
        break;
    case WGPUTextureAspect_DepthOnly:
        options = MTLBlitOptionDepthFromDepthStencil;
        break;
    case WGPUTextureAspect_Force32:
        ASSERT_NOT_REACHED();
        return;
    }

    auto logicalSize = sourceTexture.logicalMiplevelSpecificTextureExtent(source.mipLevel);
    auto widthForMetal = logicalSize.width < source.origin.x ? 0 : std::min(copySize.width, logicalSize.width - source.origin.x);
    auto heightForMetal = logicalSize.height < source.origin.y ? 0 : std::min(copySize.height, logicalSize.height - source.origin.y);
    auto depthForMetal = logicalSize.depthOrArrayLayers < source.origin.z ? 0 : std::min(copySize.depthOrArrayLayers, logicalSize.depthOrArrayLayers - source.origin.z);

    auto destinationBuffer = apiDestinationBuffer.buffer();
    NSUInteger destinationBytesPerRow = destination.layout.bytesPerRow;
    if (destinationBytesPerRow == WGPU_COPY_STRIDE_UNDEFINED)
        destinationBytesPerRow = destinationBuffer.length;

    auto sourceTextureFormat = sourceTexture.format();
    auto aspectSpecificFormat = Texture::aspectSpecificFormat(sourceTextureFormat, source.aspect);
    auto blockSize = Texture::texelBlockSize(aspectSpecificFormat);
    auto textureDimension = sourceTexture.dimension();
    switch (textureDimension) {
    case WGPUTextureDimension_1D:
        destinationBytesPerRow = std::min<uint32_t>(destinationBytesPerRow, blockSize * m_device->limits().maxTextureDimension1D);
        break;
    case WGPUTextureDimension_2D:
    case WGPUTextureDimension_3D:
        destinationBytesPerRow = std::min<uint32_t>(destinationBytesPerRow, blockSize * m_device->limits().maxTextureDimension2D);
        break;
    case WGPUTextureDimension_Force32:
        break;
    }

    if (textureDimension == WGPUTextureDimension_3D && copySize.depthOrArrayLayers <= 1 && copySize.height <= 1)
        destinationBytesPerRow = 0;

    auto rowsPerImage = destination.layout.rowsPerImage;
    if (rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED)
        rowsPerImage = heightForMetal ?: 1;
    NSUInteger destinationBytesPerImage = rowsPerImage * destinationBytesPerRow;

    NSUInteger maxDestinationBytesPerRow = textureDimension == WGPUTextureDimension_3D ? (2048 * blockSize) : destinationBytesPerRow;
    if (destinationBytesPerRow > maxDestinationBytesPerRow) {
        for (uint32_t z = 0; z < copySize.depthOrArrayLayers; ++z) {
            for (uint32_t y = 0; y < copySize.height; ++y) {
                WGPUImageCopyTexture newSource {
                    .nextInChain = nullptr,
                    .texture = source.texture,
                    .mipLevel = source.mipLevel,
                    .origin = { .x = source.origin.x, .y = source.origin.y + y, .z = source.origin.z + z },
                    .aspect = source.aspect
                };
                WGPUImageCopyBuffer newDestination {
                    .nextInChain = nullptr,
                    .layout = WGPUTextureDataLayout {
                        .nextInChain = nullptr,
                        .offset = z * destinationBytesPerImage + y * destinationBytesPerRow + destination.layout.offset,
                        .bytesPerRow = WGPU_COPY_STRIDE_UNDEFINED,
                        .rowsPerImage = WGPU_COPY_STRIDE_UNDEFINED,
                    },
                    .buffer = destination.buffer
                };
                copyTextureToBuffer(newSource, newDestination, {
                    .width = copySize.width,
                    .height = 1,
                    .depthOrArrayLayers = 1
                });
            }
        }
        return;
    }

    ensureBlitCommandEncoder();

    for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
        NSUInteger sourceSlice = sourceTexture.dimension() == WGPUTextureDimension_3D ? 0 : (source.origin.z + layer);
        if (!sourceTexture.previouslyCleared(source.mipLevel, sourceSlice))
            clearTextureIfNeeded(source, sourceSlice);
    }

    if (destinationBuffer.length < Texture::bytesPerRow(aspectSpecificFormat, widthForMetal, sourceTexture.sampleCount()))
        return;

    switch (sourceTexture.dimension()) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, 1, 1);
        if (!widthForMetal)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, 0, 0);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            auto destinationOffset = static_cast<NSUInteger>(destination.layout.offset + layer * destinationBytesPerImage);
            NSUInteger sourceSlice = source.origin.z + layer;
            [m_blitCommandEncoder
                copyFromTexture:sourceTexture.texture()
                sourceSlice:sourceSlice
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toBuffer:destinationBuffer
                destinationOffset:destinationOffset
                destinationBytesPerRow:destinationBytesPerRow
                destinationBytesPerImage:destinationBytesPerImage
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_2D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
        // "When you copy to a 2D texture, depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, 1);
        if (!widthForMetal || !heightForMetal)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, 0);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            auto destinationOffset = static_cast<NSUInteger>(destination.layout.offset + layer * destinationBytesPerImage);
            NSUInteger sourceSlice = source.origin.z + layer;
            [m_blitCommandEncoder
                copyFromTexture:sourceTexture.texture()
                sourceSlice:sourceSlice
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toBuffer:destinationBuffer
                destinationOffset:destinationOffset
                destinationBytesPerRow:destinationBytesPerRow
                destinationBytesPerImage:destinationBytesPerImage
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_3D: {
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, depthForMetal);
        if (!widthForMetal || !heightForMetal || !depthForMetal)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, source.origin.z);
        auto destinationOffset = static_cast<NSUInteger>(destination.layout.offset);
            [m_blitCommandEncoder
                copyFromTexture:sourceTexture.texture()
                sourceSlice:0
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toBuffer:destinationBuffer
                destinationOffset:destinationOffset
                destinationBytesPerRow:destinationBytesPerRow
                destinationBytesPerImage:destinationBytesPerImage
                options:options];
        break;
    }
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return;
    }
}

static bool areCopyCompatible(WGPUTextureFormat format1, WGPUTextureFormat format2)
{
    // https://gpuweb.github.io/gpuweb/#copy-compatible

    if (format1 == format2)
        return true;

    return Texture::removeSRGBSuffix(format1) == Texture::removeSRGBSuffix(format2);
}

static NSString* errorValidatingCopyTextureToTexture(const WGPUImageCopyTexture& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize, const CommandEncoder& commandEncoder)
{
#define ERROR_STRING(x) [NSString stringWithFormat:@"GPUCommandEncoder.copyTextureToTexture: %@", x]
    const auto& sourceTexture = fromAPI(source.texture);
    if (!isValidToUseWith(sourceTexture, commandEncoder))
        return ERROR_STRING(@"source texture is not valid to use with this GPUCommandEncoder");

    const auto& destinationTexture = fromAPI(destination.texture);
    if (!isValidToUseWith(destinationTexture, commandEncoder))
        return ERROR_STRING(@"desintation texture is not valid to use with this GPUCommandEncoder");

    if (NSString* error = Texture::errorValidatingImageCopyTexture(source, copySize))
        return ERROR_STRING(error);

    if (!(sourceTexture.usage() & WGPUTextureUsage_CopySrc))
        return ERROR_STRING(@"source texture usage does not contain CopySrc");

    if (NSString* error = Texture::errorValidatingImageCopyTexture(destination, copySize))
        return ERROR_STRING(error);

    if (!(destinationTexture.usage() & WGPUTextureUsage_CopyDst))
        return ERROR_STRING(@"destination texture usage does not contain CopyDst");

    if (sourceTexture.sampleCount() != destinationTexture.sampleCount())
        return ERROR_STRING(@"destination texture sample count does not equal source texture sample count");

    if (!areCopyCompatible(sourceTexture.format(), destinationTexture.format()))
        return ERROR_STRING(@"destination texture and source texture are not copy compatible");

    bool srcIsDepthOrStencil = Texture::isDepthOrStencilFormat(sourceTexture.format());
    bool dstIsDepthOrStencil = Texture::isDepthOrStencilFormat(destinationTexture.format());

    if (srcIsDepthOrStencil) {
        if (!refersToAllAspects(sourceTexture.format(), source.aspect)
            || !refersToAllAspects(destinationTexture.format(), destination.aspect))
            return ERROR_STRING(@"source or destination do not refer to a single copy aspect");
    } else {
        if (source.aspect != WGPUTextureAspect_All)
            return ERROR_STRING(@"source aspect is not All");
        if (!dstIsDepthOrStencil) {
            if (destination.aspect != WGPUTextureAspect_All)
                return ERROR_STRING(@"destination aspect is not All");
        }
    }

    if (NSString* error = Texture::errorValidatingTextureCopyRange(source, copySize))
        return ERROR_STRING(error);

    if (NSString* error = Texture::errorValidatingTextureCopyRange(destination, copySize))
        return ERROR_STRING(error);

    // https://gpuweb.github.io/gpuweb/#abstract-opdef-set-of-subresources-for-texture-copy
    if (source.texture == destination.texture) {
        // Mip levels are never ranges.
        if (source.mipLevel == destination.mipLevel) {
            switch (fromAPI(source.texture).dimension()) {
            case WGPUTextureDimension_1D:
                return ERROR_STRING(@"can't copy 1D texture to itself");
            case WGPUTextureDimension_2D: {
                Range<uint32_t> sourceRange(source.origin.z, source.origin.z + copySize.depthOrArrayLayers);
                Range<uint32_t> destinationRange(destination.origin.z, destination.origin.z + copySize.depthOrArrayLayers);
                if (sourceRange.overlaps(destinationRange))
                    return ERROR_STRING(@"can't copy 2D texture to itself with overlapping array range");
                break;
            }
            case WGPUTextureDimension_3D:
                return ERROR_STRING(@"can't copy 3D texture to itself");
            case WGPUTextureDimension_Force32:
                ASSERT_NOT_REACHED();
                return ERROR_STRING(@"unknown texture format");
            }
        }
    }

#undef ERROR_STRING
    return nil;
}

void CommandEncoder::copyTextureToTexture(const WGPUImageCopyTexture& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize)
{
    if (source.nextInChain || destination.nextInChain)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copytexturetotexture

    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return;
    }

    if (NSString* error = errorValidatingCopyTextureToTexture(source, destination, copySize, *this)) {
        makeInvalid(error);
        return;
    }

    auto& sourceTexture = fromAPI(source.texture);
    auto& destinationTexture = fromAPI(destination.texture);
    sourceTexture.setCommandEncoder(*this);
    destinationTexture.setCommandEncoder(*this);

    if (sourceTexture.isDestroyed() || destinationTexture.isDestroyed())
        return;

    ensureBlitCommandEncoder();

    auto destinationTextureDimension = destinationTexture.dimension();
    uint32_t sliceCount = destinationTextureDimension == WGPUTextureDimension_3D ? 1 : copySize.depthOrArrayLayers;
    for (uint32_t layer = 0; layer < sliceCount; ++layer) {
        NSUInteger sourceSlice = sourceTexture.dimension() == WGPUTextureDimension_3D ? 0 : (source.origin.z + layer);
        clearTextureIfNeeded(source, sourceSlice);

        NSUInteger destinationSlice = destinationTexture.dimension() == WGPUTextureDimension_3D ? 0 : (destination.origin.z + layer);
        auto logicalSize = destinationTexture.logicalMiplevelSpecificTextureExtent(destination.mipLevel);
        if (Queue::writeWillCompletelyClear(destinationTextureDimension, copySize.width, logicalSize.width, copySize.height, logicalSize.height, copySize.depthOrArrayLayers, logicalSize.depthOrArrayLayers))
            destinationTexture.setPreviouslyCleared(destination.mipLevel, destinationSlice);
        else
            clearTextureIfNeeded(destination, destinationSlice);
    }

    // FIXME(PERFORMANCE): Is it actually faster to use the -[MTLBlitCommandEncoder copyFromTexture:...toTexture:...levelCount:]
    // variant, where possible, rather than calling the other variant in a loop?
    switch (sourceTexture.dimension()) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(copySize.width, 1, 1);
        if (!sourceSize.width)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, 0, 0);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, 0, 0);

        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            NSUInteger sourceSlice = source.origin.z + layer;
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromTexture:fromAPI(source.texture).texture()
                sourceSlice:sourceSlice
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toTexture:destinationTexture.texture()
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin];
        }
        break;
    }
    case WGPUTextureDimension_2D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
        // "When you copy to a 2D texture, depth must be 1."
        auto sourceSize = MTLSizeMake(copySize.width, copySize.height, 1);
        if (!sourceSize.width || !sourceSize.height)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, 0);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, 0);

        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            NSUInteger sourceSlice = source.origin.z + layer;
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromTexture:fromAPI(source.texture).texture()
                sourceSlice:sourceSlice
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toTexture:destinationTexture.texture()
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin];
        }
        break;
    }
    case WGPUTextureDimension_3D: {
        auto sourceSize = MTLSizeMake(copySize.width, copySize.height, copySize.depthOrArrayLayers);
        if (!sourceSize.width || !sourceSize.height || !sourceSize.depth)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, source.origin.z);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, destination.origin.z);

        [m_blitCommandEncoder
            copyFromTexture:fromAPI(source.texture).texture()
            sourceSlice:0
            sourceLevel:source.mipLevel
            sourceOrigin:sourceOrigin
            sourceSize:sourceSize
            toTexture:destinationTexture.texture()
            destinationSlice:0
            destinationLevel:destination.mipLevel
            destinationOrigin:destinationOrigin];
        break;
    }
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return;
    }
}

bool CommandEncoder::validateClearBuffer(const Buffer& buffer, uint64_t offset, uint64_t size)
{
    if (!isValidToUseWith(buffer, *this))
        return false;

    if (!(buffer.usage() & WGPUBufferUsage_CopyDst))
        return false;

    if (size % 4)
        return false;

    if (offset % 4)
        return false;

    auto end = checkedSum<uint64_t>(offset, size);
    if (end.hasOverflowed() || buffer.initialSize() < end.value())
        return false;

    return true;
}

void CommandEncoder::clearBuffer(const Buffer& buffer, uint64_t offset, uint64_t size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-clearbuffer

    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return;
    }

    if (size == WGPU_WHOLE_SIZE) {
        auto localSize = checkedDifference<uint64_t>(buffer.initialSize(), offset);
        if (localSize.hasOverflowed()) {
            m_device->generateAValidationError("CommandEncoder::clearBuffer(): offset > buffer.size"_s);
            return;
        }
        size = localSize.value();
    }

    if (!validateClearBuffer(buffer, offset, size)) {
        makeInvalid(@"GPUCommandEncoder.clearBuffer validation failed");
        return;
    }

    buffer.setCommandEncoder(*this);
    if (buffer.isDestroyed() || !size)
        return;

    ensureBlitCommandEncoder();

    [m_blitCommandEncoder fillBuffer:buffer.buffer() range:NSMakeRange(static_cast<NSUInteger>(offset), static_cast<NSUInteger>(size)) value:0];
}

void CommandEncoder::setLastError(NSString* errorString)
{
    m_lastErrorString = errorString;
}

NSString* CommandEncoder::validateFinishError() const
{
    if (!isValid())
        return @"GPUCommandEncoder.finish: encoder is not valid";

    if (m_state != EncoderState::Open)
        return [NSString stringWithFormat:@"GPUCommandEncoder.finish: encoder state is '%@', expeted 'Open'", encoderStateName()];

    if (m_debugGroupStackSize)
        return [NSString stringWithFormat:@"GPUCommandEncoder.finish: encoder stack size '%llu'", m_debugGroupStackSize];

    // FIXME: "Every usage scope contained in this must satisfy the usage scope validation."

    return nil;
}

Ref<CommandBuffer> CommandEncoder::finish(const WGPUCommandBufferDescriptor& descriptor)
{
    if (descriptor.nextInChain || !isValid() || (m_existingCommandEncoder && m_existingCommandEncoder != m_blitCommandEncoder)) {
        m_state = EncoderState::Ended;
        m_device->generateAValidationError(m_lastErrorString ?: @"Invalid CommandEncoder.");
        return CommandBuffer::createInvalid(m_device);
    }

    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-finish

    auto validationFailedError = validateFinishError();

    auto priorState = m_state;
    m_state = EncoderState::Ended;
    UNUSED_PARAM(priorState);
    if (validationFailedError) {
        m_device->generateAValidationError(m_lastErrorString ?: validationFailedError);
        return CommandBuffer::createInvalid(m_device);
    }

    finalizeBlitCommandEncoder();

    auto *commandBuffer = m_commandBuffer;
    m_commandBuffer = nil;

    commandBuffer.label = fromAPI(descriptor.label);

    auto result = CommandBuffer::create(commandBuffer, m_device);
    m_cachedCommandBuffer = result;
    m_cachedCommandBuffer->setBufferMapCount(m_bufferMapCount);
    if (m_makeSubmitInvalid)
        m_cachedCommandBuffer->makeInvalid(m_lastErrorString);

    return result;
}

void CommandEncoder::insertDebugMarker(String&& markerLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return;
    }

    finalizeBlitCommandEncoder();

    // There's no direct way of doing this, so we just push/pop an empty debug group.
    [m_commandBuffer pushDebugGroup:markerLabel];
    [m_commandBuffer popDebugGroup];
}

bool CommandEncoder::validatePopDebugGroup() const
{
    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void CommandEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return;
    }

    if (!validatePopDebugGroup()) {
        makeInvalid(@"validatePopDebugGroup failed");
        return;
    }

    finalizeBlitCommandEncoder();

    --m_debugGroupStackSize;
    [m_commandBuffer popDebugGroup];
}

void CommandEncoder::pushDebugGroup(String&& groupLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return;
    }
    
    finalizeBlitCommandEncoder();

    ++m_debugGroupStackSize;
    [m_commandBuffer pushDebugGroup:groupLabel];
}

static bool validateResolveQuerySet(const QuerySet& querySet, uint32_t firstQuery, uint32_t queryCount, const Buffer& destination, uint64_t destinationOffset)
{
    if (!querySet.isDestroyed() && !querySet.isValid())
        return false;
    if (!destination.isDestroyed() && !destination.isValid())
        return false;
    if (!(destination.usage() & WGPUBufferUsage_QueryResolve))
        return false;

    if (firstQuery >= querySet.count())
        return false;

    if (firstQuery + queryCount > querySet.count())
        return false;

    if (destinationOffset % 256)
        return false;

    if (destinationOffset + 8 * queryCount > destination.initialSize())
        return false;

    return true;
}

void CommandEncoder::resolveQuerySet(const QuerySet& querySet, uint32_t firstQuery, uint32_t queryCount, const Buffer& destination, uint64_t destinationOffset)
{
    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return;
    }

    if (!validateResolveQuerySet(querySet, firstQuery, queryCount, destination, destinationOffset) || !isValidToUseWith(querySet, *this) || !isValidToUseWith(destination, *this)) {
        makeInvalid(@"GPUCommandEncoder.resolveQuerySet validation failed");
        return;
    }

    querySet.setCommandEncoder(*this);
    destination.setCommandEncoder(*this);

    if (querySet.isDestroyed() || destination.isDestroyed())
        return;

    ensureBlitCommandEncoder();
    switch (querySet.type()) {
    case WGPUQueryType_Occlusion: {
        [m_blitCommandEncoder copyFromBuffer:querySet.visibilityBuffer() sourceOffset:sizeof(uint64_t) * firstQuery toBuffer:destination.buffer() destinationOffset:destinationOffset size:sizeof(uint64_t) * queryCount];
        break;
    }
    case WGPUQueryType_Timestamp: {
        querySet.encodeResolveCommands(m_blitCommandEncoder, firstQuery, queryCount, destination, destinationOffset);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void CommandEncoder::writeTimestamp(QuerySet& querySet, uint32_t queryIndex)
{
    if (!prepareTheEncoderState()) {
        GENERATE_INVALID_ENCODER_STATE_ERROR();
        return;
    }

    if (!m_device->hasFeature(WGPUFeatureName_TimestampQuery))
        return;

    if (querySet.type() != WGPUQueryType_Timestamp || queryIndex >= querySet.count() || !isValidToUseWith(querySet, *this)) {
        makeInvalid(@"GPUCommandEncoder.writeTimestamp validation failed");
        return;
    }

    querySet.setCommandEncoder(*this);
    switch (m_device->baseCapabilities().counterSamplingAPI) {
    case HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary:
        m_pendingTimestampWrites.append({ querySet, queryIndex });
        break;
    case HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::CommandBoundary:
        ensureBlitCommandEncoder();
        [m_blitCommandEncoder sampleCountersInBuffer:querySet.counterSampleBuffer() atSampleIndex:queryIndex withBarrier:NO];
        break;
    }
}

void CommandEncoder::setLabel(String&& label)
{
    m_commandBuffer.label = label;
}

void CommandEncoder::lock(bool shouldLock)
{
    if (m_state == EncoderState::Ended)
        return;

    m_state = shouldLock ? EncoderState::Locked : EncoderState::Open;
    if (!shouldLock)
        setExistingEncoder(nil);
}

bool CommandEncoder::isLocked() const
{
    return m_state == EncoderState::Locked;
}

bool CommandEncoder::isFinished() const
{
    return m_state == EncoderState::Ended;
}

#undef GENERATE_INVALID_ENCODER_STATE_ERROR

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuCommandEncoderReference(WGPUCommandEncoder commandEncoder)
{
    WebGPU::fromAPI(commandEncoder).ref();
}

void wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder)
{
    WebGPU::fromAPI(commandEncoder).deref();
}

WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder commandEncoder, const WGPUComputePassDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(commandEncoder).beginComputePass(*descriptor));
}

WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder, const WGPURenderPassDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(commandEncoder).beginRenderPass(*descriptor));
}

void wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size)
{
    WebGPU::fromAPI(commandEncoder).copyBufferToBuffer(WebGPU::fromAPI(source), sourceOffset, WebGPU::fromAPI(destination), destinationOffset, size);
}

void wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder commandEncoder, const WGPUImageCopyBuffer* source, const WGPUImageCopyTexture* destination, const WGPUExtent3D* copySize)
{
    WebGPU::fromAPI(commandEncoder).copyBufferToTexture(*source, *destination, *copySize);
}

void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder commandEncoder, const WGPUImageCopyTexture* source, const WGPUImageCopyBuffer* destination, const WGPUExtent3D* copySize)
{
    WebGPU::fromAPI(commandEncoder).copyTextureToBuffer(*source, *destination, *copySize);
}

void wgpuCommandEncoderCopyTextureToTexture(WGPUCommandEncoder commandEncoder, const WGPUImageCopyTexture* source, const WGPUImageCopyTexture* destination, const WGPUExtent3D* copySize)
{
    WebGPU::fromAPI(commandEncoder).copyTextureToTexture(*source, *destination, *copySize);
}

void wgpuCommandEncoderClearBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(commandEncoder).clearBuffer(WebGPU::fromAPI(buffer), offset, size);
}

WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder, const WGPUCommandBufferDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(commandEncoder).finish(*descriptor));
}

void wgpuCommandEncoderInsertDebugMarker(WGPUCommandEncoder commandEncoder, const char* markerLabel)
{
    WebGPU::fromAPI(commandEncoder).insertDebugMarker(WebGPU::fromAPI(markerLabel));
}

void wgpuCommandEncoderPopDebugGroup(WGPUCommandEncoder commandEncoder)
{
    WebGPU::fromAPI(commandEncoder).popDebugGroup();
}

void wgpuCommandEncoderPushDebugGroup(WGPUCommandEncoder commandEncoder, const char* groupLabel)
{
    WebGPU::fromAPI(commandEncoder).pushDebugGroup(WebGPU::fromAPI(groupLabel));
}

void wgpuCommandEncoderResolveQuerySet(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset)
{
    WebGPU::fromAPI(commandEncoder).resolveQuerySet(WebGPU::fromAPI(querySet), firstQuery, queryCount, WebGPU::fromAPI(destination), destinationOffset);
}

void wgpuCommandEncoderWriteTimestamp(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t queryIndex)
{
    WebGPU::fromAPI(commandEncoder).writeTimestamp(WebGPU::fromAPI(querySet), queryIndex);
}

void wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, const char* label)
{
    WebGPU::fromAPI(commandEncoder).setLabel(WebGPU::fromAPI(label));
}
