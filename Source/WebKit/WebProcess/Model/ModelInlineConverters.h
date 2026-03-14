/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBGPU_SWIFT) && ENABLE(GPU_PROCESS_MODEL)

#include "ModelTypes.h"
#include <ImageIO/CGImageSource.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

static WebModel::VertexAttributeFormat toCpp(WKBridgeVertexAttributeFormat *format)
{
    return WebModel::VertexAttributeFormat {
        .semantic = format.semantic,
        .format = format.format,
        .layoutIndex = format.layoutIndex,
        .offset = format.offset
    };
}

static Vector<WebModel::VertexAttributeFormat> toCpp(NSArray<WKBridgeVertexAttributeFormat *> *formats)
{
    Vector<WebModel::VertexAttributeFormat> result;
    for (WKBridgeVertexAttributeFormat *f in formats)
        result.append(toCpp(f));
    return result;
}

static WebModel::VertexLayout toCpp(WKBridgeVertexLayout *layout)
{
    return WebModel::VertexLayout {
        .bufferIndex = layout.bufferIndex,
        .bufferOffset = layout.bufferOffset,
        .bufferStride = layout.bufferStride,
    };
}
static Vector<WebModel::VertexLayout> toCpp(NSArray<WKBridgeVertexLayout *> *layouts)
{
    Vector<WebModel::VertexLayout> result;
    for (WKBridgeVertexLayout *l in layouts)
        result.append(toCpp(l));
    return result;
}

static WebModel::MeshPart toCpp(WKBridgeMeshPart *part)
{
    return WebModel::MeshPart {
        static_cast<uint32_t>(part.indexOffset),
        static_cast<uint32_t>(part.indexCount),
        static_cast<uint32_t>(part.topology),
        static_cast<uint32_t>(part.materialIndex),
        part.boundsMin,
        part.boundsMax
    };
}

static Vector<WebModel::MeshPart> toCpp(NSArray<WKBridgeMeshPart *> *parts)
{
    Vector<WebModel::MeshPart> result;
    for (WKBridgeMeshPart *p in parts)
        result.append(toCpp(p));
    return result;
}

static WebModel::MeshDescriptor toCpp(WKBridgeMeshDescriptor *descriptor)
{
    return WebModel::MeshDescriptor {
        .vertexBufferCount = descriptor.vertexBufferCount,
        .vertexCapacity = descriptor.vertexCapacity,
        .vertexAttributes = toCpp(descriptor.vertexAttributes),
        .vertexLayouts = toCpp(descriptor.vertexLayouts),
        .indexCapacity = descriptor.indexCapacity,
        .indexType = static_cast<long>(descriptor.indexType)
    };
}

static Vector<Vector<uint8_t>> toCpp(NSArray<NSData *> *dataVector)
{
    Vector<Vector<uint8_t>> result;
    for (NSData *data in dataVector)
        result.append(makeVector(data));

    return result;
}

static Vector<String> toCpp(NSArray<NSString *> *stringVector)
{
    Vector<String> result;
    for (NSString *s in stringVector)
        result.append(s);

    return result;
}

template<typename T>
static Vector<T> toCpp(NSData *data)
{
    return Vector<T> { unsafeMakeSpan(static_cast<const T*>(data.bytes), data.length / sizeof(T)) };
}

template<typename T>
static Vector<Vector<T>> toCpp(NSArray<NSData *> *dataVector)
{
    Vector<Vector<T>> result;
    for (NSData *d in dataVector)
        result.append(toCpp<T>(d));

    return result;
}

static std::optional<WebModel::SkinningData> toCpp(WKBridgeSkinningData* data)
{
    if (!data)
        return std::nullopt;

    return WebModel::SkinningData {
        .influencePerVertexCount = data.influencePerVertexCount,
        .jointTransforms = toCpp<WebModel::Float4x4>(data.jointTransformsData),
        .inverseBindPoses = toCpp<WebModel::Float4x4>(data.inverseBindPosesData),
        .influenceJointIndices = toCpp<uint32_t>(data.influenceJointIndicesData),
        .influenceWeights = toCpp<float>(data.influenceWeightsData),
        .geometryBindTransform = data.geometryBindTransform
    };
}

static std::optional<WebModel::BlendShapeData> toCpp(WKBridgeBlendShapeData* data)
{
    if (!data)
        return std::nullopt;

    return WebModel::BlendShapeData {
        .weights = toCpp<float>(data.weightsData),
        .positionOffsets = toCpp<WebModel::Float3>(data.positionOffsetsData),
        .normalOffsets = toCpp<WebModel::Float3>(data.normalOffsetsData)
    };
}

static std::optional<WebModel::RenormalizationData> toCpp(WKBridgeRenormalizationData* data)
{
    if (!data)
        return std::nullopt;

    return WebModel::RenormalizationData {
        .vertexIndicesPerTriangle = toCpp<uint32_t>(data.vertexIndicesPerTriangleData),
        .vertexAdjacencies = toCpp<uint32_t>(data.vertexAdjacenciesData),
        .vertexAdjacencyEndIndices = toCpp<uint32_t>(data.vertexAdjacencyEndIndicesData)
    };
}

static std::optional<WebModel::DeformationData> toCpp(WKBridgeDeformationData* data)
{
    if (!data)
        return std::nullopt;

    return WebModel::DeformationData {
        .skinningData = toCpp(data.skinningData),
        .blendShapeData = toCpp(data.blendShapeData),
        .renormalizationData = toCpp(data.renormalizationData)
    };
}

static WebModel::UpdateMeshDescriptor toCpp(WKBridgeUpdateMesh *update)
{
    return WebModel::UpdateMeshDescriptor {
        .identifier = update.identifier,
        .updateType = static_cast<uint8_t>(update.updateType),
        .descriptor = toCpp(update.descriptor),
        .parts = toCpp(update.parts),
        .indexData = makeVector(update.indexData),
        .vertexData = toCpp(update.vertexData),
        .instanceTransforms = toCpp<WebModel::Float4x4>(update.instanceTransformsData),
        .materialPrims = toCpp(update.materialPrims),
        .deformationData = toCpp(update.deformationData)
    };
}

static WebModel::NodeType toCpp(WKBridgeNodeType nodeType)
{
    switch (nodeType) {
    case WKBridgeNodeTypeBuiltin:
        return WebModel::NodeType::Builtin;
    case WKBridgeNodeTypeConstant:
        return WebModel::NodeType::Constant;
    case WKBridgeNodeTypeArguments:
        return WebModel::NodeType::Arguments;
    default:
    case WKBridgeNodeTypeResults:
        return WebModel::NodeType::Results;
    }
}

static WebModel::Builtin toCpp(WKBridgeBuiltin *builtin)
{
    return WebModel::Builtin {
        .definition = builtin.definition,
        .name = builtin.name
    };
}

static WebModel::Constant toCpp(WKBridgeConstant constant)
{
    switch (constant) {
    case WKBridgeConstantBool:
        return WebModel::Constant::kBool;
    case WKBridgeConstantUchar:
        return WebModel::Constant::kUchar;
    case WKBridgeConstantInt:
        return WebModel::Constant::kInt;
    case WKBridgeConstantUint:
        return WebModel::Constant::kUint;
    case WKBridgeConstantHalf:
        return WebModel::Constant::kHalf;
    case WKBridgeConstantFloat:
        return WebModel::Constant::kFloat;
    case WKBridgeConstantTimecode:
        return WebModel::Constant::kTimecode;
    case WKBridgeConstantString:
        return WebModel::Constant::kString;
    case WKBridgeConstantToken:
        return WebModel::Constant::kToken;
    case WKBridgeConstantAsset:
        return WebModel::Constant::kAsset;
    case WKBridgeConstantMatrix2f:
        return WebModel::Constant::kMatrix2f;
    case WKBridgeConstantMatrix3f:
        return WebModel::Constant::kMatrix3f;
    case WKBridgeConstantMatrix4f:
        return WebModel::Constant::kMatrix4f;
    case WKBridgeConstantQuatf:
        return WebModel::Constant::kQuatf;
    case WKBridgeConstantQuath:
        return WebModel::Constant::kQuath;
    case WKBridgeConstantFloat2:
        return WebModel::Constant::kFloat2;
    case WKBridgeConstantHalf2:
        return WebModel::Constant::kHalf2;
    case WKBridgeConstantInt2:
        return WebModel::Constant::kInt2;
    case WKBridgeConstantFloat3:
        return WebModel::Constant::kFloat3;
    case WKBridgeConstantHalf3:
        return WebModel::Constant::kHalf3;
    case WKBridgeConstantInt3:
        return WebModel::Constant::kInt3;
    case WKBridgeConstantFloat4:
        return WebModel::Constant::kFloat4;
    case WKBridgeConstantHalf4:
        return WebModel::Constant::kHalf4;
    case WKBridgeConstantInt4:
        return WebModel::Constant::kInt4;

    case WKBridgeConstantPoint3f:
        return WebModel::Constant::kPoint3f;
    case WKBridgeConstantPoint3h:
        return WebModel::Constant::kPoint3h;
    case WKBridgeConstantNormal3f:
        return WebModel::Constant::kNormal3f;
    case WKBridgeConstantNormal3h:
        return WebModel::Constant::kNormal3h;
    case WKBridgeConstantVector3f:
        return WebModel::Constant::kVector3f;
    case WKBridgeConstantVector3h:
        return WebModel::Constant::kVector3h;
    case WKBridgeConstantColor3f:
        return WebModel::Constant::kColor3f;
    case WKBridgeConstantColor3h:
        return WebModel::Constant::kColor3h;
    case WKBridgeConstantColor4f:
        return WebModel::Constant::kColor4f;
    case WKBridgeConstantColor4h:
        return WebModel::Constant::kColor4h;
    case WKBridgeConstantTexCoord2h:
        return WebModel::Constant::kTexCoord2h;
    case WKBridgeConstantTexCoord2f:
        return WebModel::Constant::kTexCoord2f;
    case WKBridgeConstantTexCoord3h:
        return WebModel::Constant::kTexCoord3h;
    case WKBridgeConstantTexCoord3f:
        return WebModel::Constant::kTexCoord3f;
    }
}

static Vector<WebModel::NumberOrString> toCpp(NSArray<WKBridgeValueString *> *constantValues)
{
    Vector<WebModel::NumberOrString> result;
    result.reserveCapacity(constantValues.count);
    for (WKBridgeValueString* v in constantValues) {
        if (v.isString)
            result.append(String(v.string));
        else
            result.append(v.number.doubleValue);
    }

    return result;
}

static WebModel::ConstantContainer toCpp(WKBridgeConstantContainer *container)
{
    return WebModel::ConstantContainer {
        .constant = toCpp(container.constant),
        .constantValues = toCpp(container.constantValues),
        .name = String(container.name)
    };
}

static WebModel::Node toCpp(WKBridgeNode *node)
{
    return WebModel::Node {
        .bridgeNodeType = toCpp(node.bridgeNodeType),
        .builtin = toCpp(node.builtin),
        .constant = toCpp(node.constant)
    };
}

static WebModel::Edge toCpp(WKBridgeEdge *edge)
{
    return WebModel::Edge {
        .outputNode = String(edge.outputNode),
        .outputPort = String(edge.outputPort),
        .inputNode = String(edge.inputNode),
        .inputPort = String(edge.inputPort)
    };
}

static WebModel::DataType toCpp(WKBridgeDataType type)
{
    switch (type) {
    case WKBridgeDataTypeBool:
        return WebModel::DataType::kBool;
    case WKBridgeDataTypeUchar:
        return WebModel::DataType::kUchar;
    case WKBridgeDataTypeInt:
        return WebModel::DataType::kInt;
    case WKBridgeDataTypeUint:
        return WebModel::DataType::kUint;
    case WKBridgeDataTypeInt2:
        return WebModel::DataType::kInt2;
    case WKBridgeDataTypeInt3:
        return WebModel::DataType::kInt3;
    case WKBridgeDataTypeInt4:
        return WebModel::DataType::kInt4;
    case WKBridgeDataTypeFloat:
        return WebModel::DataType::kFloat;
    case WKBridgeDataTypeColor3f:
        return WebModel::DataType::kColor3f;
    case WKBridgeDataTypeColor3h:
        return WebModel::DataType::kColor3h;
    case WKBridgeDataTypeColor4f:
        return WebModel::DataType::kColor4f;
    case WKBridgeDataTypeColor4h:
        return WebModel::DataType::kColor4h;
    case WKBridgeDataTypeFloat2:
        return WebModel::DataType::kFloat2;
    case WKBridgeDataTypeFloat3:
        return WebModel::DataType::kFloat3;
    case WKBridgeDataTypeFloat4:
        return WebModel::DataType::kFloat4;
    case WKBridgeDataTypeHalf:
        return WebModel::DataType::kHalf;
    case WKBridgeDataTypeHalf2:
        return WebModel::DataType::kHalf2;
    case WKBridgeDataTypeHalf3:
        return WebModel::DataType::kHalf3;
    case WKBridgeDataTypeHalf4:
        return WebModel::DataType::kHalf4;
    case WKBridgeDataTypeMatrix2f:
        return WebModel::DataType::kMatrix2f;
    case WKBridgeDataTypeMatrix3f:
        return WebModel::DataType::kMatrix3f;
    case WKBridgeDataTypeMatrix4f:
        return WebModel::DataType::kMatrix4f;
    case WKBridgeDataTypeMatrix2h:
        return WebModel::DataType::kMatrix2h;
    case WKBridgeDataTypeMatrix3h:
        return WebModel::DataType::kMatrix3h;
    case WKBridgeDataTypeMatrix4h:
        return WebModel::DataType::kMatrix4h;
    case WKBridgeDataTypeQuat:
        return WebModel::DataType::kQuat;
    case WKBridgeDataTypeSurfaceShader:
        return WebModel::DataType::kSurfaceShader;
    case WKBridgeDataTypeGeometryModifier:
        return WebModel::DataType::kGeometryModifier;
    case WKBridgeDataTypePostLightingShader:
        return WebModel::DataType::kPostLightingShader;
    case WKBridgeDataTypeString:
        return WebModel::DataType::kString;
    case WKBridgeDataTypeToken:
        return WebModel::DataType::kToken;
    case WKBridgeDataTypeAsset:
        return WebModel::DataType::kAsset;
    default:
        RELEASE_ASSERT_NOT_REACHED("USD file is corrupt");
    }
}

static WebModel::InputOutput toCpp(WKBridgeInputOutput *inputOutput)
{
    std::optional<WebModel::DataType> semanticType;
    if (inputOutput.hasSemanticType)
        semanticType = toCpp(inputOutput.semanticType);

    std::optional<WebModel::ConstantContainer> defaultValue;
    if (inputOutput.defaultValue)
        defaultValue = toCpp(inputOutput.defaultValue);

    return WebModel::InputOutput {
        .type = toCpp(inputOutput.type),
        .name = String(inputOutput.name),
        .semanticType = semanticType,
        .defaultValue = defaultValue
    };
}

template<typename T, typename U>
static Vector<U> toCpp(NSArray<T *> *nsArray)
{
    Vector<U> result;
    result.reserveCapacity(nsArray.count);
    for (T *v in nsArray)
        result.append(toCpp(v));

    return result;
}

static WebModel::MaterialGraph toCpp(WKBridgeMaterialGraph *materialGraph)
{
    return WebModel::MaterialGraph {
        .nodes = toCpp<WKBridgeNode, WebModel::Node>(materialGraph.nodes),
        .edges = toCpp<WKBridgeEdge, WebModel::Edge>(materialGraph.edges),
        .arguments = toCpp(materialGraph.arguments),
        .results = toCpp(materialGraph.results),
        .inputs = toCpp<WKBridgeInputOutput, WebModel::InputOutput>(materialGraph.inputs),
        .outputs = toCpp<WKBridgeInputOutput, WebModel::InputOutput>(materialGraph.outputs),
    };
}

static WebModel::ImageAssetSwizzle convert(MTLTextureSwizzleChannels swizzle)
{
    return WebModel::ImageAssetSwizzle {
        .red = swizzle.red,
        .green = swizzle.green,
        .blue = swizzle.blue,
        .alpha = swizzle.alpha
    };
}

static WebModel::ImageAsset convert(WKBridgeImageAsset *imageAsset)
{
    return WebModel::ImageAsset {
        .data = makeVector(imageAsset.data),
        .width = imageAsset.width,
        .height = imageAsset.height,
        .depth = 1,
        .bytesPerPixel = imageAsset.bytesPerPixel,
        .textureType = imageAsset.textureType,
        .pixelFormat = imageAsset.pixelFormat,
        .mipmapLevelCount = imageAsset.mipmapLevelCount,
        .arrayLength = imageAsset.arrayLength,
        .textureUsage = imageAsset.textureUsage,
        .swizzle = convert(imageAsset.swizzle)
    };
}

static WebModel::UpdateTextureDescriptor toCpp(WKBridgeUpdateTexture *update)
{
    return WebModel::UpdateTextureDescriptor {
        .imageAsset = convert(update.imageAsset),
        .identifier = update.identifier,
        .hashString = update.hashString
    };
}

static WebModel::UpdateMaterialDescriptor toCpp(WKBridgeUpdateMaterial *update)
{
    return WebModel::UpdateMaterialDescriptor {
        .materialGraph = toCpp(update.materialGraph),
        .identifier = update.identifier
    };
}

}

#endif

