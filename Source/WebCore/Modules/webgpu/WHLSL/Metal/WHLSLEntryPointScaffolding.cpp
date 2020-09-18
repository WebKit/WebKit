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
#include "WHLSLEntryPointScaffolding.h"

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLBuiltInSemantic.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLGatherEntryPointItems.h"
#include "WHLSLPipelineDescriptor.h"
#include "WHLSLReferenceType.h"
#include "WHLSLResourceSemantic.h"
#include "WHLSLStageInOutSemantic.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLTypeNamer.h"
#include <algorithm>
#include <wtf/Optional.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

namespace WHLSL {

namespace Metal {

static String attributeForSemantic(AST::BuiltInSemantic& builtInSemantic)
{
    switch (builtInSemantic.variable()) {
    case AST::BuiltInSemantic::Variable::SVInstanceID:
        return "[[instance_id]]"_str;
    case AST::BuiltInSemantic::Variable::SVVertexID:
        return "[[vertex_id]]"_str;
    case AST::BuiltInSemantic::Variable::PSize:
        return "[[point_size]]"_str;
    case AST::BuiltInSemantic::Variable::SVPosition:
        return "[[position]]"_str;
    case AST::BuiltInSemantic::Variable::SVIsFrontFace:
        return "[[front_facing]]"_str;
    case AST::BuiltInSemantic::Variable::SVSampleIndex:
        return "[[sample_id]]"_str;
    case AST::BuiltInSemantic::Variable::SVInnerCoverage:
        return "[[sample_mask]]"_str;
    case AST::BuiltInSemantic::Variable::SVTarget:
        return makeString("[[color(", *builtInSemantic.targetIndex(), ")]]");
    case AST::BuiltInSemantic::Variable::SVDepth:
        return "[[depth(any)]]"_str;
    case AST::BuiltInSemantic::Variable::SVCoverage:
        return "[[sample_mask]]"_str;
    case AST::BuiltInSemantic::Variable::SVDispatchThreadID:
        return "[[thread_position_in_grid]]"_str;
    case AST::BuiltInSemantic::Variable::SVGroupID:
        return "[[threadgroup_position_in_grid]]"_str;
    case AST::BuiltInSemantic::Variable::SVGroupIndex:
        return "[[thread_index_in_threadgroup]]"_str;
    default:
        ASSERT(builtInSemantic.variable() == AST::BuiltInSemantic::Variable::SVGroupThreadID);
        return "[[thread_position_in_threadgroup]]"_str;
    }
}

static String attributeForSemantic(AST::Semantic& semantic)
{
    if (WTF::holds_alternative<AST::BuiltInSemantic>(semantic))
        return attributeForSemantic(WTF::get<AST::BuiltInSemantic>(semantic));
    auto& stageInOutSemantic = WTF::get<AST::StageInOutSemantic>(semantic);
    return makeString("[[user(user", stageInOutSemantic.index(), ")]]");
}

EntryPointScaffolding::EntryPointScaffolding(AST::FunctionDefinition& functionDefinition, Intrinsics& intrinsics, TypeNamer& typeNamer, EntryPointItems& entryPointItems, HashMap<Binding*, size_t>& resourceMap, Layout& layout, std::function<MangledVariableName()>&& generateNextVariableName)
    : m_functionDefinition(functionDefinition)
    , m_intrinsics(intrinsics)
    , m_typeNamer(typeNamer)
    , m_entryPointItems(entryPointItems)
    , m_resourceMap(resourceMap)
    , m_layout(layout)
    , m_generateNextVariableName(generateNextVariableName)
{
    m_namedBindGroups.reserveInitialCapacity(m_layout.size());
    for (size_t i = 0; i < m_layout.size(); ++i) {
        NamedBindGroup namedBindGroup;
        namedBindGroup.structName = m_typeNamer.generateNextTypeName();
        namedBindGroup.variableName = m_generateNextVariableName();
        namedBindGroup.argumentBufferIndex = m_layout[i].name; // convertLayout() in GPURenderPipelineMetal.mm makes sure these don't collide.
        namedBindGroup.namedBindings.reserveInitialCapacity(m_layout[i].bindings.size());
        for (size_t j = 0; j < m_layout[i].bindings.size(); ++j) {
            NamedBinding namedBinding;
            namedBinding.elementName = m_typeNamer.generateNextStructureElementName();
            namedBinding.index = m_layout[i].bindings[j].internalName;
            WTF::visit(WTF::makeVisitor([&](UniformBufferBinding& uniformBufferBinding) {
                LengthInformation lengthInformation { m_typeNamer.generateNextStructureElementName(), m_generateNextVariableName(), uniformBufferBinding.lengthName };
                namedBinding.lengthInformation = lengthInformation;
            }, [&](SamplerBinding&) {
            }, [&](TextureBinding&) {
            }, [&](StorageBufferBinding& storageBufferBinding) {
                LengthInformation lengthInformation { m_typeNamer.generateNextStructureElementName(), m_generateNextVariableName(), storageBufferBinding.lengthName };
                namedBinding.lengthInformation = lengthInformation;
            }), m_layout[i].bindings[j].binding);
            namedBindGroup.namedBindings.uncheckedAppend(WTFMove(namedBinding));
        }
        m_namedBindGroups.uncheckedAppend(WTFMove(namedBindGroup));
    }

    for (size_t i = 0; i < m_entryPointItems.inputs.size(); ++i) {
        if (!WTF::holds_alternative<AST::BuiltInSemantic>(*m_entryPointItems.inputs[i].semantic))
            continue;
        NamedBuiltIn namedBuiltIn;
        namedBuiltIn.indexInEntryPointItems = i;
        namedBuiltIn.variableName = m_generateNextVariableName();
        m_namedBuiltIns.append(WTFMove(namedBuiltIn));
    }

    m_parameterVariables.reserveInitialCapacity(m_functionDefinition.parameters().size());
    for (size_t i = 0; i < m_functionDefinition.parameters().size(); ++i)
        m_parameterVariables.uncheckedAppend(m_generateNextVariableName());
}

void EntryPointScaffolding::emitResourceHelperTypes(StringBuilder& stringBuilder, Indentation<4> indent, ShaderStage shaderStage)
{
    for (size_t i = 0; i < m_layout.size(); ++i) {
        stringBuilder.append(indent, "struct ", m_namedBindGroups[i].structName, " {\n");
        {
            IndentationScope scope(indent);
            Vector<std::pair<unsigned, String>> structItems;
            for (size_t j = 0; j < m_layout[i].bindings.size(); ++j) {
                auto& binding = m_layout[i].bindings[j];
                if (!binding.visibility.contains(shaderStage))
                    continue;

                auto elementName = m_namedBindGroups[i].namedBindings[j].elementName;
                auto index = m_namedBindGroups[i].namedBindings[j].index;
                if (auto lengthInformation = m_namedBindGroups[i].namedBindings[j].lengthInformation)
                    structItems.append(std::make_pair(lengthInformation->index, makeString("uint2 ", lengthInformation->elementName, " [[id(", lengthInformation->index, ")]];")));

                auto iterator = m_resourceMap.find(&m_layout[i].bindings[j]);
                if (iterator != m_resourceMap.end()) {
                    auto& type = m_entryPointItems.inputs[iterator->value].unnamedType->unifyNode();
                    if (is<AST::UnnamedType>(type) && is<AST::ReferenceType>(downcast<AST::UnnamedType>(type))) {
                        auto& referenceType = downcast<AST::ReferenceType>(downcast<AST::UnnamedType>(type));
                        auto mangledTypeName = m_typeNamer.mangledNameForType(referenceType.elementType());
                        auto addressSpace = toString(referenceType.addressSpace());
                        structItems.append(std::make_pair(index, makeString(addressSpace, " ", mangledTypeName, "* ", elementName, " [[id(", index, ")]];")));
                    } else if (is<AST::NamedType>(type) && is<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(type))) {
                        auto& namedType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(type));
                        auto mangledTypeName = m_typeNamer.mangledNameForType(namedType);
                        structItems.append(std::make_pair(index, makeString(mangledTypeName, ' ', elementName, " [[id(", index, ")]];")));
                    }
                } else {
                    // The binding doesn't appear in the shader source.
                    // However, we must still emit a placeholder, so successive items in the argument buffer struct have the correct offset.
                    // Because the binding doesn't appear in the shader source, we don't know which exact type the bind point should have.
                    // Therefore, we must synthesize a type out of thin air.
                    WTF::visit(WTF::makeVisitor([&](UniformBufferBinding) {
                        structItems.append(std::make_pair(index, makeString("constant void* ", elementName, " [[id(", index, ")]];")));
                    }, [&](SamplerBinding) {
                        structItems.append(std::make_pair(index, makeString("sampler ", elementName, " [[id(", index, ")]];")));
                    }, [&](TextureBinding) {
                        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=201384 We don't know which texture type the binding represents. This is no good very bad.
                        structItems.append(std::make_pair(index, makeString("texture2d<float> ", elementName, " [[id(", index, ")]];")));
                    }, [&](StorageBufferBinding) {
                        structItems.append(std::make_pair(index, makeString("device void* ", elementName, " [[id(", index, ")]];")));
                    }), binding.binding);
                }
            }
            std::sort(structItems.begin(), structItems.end(), [](const std::pair<unsigned, String>& left, const std::pair<unsigned, String>& right) {
                return left.first < right.first;
            });
            for (const auto& structItem : structItems)
                stringBuilder.append(indent, structItem.second, '\n');
        }
        stringBuilder.append(indent, "};\n\n");
    }
}

bool EntryPointScaffolding::emitResourceSignature(StringBuilder& stringBuilder, IncludePrecedingComma includePrecedingComma)
{
    if (!m_layout.size())
        return false;

    if (includePrecedingComma == IncludePrecedingComma::Yes)
        stringBuilder.append(", ");

    for (size_t i = 0; i < m_layout.size(); ++i) {
        if (i)
            stringBuilder.append(", ");
        auto& namedBindGroup = m_namedBindGroups[i];
        stringBuilder.append("device ", namedBindGroup.structName, "& ", namedBindGroup.variableName, " [[buffer(", namedBindGroup.argumentBufferIndex, ")]]");
    }
    return true;
}

static StringView internalTypeForSemantic(const AST::BuiltInSemantic& builtInSemantic)
{
    switch (builtInSemantic.variable()) {
    case AST::BuiltInSemantic::Variable::SVInstanceID:
        return "uint";
    case AST::BuiltInSemantic::Variable::SVVertexID:
        return "uint";
    case AST::BuiltInSemantic::Variable::PSize:
        return "float";
    case AST::BuiltInSemantic::Variable::SVPosition:
        return "float4";
    case AST::BuiltInSemantic::Variable::SVIsFrontFace:
        return "bool";
    case AST::BuiltInSemantic::Variable::SVSampleIndex:
        return "uint";
    case AST::BuiltInSemantic::Variable::SVInnerCoverage:
        return "uint";
    case AST::BuiltInSemantic::Variable::SVTarget:
        return { };
    case AST::BuiltInSemantic::Variable::SVDepth:
        return "float";
    case AST::BuiltInSemantic::Variable::SVCoverage:
        return "uint";
    case AST::BuiltInSemantic::Variable::SVDispatchThreadID:
        return "uint3";
    case AST::BuiltInSemantic::Variable::SVGroupID:
        return "uint3";
    case AST::BuiltInSemantic::Variable::SVGroupIndex:
        return "uint";
    default:
        ASSERT(builtInSemantic.variable() == AST::BuiltInSemantic::Variable::SVGroupThreadID);
        return "uint3";
    }
}

bool EntryPointScaffolding::emitBuiltInsSignature(StringBuilder& stringBuilder, IncludePrecedingComma includePrecedingComma)
{
    if (!m_namedBuiltIns.size())
        return false;

    if (includePrecedingComma == IncludePrecedingComma::Yes)
        stringBuilder.append(", ");

    for (size_t i = 0; i < m_namedBuiltIns.size(); ++i) {
        if (i)
            stringBuilder.append(", ");
        auto& namedBuiltIn = m_namedBuiltIns[i];
        auto& item = m_entryPointItems.inputs[namedBuiltIn.indexInEntryPointItems];
        auto& builtInSemantic = WTF::get<AST::BuiltInSemantic>(*item.semantic);
        auto internalType = internalTypeForSemantic(builtInSemantic);
        if (!internalType.isNull())
            stringBuilder.append(internalType);
        else
            stringBuilder.append(m_typeNamer.mangledNameForType(*item.unnamedType));
        stringBuilder.append(' ', namedBuiltIn.variableName, ' ', attributeForSemantic(builtInSemantic));
    }
    return true;
}

void EntryPointScaffolding::emitMangledInputPath(StringBuilder& stringBuilder, Vector<String>& path)
{
    ASSERT(!path.isEmpty());
    bool found = false;
    AST::StructureDefinition* structureDefinition = nullptr;
    for (size_t i = 0; i < m_functionDefinition.parameters().size(); ++i) {
        if (m_functionDefinition.parameters()[i]->name() == path[0]) {
            stringBuilder.append(m_parameterVariables[i]);
            auto& unifyNode = m_functionDefinition.parameters()[i]->type()->unifyNode();
            if (is<AST::NamedType>(unifyNode)) {
                auto& namedType = downcast<AST::NamedType>(unifyNode);
                if (is<AST::StructureDefinition>(namedType))
                    structureDefinition = &downcast<AST::StructureDefinition>(namedType);
            }
            found = true;
            break;
        }
    }
    ASSERT(found);
    for (size_t i = 1; i < path.size(); ++i) {
        ASSERT(structureDefinition);
        auto* next = structureDefinition->find(path[i]);
        ASSERT(next);
        stringBuilder.append('.', m_typeNamer.mangledNameForStructureElement(*next));
        structureDefinition = nullptr;
        auto& unifyNode = next->type().unifyNode();
        if (is<AST::NamedType>(unifyNode)) {
            auto& namedType = downcast<AST::NamedType>(unifyNode);
            if (is<AST::StructureDefinition>(namedType))
                structureDefinition = &downcast<AST::StructureDefinition>(namedType);
        }
    }
}

void EntryPointScaffolding::emitMangledOutputPath(StringBuilder& stringBuilder, Vector<String>& path)
{
    AST::StructureDefinition* structureDefinition = nullptr;
    auto& unifyNode = m_functionDefinition.type().unifyNode();
    structureDefinition = &downcast<AST::StructureDefinition>(downcast<AST::NamedType>(unifyNode));
    for (auto& component : path) {
        ASSERT(structureDefinition);
        auto* next = structureDefinition->find(component);
        ASSERT(next);
        stringBuilder.append('.', m_typeNamer.mangledNameForStructureElement(*next));
        structureDefinition = nullptr;
        auto& unifyNode = next->type().unifyNode();
        if (is<AST::NamedType>(unifyNode)) {
            auto& namedType = downcast<AST::NamedType>(unifyNode);
            if (is<AST::StructureDefinition>(namedType))
                structureDefinition = &downcast<AST::StructureDefinition>(namedType);
        }
    }
}

void EntryPointScaffolding::emitUnpackResourcesAndNamedBuiltIns(StringBuilder& stringBuilder, Indentation<4> indent)
{
    for (size_t i = 0; i < m_functionDefinition.parameters().size(); ++i)
        stringBuilder.append(indent, m_typeNamer.mangledNameForType(*m_functionDefinition.parameters()[i]->type()), ' ', m_parameterVariables[i], ";\n");

    for (size_t i = 0; i < m_layout.size(); ++i) {
        auto variableName = m_namedBindGroups[i].variableName;
        for (size_t j = 0; j < m_layout[i].bindings.size(); ++j) {
            auto iterator = m_resourceMap.find(&m_layout[i].bindings[j]);
            if (iterator == m_resourceMap.end())
                continue;
            if (m_namedBindGroups[i].namedBindings[j].lengthInformation) {
                auto& path = m_entryPointItems.inputs[iterator->value].path;
                auto elementName = m_namedBindGroups[i].namedBindings[j].elementName;
                auto lengthElementName = m_namedBindGroups[i].namedBindings[j].lengthInformation->elementName;
                auto lengthTemporaryName = m_namedBindGroups[i].namedBindings[j].lengthInformation->temporaryName;

                auto& unnamedType = *m_entryPointItems.inputs[iterator->value].unnamedType;
                auto mangledTypeName = m_typeNamer.mangledNameForType(downcast<AST::ReferenceType>(unnamedType).elementType());

                stringBuilder.append(
                    indent, "size_t ", lengthTemporaryName, " = ", variableName, '.', lengthElementName, ".y;\n",
                    indent, lengthTemporaryName, " = ", lengthTemporaryName, " << 32;\n",
                    indent, lengthTemporaryName, " = ", lengthTemporaryName, " | ", variableName, '.', lengthElementName, ".x;\n",
                    indent, lengthTemporaryName, " = ", lengthTemporaryName, " / sizeof(", mangledTypeName, ");\n",
                    indent, "if (", lengthTemporaryName, " > 0xFFFFFFFF)\n",
                    indent, "    ", lengthTemporaryName, " = 0xFFFFFFFF;\n"
                );

                stringBuilder.append(indent);
                emitMangledInputPath(stringBuilder, path);
                stringBuilder.append(
                    " = { ", variableName, '.', elementName, ", static_cast<uint32_t>(", lengthTemporaryName, ") };\n"
                );
            } else {
                auto& path = m_entryPointItems.inputs[iterator->value].path;
                auto elementName = m_namedBindGroups[i].namedBindings[j].elementName;
                
                stringBuilder.append(indent);
                emitMangledInputPath(stringBuilder, path);
                stringBuilder.append(" = ", variableName, '.', elementName, ";\n");
            }
        }
    }

    for (auto& namedBuiltIn : m_namedBuiltIns) {
        auto& item = m_entryPointItems.inputs[namedBuiltIn.indexInEntryPointItems];
        auto& path = item.path;
        auto& variableName = namedBuiltIn.variableName;
        auto mangledTypeName = m_typeNamer.mangledNameForType(*item.unnamedType);

        stringBuilder.append(indent);
        emitMangledInputPath(stringBuilder, path);
        stringBuilder.append(" = ", mangledTypeName, '(', variableName, ");\n");
    }
}

VertexEntryPointScaffolding::VertexEntryPointScaffolding(AST::FunctionDefinition& functionDefinition, Intrinsics& intrinsics, TypeNamer& typeNamer, EntryPointItems& entryPointItems, HashMap<Binding*, size_t>& resourceMap, Layout& layout, std::function<MangledVariableName()>&& generateNextVariableName, HashMap<VertexAttribute*, size_t>& matchedVertexAttributes)
    : EntryPointScaffolding(functionDefinition, intrinsics, typeNamer, entryPointItems, resourceMap, layout, WTFMove(generateNextVariableName))
    , m_matchedVertexAttributes(matchedVertexAttributes)
    , m_stageInStructName(typeNamer.generateNextTypeName())
    , m_returnStructName(typeNamer.generateNextTypeName())
    , m_stageInParameterName(m_generateNextVariableName())
{
    m_namedStageIns.reserveInitialCapacity(m_matchedVertexAttributes.size());
    for (auto& keyValuePair : m_matchedVertexAttributes) {
        NamedStageIn namedStageIn;
        namedStageIn.indexInEntryPointItems = keyValuePair.value;
        namedStageIn.elementName = m_typeNamer.generateNextStructureElementName();
        namedStageIn.attributeIndex = keyValuePair.key->metalLocation;
        m_namedStageIns.uncheckedAppend(WTFMove(namedStageIn));
    }

    m_namedOutputs.reserveInitialCapacity(m_entryPointItems.outputs.size());
    for (size_t i = 0; i < m_entryPointItems.outputs.size(); ++i) {
        auto& outputItem = m_entryPointItems.outputs[i];
        NamedOutput namedOutput;
        namedOutput.elementName = m_typeNamer.generateNextStructureElementName();
        StringView internalType;
        if (WTF::holds_alternative<AST::BuiltInSemantic>(*outputItem.semantic))
            internalType = internalTypeForSemantic(WTF::get<AST::BuiltInSemantic>(*outputItem.semantic));
        if (!internalType.isNull())
            namedOutput.internalTypeName = internalType.toString();
        else
            namedOutput.internalTypeName = m_typeNamer.mangledNameForType(*outputItem.unnamedType);
        m_namedOutputs.uncheckedAppend(WTFMove(namedOutput));
    }
}

void VertexEntryPointScaffolding::emitHelperTypes(StringBuilder& stringBuilder, Indentation<4> indent)
{
    stringBuilder.append(indent, "struct ", m_stageInStructName, " {\n");
    {
        IndentationScope scope(indent);
        for (auto& namedStageIn : m_namedStageIns) {
            auto mangledTypeName = m_typeNamer.mangledNameForType(*m_entryPointItems.inputs[namedStageIn.indexInEntryPointItems].unnamedType);
            auto elementName = namedStageIn.elementName;
            auto attributeIndex = namedStageIn.attributeIndex;
            stringBuilder.append(indent, mangledTypeName, ' ', elementName, " [[attribute(", attributeIndex, ")]];\n");
        }
    }
    stringBuilder.append(
        indent, "};\n\n",
        indent, "struct ", m_returnStructName, " {\n"
    );
    {
        IndentationScope scope(indent);
        for (size_t i = 0; i < m_entryPointItems.outputs.size(); ++i) {
            auto& outputItem = m_entryPointItems.outputs[i];
            auto& internalTypeName = m_namedOutputs[i].internalTypeName;
            auto elementName = m_namedOutputs[i].elementName;
            auto attribute = attributeForSemantic(*outputItem.semantic);
            stringBuilder.append(indent, internalTypeName, ' ', elementName, ' ', attribute, ";\n");
        }
    }
    stringBuilder.append(indent, "};\n\n");
    
    emitResourceHelperTypes(stringBuilder, indent, ShaderStage::Vertex);
}

void VertexEntryPointScaffolding::emitSignature(StringBuilder& stringBuilder, MangledFunctionName functionName, Indentation<4> indent)
{
    stringBuilder.append(indent, "vertex ", m_returnStructName, ' ', functionName, '(', m_stageInStructName, ' ', m_stageInParameterName, " [[stage_in]]");
    emitResourceSignature(stringBuilder, IncludePrecedingComma::Yes);
    emitBuiltInsSignature(stringBuilder, IncludePrecedingComma::Yes);
    stringBuilder.append(")\n");
}

void VertexEntryPointScaffolding::emitUnpack(StringBuilder& stringBuilder, Indentation<4> indent)
{
    emitUnpackResourcesAndNamedBuiltIns(stringBuilder, indent);

    for (auto& namedStageIn : m_namedStageIns) {
        auto& path = m_entryPointItems.inputs[namedStageIn.indexInEntryPointItems].path;
        auto& elementName = namedStageIn.elementName;
        
        stringBuilder.append(indent);
        emitMangledInputPath(stringBuilder, path);
        stringBuilder.append(" = ", m_stageInParameterName, '.', elementName, ";\n");
    }
}

void VertexEntryPointScaffolding::emitPack(StringBuilder& stringBuilder, MangledVariableName inputVariableName, MangledVariableName outputVariableName, Indentation<4> indent)
{
    stringBuilder.append(indent, m_returnStructName, ' ', outputVariableName, ";\n");
    if (m_entryPointItems.outputs.size() == 1 && !m_entryPointItems.outputs[0].path.size()) {
        auto& elementName = m_namedOutputs[0].elementName;
        stringBuilder.append(indent, outputVariableName, '.', elementName, " = ", inputVariableName, ";\n");
        return;
    }
    for (size_t i = 0; i < m_entryPointItems.outputs.size(); ++i) {
        auto& elementName = m_namedOutputs[i].elementName;
        auto& internalTypeName = m_namedOutputs[i].internalTypeName;
        auto& path = m_entryPointItems.outputs[i].path;
        stringBuilder.append(indent, outputVariableName, '.', elementName, " = ", internalTypeName, '(', inputVariableName);
        emitMangledOutputPath(stringBuilder, path);
        stringBuilder.append(");\n");
    }
}

FragmentEntryPointScaffolding::FragmentEntryPointScaffolding(AST::FunctionDefinition& functionDefinition, Intrinsics& intrinsics, TypeNamer& typeNamer, EntryPointItems& entryPointItems, HashMap<Binding*, size_t>& resourceMap, Layout& layout, std::function<MangledVariableName()>&& generateNextVariableName, HashMap<AttachmentDescriptor*, size_t>&)
    : EntryPointScaffolding(functionDefinition, intrinsics, typeNamer, entryPointItems, resourceMap, layout, WTFMove(generateNextVariableName))
    , m_stageInStructName(typeNamer.generateNextTypeName())
    , m_returnStructName(typeNamer.generateNextTypeName())
    , m_stageInParameterName(m_generateNextVariableName())
{
    for (size_t i = 0; i < m_entryPointItems.inputs.size(); ++i) {
        auto& inputItem = m_entryPointItems.inputs[i];
        if (!WTF::holds_alternative<AST::StageInOutSemantic>(*inputItem.semantic))
            continue;
        auto& stageInOutSemantic = WTF::get<AST::StageInOutSemantic>(*inputItem.semantic);
        NamedStageIn namedStageIn;
        namedStageIn.indexInEntryPointItems = i;
        namedStageIn.elementName = m_typeNamer.generateNextStructureElementName();
        namedStageIn.attributeIndex = stageInOutSemantic.index();
        m_namedStageIns.append(WTFMove(namedStageIn));
    }

    m_namedOutputs.reserveInitialCapacity(m_entryPointItems.outputs.size());
    for (size_t i = 0; i < m_entryPointItems.outputs.size(); ++i) {
        auto& outputItem = m_entryPointItems.outputs[i];
        NamedOutput namedOutput;
        namedOutput.elementName = m_typeNamer.generateNextStructureElementName();
        StringView internalType;
        if (WTF::holds_alternative<AST::BuiltInSemantic>(*outputItem.semantic))
            internalType = internalTypeForSemantic(WTF::get<AST::BuiltInSemantic>(*outputItem.semantic));
        if (!internalType.isNull())
            namedOutput.internalTypeName = internalType.toString();
        else
            namedOutput.internalTypeName = m_typeNamer.mangledNameForType(*outputItem.unnamedType);
        m_namedOutputs.uncheckedAppend(WTFMove(namedOutput));
    }
}

void FragmentEntryPointScaffolding::emitHelperTypes(StringBuilder& stringBuilder, Indentation<4> indent)
{
    stringBuilder.append(indent, "struct ", m_stageInStructName, " {\n");
    {
        IndentationScope scope(indent);
        for (auto& namedStageIn : m_namedStageIns) {
            auto mangledTypeName = m_typeNamer.mangledNameForType(*m_entryPointItems.inputs[namedStageIn.indexInEntryPointItems].unnamedType);
            auto elementName = namedStageIn.elementName;
            auto attributeIndex = namedStageIn.attributeIndex;
            stringBuilder.append(indent, mangledTypeName, ' ', elementName, " [[user(user", attributeIndex, ")]];\n");
        }
    }
    stringBuilder.append(
        indent, "};\n\n",
        indent, "struct ", m_returnStructName, " {\n"
    );
    {
        IndentationScope scope(indent);
        for (size_t i = 0; i < m_entryPointItems.outputs.size(); ++i) {
            auto& outputItem = m_entryPointItems.outputs[i];
            auto& internalTypeName = m_namedOutputs[i].internalTypeName;
            auto elementName = m_namedOutputs[i].elementName;
            auto attribute = attributeForSemantic(*outputItem.semantic);
            stringBuilder.append(indent, internalTypeName, ' ', elementName, ' ', attribute, ";\n");
        }
    }
    stringBuilder.append(indent, "};\n\n");

    emitResourceHelperTypes(stringBuilder, indent, ShaderStage::Fragment);
}

void FragmentEntryPointScaffolding::emitSignature(StringBuilder& stringBuilder, MangledFunctionName functionName, Indentation<4> indent)
{
    stringBuilder.append(indent, "fragment ", m_returnStructName, ' ', functionName, '(', m_stageInStructName, ' ', m_stageInParameterName, " [[stage_in]]");
    emitResourceSignature(stringBuilder, IncludePrecedingComma::Yes);
    emitBuiltInsSignature(stringBuilder, IncludePrecedingComma::Yes);
    stringBuilder.append(")\n");
}

void FragmentEntryPointScaffolding::emitUnpack(StringBuilder& stringBuilder, Indentation<4> indent)
{
    emitUnpackResourcesAndNamedBuiltIns(stringBuilder, indent);

    for (auto& namedStageIn : m_namedStageIns) {
        auto& path = m_entryPointItems.inputs[namedStageIn.indexInEntryPointItems].path;
        auto& elementName = namedStageIn.elementName;

        stringBuilder.append(indent);
        emitMangledInputPath(stringBuilder, path);
        stringBuilder.append(" = ", m_stageInParameterName, '.', elementName, ";\n");
    }
}

void FragmentEntryPointScaffolding::emitPack(StringBuilder& stringBuilder, MangledVariableName inputVariableName, MangledVariableName outputVariableName, Indentation<4> indent)
{
    stringBuilder.append(indent, m_returnStructName, ' ', outputVariableName, ";\n");
    if (m_entryPointItems.outputs.size() == 1 && !m_entryPointItems.outputs[0].path.size()) {
        auto& elementName = m_namedOutputs[0].elementName;
        stringBuilder.append(indent, outputVariableName, '.', elementName, " = ", inputVariableName, ";\n");
        return;
    }
    for (size_t i = 0; i < m_entryPointItems.outputs.size(); ++i) {
        auto& elementName = m_namedOutputs[i].elementName;
        auto& internalTypeName = m_namedOutputs[i].internalTypeName;
        auto& path = m_entryPointItems.outputs[i].path;
        stringBuilder.append(indent, outputVariableName, '.', elementName, " = ", internalTypeName, '(', inputVariableName);
        emitMangledOutputPath(stringBuilder, path);
        stringBuilder.append(");\n");
    }
}

ComputeEntryPointScaffolding::ComputeEntryPointScaffolding(AST::FunctionDefinition& functionDefinition, Intrinsics& intrinsics, TypeNamer& typeNamer, EntryPointItems& entryPointItems, HashMap<Binding*, size_t>& resourceMap, Layout& layout, std::function<MangledVariableName()>&& generateNextVariableName)
    : EntryPointScaffolding(functionDefinition, intrinsics, typeNamer, entryPointItems, resourceMap, layout, WTFMove(generateNextVariableName))
{
}

void ComputeEntryPointScaffolding::emitHelperTypes(StringBuilder& stringBuilder, Indentation<4> indent)
{
    emitResourceHelperTypes(stringBuilder, indent, ShaderStage::Compute);
}

void ComputeEntryPointScaffolding::emitSignature(StringBuilder& stringBuilder, MangledFunctionName functionName, Indentation<4> indent)
{
    stringBuilder.append(indent, "kernel void ", functionName, '(');
    bool addedToSignature = emitResourceSignature(stringBuilder, IncludePrecedingComma::No);
    emitBuiltInsSignature(stringBuilder, addedToSignature ? IncludePrecedingComma::Yes : IncludePrecedingComma::No);
    stringBuilder.append(")\n");
}

void ComputeEntryPointScaffolding::emitUnpack(StringBuilder& stringBuilder, Indentation<4> indent)
{
    emitUnpackResourcesAndNamedBuiltIns(stringBuilder, indent);
}

void ComputeEntryPointScaffolding::emitPack(StringBuilder&, MangledVariableName, MangledVariableName, Indentation<4>)
{
    ASSERT_NOT_REACHED();
}

}

}

}

#endif // ENABLE(WHLSL_COMPILER)
