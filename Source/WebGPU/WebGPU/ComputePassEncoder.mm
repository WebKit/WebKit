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
#import "ComputePassEncoder.h"

#import "APIConversions.h"
#import "BindGroup.h"
#import "BindableResource.h"
#import "Buffer.h"
#import "CommandEncoder.h"
#import "ComputePipeline.h"
#import "Instance.h"
#import "IsValidToUseWith.h"
#import "Pipeline.h"
#import "QuerySet.h"
#import <wtf/Borrow.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/Variant.h>

namespace WebGPU::Metal {

struct ComputePassEncoder::DeferredState {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(DeferredState);
    struct UseResources {
        Vector<RetainPtr<id<MTLResource>>> retainedResources;
        Vector<id<MTLResource>> rawResources;
        MTLResourceUsage usage;
    };
    struct SetPipeline {
        RetainPtr<id<MTLComputePipelineState>> readyPSO;
        std::shared_ptr<PendingComputePipelineCompilation> pending;
    };
    struct SetBuffer {
        RetainPtr<id<MTLBuffer>> buffer;
        NSUInteger offset;
        uint32_t index;
    };
    struct SetBytes {
        Vector<uint8_t, 64> bytes;
        uint32_t index;
    };
    struct Dispatch {
        MTLSize threadgroups;
        MTLSize threadsPerThreadgroup;
    };
    using Command = Variant<UseResources, SetPipeline, SetBuffer, SetBytes, Dispatch>;
    Vector<Command, 16> commands;

    void appendBindGroupResources(const BindGroup& group)
    {
        for (const auto& resource : group.resources()) {
            if (resource.renderStages == BindGroup::MTLRenderStageCompute && resource.mtlResources.size()) {
                UseResources cmd;
                cmd.usage = resource.usage;
                cmd.retainedResources.reserveInitialCapacity(resource.mtlResources.size());
                cmd.rawResources.reserveInitialCapacity(resource.mtlResources.size());
                for (id<MTLResource> res : resource.mtlResources) {
                    cmd.retainedResources.append(res);
                    cmd.rawResources.append(res);
                }
                commands.append(WTF::move(cmd));
            }
        }
    }
};

#define RETURN_IF_FINISHED() \
if (!m_parentEncoder->isLocked() || m_parentEncoder->isFinished()) { \
    protect(m_device)->generateAValidationError([NSString stringWithFormat:@"%s: failed as encoding has finished", __PRETTY_FUNCTION__]); \
    m_computeCommandEncoder = nil; \
    return; \
} \
if (!m_computeCommandEncoder || !m_parentEncoder->isValid() || !protect(m_parentEncoder)->encoderIsCurrent(m_computeCommandEncoder)) { \
    m_computeCommandEncoder = nil; \
    return; \
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ComputePassEncoder);

ComputePassEncoder::ComputePassEncoder(id<MTLComputeCommandEncoder> computeCommandEncoder, const WebGPU::ComputePassDescriptor& descriptor, CommandEncoder& parentEncoder, Device& device)
    : m_computeCommandEncoder(computeCommandEncoder)
    , m_device(device)
    , m_parentEncoder(parentEncoder)
#if PLATFORM(MAC)
    , m_canDeferEncoding(!descriptor.timestampWrites.has_value() && !m_device->enableEncoderTimestamps())
#endif
{
#if !PLATFORM(MAC)
    UNUSED_PARAM(descriptor);
#endif
    protect(m_parentEncoder)->lock(true);
    RELEASE_ASSERT(m_maxDynamicOffsetAtIndex.size() >= m_device->limits().maxBindGroups);
}

ComputePassEncoder::ComputePassEncoder(CommandEncoder& parentEncoder, Device& device, NSString* errorString)
    : m_device(device)
    , m_parentEncoder(parentEncoder)
    , m_lastErrorString(errorString)
{
    protect(m_parentEncoder)->lock(true);
    m_parentEncoder->setLastError(errorString);
    RELEASE_ASSERT(m_maxDynamicOffsetAtIndex.size() >= m_device->limits().maxBindGroups);
}

ComputePassEncoder::~ComputePassEncoder()
{
    if (m_computeCommandEncoder)
        protect(m_parentEncoder)->makeInvalid(@"GPUComputePassEncoder.finish was never called");
    m_computeCommandEncoder = nil;
}

using EntryUsage = OptionSet<BindGroupEntryUsage>;
struct EntryUsageData {
    EntryUsage usage;
    uint32_t bindGroup;
};
using EntryMap = HashMap<uint64_t, EntryUsageData>;
using TextureEntryMapContainer = HashMap<const void*, EntryMap>;
using EntryMapContainer = HashMap<const void*, EntryUsage>;
struct BindGroupId {
    uint32_t bindGroup { 0 };
};
static bool addTextureToActiveResources(const void* resourceAddress, id<MTLResource> mtlResource, OptionSet<BindGroupEntryUsage> initialUsage, TextureEntryMapContainer& usagesForResource, BindGroupId bindGroup, uint32_t baseMipLevel, uint32_t baseArrayLayer, WebGPU::TextureAspect aspect)
{
    UNUSED_PARAM(mtlResource);

    auto mapKey = BindGroup::makeEntryMapKey(baseMipLevel, baseArrayLayer, aspect);
    EntryUsage resourceUsage = initialUsage;
    EntryMap* entryMap = nullptr;
    if (auto it = usagesForResource.find(resourceAddress); it != usagesForResource.end()) {
        entryMap = &it->value;
        if (auto innerIt = it->value.find(mapKey); innerIt != it->value.end()) {
            auto existingUsage = innerIt->value.usage;
            resourceUsage.add(existingUsage);
            if (innerIt->value.bindGroup != bindGroup.bindGroup) {
                if ((initialUsage == BindGroupEntryUsage::StorageTextureWriteOnly && existingUsage == BindGroupEntryUsage::StorageTextureWriteOnly) || (initialUsage == BindGroupEntryUsage::StorageTextureReadWrite && existingUsage == BindGroupEntryUsage::StorageTextureReadWrite))
                    return false;
            }
        } else
            entryMap->set(mapKey, EntryUsageData { .usage = resourceUsage, .bindGroup = bindGroup.bindGroup });
    }

    if (!BindGroup::allowedUsage(resourceUsage))
        return false;

    if (!entryMap) {
        EntryMap entryMap;
        entryMap.set(mapKey, EntryUsageData { .usage = resourceUsage, .bindGroup = bindGroup.bindGroup });
        usagesForResource.set(resourceAddress, entryMap);
    }

    return true;
}
static bool addResourceToActiveResources(const void* resourceAddress, OptionSet<BindGroupEntryUsage> initialUsage, EntryMapContainer& usagesForResource)
{
    EntryUsage resourceUsage = initialUsage;
    if (auto it = usagesForResource.find(resourceAddress); it != usagesForResource.end())
        resourceUsage.add(it->value);

    if (!BindGroup::allowedUsage(resourceUsage))
        return false;

    usagesForResource.set(resourceAddress, resourceUsage);

    return true;
}

static bool addResourceToActiveResources(const TextureView& texture, OptionSet<BindGroupEntryUsage> resourceUsage, BindGroupId bindGroup, auto& usagesForResource)
{
    WebGPU::TextureAspect textureAspect = texture.aspect();
    if (textureAspect != WebGPU::TextureAspect::All)
        return addTextureToActiveResources(&texture.apiParentTexture(), texture.parentTexture(), resourceUsage, usagesForResource, bindGroup, texture.baseMipLevel(), texture.baseArrayLayer(), textureAspect);

    return addTextureToActiveResources(&texture.apiParentTexture(), texture.parentTexture(), resourceUsage, usagesForResource, bindGroup, texture.baseMipLevel(), texture.baseArrayLayer(), WebGPU::TextureAspect::DepthOnly) && addTextureToActiveResources(&texture.apiParentTexture(), texture.parentTexture(), resourceUsage, usagesForResource, bindGroup, texture.baseMipLevel(), texture.baseArrayLayer(), WebGPU::TextureAspect::StencilOnly);
}

static bool addResourceToActiveResources(const Texture& texture, OptionSet<BindGroupEntryUsage> resourceUsage, BindGroupId bindGroup, auto& usagesForResource)
{
    constexpr uint32_t baseMipLevel = 0;
    constexpr uint32_t baseArrayLayer = 0;
    return addTextureToActiveResources(&texture, texture.texture(), resourceUsage, usagesForResource, bindGroup, baseMipLevel, baseArrayLayer, WebGPU::TextureAspect::DepthOnly) && addTextureToActiveResources(&texture, texture.texture(), resourceUsage, usagesForResource, bindGroup, baseMipLevel, baseArrayLayer, WebGPU::TextureAspect::StencilOnly);
}

static bool addResourceToActiveResources(const BindGroupEntryUsageData::Resource& resource, OptionSet<BindGroupEntryUsage> resourceUsage, BindGroupId bindGroup, EntryMapContainer& usagesForResource, TextureEntryMapContainer& textureUsagesForResource, CommandEncoder& parentEncoder)
{
    return WTF::switchOn(resource, [&](const RefPtr<Buffer>& buffer) {
        if (buffer.get()) {
            if (resourceUsage.contains(BindGroupEntryUsage::Storage))
                buffer->indirectBufferInvalidated(parentEncoder);
            return addResourceToActiveResources(buffer.get(), resourceUsage, usagesForResource);
        }
        return true;
    }, [&](const RefPtr<const Texture>& texture) {
        if (texture.get())
            return addResourceToActiveResources(*texture.get(), resourceUsage, bindGroup, textureUsagesForResource);
        return true;
    }, [&](const RefPtr<const TextureView>& textureView) {
        if (textureView.get())
            return addResourceToActiveResources(*textureView.get(), resourceUsage, bindGroup, textureUsagesForResource);
        return true;
    }, [&](const RefPtr<const ExternalTexture>& externalTexture) {
        return addResourceToActiveResources(externalTexture.get(), resourceUsage, usagesForResource);
    });
}

void ComputePassEncoder::executePreDispatchCommands(const Buffer* indirectBuffer)
{
    auto pipeline = m_pipeline;
    if (!pipeline) {
        makeInvalid(@"pipeline is not set prior to dispatch");
        return;
    }

    if (!pipeline->isValid()) {
        makeInvalid(@"pipeline is not valid prior to dispatch");
        return;
    }

    if (NSString *error = protect(pipeline->pipelineLayout())->errorValidatingBindGroupCompatibility(m_bindGroups)) {
        makeInvalid(error);
        return;
    }

    if (!m_isDeferringEncoding && m_canDeferEncoding && !m_hasEncodedToMetalEncoder && !indirectBuffer && !m_debugGroupStackSize && !pipeline->isPipelineStateReady() && m_device->getQueue()->canDeferPass()) {
        m_isDeferringEncoding = true;
        m_deferredState = makeUnique<DeferredState>();
        for (auto& kvp : m_bindGroups)
            m_deferredState->appendBindGroupResources(kvp.value);
    }

    if (m_isDeferringEncoding) {
        if (!m_parentEncoder->submitWillBeInvalid())
            m_deferredState->commands.append(DeferredState::SetPipeline { pipeline->precompiledPipelineState(), pipeline->pendingCompilation() });
    } else {
        id<MTLComputePipelineState> computePipelineState = pipeline->computePipelineState();
        if (!computePipelineState) {
            makeInvalid(@"pipeline is not valid prior to dispatch");
            return;
        }
        [computeCommandEncoder() setComputePipelineState:computePipelineState];
        m_hasEncodedToMetalEncoder = true;
    }

    EntryMapContainer usagesForResource;
    TextureEntryMapContainer textureUsagesForResource;
    if (indirectBuffer)
        addResourceToActiveResources(indirectBuffer, BindGroupEntryUsage::Input, usagesForResource);

    Ref pipelineLayout = pipeline->pipelineLayout();
    auto pipelineLayoutCount = pipelineLayout->numberOfBindGroupLayouts();
    auto pipelineIdentifier = pipeline->uniqueId();
    for (auto& kvp : m_bindGroups) {
        auto bindGroupIndex = kvp.key;
        Ref group = kvp.value;
        if (group->hasSamplers())
            protect(m_parentEncoder)->rebindSamplersPreCommit(group);

        if (!group->previouslyValidatedBindGroup(bindGroupIndex, pipelineIdentifier, m_maxDynamicOffsetAtIndex[bindGroupIndex])) {
            if (group->makeSubmitInvalid(ShaderStage::Compute, protect(pipelineLayout->optionalBindGroupLayout(bindGroupIndex)).get())) {
                protect(m_parentEncoder)->makeSubmitInvalid();
                return;
            }

            const Vector<uint32_t>* dynamicOffsets = nullptr;
            if (auto it = m_bindGroupDynamicOffsets.find(bindGroupIndex); it != m_bindGroupDynamicOffsets.end())
                dynamicOffsets = &it->value;
            if (NSString* error = errorValidatingBindGroup(group, pipeline->minimumBufferSizes(bindGroupIndex), dynamicOffsets)) {
                makeInvalid(error);
                return;
            }
            group->validatedSuccessfully(bindGroupIndex, pipelineIdentifier, m_maxDynamicOffsetAtIndex[bindGroupIndex]);
        }
        if (m_isDeferringEncoding) {
            if (!m_parentEncoder->submitWillBeInvalid())
                m_deferredState->commands.append(DeferredState::SetBuffer { group->computeArgumentBuffer(), group->computeArgumentBufferOffset(), bindGroupIndex });
        } else
            [computeCommandEncoder() setBuffer:group->computeArgumentBuffer() offset:group->computeArgumentBufferOffset() atIndex:bindGroupIndex];
    }

    for (auto& kvp : m_bindGroupResources) {
        auto bindGroupIndex = kvp.key;
        if (bindGroupIndex >= pipelineLayoutCount)
            continue;

        Ref bindGroupLayout = pipelineLayout->bindGroupLayout(bindGroupIndex);
        for (auto* bindGroupResources : borrow(kvp.value)) {
            for (size_t i = 0, sz = bindGroupResources->mtlResources.size(); i < sz; ++i) {
                auto& usageData = bindGroupResources->resourceUsages[i];
                constexpr ShaderStage shaderStages[] = { ShaderStage::Vertex, ShaderStage::Fragment, ShaderStage::Compute, ShaderStage::Undefined };
                std::optional<BindGroupLayout::StageMapValue> bindingAccess;
                for (auto shaderStage : shaderStages) {
                    bindingAccess = bindGroupLayout->bindingAccessForBindingIndex(usageData.binding, shaderStage);
                    if (bindingAccess)
                        break;
                }
                if (!bindingAccess)
                    continue;

                if (!addResourceToActiveResources(usageData.resource, usageData.usage, BindGroupId { bindGroupIndex }, usagesForResource, textureUsagesForResource, protect(m_parentEncoder))) {
                    makeInvalid(@"GPUComputePassEncoder.executePreDispatchCommands - could not track resource");
                    return;
                }
            }
        }
    }

    m_bindGroupResources.clear();

    auto requiredImmediateSlots = pipeline->requiredImmediateSlots();
    if ((m_setImmediateSlots & requiredImmediateSlots) != requiredImmediateSlots) {
        makeInvalid(@"Missing required immediate data slots prior to dispatch");
        return;
    }

    if (pipeline->usesImmediates() && m_immediatesDirty) {
        if (m_isDeferringEncoding) {
            if (!m_parentEncoder->submitWillBeInvalid()) {
                Vector<uint8_t, 64> bytes;
                bytes.append(std::span<const uint8_t> { m_immediateData });
                m_deferredState->commands.append(DeferredState::SetBytes { WTF::move(bytes), m_device->maxBuffersForComputeStage() + 1 });
            }
        } else
            [computeCommandEncoder() setBytes:m_immediateData.data() length:m_immediateData.size() atIndex:m_device->maxBuffersForComputeStage() + 1];
        m_immediatesDirty = false;
    }

    if (!m_computeDynamicOffsets.size())
        return;

    for (auto& kvp : m_bindGroupDynamicOffsets) {
        Ref pipelineLayout = pipeline->pipelineLayout();
        auto bindGroupIndex = kvp.key;
        if (!pipelineLayout->updateComputeOffsets(bindGroupIndex, kvp.value, borrow(m_computeDynamicOffsets)->mutableSpan())) {
            makeInvalid(@"Invalid offset calculation");
            return;
        }
    }

    if (m_computeDynamicOffsets != m_priorComputeDynamicOffsets) {
        if (m_isDeferringEncoding) {
            if (!m_parentEncoder->submitWillBeInvalid()) {
                Vector<uint8_t, 64> bytes;
                bytes.append(asBytes(borrow(m_computeDynamicOffsets)->span()));
                m_deferredState->commands.append(DeferredState::SetBytes { WTF::move(bytes), m_device->maxBuffersForComputeStage() });
            }
        } else
            [computeCommandEncoder() setBytes:&borrow(m_computeDynamicOffsets)[0] length:m_computeDynamicOffsets.size() * sizeof(m_computeDynamicOffsets[0]) atIndex:m_device->maxBuffersForComputeStage()];
        m_priorComputeDynamicOffsets = m_computeDynamicOffsets;
    }
}

void ComputePassEncoder::flushDeferredStateToMetalEncoder()
{
    if (!m_isDeferringEncoding)
        return;

    auto deferredState = std::exchange(m_deferredState, nullptr);
    m_isDeferringEncoding = false;
    m_hasEncodedToMetalEncoder = true;
    if (!deferredState)
        return;

    for (auto& command : deferredState->commands) {
        bool failed = false;
        WTF::switchOn(command,
            [&](DeferredState::UseResources& cmd) {
                if (cmd.rawResources.size())
                    [computeCommandEncoder() useResources:borrow(cmd.rawResources)->span().data() count:cmd.rawResources.size() usage:cmd.usage];
            },
            [&](DeferredState::SetPipeline& cmd) {
                id<MTLComputePipelineState> pso = cmd.readyPSO ? cmd.readyPSO.get() : (cmd.pending ? cmd.pending->wait() : nil);
                if (!pso) {
                    if (cmd.pending && cmd.pending->hadLibraryError)
                        m_device->generateAValidationError(cmd.pending->libraryErrorMessage.get() ?: @"Compute library failed creation");
                    else
                        m_device->generateAnOutOfMemoryError("Compute pipeline failed compilation likely due to being too complex, please reduce its size"_s);
                    makeInvalid(@"pipeline is not valid prior to dispatch");
                    failed = true;
                    return;
                }
                [computeCommandEncoder() setComputePipelineState:pso];
            },
            [&](DeferredState::SetBuffer& cmd) {
                [computeCommandEncoder() setBuffer:cmd.buffer.get() offset:cmd.offset atIndex:cmd.index];
            },
            [&](DeferredState::SetBytes& cmd) {
                [computeCommandEncoder() setBytes:borrow(cmd.bytes)->span().data() length:cmd.bytes.size() atIndex:cmd.index];
            },
            [&](DeferredState::Dispatch& cmd) {
                [computeCommandEncoder() dispatchThreadgroups:cmd.threadgroups threadsPerThreadgroup:cmd.threadsPerThreadgroup];
            });
        if (failed)
            return;
    }
}

void ComputePassEncoder::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    RETURN_IF_FINISHED();
    executePreDispatchCommands();
    auto dimensionMax = m_device->limits().maxComputeWorkgroupsPerDimension;
    auto maxLinearWorkgroups = m_pipeline ? m_pipeline->maxLinearIndexingWorkgroups() : std::numeric_limits<uint64_t>::max();
    auto totalWorkgroups = checkedProduct<uint64_t>(x, y, z);
    if (x > dimensionMax || y > dimensionMax || z > dimensionMax || totalWorkgroups.hasOverflowed() || totalWorkgroups.value() > maxLinearWorkgroups) {
        makeInvalid([NSString stringWithFormat:@"x(%u) > dimensionMax(%u) || y(%u) > dimensionMax(%u) || z(%u) > dimensionMax(%u)", x, dimensionMax, y, dimensionMax, z, dimensionMax]);
        return;
    }

    if (!(x * y * z))
        return;

    if (m_isDeferringEncoding) {
        if (m_deferredState && !m_parentEncoder->submitWillBeInvalid())
            m_deferredState->commands.append(DeferredState::Dispatch { MTLSizeMake(x, y, z), m_threadsPerThreadgroup });
        return;
    }

    [computeCommandEncoder() dispatchThreadgroups:MTLSizeMake(x, y, z) threadsPerThreadgroup:m_threadsPerThreadgroup];
}

id<MTLBuffer> ComputePassEncoder::runPredispatchIndirectCallValidation(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    static id<MTLFunction> function = [this] {
        id<MTLDevice> mtlDevice = m_device->device();
        auto dimensionMax = m_device->limits().maxComputeWorkgroupsPerDimension;
        MTLCompileOptions* options = [MTLCompileOptions new];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        options.fastMathEnabled = YES;
        ALLOW_DEPRECATED_DECLARATIONS_END
        NSError *error = nil;
        id<MTLLibrary> library = [mtlDevice newLibraryWithSource:[NSString stringWithFormat:@"[[kernel]] void csDispatchClamp(device const uint* indirectBuffer [[buffer(0)]], device uint* dispatchCallBuffer [[buffer(1)]], constant ulong& maxLinearWorkgroups [[buffer(2)]], uint index [[thread_position_in_grid]]) { ulong totalWorkgroups = ulong(indirectBuffer[0]) * ulong(indirectBuffer[1]) * ulong(indirectBuffer[2]); dispatchCallBuffer[index] = metal::select(indirectBuffer[index], 0u, (indirectBuffer[index] > %u) || (totalWorkgroups > maxLinearWorkgroups)); }", dimensionMax] options:options error:&error];
        if (error)
            WTFLogAlways("%@", error); // NOLINT

        return [library newFunctionWithName:@"csDispatchClamp"];
    }();
    RELEASE_ASSERT(function);

    auto device = m_device;
    id<MTLComputePipelineState> computePipelineState = device->dispatchCallPipelineState(function);
    id<MTLBuffer> dispatchCallBuffer = device->dispatchCallBuffer();
    uint64_t maxLinearWorkgroups = m_pipeline ? m_pipeline->maxLinearIndexingWorkgroups() : std::numeric_limits<uint64_t>::max();
    [computeCommandEncoder() setComputePipelineState:computePipelineState];
    [computeCommandEncoder() setBuffer:indirectBuffer.buffer() offset:indirectOffset atIndex:0];
    [computeCommandEncoder() setBuffer:dispatchCallBuffer offset:0 atIndex:1];
    [computeCommandEncoder() setBytes:&maxLinearWorkgroups length:sizeof(maxLinearWorkgroups) atIndex:2];
    [computeCommandEncoder() dispatchThreads:MTLSizeMake(3, 1, 1) threadsPerThreadgroup:MTLSizeMake(3, 1, 1)];
    return dispatchCallBuffer;
}

void ComputePassEncoder::dispatchIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    RETURN_IF_FINISHED();
    flushDeferredStateToMetalEncoder();
    if (!m_computeCommandEncoder)
        return;
    if (!isValidToUseWith(indirectBuffer, *this)) {
        makeInvalid(@"GPUComputePassEncoder.dispatchIndirect: indirectBuffer is not valid to use with this GPUComputePassEncoder");
        return;
    }

    auto indirectOffsetSum = checkedSum<uint64_t>(indirectOffset, 3 * sizeof(uint32_t));
    if ((indirectOffset % 4) || !indirectBuffer.usage().contains(WebGPU::BufferUsage::Indirect) || indirectOffsetSum.hasOverflowed() || (indirectOffsetSum.value() > indirectBuffer.initialSize())) {
        makeInvalid([NSString stringWithFormat:@"GPUComputePassEncoder.dispatchIndirect: (indirectOffset(%llu) mod 4) || !(indirectBuffer.usage(%llu) & BufferUsage::Indirect(%llu)) || indirectOffsetSum.hasOverflowed(%d) || (indirectOffsetSum(%llu) > indirectBuffer.initialSize(%llu))", indirectOffset, static_cast<unsigned long long>(indirectBuffer.usage().toRaw()), static_cast<unsigned long long>(WebGPU::BufferUsage::Indirect), indirectOffsetSum.hasOverflowed(), indirectOffsetSum.hasOverflowed() ? 0 : indirectOffsetSum.value(), indirectBuffer.initialSize()]);
        return;
    }

    indirectBuffer.setCommandEncoder(m_parentEncoder);
    if (indirectBuffer.isDestroyed())
        return;

    if (id<MTLBuffer> dispatchBuffer = runPredispatchIndirectCallValidation(indirectBuffer, indirectOffset)) {
        executePreDispatchCommands(&indirectBuffer);
        [computeCommandEncoder() dispatchThreadgroupsWithIndirectBuffer:dispatchBuffer indirectBufferOffset:0 threadsPerThreadgroup:m_threadsPerThreadgroup];
    } else
        makeInvalid(@"GPUComputePassEncoder.dispatchWorkgroupsIndirect: Unable to validate dispatch size");
}

void ComputePassEncoder::endPass()
{
    if (m_passEnded) {
        protect(m_device)->generateAValidationError([NSString stringWithFormat:@"%s: failed as pass is already ended", __PRETTY_FUNCTION__]);
        return;
    }
    m_passEnded = true;

    // https://gpuweb.github.io/gpuweb/#dom-gpucomputepassencoder-end
    // A pass begun while the command encoder was already locked by another pass never took the
    // encoder over. Ending it is a validation error the page can catch, not a silent no-op.
    if (m_encoderStateWasNotOpen) {
        protect(m_device)->generateAValidationError([NSString stringWithFormat:@"%s: failed as the command encoder was not open when the pass began", __PRETTY_FUNCTION__]);
        return;
    }

    auto parentEncoder = m_parentEncoder;

    // Spelled out rather than RETURN_IF_FINISHED() because ending a pass hands the command encoder
    // back and only then looks at whether the pass ended in a good state, so every path from here
    // unlocks the encoder. A pass that ended badly invalidates its encoder instead of leaving it
    // locked: a later pass can still be begun on an invalid encoder.
    if (!parentEncoder->isLocked() || parentEncoder->isFinished()) {
        protect(m_device)->generateAValidationError([NSString stringWithFormat:@"%s: failed as encoding has finished", __PRETTY_FUNCTION__]);
        m_computeCommandEncoder = nil;
        return;
    }

    auto passIsValid = isValid();
    if (m_debugGroupStackSize || !passIsValid) {
        // An already-invalidated pass has handed its command encoder back already.
        if (m_computeCommandEncoder)
            parentEncoder->endEncoding(m_computeCommandEncoder);
        m_computeCommandEncoder = nil;
        parentEncoder->lock(false);
        parentEncoder->makeInvalid([NSString stringWithFormat:@"ComputePassEncoder.endPass failure, m_debugGroupStackSize = %llu, isValid = %d, error = %@", m_debugGroupStackSize, passIsValid, m_lastErrorString]);
        return;
    }

    if (!parentEncoder->isValid() || !parentEncoder->encoderIsCurrent(m_computeCommandEncoder)) {
        m_computeCommandEncoder = nil;
        parentEncoder->lock(false);
        parentEncoder->makeInvalid(@"ComputePassEncoder.endPass: the pass no longer holds the command encoder");
        return;
    }

    RetainPtr<NSString> encoderLabel = m_computeCommandEncoder.label;
    parentEncoder->endEncoding(m_computeCommandEncoder);
    m_computeCommandEncoder = nil;
    if (m_isDeferringEncoding && m_deferredState && !m_deferredState->commands.isEmpty() && !parentEncoder->submitWillBeInvalid()) {
        auto deferredState = std::exchange(m_deferredState, nullptr);
        m_isDeferringEncoding = false;
        ThreadSafeWeakPtr<Instance> weakInstance = m_device->weakInstance();
        ThreadSafeWeakPtr<Device> weakDevice { m_device };
        parentEncoder->addDeferredPassReplay([deferredState = WTF::move(deferredState), encoderLabel = WTF::move(encoderLabel), weakInstance = WTF::move(weakInstance), weakDevice = WTF::move(weakDevice)](id<MTLCommandBuffer> deferredCommandBuffer) mutable {
            for (auto& command : deferredState->commands) {
                if (auto* setPipeline = std::get_if<DeferredState::SetPipeline>(&command)) {
                    if (!setPipeline->readyPSO && setPipeline->pending) {
                        setPipeline->readyPSO = setPipeline->pending->wait();
                        if (!setPipeline->readyPSO) {
                            if (RefPtr protectedInstance = weakInstance) {
                                bool hadLibErr = setPipeline->pending->hadLibraryError;
                                RetainPtr<NSString> errMsg = setPipeline->pending->libraryErrorMessage;
                                protectedInstance->scheduleWork([weakDevice, hadLibErr, errMsg = WTF::move(errMsg)] mutable {
                                    if (RefPtr device = weakDevice) {
                                        if (hadLibErr)
                                            device->generateAValidationError(errMsg.get() ?: @"Compute library failed creation");
                                        else
                                            device->generateAnOutOfMemoryError("Compute pipeline failed compilation likely due to being too complex, please reduce its size"_s);
                                    }
                                });
                            }
                            return;
                        }
                    }
                }
            }

            MTLComputePassDescriptor* desc = [MTLComputePassDescriptor new];
            desc.dispatchType = MTLDispatchTypeSerial;
            id<MTLComputeCommandEncoder> enc = [deferredCommandBuffer computeCommandEncoderWithDescriptor:desc];
            if (!enc)
                return;
            if (encoderLabel)
                enc.label = encoderLabel.get();

            for (auto& command : deferredState->commands) {
                WTF::switchOn(command,
                    [&](DeferredState::UseResources& cmd) {
                        if (cmd.rawResources.size())
                            [enc useResources:borrow(cmd.rawResources)->span().data() count:cmd.rawResources.size() usage:cmd.usage];
                    },
                    [&](DeferredState::SetPipeline& cmd) {
                        [enc setComputePipelineState:cmd.readyPSO.get()];
                    },
                    [&](DeferredState::SetBuffer& cmd) {
                        [enc setBuffer:cmd.buffer.get() offset:cmd.offset atIndex:cmd.index];
                    },
                    [&](DeferredState::SetBytes& cmd) {
                        [enc setBytes:borrow(cmd.bytes)->span().data() length:cmd.bytes.size() atIndex:cmd.index];
                    },
                    [&](DeferredState::Dispatch& cmd) {
                        [enc dispatchThreadgroups:cmd.threadgroups threadsPerThreadgroup:cmd.threadsPerThreadgroup];
                    });
            }
            [enc endEncoding];
        });
    } else {
        m_deferredState = nullptr;
        m_isDeferringEncoding = false;
    }
    parentEncoder->lock(false);
}

void ComputePassEncoder::insertDebugMarker(String&& markerLabel)
{
    RETURN_IF_FINISHED();
    flushDeferredStateToMetalEncoder();
    if (!m_computeCommandEncoder)
        return;
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    if (!prepareTheEncoderState())
        return;

    [m_computeCommandEncoder insertDebugSignpost:markerLabel.createNSString().get()];
}

bool ComputePassEncoder::validatePopDebugGroup() const
{
    if (!m_parentEncoder->isLocked())
        return false;

    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void ComputePassEncoder::makeInvalid(NSString* errorString)
{
    m_lastErrorString = errorString;
    m_deferredState = nullptr;
    m_isDeferringEncoding = false;

    auto parentEncoder = m_parentEncoder;

    if (!m_computeCommandEncoder) {
        parentEncoder->makeInvalid(@"RenderPassEncoder.makeInvalid");
        return;
    }

    parentEncoder->setLastError(errorString);
    parentEncoder->endEncoding(m_computeCommandEncoder);
    m_computeCommandEncoder = nil;
}

void ComputePassEncoder::popDebugGroup()
{
    RETURN_IF_FINISHED();
    flushDeferredStateToMetalEncoder();
    if (!m_computeCommandEncoder)
        return;
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid();
        return;
    }

    --m_debugGroupStackSize;
    [m_computeCommandEncoder popDebugGroup];
}

void ComputePassEncoder::pushDebugGroup(String&& groupLabel)
{
    RETURN_IF_FINISHED();
    flushDeferredStateToMetalEncoder();
    if (!m_computeCommandEncoder)
        return;
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;

    ++m_debugGroupStackSize;
    [m_computeCommandEncoder pushDebugGroup:groupLabel.createNSString().get()];
}

static void setCommandEncoder(const BindGroupEntryUsageData::Resource& resource, CommandEncoder& parentEncoder)
{
    WTF::switchOn(resource, [&](const RefPtr<Buffer>& buffer) {
        if (buffer)
            buffer->setCommandEncoder(parentEncoder);
        }, [&](const RefPtr<const Texture>& texture) {
            if (texture)
                texture->setCommandEncoder(parentEncoder);
        }, [&](const RefPtr<const TextureView>& textureView) {
            if (textureView)
                textureView->setCommandEncoder(parentEncoder);
        }, [&](const RefPtr<const ExternalTexture>& externalTexture) {
            if (externalTexture)
                externalTexture->setCommandEncoder(parentEncoder);
    });
}

void ComputePassEncoder::setPipeline(const WebGPU::ComputePipeline& pipeline)
{
    setPipeline(downcast<ComputePipeline>(pipeline));
}

void ComputePassEncoder::dispatchIndirect(const WebGPU::Buffer& indirectBuffer, uint64_t indirectOffset)
{
    dispatchIndirect(downcast<Buffer>(indirectBuffer), indirectOffset);
}

void ComputePassEncoder::setBindGroup(uint32_t groupIndex, const WebGPU::BindGroup* group, std::optional<std::span<const uint32_t>> dynamicOffsets)
{
    setBindGroup(groupIndex, downcast<BindGroup>(group), dynamicOffsets);
}

void ComputePassEncoder::setImmediates(uint32_t rangeOffset, std::span<const uint8_t> data)
{
    RETURN_IF_FINISHED();

    auto endOffset = checkedSum<uint64_t>(rangeOffset, data.size());
    if ((rangeOffset % 4) || (data.size() % 4) || endOffset.hasOverflowed() || endOffset.value() > m_device->limits().maxImmediateSize) {
        makeInvalid(@"GPUComputePassEncoder.setImmediates: invalid rangeOffset or contentSize");
        return;
    }

    if (data.size()) {
        memcpySpan(std::span { m_immediateData }.subspan(rangeOffset, data.size()), data);
        for (size_t slot = rangeOffset / 4, endSlot = endOffset.value() / 4; slot < endSlot; ++slot)
            m_setImmediateSlots |= (1ull << slot);
        m_immediatesDirty = true;
    }
}

void ComputePassEncoder::setBindGroup(uint32_t groupIndex, const BindGroup* groupPtr, std::optional<std::span<const uint32_t>> apiDynamicOffsets)
{
    // The encoder keeps the dynamic offsets.
    std::optional<Vector<uint32_t>> dynamicOffsets;
    if (apiDynamicOffsets)
        dynamicOffsets = Vector<uint32_t> { *apiDynamicOffsets };

    RETURN_IF_FINISHED();

    RefPtr groupLayout = groupPtr ? groupPtr->bindGroupLayout() : nullptr;
    auto dynamicOffsetCount = groupLayout ? groupLayout->dynamicBufferCount() : 0;
    if (groupIndex >= m_device->limits().maxBindGroups || (dynamicOffsets && dynamicOffsetCount != dynamicOffsets->size())) {
        makeInvalid(@"GPUComputePassEncoder.setBindGroup: groupIndex >= limits.maxBindGroups");
        return;
    }

    if (!groupPtr) {
        m_bindGroups.remove(groupIndex);
        m_bindGroupResources.remove(groupIndex);
        m_bindGroupDynamicOffsets.remove(groupIndex);
        m_maxDynamicOffsetAtIndex[groupIndex] = 0;
        return;
    }

    auto& group = *groupPtr;
    if (!isValidToUseWith(group, *this)) {
        makeInvalid(@"GPUComputePassEncoder.setBindGroup: invalid bind group");
        return;
    }

    RefPtr bindGroupLayout = group.bindGroupLayout();
    if (!bindGroupLayout) {
        makeInvalid(@"GPUComputePassEncoder.setBindGroup: bind group is nil");
        return;
    }
    if (NSString* error = bindGroupLayout->errorValidatingDynamicOffsets(dynamicOffsets ? borrow(*dynamicOffsets)->span() : std::span<const uint32_t> { }, group, m_maxDynamicOffsetAtIndex[groupIndex])) {
        makeInvalid([NSString stringWithFormat:@"GPUComputePassEncoder.setBindGroup: %@", error]);
        return;
    }

    if (dynamicOffsets && dynamicOffsets->size()) {
        m_bindGroupDynamicOffsets.set(groupIndex, WTF::move(*dynamicOffsets));
        m_maxDynamicOffsetAtIndex[groupIndex] = 0;
    } else if (m_bindGroupDynamicOffsets.remove(groupIndex))
        m_maxDynamicOffsetAtIndex[groupIndex] = 0;

    Vector<const BindableResources*> resourceList;
    if (m_isDeferringEncoding && m_deferredState && !m_parentEncoder->submitWillBeInvalid())
        m_deferredState->appendBindGroupResources(group);
    for (const auto& resource : group.resources()) {
        if (!m_isDeferringEncoding && resource.renderStages == BindGroup::MTLRenderStageCompute && resource.mtlResources.size())
            [computeCommandEncoder() useResources:&borrow(resource.mtlResources)[0] count:resource.mtlResources.size() usage:resource.usage];

        ASSERT(resource.mtlResources.size() == resource.resourceUsages.size());
        resourceList.append(&resource);
        ASSERT(resource.mtlResources.size() == resource.resourceUsages.size());
        for (size_t i = 0, sz = resource.mtlResources.size(); i < sz; ++i) {
            auto& resourceUsage = resource.resourceUsages[i];
            m_parentEncoder->addResidentResource(resource.mtlResources[i]);
            setCommandEncoder(resourceUsage.resource, m_parentEncoder);
        }
    }

    m_bindGroupResources.set(groupIndex, resourceList);
    m_bindGroups.set(groupIndex, group);
}

void ComputePassEncoder::setPipeline(const ComputePipeline& pipeline)
{
    RETURN_IF_FINISHED();
    if (!isValidToUseWith(pipeline, *this)) {
        makeInvalid(@"GPUComputePipeline is invalid to use with this GPUComputePassEncoder");
        return;
    }

    m_pipeline = pipeline;
    m_computeDynamicOffsets.fill(0, protect(m_pipeline->pipelineLayout())->sizeOfComputeDynamicOffsets());
    m_maxDynamicOffsetAtIndex.fill(0);

    m_threadsPerThreadgroup = pipeline.threadsPerThreadgroup();
}

void ComputePassEncoder::setLabel(String&& label)
{
    m_computeCommandEncoder.label = label.createNSString().get();
}

bool ComputePassEncoder::isValid() const
{
    return m_computeCommandEncoder;
}

id<MTLComputeCommandEncoder> ComputePassEncoder::computeCommandEncoder() const
{
    return m_parentEncoder->submitWillBeInvalid() ? nil : m_computeCommandEncoder;
}

#undef RETURN_IF_FINISHED

} // namespace WebGPU::Metal
