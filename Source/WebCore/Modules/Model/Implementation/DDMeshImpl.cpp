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

#include "DDMeshDescriptor.h"
#include "DDUpdateMeshDescriptor.h"
#include "ModelConvertToBackingContext.h"
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

#if PLATFORM(COCOA)
static Vector<KeyValuePair<int32_t, WGPUDDMeshPart>> convertToBacking(const Vector<KeyValuePair<int32_t, DDMeshPart>>& parts)
{
    Vector<KeyValuePair<int32_t, WGPUDDMeshPart>> result;
    for (auto& p : parts) {
        result.append(KeyValuePair(WTFMove(p.key), WGPUDDMeshPart {
            .indexOffset = p.value.indexOffset,
            .indexCount = p.value.indexCount,
            .topology = p.value.topology,
            .materialIndex = p.value.materialIndex,
            .boundsMin = WTFMove(p.value.boundsMin),
            .boundsMax = WTFMove(p.value.boundsMax)
        }));
    }
    return result;
}
static Vector<WGPUDDReplaceVertices> convertToBacking(const Vector<DDReplaceVertices>& vertices)
{
    Vector<WGPUDDReplaceVertices> result;
    for (auto& p : vertices) {
        result.append(WGPUDDReplaceVertices {
            .bufferIndex = p.bufferIndex,
            .buffer = WTFMove(p.buffer)
        });
    }
    return result;
}

static Vector<simd_float4x4> toVector(const Vector<DDFloat4x4>& input)
{
    Vector<simd_float4x4> result;
    result.reserveCapacity(input.size());
    for (auto& float4x4 : input)
        result.append(float4x4);

    return result;
}

static Vector<WGPUDDVertexAttributeFormat> convertToBacking(const Vector<DDModel::DDVertexAttributeFormat>& descriptor)
{
    Vector<WGPUDDVertexAttributeFormat> result;
    for (auto& d : descriptor) {
        result.append(WGPUDDVertexAttributeFormat {
            .semantic = d.semantic,
            .format = d.format,
            .layoutIndex = d.layoutIndex,
            .offset = d.offset
        });
    }
    return result;
}

static Vector<WGPUDDVertexLayout> convertToBacking(const Vector<DDModel::DDVertexLayout>& descriptor)
{
    Vector<WGPUDDVertexLayout> result;
    for (auto& d : descriptor) {
        result.append(WGPUDDVertexLayout {
            .bufferIndex = d.bufferIndex,
            .bufferOffset = d.bufferOffset,
            .bufferStride = d.bufferStride,
        });
    }
    return result;
}

void DDMeshImpl::addMesh(const DDModel::DDMeshDescriptor& descriptor)
{
    WGPUDDMeshDescriptor backingDescriptor {
        .indexCapacity = descriptor.indexCapacity,
        .indexType = descriptor.indexType,
        .vertexBufferCount = descriptor.vertexBufferCount,
        .vertexCapacity = descriptor.vertexCapacity,
        .vertexAttributes = convertToBacking(descriptor.vertexAttributes),
        .vertexLayouts = convertToBacking(descriptor.vertexLayouts)
    };

    wgpuDDMeshAdd(m_backing.get(), &backingDescriptor);
}

void DDMeshImpl::update(const DDUpdateMeshDescriptor& descriptor)
{
    WGPUDDUpdateMeshDescriptor backingDescriptor {
        .partCount = descriptor.partCount,
        .parts = convertToBacking(descriptor.parts),
        .renderFlags = WTFMove(descriptor.renderFlags),
        .vertices = convertToBacking(descriptor.vertices),
        .indices = WTFMove(descriptor.indices),
        .transform = WTFMove(descriptor.transform),
        .instanceTransforms4x4 = toVector(descriptor.instanceTransforms4x4),
        .materialIds = WTFMove(descriptor.materialIds)
    };

    wgpuDDMeshUpdate(m_backing.get(), &backingDescriptor);
}

void DDMeshImpl::render()
{
    wgpuDDMeshRender(m_backing.get());
}
#endif

Vector<MachSendRight> DDMeshImpl::ioSurfaceHandles()
{
    return m_renderBuffers.map([](const auto& renderBuffer) {
        return renderBuffer->createSendRight();
    });
}

}

#endif // HAVE(WEBGPU_IMPLEMENTATION)
