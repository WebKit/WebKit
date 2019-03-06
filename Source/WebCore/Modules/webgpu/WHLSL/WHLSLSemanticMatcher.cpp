/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WHLSLSemanticMatcher.h"

#if ENABLE(WEBGPU)

#include "WHLSLBuiltInSemantic.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLGatherEntryPointItems.h"
#include "WHLSLInferTypes.h"
#include "WHLSLPipelineDescriptor.h"
#include "WHLSLProgram.h"
#include "WHLSLResourceSemantic.h"
#include "WHLSLStageInOutSemantic.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

static AST::FunctionDefinition* findEntryPoint(Vector<UniqueRef<AST::FunctionDefinition>>& functionDefinitions, String& name)
{
    auto iterator = std::find_if(functionDefinitions.begin(), functionDefinitions.end(), [&](AST::FunctionDefinition& functionDefinition) {
        return functionDefinition.entryPointType() && functionDefinition.name() == name;
    });
    if (iterator == functionDefinitions.end())
        return nullptr;
    return &*iterator;
};

static bool matchMode(BindingType bindingType, AST::ResourceSemantic::Mode mode)
{
    switch (bindingType) {
    case BindingType::UniformBuffer:
        return mode == AST::ResourceSemantic::Mode::Buffer;
    case BindingType::Sampler:
        return mode == AST::ResourceSemantic::Mode::Sampler;
    case BindingType::Texture:
        return mode == AST::ResourceSemantic::Mode::Texture;
    default:
        ASSERT(bindingType == BindingType::StorageBuffer);
        return mode == AST::ResourceSemantic::Mode::UnorderedAccessView;
    }
}

static Optional<HashMap<Binding*, size_t>> matchResources(Vector<EntryPointItem>& entryPointItems, Layout& layout, ShaderStage shaderStage)
{
    HashMap<Binding*, size_t> result;
    HashSet<size_t> itemIndices;
    if (entryPointItems.size() == std::numeric_limits<size_t>::max())
        return WTF::nullopt; // Work around the fact that HashSet's keys are restricted.
    for (auto& bindGroup : layout) {
        auto space = bindGroup.name;
        for (auto& binding : bindGroup.bindings) {
            if (!binding.visibility.contains(shaderStage))
                continue;
            for (size_t i = 0; i < entryPointItems.size(); ++i) {
                auto& item = entryPointItems[i];
                auto& semantic = *item.semantic;
                if (!WTF::holds_alternative<AST::ResourceSemantic>(semantic))
                    continue;
                auto& resourceSemantic = WTF::get<AST::ResourceSemantic>(semantic);
                if (!matchMode(binding.bindingType, resourceSemantic.mode()))
                    continue;
                if (binding.name != resourceSemantic.index())
                    continue;
                if (space != resourceSemantic.space())
                    continue;
                result.add(&binding, i);
                itemIndices.add(i + 1); // Work around the fact that HashSet's keys are restricted.
            }
        }
    }

    for (size_t i = 0; i < entryPointItems.size(); ++i) {
        auto& item = entryPointItems[i];
        auto& semantic = *item.semantic;
        if (!WTF::holds_alternative<AST::ResourceSemantic>(semantic))
            continue;
        if (!itemIndices.contains(i + 1))
            return WTF::nullopt;
    }

    return result;
}

static bool matchInputsOutputs(Vector<EntryPointItem>& vertexOutputs, Vector<EntryPointItem>& fragmentInputs)
{
    for (auto& fragmentInput : fragmentInputs) {
        if (!WTF::holds_alternative<AST::StageInOutSemantic>(*fragmentInput.semantic))
            continue;
        auto& fragmentInputStageInOutSemantic = WTF::get<AST::StageInOutSemantic>(*fragmentInput.semantic);
        bool found = false;
        for (auto& vertexOutput : vertexOutputs) {
            if (!WTF::holds_alternative<AST::StageInOutSemantic>(*vertexOutput.semantic))
                continue;
            auto& vertexOutputStageInOutSemantic = WTF::get<AST::StageInOutSemantic>(*vertexOutput.semantic);
            if (fragmentInputStageInOutSemantic.index() == vertexOutputStageInOutSemantic.index()) {
                if (matches(*fragmentInput.unnamedType, *vertexOutput.unnamedType)) {
                    found = true;
                    break;
                }
                return false;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

static bool isAcceptableFormat(VertexFormat vertexFormat, AST::UnnamedType& unnamedType, Intrinsics& intrinsics)
{
    switch (vertexFormat) {
    case VertexFormat::FloatR32G32B32A32:
        return matches(unnamedType, intrinsics.float4Type());
    case VertexFormat::FloatR32G32B32:
        return matches(unnamedType, intrinsics.float3Type());
    case VertexFormat::FloatR32G32:
        return matches(unnamedType, intrinsics.float2Type());
    default:
        ASSERT(vertexFormat == VertexFormat::FloatR32);
        return matches(unnamedType, intrinsics.floatType());
    }
}

static Optional<HashMap<VertexAttribute*, size_t>> matchVertexAttributes(Vector<EntryPointItem>& vertexInputs, VertexAttributes& vertexAttributes, Intrinsics& intrinsics)
{
    HashMap<VertexAttribute*, size_t> result;
    HashSet<size_t> itemIndices;
    if (vertexInputs.size() == std::numeric_limits<size_t>::max())
        return WTF::nullopt; // Work around the fact that HashSet's keys are restricted.
    for (auto& vertexAttribute : vertexAttributes) {
        for (size_t i = 0; i < vertexInputs.size(); ++i) {
            auto& item = vertexInputs[i];
            auto& semantic = *item.semantic;
            if (!WTF::holds_alternative<AST::StageInOutSemantic>(semantic))
                continue;
            auto& stageInOutSemantic = WTF::get<AST::StageInOutSemantic>(semantic);
            if (stageInOutSemantic.index() != vertexAttribute.name)
                continue;
            if (!isAcceptableFormat(vertexAttribute.vertexFormat, *item.unnamedType, intrinsics))
                return WTF::nullopt;
            result.add(&vertexAttribute, i);
            itemIndices.add(i + 1); // Work around the fact that HashSet's keys are restricted.
        }
    }

    for (size_t i = 0; i < vertexInputs.size(); ++i) {
        auto& item = vertexInputs[i];
        auto& semantic = *item.semantic;
        if (!WTF::holds_alternative<AST::StageInOutSemantic>(semantic))
            continue;
        if (!itemIndices.contains(i + 1))
            return WTF::nullopt;
    }

    return result;
}

static bool isAcceptableFormat(TextureFormat textureFormat, AST::UnnamedType& unnamedType, Intrinsics& intrinsics, bool isColor)
{
    if (isColor) {
        switch (textureFormat) {
        case TextureFormat::R8G8B8A8Unorm:
        case TextureFormat::R8G8B8A8Uint:
        case TextureFormat::B8G8R8A8Unorm:
            return matches(unnamedType, intrinsics.float4Type());
        default:
            ASSERT(textureFormat == TextureFormat::D32FloatS8Uint);
            return false;
        }
    }
    return textureFormat == TextureFormat::D32FloatS8Uint && matches(unnamedType, intrinsics.floatType());
}

static Optional<HashMap<AttachmentDescriptor*, size_t>> matchColorAttachments(Vector<EntryPointItem>& fragmentOutputs, Vector<AttachmentDescriptor>& attachmentDescriptors, Intrinsics& intrinsics)
{
    HashMap<AttachmentDescriptor*, size_t> result;
    HashSet<size_t> itemIndices;
    if (attachmentDescriptors.size() == std::numeric_limits<size_t>::max())
        return WTF::nullopt; // Work around the fact that HashSet's keys are restricted.
    for (auto& attachmentDescriptor : attachmentDescriptors) {
        for (size_t i = 0; i < fragmentOutputs.size(); ++i) {
            auto& item = fragmentOutputs[i];
            auto& semantic = *item.semantic;
            if (!WTF::holds_alternative<AST::StageInOutSemantic>(semantic))
                continue;
            auto& stageInOutSemantic = WTF::get<AST::StageInOutSemantic>(semantic);
            if (stageInOutSemantic.index() != attachmentDescriptor.name)
                continue;
            if (!isAcceptableFormat(attachmentDescriptor.textureFormat, *item.unnamedType, intrinsics, true))
                return WTF::nullopt;
            result.add(&attachmentDescriptor, i);
            itemIndices.add(i + 1); // Work around the fact that HashSet's keys are restricted.
        }
    }

    for (size_t i = 0; i < fragmentOutputs.size(); ++i) {
        auto& item = fragmentOutputs[i];
        auto& semantic = *item.semantic;
        if (!WTF::holds_alternative<AST::StageInOutSemantic>(semantic))
            continue;
        if (!itemIndices.contains(i + 1))
            return WTF::nullopt;
    }

    return result;
}

static bool matchDepthAttachment(Vector<EntryPointItem>& fragmentOutputs, Optional<AttachmentDescriptor>& depthStencilAttachmentDescriptor, Intrinsics& intrinsics)
{
    auto iterator = std::find_if(fragmentOutputs.begin(), fragmentOutputs.end(), [&](EntryPointItem& item) {
        auto& semantic = *item.semantic;
        if (!WTF::holds_alternative<AST::BuiltInSemantic>(semantic))
            return false;
        auto& builtInSemantic = WTF::get<AST::BuiltInSemantic>(semantic);
        return builtInSemantic.variable() == AST::BuiltInSemantic::Variable::SVDepth;
    });
    if (iterator == fragmentOutputs.end())
        return true;

    if (depthStencilAttachmentDescriptor) {
        ASSERT(!depthStencilAttachmentDescriptor->name);
        return isAcceptableFormat(depthStencilAttachmentDescriptor->textureFormat, *iterator->unnamedType, intrinsics, false);
    }
    return false;
}

Optional<MatchedRenderSemantics> matchSemantics(Program& program, RenderPipelineDescriptor& renderPipelineDescriptor)
{
    auto vertexShaderEntryPoint = findEntryPoint(program.functionDefinitions(), renderPipelineDescriptor.vertexEntryPointName);
    auto fragmentShaderEntryPoint = findEntryPoint(program.functionDefinitions(), renderPipelineDescriptor.fragmentEntryPointName);
    if (!vertexShaderEntryPoint || !fragmentShaderEntryPoint)
        return WTF::nullopt;
    auto vertexShaderEntryPointItems = gatherEntryPointItems(program.intrinsics(), *vertexShaderEntryPoint);
    auto fragmentShaderEntryPointItems = gatherEntryPointItems(program.intrinsics(), *fragmentShaderEntryPoint);
    if (!vertexShaderEntryPointItems || !fragmentShaderEntryPointItems)
        return WTF::nullopt;
    auto vertexShaderResourceMap = matchResources(vertexShaderEntryPointItems->inputs, renderPipelineDescriptor.layout, ShaderStage::Vertex);
    auto fragmentShaderResourceMap = matchResources(fragmentShaderEntryPointItems->inputs, renderPipelineDescriptor.layout, ShaderStage::Fragment);
    if (!vertexShaderResourceMap || !fragmentShaderResourceMap)
        return WTF::nullopt;
    if (!matchInputsOutputs(vertexShaderEntryPointItems->outputs, fragmentShaderEntryPointItems->inputs))
        return WTF::nullopt;
    auto matchedVertexAttributes = matchVertexAttributes(vertexShaderEntryPointItems->inputs, renderPipelineDescriptor.vertexAttributes, program.intrinsics());
    if (!matchedVertexAttributes)
        return WTF::nullopt;
    auto matchedColorAttachments = matchColorAttachments(fragmentShaderEntryPointItems->outputs, renderPipelineDescriptor.attachmentsStateDescriptor.attachmentDescriptors, program.intrinsics());
    if (!matchedColorAttachments)
        return WTF::nullopt;
    if (!matchDepthAttachment(fragmentShaderEntryPointItems->outputs, renderPipelineDescriptor.attachmentsStateDescriptor.depthStencilAttachmentDescriptor, program.intrinsics()))
        return WTF::nullopt;
    return {{ vertexShaderEntryPoint, fragmentShaderEntryPoint, *vertexShaderEntryPointItems, *fragmentShaderEntryPointItems, *vertexShaderResourceMap, *fragmentShaderResourceMap, *matchedVertexAttributes, *matchedColorAttachments }};
}

Optional<MatchedComputeSemantics> matchSemantics(Program& program, ComputePipelineDescriptor& computePipelineDescriptor)
{
    auto entryPoint = findEntryPoint(program.functionDefinitions(), computePipelineDescriptor.entryPointName);
    if (!entryPoint)
        return WTF::nullopt;
    auto entryPointItems = gatherEntryPointItems(program.intrinsics(), *entryPoint);
    if (!entryPointItems)
        return WTF::nullopt;
    ASSERT(entryPointItems->outputs.isEmpty());
    auto resourceMap = matchResources(entryPointItems->inputs, computePipelineDescriptor.layout, ShaderStage::Compute);
    if (!resourceMap)
        return WTF::nullopt;
    return {{ entryPoint, *entryPointItems, *resourceMap }};
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
