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

#if ENABLE(WEBGPU)

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

EntryPointScaffolding::EntryPointScaffolding(AST::FunctionDefinition& functionDefinition, Intrinsics& intrinsics, TypeNamer& typeNamer, EntryPointItems& entryPointItems, HashMap<Binding*, size_t>& resourceMap, Layout& layout, std::function<String()>&& generateNextVariableName)
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

String EntryPointScaffolding::resourceHelperTypes()
{
    StringBuilder stringBuilder;
    for (size_t i = 0; i < m_layout.size(); ++i) {
        stringBuilder.append(makeString("struct ", m_namedBindGroups[i].structName, " {\n"));
        Vector<std::pair<unsigned, String>> structItems;
        for (size_t j = 0; j < m_layout[i].bindings.size(); ++j) {
            auto iterator = m_resourceMap.find(&m_layout[i].bindings[j]);
            if (iterator == m_resourceMap.end())
                continue;
            auto& type = m_entryPointItems.inputs[iterator->value].unnamedType->unifyNode();
            if (is<AST::UnnamedType>(type) && is<AST::ReferenceType>(downcast<AST::UnnamedType>(type))) {
                auto& referenceType = downcast<AST::ReferenceType>(downcast<AST::UnnamedType>(type));
                auto mangledTypeName = m_typeNamer.mangledNameForType(referenceType.elementType());
                auto addressSpace = toString(referenceType.addressSpace());
                auto elementName = m_namedBindGroups[i].namedBindings[j].elementName;
                auto index = m_namedBindGroups[i].namedBindings[j].index;
                structItems.append(std::make_pair(index, makeString(addressSpace, " ", mangledTypeName, "* ", elementName, " [[id(", index, ")]];")));
                if (auto lengthInformation = m_namedBindGroups[i].namedBindings[j].lengthInformation)
                    structItems.append(std::make_pair(lengthInformation->index, makeString("uint2 ", lengthInformation->elementName, " [[id(", lengthInformation->index, ")]];")));
            } else if (is<AST::NamedType>(type) && is<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(type))) {
                auto& namedType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(type));
                auto mangledTypeName = m_typeNamer.mangledNameForType(namedType);
                auto elementName = m_namedBindGroups[i].namedBindings[j].elementName;
                auto index = m_namedBindGroups[i].namedBindings[j].index;
                structItems.append(std::make_pair(index, makeString(mangledTypeName, ' ', elementName, " [[id(", index, ")]];")));
            }
        }
        std::sort(structItems.begin(), structItems.end(), [](const std::pair<unsigned, String>& left, const std::pair<unsigned, String>& right) {
            return left.first < right.first;
        });
        for (const auto& structItem : structItems)
            stringBuilder.append(makeString("    ", structItem.second, '\n'));
        stringBuilder.append("};\n\n");
    }
    return stringBuilder.toString();
}

Optional<String> EntryPointScaffolding::resourceSignature()
{
    if (!m_layout.size())
        return WTF::nullopt;

    StringBuilder stringBuilder;
    for (size_t i = 0; i < m_layout.size(); ++i) {
        if (i)
            stringBuilder.append(", ");
        auto& namedBindGroup = m_namedBindGroups[i];
        stringBuilder.append(makeString("device ", namedBindGroup.structName, "& ", namedBindGroup.variableName, " [[buffer(", namedBindGroup.argumentBufferIndex, ")]]"));
    }
    return stringBuilder.toString();
}

static String internalTypeForSemantic(const AST::BuiltInSemantic& builtInSemantic)
{
    switch (builtInSemantic.variable()) {
    case AST::BuiltInSemantic::Variable::SVInstanceID:
        return "uint"_str;
    case AST::BuiltInSemantic::Variable::SVVertexID:
        return "uint"_str;
    case AST::BuiltInSemantic::Variable::PSize:
        return "float"_str;
    case AST::BuiltInSemantic::Variable::SVPosition:
        return "float4"_str;
    case AST::BuiltInSemantic::Variable::SVIsFrontFace:
        return "bool"_str;
    case AST::BuiltInSemantic::Variable::SVSampleIndex:
        return "uint"_str;
    case AST::BuiltInSemantic::Variable::SVInnerCoverage:
        return "uint"_str;
    case AST::BuiltInSemantic::Variable::SVTarget:
        return String();
    case AST::BuiltInSemantic::Variable::SVDepth:
        return "float"_str;
    case AST::BuiltInSemantic::Variable::SVCoverage:
        return "uint"_str;
    case AST::BuiltInSemantic::Variable::SVDispatchThreadID:
        return "uint3"_str;
    case AST::BuiltInSemantic::Variable::SVGroupID:
        return "uint3"_str;
    case AST::BuiltInSemantic::Variable::SVGroupIndex:
        return "uint"_str;
    default:
        ASSERT(builtInSemantic.variable() == AST::BuiltInSemantic::Variable::SVGroupThreadID);
        return "uint3"_str;
    }
}

Optional<String> EntryPointScaffolding::builtInsSignature()
{
    if (!m_namedBuiltIns.size())
        return WTF::nullopt;

    StringBuilder stringBuilder;
    for (size_t i = 0; i < m_namedBuiltIns.size(); ++i) {
        if (i)
            stringBuilder.append(", ");
        auto& namedBuiltIn = m_namedBuiltIns[i];
        auto& item = m_entryPointItems.inputs[namedBuiltIn.indexInEntryPointItems];
        auto& builtInSemantic = WTF::get<AST::BuiltInSemantic>(*item.semantic);
        auto internalType = internalTypeForSemantic(builtInSemantic);
        if (internalType.isNull())
            internalType = m_typeNamer.mangledNameForType(*item.unnamedType);
        auto variableName = namedBuiltIn.variableName;
        stringBuilder.append(makeString(internalType, ' ', variableName, ' ', attributeForSemantic(builtInSemantic)));
    }
    return stringBuilder.toString();
}

String EntryPointScaffolding::mangledInputPath(Vector<String>& path)
{
    ASSERT(!path.isEmpty());
    StringBuilder stringBuilder;
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
        stringBuilder.append(makeString('.', m_typeNamer.mangledNameForStructureElement(*next)));
        structureDefinition = nullptr;
        auto& unifyNode = next->type().unifyNode();
        if (is<AST::NamedType>(unifyNode)) {
            auto& namedType = downcast<AST::NamedType>(unifyNode);
            if (is<AST::StructureDefinition>(namedType))
                structureDefinition = &downcast<AST::StructureDefinition>(namedType);
        }
    }

    return stringBuilder.toString();
}

String EntryPointScaffolding::mangledOutputPath(Vector<String>& path)
{
    StringBuilder stringBuilder;

    AST::StructureDefinition* structureDefinition = nullptr;
    auto& unifyNode = m_functionDefinition.type().unifyNode();
    structureDefinition = &downcast<AST::StructureDefinition>(downcast<AST::NamedType>(unifyNode));
    for (auto& component : path) {
        ASSERT(structureDefinition);
        auto* next = structureDefinition->find(component);
        ASSERT(next);
        stringBuilder.append(makeString('.', m_typeNamer.mangledNameForStructureElement(*next)));
        structureDefinition = nullptr;
        auto& unifyNode = next->type().unifyNode();
        if (is<AST::NamedType>(unifyNode)) {
            auto& namedType = downcast<AST::NamedType>(unifyNode);
            if (is<AST::StructureDefinition>(namedType))
                structureDefinition = &downcast<AST::StructureDefinition>(namedType);
        }
    }

    return stringBuilder.toString();
}

String EntryPointScaffolding::unpackResourcesAndNamedBuiltIns()
{
    StringBuilder stringBuilder;
    for (size_t i = 0; i < m_functionDefinition.parameters().size(); ++i)
        stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*m_functionDefinition.parameters()[i]->type()), ' ', m_parameterVariables[i], ";\n"));

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

                stringBuilder.append(makeString("size_t ", lengthTemporaryName, " = ", variableName, '.', lengthElementName, ".y;\n"));
                stringBuilder.append(makeString(lengthTemporaryName, " = ", lengthTemporaryName, " << 32;\n"));
                stringBuilder.append(makeString(lengthTemporaryName, " = ", lengthTemporaryName, " | ", variableName, '.', lengthElementName, ".x;\n"));
                stringBuilder.append(makeString(lengthTemporaryName, " = ", lengthTemporaryName, " / sizeof(", mangledTypeName, ");\n"));
                stringBuilder.append(makeString("if (", lengthTemporaryName, " > 0xFFFFFFFF) ", lengthTemporaryName, " = 0xFFFFFFFF;\n"));
                stringBuilder.append(makeString(mangledInputPath(path), " = { ", variableName, '.', elementName, ", static_cast<uint32_t>(", lengthTemporaryName, ") };\n"));
            } else {
                auto& path = m_entryPointItems.inputs[iterator->value].path;
                auto elementName = m_namedBindGroups[i].namedBindings[j].elementName;
                stringBuilder.append(makeString(mangledInputPath(path), " = ", variableName, '.', elementName, ";\n"));
            }
        }
    }

    for (auto& namedBuiltIn : m_namedBuiltIns) {
        auto& item = m_entryPointItems.inputs[namedBuiltIn.indexInEntryPointItems];
        auto& path = item.path;
        auto& variableName = namedBuiltIn.variableName;
        auto mangledTypeName = m_typeNamer.mangledNameForType(*item.unnamedType);
        stringBuilder.append(makeString(mangledInputPath(path), " = ", mangledTypeName, '(', variableName, ");\n"));
    }
    return stringBuilder.toString();
}

VertexEntryPointScaffolding::VertexEntryPointScaffolding(AST::FunctionDefinition& functionDefinition, Intrinsics& intrinsics, TypeNamer& typeNamer, EntryPointItems& entryPointItems, HashMap<Binding*, size_t>& resourceMap, Layout& layout, std::function<String()>&& generateNextVariableName, HashMap<VertexAttribute*, size_t>& matchedVertexAttributes)
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
        if (WTF::holds_alternative<AST::BuiltInSemantic>(*outputItem.semantic))
            namedOutput.internalTypeName = internalTypeForSemantic(WTF::get<AST::BuiltInSemantic>(*outputItem.semantic));
        if (namedOutput.internalTypeName.isNull())
            namedOutput.internalTypeName = m_typeNamer.mangledNameForType(*outputItem.unnamedType);
        m_namedOutputs.uncheckedAppend(WTFMove(namedOutput));
    }
}

String VertexEntryPointScaffolding::helperTypes()
{
    StringBuilder stringBuilder;

    stringBuilder.append(makeString("struct ", m_stageInStructName, " {\n"));
    for (auto& namedStageIn : m_namedStageIns) {
        auto mangledTypeName = m_typeNamer.mangledNameForType(*m_entryPointItems.inputs[namedStageIn.indexInEntryPointItems].unnamedType);
        auto elementName = namedStageIn.elementName;
        auto attributeIndex = namedStageIn.attributeIndex;
        stringBuilder.append(makeString("    ", mangledTypeName, ' ', elementName, " [[attribute(", attributeIndex, ")]];\n"));
    }
    stringBuilder.append("};\n\n");

    stringBuilder.append(makeString("struct ", m_returnStructName, " {\n"));
    for (size_t i = 0; i < m_entryPointItems.outputs.size(); ++i) {
        auto& outputItem = m_entryPointItems.outputs[i];
        auto& internalTypeName = m_namedOutputs[i].internalTypeName;
        auto elementName = m_namedOutputs[i].elementName;
        auto attribute = attributeForSemantic(*outputItem.semantic);
        stringBuilder.append(makeString("    ", internalTypeName, ' ', elementName, ' ', attribute, ";\n"));
    }
    stringBuilder.append("};\n\n");

    stringBuilder.append(resourceHelperTypes());

    return stringBuilder.toString();
}

String VertexEntryPointScaffolding::signature(String& functionName)
{
    StringBuilder stringBuilder;

    stringBuilder.append(makeString("vertex ", m_returnStructName, ' ', functionName, '(', m_stageInStructName, ' ', m_stageInParameterName, " [[stage_in]]"));
    if (auto resourceSignature = this->resourceSignature())
        stringBuilder.append(makeString(", ", *resourceSignature));
    if (auto builtInsSignature = this->builtInsSignature())
        stringBuilder.append(makeString(", ", *builtInsSignature));
    stringBuilder.append(")");

    return stringBuilder.toString();
}

String VertexEntryPointScaffolding::unpack()
{
    StringBuilder stringBuilder;

    stringBuilder.append(unpackResourcesAndNamedBuiltIns());

    for (auto& namedStageIn : m_namedStageIns) {
        auto& path = m_entryPointItems.inputs[namedStageIn.indexInEntryPointItems].path;
        auto& elementName = namedStageIn.elementName;
        stringBuilder.append(makeString(mangledInputPath(path), " = ", m_stageInParameterName, '.', elementName, ";\n"));
    }

    return stringBuilder.toString();
}

String VertexEntryPointScaffolding::pack(const String& inputVariableName, const String& outputVariableName)
{
    StringBuilder stringBuilder;
    stringBuilder.append(makeString(m_returnStructName, ' ', outputVariableName, ";\n"));
    if (m_entryPointItems.outputs.size() == 1 && !m_entryPointItems.outputs[0].path.size()) {
        auto& elementName = m_namedOutputs[0].elementName;
        stringBuilder.append(makeString(outputVariableName, '.', elementName, " = ", inputVariableName, ";\n"));
        return stringBuilder.toString();
    }
    for (size_t i = 0; i < m_entryPointItems.outputs.size(); ++i) {
        auto& elementName = m_namedOutputs[i].elementName;
        auto& internalTypeName = m_namedOutputs[i].internalTypeName;
        auto& path = m_entryPointItems.outputs[i].path;
        stringBuilder.append(makeString(outputVariableName, '.', elementName, " = ", internalTypeName, '(', inputVariableName, mangledOutputPath(path), ");\n"));
    }
    return stringBuilder.toString();
}

FragmentEntryPointScaffolding::FragmentEntryPointScaffolding(AST::FunctionDefinition& functionDefinition, Intrinsics& intrinsics, TypeNamer& typeNamer, EntryPointItems& entryPointItems, HashMap<Binding*, size_t>& resourceMap, Layout& layout, std::function<String()>&& generateNextVariableName, HashMap<AttachmentDescriptor*, size_t>&)
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
        if (WTF::holds_alternative<AST::BuiltInSemantic>(*outputItem.semantic))
            namedOutput.internalTypeName = internalTypeForSemantic(WTF::get<AST::BuiltInSemantic>(*outputItem.semantic));
        if (namedOutput.internalTypeName.isNull())
            namedOutput.internalTypeName = m_typeNamer.mangledNameForType(*outputItem.unnamedType);
        m_namedOutputs.uncheckedAppend(WTFMove(namedOutput));
    }
}

String FragmentEntryPointScaffolding::helperTypes()
{
    StringBuilder stringBuilder;

    stringBuilder.append(makeString("struct ", m_stageInStructName, " {\n"));
    for (auto& namedStageIn : m_namedStageIns) {
        auto mangledTypeName = m_typeNamer.mangledNameForType(*m_entryPointItems.inputs[namedStageIn.indexInEntryPointItems].unnamedType);
        auto elementName = namedStageIn.elementName;
        auto attributeIndex = namedStageIn.attributeIndex;
        stringBuilder.append(makeString("    ", mangledTypeName, ' ', elementName, " [[user(user", attributeIndex, ")]];\n"));
    }
    stringBuilder.append("};\n\n");

    stringBuilder.append(makeString("struct ", m_returnStructName, " {\n"));
    for (size_t i = 0; i < m_entryPointItems.outputs.size(); ++i) {
        auto& outputItem = m_entryPointItems.outputs[i];
        auto& internalTypeName = m_namedOutputs[i].internalTypeName;
        auto elementName = m_namedOutputs[i].elementName;
        auto attribute = attributeForSemantic(*outputItem.semantic);
        stringBuilder.append(makeString("    ", internalTypeName, ' ', elementName, ' ', attribute, ";\n"));
    }
    stringBuilder.append("};\n\n");

    stringBuilder.append(resourceHelperTypes());

    return stringBuilder.toString();
}

String FragmentEntryPointScaffolding::signature(String& functionName)
{
    StringBuilder stringBuilder;

    stringBuilder.append(makeString("fragment ", m_returnStructName, ' ', functionName, '(', m_stageInStructName, ' ', m_stageInParameterName, " [[stage_in]]"));
    if (auto resourceSignature = this->resourceSignature())
        stringBuilder.append(makeString(", ", *resourceSignature));
    if (auto builtInsSignature = this->builtInsSignature())
        stringBuilder.append(makeString(", ", *builtInsSignature));
    stringBuilder.append(")");

    return stringBuilder.toString();
}

String FragmentEntryPointScaffolding::unpack()
{
    StringBuilder stringBuilder;

    stringBuilder.append(unpackResourcesAndNamedBuiltIns());

    for (auto& namedStageIn : m_namedStageIns) {
        auto& path = m_entryPointItems.inputs[namedStageIn.indexInEntryPointItems].path;
        auto& elementName = namedStageIn.elementName;
        stringBuilder.append(makeString(mangledInputPath(path), " = ", m_stageInParameterName, '.', elementName, ";\n"));
    }

    return stringBuilder.toString();
}

String FragmentEntryPointScaffolding::pack(const String& inputVariableName, const String& outputVariableName)
{
    StringBuilder stringBuilder;
    stringBuilder.append(makeString(m_returnStructName, ' ', outputVariableName, ";\n"));
    if (m_entryPointItems.outputs.size() == 1 && !m_entryPointItems.outputs[0].path.size()) {
        auto& elementName = m_namedOutputs[0].elementName;
        stringBuilder.append(makeString(outputVariableName, '.', elementName, " = ", inputVariableName, ";\n"));
        return stringBuilder.toString();
    }
    for (size_t i = 0; i < m_entryPointItems.outputs.size(); ++i) {
        auto& elementName = m_namedOutputs[i].elementName;
        auto& internalTypeName = m_namedOutputs[i].internalTypeName;
        auto& path = m_entryPointItems.outputs[i].path;
        stringBuilder.append(makeString(outputVariableName, '.', elementName, " = ", internalTypeName, '(', inputVariableName, mangledOutputPath(path), ");\n"));
    }
    return stringBuilder.toString();
}

ComputeEntryPointScaffolding::ComputeEntryPointScaffolding(AST::FunctionDefinition& functionDefinition, Intrinsics& intrinsics, TypeNamer& typeNamer, EntryPointItems& entryPointItems, HashMap<Binding*, size_t>& resourceMap, Layout& layout, std::function<String()>&& generateNextVariableName)
    : EntryPointScaffolding(functionDefinition, intrinsics, typeNamer, entryPointItems, resourceMap, layout, WTFMove(generateNextVariableName))
{
}

String ComputeEntryPointScaffolding::helperTypes()
{
    return resourceHelperTypes();
}

String ComputeEntryPointScaffolding::signature(String& functionName)
{
    StringBuilder stringBuilder;

    stringBuilder.append(makeString("kernel void ", functionName, '('));
    bool empty = true;
    if (auto resourceSignature = this->resourceSignature()) {
        empty = false;
        stringBuilder.append(makeString(*resourceSignature));
    }
    if (auto builtInsSignature = this->builtInsSignature()) {
        if (!empty)
            stringBuilder.append(", ");
        stringBuilder.append(*builtInsSignature);
    }
    stringBuilder.append(")");

    return stringBuilder.toString();
}

String ComputeEntryPointScaffolding::unpack()
{
    return unpackResourcesAndNamedBuiltIns();
}

String ComputeEntryPointScaffolding::pack(const String&, const String&)
{
    ASSERT_NOT_REACHED();
    return String();
}

}

}

}

#endif
