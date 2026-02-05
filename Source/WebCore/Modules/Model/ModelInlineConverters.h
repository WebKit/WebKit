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

#include <ImageIO/CGImageSource.h>
#include <WebCore/WebModel.h>
#include <WebGPU/ModelTypes.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/cocoa/VectorCocoa.h>

namespace WebCore {

static WebModel::VertexAttributeFormat toCpp(WebBridgeVertexAttributeFormat *format)
{
    return WebModel::VertexAttributeFormat {
        .semantic = format.semantic,
        .format = format.format,
        .layoutIndex = format.layoutIndex,
        .offset = format.offset
    };
}

static Vector<WebModel::VertexAttributeFormat> toCpp(NSArray<WebBridgeVertexAttributeFormat *> *formats)
{
    Vector<WebModel::VertexAttributeFormat> result;
    for (WebBridgeVertexAttributeFormat *f in formats)
        result.append(toCpp(f));
    return result;
}

static WebModel::VertexLayout toCpp(WebBridgeVertexLayout *layout)
{
    return WebModel::VertexLayout {
        .bufferIndex = layout.bufferIndex,
        .bufferOffset = layout.bufferOffset,
        .bufferStride = layout.bufferStride,
    };
}
static Vector<WebModel::VertexLayout> toCpp(NSArray<WebBridgeVertexLayout *> *layouts)
{
    Vector<WebModel::VertexLayout> result;
    for (WebBridgeVertexLayout *l in layouts)
        result.append(toCpp(l));
    return result;
}

static WebModel::MeshPart toCpp(WebBridgeMeshPart *part)
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

static Vector<WebModel::MeshPart> toCpp(NSArray<WebBridgeMeshPart *> *parts)
{
    Vector<WebModel::MeshPart> result;
    for (WebBridgeMeshPart *p in parts)
        result.append(toCpp(p));
    return result;
}

static WebModel::MeshDescriptor toCpp(WebBridgeMeshDescriptor *descriptor)
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

static std::optional<WebModel::SkinningData> toCpp(WebBridgeSkinningData* data)
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

static std::optional<WebModel::BlendShapeData> toCpp(WebBridgeBlendShapeData* data)
{
    if (!data)
        return std::nullopt;

    return WebModel::BlendShapeData {
        .weights = toCpp<float>(data.weights),
        .positionOffsets = toCpp<WebModel::Float3>(data.positionOffsets),
        .normalOffsets = toCpp<WebModel::Float3>(data.normalOffsets)
    };
}

static std::optional<WebModel::RenormalizationData> toCpp(WebBridgeRenormalizationData* data)
{
    if (!data)
        return std::nullopt;

    return WebModel::RenormalizationData {
        .vertexIndicesPerTriangle = toCpp<uint32_t>(data.vertexIndicesPerTriangle),
        .vertexAdjacencies = toCpp<uint32_t>(data.vertexAdjacencies),
        .vertexAdjacencyEndIndices = toCpp<uint32_t>(data.vertexAdjacencyEndIndices)
    };
}

static std::optional<WebModel::DeformationData> toCpp(WebBridgeDeformationData* data)
{
    if (!data)
        return std::nullopt;

    return WebModel::DeformationData {
        .skinningData = toCpp(data.skinningData),
        .blendShapeData = toCpp(data.blendShapeData),
        .renormalizationData = toCpp(data.renormalizationData)
    };
}

static WebModel::UpdateMeshDescriptor toCpp(WebBridgeUpdateMesh *update)
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

#if ENABLE(GPU_PROCESS_MODEL_MATERIALS)
static WebModel::NodeType toCpp(WebBridgeNodeType nodeType)
{
    switch (nodeType) {
    case WebBridgeNodeType::kBuiltin:
        return WebModel::NodeType::Builtin;
    case WebBridgeNodeType::kConstant:
        return WebModel::NodeType::Constant;
    case WebBridgeNodeType::kArguments:
        return WebModel::NodeType::Arguments;
    default:
    case WebBridgeNodeType::kResults:
        return WebModel::NodeType::Results;
    }
}

static WebModel::Builtin toCpp(WebBridgeBuiltin *builtin)
{
    return WebModel::Builtin {
        .definition = builtin.definition,
        .name = builtin.name
    };
}

static WebModel::Constant toCpp(WebBridgeConstant constant)
{
    switch (constant) {
    case WebBridgeConstant::kBool:
        return WebModel::Constant::kBool;
    case WebBridgeConstant::kUchar:
        return WebModel::Constant::kUchar;
    case WebBridgeConstant::kInt:
        return WebModel::Constant::kInt;
    case WebBridgeConstant::kUint:
        return WebModel::Constant::kUint;
    case WebBridgeConstant::kHalf:
        return WebModel::Constant::kHalf;
    case WebBridgeConstant::kFloat:
        return WebModel::Constant::kFloat;
    case WebBridgeConstant::kTimecode:
        return WebModel::Constant::kTimecode;
    case WebBridgeConstant::kString:
        return WebModel::Constant::kString;
    case WebBridgeConstant::kToken:
        return WebModel::Constant::kToken;
    case WebBridgeConstant::kAsset:
        return WebModel::Constant::kAsset;
    case WebBridgeConstant::kMatrix2f:
        return WebModel::Constant::kMatrix2f;
    case WebBridgeConstant::kMatrix3f:
        return WebModel::Constant::kMatrix3f;
    case WebBridgeConstant::kMatrix4f:
        return WebModel::Constant::kMatrix4f;
    case WebBridgeConstant::kQuatf:
        return WebModel::Constant::kQuatf;
    case WebBridgeConstant::kQuath:
        return WebModel::Constant::kQuath;
    case WebBridgeConstant::kFloat2:
        return WebModel::Constant::kFloat2;
    case WebBridgeConstant::kHalf2:
        return WebModel::Constant::kHalf2;
    case WebBridgeConstant::kInt2:
        return WebModel::Constant::kInt2;
    case WebBridgeConstant::kFloat3:
        return WebModel::Constant::kFloat3;
    case WebBridgeConstant::kHalf3:
        return WebModel::Constant::kHalf3;
    case WebBridgeConstant::kInt3:
        return WebModel::Constant::kInt3;
    case WebBridgeConstant::kFloat4:
        return WebModel::Constant::kFloat4;
    case WebBridgeConstant::kHalf4:
        return WebModel::Constant::kHalf4;
    case WebBridgeConstant::kInt4:
        return WebModel::Constant::kInt4;

    case WebBridgeConstant::kPoint3f:
        return WebModel::Constant::kPoint3f;
    case WebBridgeConstant::kPoint3h:
        return WebModel::Constant::kPoint3h;
    case WebBridgeConstant::kNormal3f:
        return WebModel::Constant::kNormal3f;
    case WebBridgeConstant::kNormal3h:
        return WebModel::Constant::kNormal3h;
    case WebBridgeConstant::kVector3f:
        return WebModel::Constant::kVector3f;
    case WebBridgeConstant::kVector3h:
        return WebModel::Constant::kVector3h;
    case WebBridgeConstant::kColor3f:
        return WebModel::Constant::kColor3f;
    case WebBridgeConstant::kColor3h:
        return WebModel::Constant::kColor3h;
    case WebBridgeConstant::kColor4f:
        return WebModel::Constant::kColor4f;
    case WebBridgeConstant::kColor4h:
        return WebModel::Constant::kColor4h;
    case WebBridgeConstant::kTexCoord2h:
        return WebModel::Constant::kTexCoord2h;
    case WebBridgeConstant::kTexCoord2f:
        return WebModel::Constant::kTexCoord2f;
    case WebBridgeConstant::kTexCoord3h:
        return WebModel::Constant::kTexCoord3h;
    case WebBridgeConstant::kTexCoord3f:
        return WebModel::Constant::kTexCoord3f;
    }
}

static Vector<WebModel::NumberOrString> toCpp(NSArray<DDValueString *> *constantValues)
{
    Vector<WebModel::NumberOrString> result;
    result.reserveCapacity(constantValues.count);
    for (DDValueString *v in constantValues) {
        if (v.string.length)
            result.append(v.string);
        else
            result.append(v.number.doubleValue);
    }

    return result;
}

static WebModel::ConstantContainer toCpp(WebBridgeConstantContainer *container)
{
    return WebModel::ConstantContainer {
        .constant = toCpp(container.constant),
        .constantValues = toCpp(container.constantValues),
        .name = toCpp(container.name)
    };
}

static WebModel::Node toCpp(WebBridgeNode *node)
{
    return WebModel::Node {
        .bridgeNodeType = toCpp(node.bridgeNodeType),
        .builtin = toCpp(node.builtin),
        .constant = toCpp(node.constant)
    };
}

static WebModel::Edge toCpp(WebBridgeEdge *edge)
{
    return WebModel::Edge {
        .upstreamNodeIndex = edge.upstreamNodeIndex,
        .downstreamNodeIndex = edge.downstreamNodeIndex,
        .upstreamOutputName = toCpp(edge.upstreamOutputName),
        .downstreamInputName = toCpp(edge.downstreamInputName)
    };
}

static WebModel::DataType toCpp(WebBridgeDataType type)
{
    switch (type) {
    case WebBridgeDataType::kBool:
        return WebModel::DataType::kBool;
    case WebBridgeDataType::kInt:
        return WebModel::DataType::kInt;
    case WebBridgeDataType::kInt2:
        return WebModel::DataType::kInt2;
    case WebBridgeDataType::kInt3:
        return WebModel::DataType::kInt3;
    case WebBridgeDataType::kInt4:
        return WebModel::DataType::kInt4;
    case WebBridgeDataType::kFloat:
        return WebModel::DataType::kFloat;
    case WebBridgeDataType::kColor3f:
        return WebModel::DataType::kColor3f;
    case WebBridgeDataType::kColor3h:
        return WebModel::DataType::kColor3h;
    case WebBridgeDataType::kColor4f:
        return WebModel::DataType::kColor4f;
    case WebBridgeDataType::kColor4h:
        return WebModel::DataType::kColor4h;
    case WebBridgeDataType::kFloat2:
        return WebModel::DataType::kFloat2;
    case WebBridgeDataType::kFloat3:
        return WebModel::DataType::kFloat3;
    case WebBridgeDataType::kFloat4:
        return WebModel::DataType::kFloat4;
    case WebBridgeDataType::kHalf:
        return WebModel::DataType::kHalf;
    case WebBridgeDataType::kHalf2:
        return WebModel::DataType::kHalf2;
    case WebBridgeDataType::kHalf3:
        return WebModel::DataType::kHalf3;
    case WebBridgeDataType::kHalf4:
        return WebModel::DataType::kHalf4;
    case WebBridgeDataType::kMatrix2f:
        return WebModel::DataType::kMatrix2f;
    case WebBridgeDataType::kMatrix3f:
        return WebModel::DataType::kMatrix3f;
    case WebBridgeDataType::kMatrix4f:
        return WebModel::DataType::kMatrix4f;
    case WebBridgeDataType::kSurfaceShader:
        return WebModel::DataType::kSurfaceShader;
    case WebBridgeDataType::kGeometryModifier:
        return WebModel::DataType::kGeometryModifier;
    case WebBridgeDataType::kString:
        return WebModel::DataType::kString;
    case WebBridgeDataType::kToken:
        return WebModel::DataType::kToken;
    case WebBridgeDataType::kAsset:
        return WebModel::DataType::kAsset;
    default:
        RELEASE_ASSERT_NOT_REACHED("USD file is corrupt");
    }
}

static WebModel::InputOutput toCpp(WebBridgeInputOutput *inputOutput)
{
    return WebModel::InputOutput {
        .type = toCpp(inputOutput.type),
        .name = toCpp(inputOutput.name)
    };
}

static WebModel::Primvar toCpp(WebBridgePrimvar *primvar)
{
    return WebModel::Primvar {
        .name = toCpp(primvar.name),
        .referencedGeomPropName = toCpp(primvar.referencedGeomPropName),
        .attributeFormat = primvar.attributeFormat
    };
}

static WebModel::MaterialGraph toCpp(WebBridgeMaterialGraph *)
{
    return WebModel::MaterialGraph {
    };
}
#endif

template<typename T, typename U>
static Vector<U> toCpp(NSArray<T *> *nsArray)
{
    Vector<U> result;
    result.reserveCapacity(nsArray.count);
    for (T *v in nsArray)
        result.append(toCpp(v));

    return result;
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

static WebModel::ImageAsset convert(WebBridgeImageAsset *imageAsset)
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

static WebModel::UpdateTextureDescriptor toCpp(WebBridgeUpdateTexture *update)
{
    return WebModel::UpdateTextureDescriptor {
        .imageAsset = convert(update.imageAsset),
        .identifier = update.identifier,
        .hashString = update.hashString
    };
}

static WebModel::UpdateMaterialDescriptor toCpp(WebBridgeUpdateMaterial *update)
{
    return WebModel::UpdateMaterialDescriptor {
        .materialGraph = makeVector(update.materialGraph),
        .identifier = update.identifier
    };
}

}

#endif

