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

#include "config.h"
#include "DDMeshImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "DDMaterialDescriptor.h"
#include "DDMeshDescriptor.h"
#include "DDUpdateMaterialDescriptor.h"
#include "DDUpdateMeshDescriptor.h"
#include "DDUpdateTextureDescriptor.h"
#include "ModelConvertToBackingContext.h"
#include "ModelDDTypes.h"
#include <WebGPU/DDModelTypes.h>
#include <WebGPU/WebGPUExt.h>
#include <wtf/TZoneMallocInlines.h>
#include <WebCore/IOSurface.h>

namespace WebCore::DDModel {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DDMeshImpl);

DDMeshImpl::DDMeshImpl(WebGPU::WebGPUPtr<WGPUDDMesh>&& ddMesh, Vector<UniqueRef<WebCore::IOSurface>>&& renderBuffers, ConvertToBackingContext& convertToBackingContext)
    : m_convertToBackingContext(convertToBackingContext)
    , m_backing(WTFMove(ddMesh))
    , m_renderBuffers(WTFMove(renderBuffers))
{
}

DDMeshImpl::~DDMeshImpl() = default;

void DDMeshImpl::setLabelInternal(const String&)
{
    // FIXME: Implement this.
}

#if ENABLE(GPU_PROCESS_MODEL)
static DDBridgeMeshPart *convert(const DDModel::DDMeshPart& part)
{
    return [[DDBridgeMeshPart alloc] initWithIndexOffset:part.indexOffset indexCount:part.indexCount topology:static_cast<MTLPrimitiveType>(part.topology) materialIndex:part.materialIndex boundsMin:part.boundsMin boundsMax:part.boundsMax];
}

static NSArray<DDBridgeMeshPart *> *convert(const Vector<DDModel::DDMeshPart>& parts)
{
    NSMutableArray<DDBridgeMeshPart *> *result = [NSMutableArray array];
    for (auto& p : parts)
        [result addObject:convert(p)];

    return result;
}

static NSData* convert(const Vector<uint8_t>& data)
{
    return [[NSData alloc] initWithBytes:data.span().data() length:data.sizeInBytes()];
}

static NSArray<NSData*> *convert(const Vector<Vector<uint8_t>>& data)
{
    NSMutableArray<NSData*> *result = [NSMutableArray array];
    for (auto& v : data)
        [result addObject:convert(v)];

    return result;
}

static DDBridgeChainedFloat4x4 *convert(const Vector<DDFloat4x4>& v)
{
    auto count = v.size();
    if (!count)
        return nil;

    DDBridgeChainedFloat4x4 *result = [[DDBridgeChainedFloat4x4 alloc] initWithTransform:v[0]];
    DDBridgeChainedFloat4x4 *current = result;

    for (uint32_t i = 1; i < count; ++i) {
        DDBridgeChainedFloat4x4 *next = [[DDBridgeChainedFloat4x4 alloc] initWithTransform:v[i]];
        current.next = next;
        current = next;
    }

    return result;
}

static NSArray<DDBridgeVertexAttributeFormat *> *convert(const Vector<DDVertexAttributeFormat>& formats)
{
    NSMutableArray<DDBridgeVertexAttributeFormat *> *result = [NSMutableArray array];
    for (auto& format : formats)
        [result addObject:[[DDBridgeVertexAttributeFormat alloc] initWithSemantic:format.semantic format:format.format layoutIndex:format.layoutIndex offset:format.offset]];

    return result;
}

static NSArray<DDBridgeVertexLayout *> *convert(const Vector<DDVertexLayout>& layouts)
{
    NSMutableArray<DDBridgeVertexLayout *> *result = [NSMutableArray array];
    for (auto& layout : layouts)
        [result addObject:[[DDBridgeVertexLayout alloc] initWithBufferIndex:layout.bufferIndex bufferOffset:layout.bufferOffset bufferStride:layout.bufferStride]];

    return result;
}

static DDBridgeMeshDescriptor *convert(const DDMeshDescriptor& descriptor)
{
    return [[DDBridgeMeshDescriptor alloc] initWithVertexBufferCount:descriptor.vertexBufferCount
        vertexCapacity:descriptor.vertexCapacity
        vertexAttributes:convert(descriptor.vertexAttributes)
        vertexLayouts:convert(descriptor.vertexLayouts)
        indexCapacity:descriptor.indexCapacity
        indexType:static_cast<MTLIndexType>(descriptor.indexType)];
}

static NSArray<NSString *> *convert(const Vector<String>& v)
{
    NSMutableArray<NSString *> *result = [NSMutableArray array];
    for (auto& s : v)
        [result addObject:s.createNSString().get()];

    return result;
}
#endif

void DDMeshImpl::update(const DDUpdateMeshDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    DDBridgeUpdateMesh *backingDescriptor = [[DDBridgeUpdateMesh alloc] initWithIdentifier:descriptor.identifier.createNSString().get()
        updateType:static_cast<DDBridgeDataUpdateType>(descriptor.updateType)
        descriptor:convert(descriptor.descriptor)
        parts:convert(descriptor.parts)
        indexData:convert(descriptor.indexData)
        vertexData:convert(descriptor.vertexData)
        instanceTransforms:convert(descriptor.instanceTransforms)
        instanceTransformsCount:descriptor.instanceTransforms.size()
        materialPrims:convert(descriptor.materialPrims)];

    wgpuDDMeshUpdate(m_backing.get(), backingDescriptor);
#else
    UNUSED_PARAM(descriptor);
#endif
}

#if ENABLE(GPU_PROCESS_MODEL)
static MTLTextureSwizzleChannels convert(WebCore::DDModel::DDImageAssetSwizzle swizzle)
{
    return MTLTextureSwizzleChannels {
        .red = static_cast<MTLTextureSwizzle>(swizzle.red),
        .green = static_cast<MTLTextureSwizzle>(swizzle.green),
        .blue = static_cast<MTLTextureSwizzle>(swizzle.blue),
        .alpha = static_cast<MTLTextureSwizzle>(swizzle.alpha)
    };
}

static DDBridgeImageAsset* convert(const DDImageAsset& imageAsset)
{
    return [[DDBridgeImageAsset alloc] initWithData:convert(imageAsset.data) width:imageAsset.width height:imageAsset.height depth:imageAsset.depth bytesPerPixel:imageAsset.bytesPerPixel textureType:static_cast<MTLTextureType>(imageAsset.textureType) pixelFormat:static_cast<MTLPixelFormat>(imageAsset.pixelFormat) mipmapLevelCount:imageAsset.mipmapLevelCount arrayLength:imageAsset.arrayLength textureUsage:static_cast<MTLTextureUsage>(imageAsset.textureUsage) swizzle:convert(imageAsset.swizzle)];
}
#endif

void DDMeshImpl::updateTexture(const DDUpdateTextureDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    DDBridgeUpdateTexture *backingDescriptor = [[DDBridgeUpdateTexture alloc] initWithImageAsset:convert(descriptor.imageAsset) identifier:descriptor.identifier.createNSString().get() hashString:descriptor.hashString.createNSString().get()];

    wgpuDDMeshTextureUpdate(m_backing.get(), backingDescriptor);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void DDMeshImpl::updateMaterial(const DDUpdateMaterialDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    DDBridgeUpdateMaterial *backingDescriptor = [[DDBridgeUpdateMaterial alloc] initWithMaterialGraph:convert(descriptor.materialGraph) identifier:descriptor.identifier.createNSString().get()];

    wgpuDDMeshMaterialUpdate(m_backing.get(), backingDescriptor);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void DDMeshImpl::render()
{
    wgpuDDMeshRender(m_backing.get());
}

void DDMeshImpl::setEntityTransform(const DDFloat4x4& transform)
{
    wgpuDDMeshSetTransform(m_backing.get(), transform);
}

std::optional<DDFloat4x4> DDMeshImpl::entityTransform() const
{
    return std::nullopt;
}

void DDMeshImpl::setCameraDistance(float distance)
{
    wgpuDDMeshSetCameraDistance(m_backing.get(), distance);
}

void DDMeshImpl::play(bool play)
{
    wgpuDDMeshPlay(m_backing.get(), play);
}

#if PLATFORM(COCOA)
Vector<MachSendRight> DDMeshImpl::ioSurfaceHandles()
{
    return m_renderBuffers.map([](const auto& renderBuffer) {
        return renderBuffer->createSendRight();
    });
}
#endif

}

#endif // HAVE(WEBGPU_IMPLEMENTATION)
