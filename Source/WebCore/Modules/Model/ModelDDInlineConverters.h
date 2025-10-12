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

#include <WebCore/DDMesh.h>
#include <WebCore/DDMeshDescriptor.h>
#include <WebCore/DDUpdateMeshDescriptor.h>
#include <WebCore/ModelDDTypes.h>

namespace WebCore {
static DDModel::DDVertexAttributeFormat toCpp(WebDDVertexAttributeFormat *format)
{
    return DDModel::DDVertexAttributeFormat {
        .semantic = format.semantic,
        .format = format.format,
        .layoutIndex = format.layoutIndex,
        .offset = format.offset
    };
}

static Vector<DDModel::DDVertexAttributeFormat> toCppVertexAttributes(NSArray<WebDDVertexAttributeFormat *> *formats)
{
    Vector<DDModel::DDVertexAttributeFormat> result;
    for (WebDDVertexAttributeFormat *f in formats)
        result.append(toCpp(f));
    return result;
}

static DDModel::DDVertexLayout toCpp(WebDDVertexLayout *layout)
{
    return DDModel::DDVertexLayout {
        .bufferIndex = layout.bufferIndex,
        .bufferOffset = layout.bufferOffset,
        .bufferStride = layout.bufferStride,
    };
}
static Vector<DDModel::DDVertexLayout> toCppVertexLayouts(NSArray<WebDDVertexLayout *> *layouts)
{
    Vector<DDModel::DDVertexLayout> result;
    for (WebDDVertexLayout *l in layouts)
        result.append(toCpp(l));
    return result;
}

static WebCore::DDModel::DDMeshDescriptor toCpp(WebAddMeshRequest *addMesh)
{
    return WebCore::DDModel::DDMeshDescriptor {
        .indexCapacity = addMesh.indexCapacity,
        .indexType = addMesh.indexType,
        .vertexBufferCount = addMesh.vertexBufferCount,
        .vertexCapacity = addMesh.vertexCapacity,
        .vertexAttributes = toCppVertexAttributes(addMesh.vertexAttributes),
        .vertexLayouts = toCppVertexLayouts(addMesh.vertexLayouts)
    };
}

static Vector<DDModel::DDFloat4x4> toVector(WebChainedFloat4x4 *input)
{
    Vector<DDModel::DDFloat4x4> result;
    for ( ; input; input = input.next)
        result.append(input.transform);

    return result;
}

static Vector<uint8_t> toVector(NSData *input)
{
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    Vector<uint8_t> result;
    for (NSUInteger i = 0; i < input.length; ++i)
        result.append(((const uint8_t*)input.bytes)[i]);

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    return result;
}

static Vector<KeyValuePair<int32_t, uint64_t>> toCpp(NSArray<WebSetRenderFlags *> *renderFlags)
{
    Vector<KeyValuePair<int32_t, uint64_t>> result;
    for (WebSetRenderFlags *flag in renderFlags)
        result.append({ flag.partIndex, flag.renderFlags });
    return result;
}

static DDModel::DDMeshPart toCpp(WebDDMeshPart *part)
{
    return DDModel::DDMeshPart {
        { "mesh part"_s, },
        static_cast<uint32_t>(part.indexOffset),
        static_cast<uint32_t>(part.indexCount),
        static_cast<uint32_t>(part.topology),
        static_cast<uint32_t>(part.materialIndex),
        WTFMove(part.boundsMin),
        WTFMove(part.boundsMax)
    };
}

static Vector<KeyValuePair<int32_t, DDModel::DDMeshPart>> toCpp(NSArray<WebSetPart *> *parts)
{
    Vector<KeyValuePair<int32_t, DDModel::DDMeshPart>> result;
    for (WebSetPart *f in parts)
        result.append({ f.partIndex, toCpp(f.part) });
    return result;
}

static DDModel::DDReplaceVertices toCpp(WebReplaceVertices *a)
{
    return DDModel::DDReplaceVertices {
        .bufferIndex = static_cast<int32_t>(a.bufferIndex),
        .buffer = toVector(a.buffer)
    };
}

static Vector<DDModel::DDReplaceVertices> toCpp(NSArray<WebReplaceVertices *> *arr)
{
    Vector<DDModel::DDReplaceVertices> result;
    for (WebReplaceVertices *a in arr)
        result.append(toCpp(a));
    return result;
}

static Vector<String> toCpp(NSArray<NSUUID *> *arr)
{
    Vector<String> result;
    for (NSUUID *a in arr) {
        if (a)
            result.append(String(a.UUIDString));
    }
    return result;
}

static WebCore::DDModel::DDUpdateMeshDescriptor toCpp(WebUpdateMeshRequest *update)
{
    return WebCore::DDModel::DDUpdateMeshDescriptor {
        .partCount = static_cast<int32_t>(update.partCount),
        .parts = toCpp(update.parts),
        .renderFlags = toCpp(update.renderFlags),
        .vertices = toCpp(update.vertices),
        .indices = toVector(update.indices),
        .transform = update.transform,
        .instanceTransforms4x4 = toVector(update.instanceTransforms),
        .materialIds = toCpp(update.materialIds)
    };
}
}
