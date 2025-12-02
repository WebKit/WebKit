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

#if ENABLE(GPU_PROCESS_MODEL)

#include <WebCore/DDImageAsset.h>
#include <WebCore/DDMaterialDescriptor.h>
#include <WebCore/DDMesh.h>
#include <WebCore/DDMeshDescriptor.h>
#include <WebCore/DDUpdateMaterialDescriptor.h>
#include <WebCore/DDUpdateMeshDescriptor.h>
#include <WebCore/DDUpdateTextureDescriptor.h>
#include <WebGPU/DDModelTypes.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/cocoa/VectorCocoa.h>

namespace WebCore {

static DDModel::DDVertexAttributeFormat toCpp(DDBridgeVertexAttributeFormat *format)
{
    return DDModel::DDVertexAttributeFormat {
        .semantic = format.semantic,
        .format = format.format,
        .layoutIndex = format.layoutIndex,
        .offset = format.offset
    };
}

static Vector<DDModel::DDVertexAttributeFormat> toCpp(NSArray<DDBridgeVertexAttributeFormat *> *formats)
{
    Vector<DDModel::DDVertexAttributeFormat> result;
    for (DDBridgeVertexAttributeFormat *f in formats)
        result.append(toCpp(f));
    return result;
}

static DDModel::DDVertexLayout toCpp(DDBridgeVertexLayout *layout)
{
    return DDModel::DDVertexLayout {
        .bufferIndex = layout.bufferIndex,
        .bufferOffset = layout.bufferOffset,
        .bufferStride = layout.bufferStride,
    };
}
static Vector<DDModel::DDVertexLayout> toCpp(NSArray<DDBridgeVertexLayout *> *layouts)
{
    Vector<DDModel::DDVertexLayout> result;
    for (DDBridgeVertexLayout *l in layouts)
        result.append(toCpp(l));
    return result;
}

static Vector<DDModel::DDFloat4x4> toCpp(DDBridgeChainedFloat4x4 *input)
{
    Vector<DDModel::DDFloat4x4> result;
    for ( ; input; input = input.next)
        result.append(input.transform);

    return result;
}

static DDModel::DDMeshPart toCpp(DDBridgeMeshPart *part)
{
    return DDModel::DDMeshPart {
        static_cast<uint32_t>(part.indexOffset),
        static_cast<uint32_t>(part.indexCount),
        static_cast<uint32_t>(part.topology),
        static_cast<uint32_t>(part.materialIndex),
        WTFMove(part.boundsMin),
        WTFMove(part.boundsMax)
    };
}

static Vector<DDModel::DDMeshPart> toCpp(NSArray<DDBridgeMeshPart *> *parts)
{
    Vector<DDModel::DDMeshPart> result;
    for (DDBridgeMeshPart *p in parts)
        result.append(toCpp(p));
    return result;
}

static WebCore::DDModel::DDMeshDescriptor toCpp(DDBridgeMeshDescriptor *descriptor)
{
    return WebCore::DDModel::DDMeshDescriptor {
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

static WebCore::DDModel::DDUpdateMeshDescriptor toCpp(DDBridgeUpdateMesh *update)
{
    return WebCore::DDModel::DDUpdateMeshDescriptor {
        .identifier = update.identifier,
        .updateType = static_cast<uint8_t>(update.updateType),
        .descriptor = toCpp(update.descriptor),
        .parts = toCpp(update.parts),
        .indexData = makeVector(update.indexData),
        .vertexData = toCpp(update.vertexData),
        .instanceTransforms = toCpp(update.instanceTransforms),
        .materialPrims = toCpp(update.materialPrims)
    };
}

#if ENABLE(GPU_PROCESS_MODEL_MATERIALS)
static WebCore::DDModel::DDNodeType toCpp(DDBridgeNodeType nodeType)
{
    switch (nodeType) {
    case DDBridgeNodeType::kBuiltin:
        return WebCore::DDModel::DDNodeType::Builtin;
    case DDBridgeNodeType::kConstant:
        return WebCore::DDModel::DDNodeType::Constant;
    case DDBridgeNodeType::kArguments:
        return WebCore::DDModel::DDNodeType::Arguments;
    default:
    case DDBridgeNodeType::kResults:
        return WebCore::DDModel::DDNodeType::Results;
    }
}

static WebCore::DDModel::DDBuiltin toCpp(DDBridgeBuiltin *builtin)
{
    return WebCore::DDModel::DDBuiltin {
        .definition = builtin.definition,
        .name = builtin.name
    };
}

static WebCore::DDModel::DDConstant toCpp(DDBridgeConstant constant)
{
    switch (constant) {
    case DDBridgeConstant::kBool:
        return WebCore::DDModel::DDConstant::kBool;
    case DDBridgeConstant::kUchar:
        return WebCore::DDModel::DDConstant::kUchar;
    case DDBridgeConstant::kInt:
        return WebCore::DDModel::DDConstant::kInt;
    case DDBridgeConstant::kUint:
        return WebCore::DDModel::DDConstant::kUint;
    case DDBridgeConstant::kHalf:
        return WebCore::DDModel::DDConstant::kHalf;
    case DDBridgeConstant::kFloat:
        return WebCore::DDModel::DDConstant::kFloat;
    case DDBridgeConstant::kTimecode:
        return WebCore::DDModel::DDConstant::kTimecode;
    case DDBridgeConstant::kString:
        return WebCore::DDModel::DDConstant::kString;
    case DDBridgeConstant::kToken:
        return WebCore::DDModel::DDConstant::kToken;
    case DDBridgeConstant::kAsset:
        return WebCore::DDModel::DDConstant::kAsset;
    case DDBridgeConstant::kMatrix2f:
        return WebCore::DDModel::DDConstant::kMatrix2f;
    case DDBridgeConstant::kMatrix3f:
        return WebCore::DDModel::DDConstant::kMatrix3f;
    case DDBridgeConstant::kMatrix4f:
        return WebCore::DDModel::DDConstant::kMatrix4f;
    case DDBridgeConstant::kQuatf:
        return WebCore::DDModel::DDConstant::kQuatf;
    case DDBridgeConstant::kQuath:
        return WebCore::DDModel::DDConstant::kQuath;
    case DDBridgeConstant::kFloat2:
        return WebCore::DDModel::DDConstant::kFloat2;
    case DDBridgeConstant::kHalf2:
        return WebCore::DDModel::DDConstant::kHalf2;
    case DDBridgeConstant::kInt2:
        return WebCore::DDModel::DDConstant::kInt2;
    case DDBridgeConstant::kFloat3:
        return WebCore::DDModel::DDConstant::kFloat3;
    case DDBridgeConstant::kHalf3:
        return WebCore::DDModel::DDConstant::kHalf3;
    case DDBridgeConstant::kInt3:
        return WebCore::DDModel::DDConstant::kInt3;
    case DDBridgeConstant::kFloat4:
        return WebCore::DDModel::DDConstant::kFloat4;
    case DDBridgeConstant::kHalf4:
        return WebCore::DDModel::DDConstant::kHalf4;
    case DDBridgeConstant::kInt4:
        return WebCore::DDModel::DDConstant::kInt4;

    case DDBridgeConstant::kPoint3f:
        return WebCore::DDModel::DDConstant::kPoint3f;
    case DDBridgeConstant::kPoint3h:
        return WebCore::DDModel::DDConstant::kPoint3h;
    case DDBridgeConstant::kNormal3f:
        return WebCore::DDModel::DDConstant::kNormal3f;
    case DDBridgeConstant::kNormal3h:
        return WebCore::DDModel::DDConstant::kNormal3h;
    case DDBridgeConstant::kVector3f:
        return WebCore::DDModel::DDConstant::kVector3f;
    case DDBridgeConstant::kVector3h:
        return WebCore::DDModel::DDConstant::kVector3h;
    case DDBridgeConstant::kColor3f:
        return WebCore::DDModel::DDConstant::kColor3f;
    case DDBridgeConstant::kColor3h:
        return WebCore::DDModel::DDConstant::kColor3h;
    case DDBridgeConstant::kColor4f:
        return WebCore::DDModel::DDConstant::kColor4f;
    case DDBridgeConstant::kColor4h:
        return WebCore::DDModel::DDConstant::kColor4h;
    case DDBridgeConstant::kTexCoord2h:
        return WebCore::DDModel::DDConstant::kTexCoord2h;
    case DDBridgeConstant::kTexCoord2f:
        return WebCore::DDModel::DDConstant::kTexCoord2f;
    case DDBridgeConstant::kTexCoord3h:
        return WebCore::DDModel::DDConstant::kTexCoord3h;
    case DDBridgeConstant::kTexCoord3f:
        return WebCore::DDModel::DDConstant::kTexCoord3f;
    }
}

static Vector<WebCore::DDModel::DDNumberOrString> toCpp(NSArray<DDValueString *> *constantValues)
{
    Vector<WebCore::DDModel::DDNumberOrString> result;
    result.reserveCapacity(constantValues.count);
    for (DDValueString *v in constantValues) {
        if (v.string.length)
            result.append(v.string);
        else
            result.append(v.number.doubleValue);
    }

    return result;
}

static WebCore::DDModel::DDConstantContainer toCpp(DDBridgeConstantContainer *container)
{
    return WebCore::DDModel::DDConstantContainer {
        .constant = toCpp(container.constant),
        .constantValues = toCpp(container.constantValues),
        .name = toCpp(container.name)
    };
}

static WebCore::DDModel::DDNode toCpp(DDBridgeNode *node)
{
    return WebCore::DDModel::DDNode {
        .bridgeNodeType = toCpp(node.bridgeNodeType),
        .builtin = toCpp(node.builtin),
        .constant = toCpp(node.constant)
    };
}

static WebCore::DDModel::DDEdge toCpp(DDBridgeEdge *edge)
{
    return WebCore::DDModel::DDEdge {
        .upstreamNodeIndex = edge.upstreamNodeIndex,
        .downstreamNodeIndex = edge.downstreamNodeIndex,
        .upstreamOutputName = toCpp(edge.upstreamOutputName),
        .downstreamInputName = toCpp(edge.downstreamInputName)
    };
}

static WebCore::DDModel::DDDataType toCpp(DDBridgeDataType type)
{
    switch (type) {
    case DDBridgeDataType::kBool:
        return WebCore::DDModel::DDDataType::kBool;
    case DDBridgeDataType::kInt:
        return WebCore::DDModel::DDDataType::kInt;
    case DDBridgeDataType::kInt2:
        return WebCore::DDModel::DDDataType::kInt2;
    case DDBridgeDataType::kInt3:
        return WebCore::DDModel::DDDataType::kInt3;
    case DDBridgeDataType::kInt4:
        return WebCore::DDModel::DDDataType::kInt4;
    case DDBridgeDataType::kFloat:
        return WebCore::DDModel::DDDataType::kFloat;
    case DDBridgeDataType::kColor3f:
        return WebCore::DDModel::DDDataType::kColor3f;
    case DDBridgeDataType::kColor3h:
        return WebCore::DDModel::DDDataType::kColor3h;
    case DDBridgeDataType::kColor4f:
        return WebCore::DDModel::DDDataType::kColor4f;
    case DDBridgeDataType::kColor4h:
        return WebCore::DDModel::DDDataType::kColor4h;
    case DDBridgeDataType::kFloat2:
        return WebCore::DDModel::DDDataType::kFloat2;
    case DDBridgeDataType::kFloat3:
        return WebCore::DDModel::DDDataType::kFloat3;
    case DDBridgeDataType::kFloat4:
        return WebCore::DDModel::DDDataType::kFloat4;
    case DDBridgeDataType::kHalf:
        return WebCore::DDModel::DDDataType::kHalf;
    case DDBridgeDataType::kHalf2:
        return WebCore::DDModel::DDDataType::kHalf2;
    case DDBridgeDataType::kHalf3:
        return WebCore::DDModel::DDDataType::kHalf3;
    case DDBridgeDataType::kHalf4:
        return WebCore::DDModel::DDDataType::kHalf4;
    case DDBridgeDataType::kMatrix2f:
        return WebCore::DDModel::DDDataType::kMatrix2f;
    case DDBridgeDataType::kMatrix3f:
        return WebCore::DDModel::DDDataType::kMatrix3f;
    case DDBridgeDataType::kMatrix4f:
        return WebCore::DDModel::DDDataType::kMatrix4f;
    case DDBridgeDataType::kSurfaceShader:
        return WebCore::DDModel::DDDataType::kSurfaceShader;
    case DDBridgeDataType::kGeometryModifier:
        return WebCore::DDModel::DDDataType::kGeometryModifier;
    case DDBridgeDataType::kString:
        return WebCore::DDModel::DDDataType::kString;
    case DDBridgeDataType::kToken:
        return WebCore::DDModel::DDDataType::kToken;
    case DDBridgeDataType::kAsset:
        return WebCore::DDModel::DDDataType::kAsset;
    default:
        RELEASE_ASSERT_NOT_REACHED("USD file is corrupt");
    }
}

static WebCore::DDModel::DDInputOutput toCpp(DDBridgeInputOutput *inputOutput)
{
    return WebCore::DDModel::DDInputOutput {
        .type = toCpp(inputOutput.type),
        .name = toCpp(inputOutput.name)
    };
}

static WebCore::DDModel::DDPrimvar toCpp(DDBridgePrimvar *primvar)
{
    return WebCore::DDModel::DDPrimvar {
        .name = toCpp(primvar.name),
        .referencedGeomPropName = toCpp(primvar.referencedGeomPropName),
        .attributeFormat = primvar.attributeFormat
    };
}

static WebCore::DDModel::DDMaterialGraph toCpp(DDBridgeMaterialGraph *)
{
    return WebCore::DDModel::DDMaterialGraph {
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

static WebCore::DDModel::DDImageAssetSwizzle convert(MTLTextureSwizzleChannels swizzle)
{
    return WebCore::DDModel::DDImageAssetSwizzle {
        .red = swizzle.red,
        .green = swizzle.green,
        .blue = swizzle.blue,
        .alpha = swizzle.alpha
    };
}

static WebCore::DDModel::DDImageAsset convert(DDBridgeImageAsset *imageAsset)
{
    RetainPtr imageSource = adoptCF(CGImageSourceCreateWithData((CFDataRef)imageAsset.data, nullptr));
    auto platformImage = adoptCF(CGImageSourceCreateImageAtIndex(imageSource.get(), 0, nullptr));
    RetainPtr pixelDataCfData = adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(platformImage.get())));
    auto byteSpan = span(pixelDataCfData.get());

    auto width = CGImageGetWidth(platformImage.get());
    auto height = CGImageGetHeight(platformImage.get());
    auto bytesPerPixel = (int)(byteSpan.size() / (width * height));

    return WebCore::DDModel::DDImageAsset {
        .data = Vector<uint8_t> { byteSpan },
        .width = static_cast<long>(width),
        .height = static_cast<long>(height),
        .depth = 1,
        .bytesPerPixel = bytesPerPixel,
        .textureType = imageAsset.textureType,
        .pixelFormat = imageAsset.pixelFormat,
        .mipmapLevelCount = imageAsset.mipmapLevelCount,
        .arrayLength = imageAsset.arrayLength,
        .textureUsage = imageAsset.textureUsage,
        .swizzle = convert(imageAsset.swizzle)
    };
}

static WebCore::DDModel::DDUpdateTextureDescriptor toCpp(DDBridgeUpdateTexture *update)
{
    return WebCore::DDModel::DDUpdateTextureDescriptor {
        .imageAsset = convert(update.imageAsset),
        .identifier = update.identifier,
        .hashString = update.hashString
    };
}

static WebCore::DDModel::DDUpdateMaterialDescriptor toCpp(DDBridgeUpdateMaterial *update)
{
    return WebCore::DDModel::DDUpdateMaterialDescriptor {
        .materialGraph = makeVector(update.materialGraph),
        .identifier = update.identifier
    };
}

}

#endif

