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

#if ENABLE(GPU_PROCESS_MODEL_MATERIALS)
static WebModel::NodeType toCpp(WKBridgeNodeType nodeType)
{
    switch (nodeType) {
    case WKBridgeNodeType::kBuiltin:
        return WebModel::NodeType::Builtin;
    case WKBridgeNodeType::kConstant:
        return WebModel::NodeType::Constant;
    case WKBridgeNodeType::kArguments:
        return WebModel::NodeType::Arguments;
    default:
    case WKBridgeNodeType::kResults:
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
    case WKBridgeConstant::kBool:
        return WebModel::Constant::kBool;
    case WKBridgeConstant::kUchar:
        return WebModel::Constant::kUchar;
    case WKBridgeConstant::kInt:
        return WebModel::Constant::kInt;
    case WKBridgeConstant::kUint:
        return WebModel::Constant::kUint;
    case WKBridgeConstant::kHalf:
        return WebModel::Constant::kHalf;
    case WKBridgeConstant::kFloat:
        return WebModel::Constant::kFloat;
    case WKBridgeConstant::kTimecode:
        return WebModel::Constant::kTimecode;
    case WKBridgeConstant::kString:
        return WebModel::Constant::kString;
    case WKBridgeConstant::kToken:
        return WebModel::Constant::kToken;
    case WKBridgeConstant::kAsset:
        return WebModel::Constant::kAsset;
    case WKBridgeConstant::kMatrix2f:
        return WebModel::Constant::kMatrix2f;
    case WKBridgeConstant::kMatrix3f:
        return WebModel::Constant::kMatrix3f;
    case WKBridgeConstant::kMatrix4f:
        return WebModel::Constant::kMatrix4f;
    case WKBridgeConstant::kQuatf:
        return WebModel::Constant::kQuatf;
    case WKBridgeConstant::kQuath:
        return WebModel::Constant::kQuath;
    case WKBridgeConstant::kFloat2:
        return WebModel::Constant::kFloat2;
    case WKBridgeConstant::kHalf2:
        return WebModel::Constant::kHalf2;
    case WKBridgeConstant::kInt2:
        return WebModel::Constant::kInt2;
    case WKBridgeConstant::kFloat3:
        return WebModel::Constant::kFloat3;
    case WKBridgeConstant::kHalf3:
        return WebModel::Constant::kHalf3;
    case WKBridgeConstant::kInt3:
        return WebModel::Constant::kInt3;
    case WKBridgeConstant::kFloat4:
        return WebModel::Constant::kFloat4;
    case WKBridgeConstant::kHalf4:
        return WebModel::Constant::kHalf4;
    case WKBridgeConstant::kInt4:
        return WebModel::Constant::kInt4;

    case WKBridgeConstant::kPoint3f:
        return WebModel::Constant::kPoint3f;
    case WKBridgeConstant::kPoint3h:
        return WebModel::Constant::kPoint3h;
    case WKBridgeConstant::kNormal3f:
        return WebModel::Constant::kNormal3f;
    case WKBridgeConstant::kNormal3h:
        return WebModel::Constant::kNormal3h;
    case WKBridgeConstant::kVector3f:
        return WebModel::Constant::kVector3f;
    case WKBridgeConstant::kVector3h:
        return WebModel::Constant::kVector3h;
    case WKBridgeConstant::kColor3f:
        return WebModel::Constant::kColor3f;
    case WKBridgeConstant::kColor3h:
        return WebModel::Constant::kColor3h;
    case WKBridgeConstant::kColor4f:
        return WebModel::Constant::kColor4f;
    case WKBridgeConstant::kColor4h:
        return WebModel::Constant::kColor4h;
    case WKBridgeConstant::kTexCoord2h:
        return WebModel::Constant::kTexCoord2h;
    case WKBridgeConstant::kTexCoord2f:
        return WebModel::Constant::kTexCoord2f;
    case WKBridgeConstant::kTexCoord3h:
        return WebModel::Constant::kTexCoord3h;
    case WKBridgeConstant::kTexCoord3f:
        return WebModel::Constant::kTexCoord3f;
    }
}

static WebModel::DataType toCpp(WKBridgeDataType type)
{
    switch (type) {
    case WKBridgeDataType::kBool:
        return WebModel::DataType::kBool;
    case WKBridgeDataType::kInt:
        return WebModel::DataType::kInt;
    case WKBridgeDataType::kInt2:
        return WebModel::DataType::kInt2;
    case WKBridgeDataType::kInt3:
        return WebModel::DataType::kInt3;
    case WKBridgeDataType::kInt4:
        return WebModel::DataType::kInt4;
    case WKBridgeDataType::kFloat:
        return WebModel::DataType::kFloat;
    case WKBridgeDataType::kColor3f:
        return WebModel::DataType::kColor3f;
    case WKBridgeDataType::kColor3h:
        return WebModel::DataType::kColor3h;
    case WKBridgeDataType::kColor4f:
        return WebModel::DataType::kColor4f;
    case WKBridgeDataType::kColor4h:
        return WebModel::DataType::kColor4h;
    case WKBridgeDataType::kFloat2:
        return WebModel::DataType::kFloat2;
    case WKBridgeDataType::kFloat3:
        return WebModel::DataType::kFloat3;
    case WKBridgeDataType::kFloat4:
        return WebModel::DataType::kFloat4;
    case WKBridgeDataType::kHalf:
        return WebModel::DataType::kHalf;
    case WKBridgeDataType::kHalf2:
        return WebModel::DataType::kHalf2;
    case WKBridgeDataType::kHalf3:
        return WebModel::DataType::kHalf3;
    case WKBridgeDataType::kHalf4:
        return WebModel::DataType::kHalf4;
    case WKBridgeDataType::kMatrix2f:
        return WebModel::DataType::kMatrix2f;
    case WKBridgeDataType::kMatrix3f:
        return WebModel::DataType::kMatrix3f;
    case WKBridgeDataType::kMatrix4f:
        return WebModel::DataType::kMatrix4f;
    case WKBridgeDataType::kSurfaceShader:
        return WebModel::DataType::kSurfaceShader;
    case WKBridgeDataType::kGeometryModifier:
        return WebModel::DataType::kGeometryModifier;
    case WKBridgeDataType::kString:
        return WebModel::DataType::kString;
    case WKBridgeDataType::kToken:
        return WebModel::DataType::kToken;
    case WKBridgeDataType::kAsset:
        return WebModel::DataType::kAsset;
    default:
        RELEASE_ASSERT_NOT_REACHED("USD file is corrupt");
    }
}

static WebModel::InputOutput toCpp(WKBridgeInputOutput *inputOutput)
{
    return WebModel::InputOutput {
        .type = toCpp(inputOutput.type),
        .name = toCpp(inputOutput.name)
    };
}

static WebModel::Primvar toCpp(WKBridgePrimvar *primvar)
{
    return WebModel::Primvar {
        .name = toCpp(primvar.name),
        .referencedGeomPropName = toCpp(primvar.referencedGeomPropName),
        .attributeFormat = primvar.attributeFormat
    };
}

static WebModel::MaterialGraph toCpp(WKBridgeMaterialGraph *)
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
        .materialGraph = makeVector(update.materialGraph),
        .identifier = update.identifier
    };
}

}

#endif

