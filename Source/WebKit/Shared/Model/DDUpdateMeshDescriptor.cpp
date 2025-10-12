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
#include "DDUpdateMeshDescriptor.h"

#if ENABLE(GPU_PROCESS)

#include "ModelConvertFromBackingContext.h"
#include "ModelConvertToBackingContext.h"
#include <WebCore/DDUpdateMeshDescriptor.h>

namespace WebKit::DDModel {

std::optional<DDMeshPart> ConvertToBackingContext::convertToBacking(const WebCore::DDModel::DDMeshPart& format)
{
    auto base = convertToBacking(static_cast<const WebCore::DDModel::ObjectDescriptorBase&>(format));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), format.indexOffset, format.indexCount, format.topology, format.materialIndex, format.boundsMin, format.boundsMax } };
}

static Vector<KeyValuePair<int32_t, DDMeshPart>> convertVectorToBacking(const Vector<KeyValuePair<int32_t, WebCore::DDModel::DDMeshPart>>& formats, ConvertToBackingContext& context)
{
    Vector<KeyValuePair<int32_t, DDMeshPart>> result;
    for (auto& format : formats) {
        if (auto r = context.convertToBacking(format.value)) {
            auto keyCopy = format.key;
            result.append(KeyValuePair(WTFMove(keyCopy), WTFMove(*r)));
        }
    }
    return result;
}

std::optional<WebCore::DDModel::DDMeshPart> ConvertFromBackingContext::convertFromBacking(const DDMeshPart& format)
{
    auto base = convertFromBacking(static_cast<const ObjectDescriptorBase&>(format));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), format.indexOffset, format.indexCount, format.topology, format.materialIndex, format.boundsMin, format.boundsMax } };
}

static Vector<KeyValuePair<int32_t, WebCore::DDModel::DDMeshPart>> convertVectorFromBacking(const Vector<KeyValuePair<int32_t, DDMeshPart>>& formats, ConvertFromBackingContext& context)
{
    Vector<KeyValuePair<int32_t, WebCore::DDModel::DDMeshPart>> result;
    for (auto& format : formats) {
        if (auto r = context.convertFromBacking(format.value)) {
            auto keyCopy = format.key;
            result.append(KeyValuePair(WTFMove(keyCopy), WTFMove(*r)));
        }
    }
    return result;
}

std::optional<DDReplaceVertices> ConvertToBackingContext::convertToBacking(const WebCore::DDModel::DDReplaceVertices& format)
{
    auto base = convertToBacking(static_cast<const WebCore::DDModel::ObjectDescriptorBase&>(format));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), format.bufferIndex, format.buffer } };
}

static Vector<DDReplaceVertices> convertVectorToBacking(const Vector<WebCore::DDModel::DDReplaceVertices>& formats, ConvertToBackingContext& context)
{
    Vector<DDReplaceVertices> result;
    for (auto& format : formats) {
        if (auto r = context.convertToBacking(format))
            result.append(*r);
    }
    return result;
}

std::optional<WebCore::DDModel::DDReplaceVertices> ConvertFromBackingContext::convertFromBacking(const DDReplaceVertices& format)
{
    auto base = convertFromBacking(static_cast<const ObjectDescriptorBase&>(format));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), format.bufferIndex, format.buffer } };
}

static Vector<WebCore::DDModel::DDReplaceVertices> convertVectorFromBacking(const Vector<DDReplaceVertices>& replace, ConvertFromBackingContext& context)
{
    Vector<WebCore::DDModel::DDReplaceVertices> result;
    for (auto& v : replace) {
        if (auto r = context.convertFromBacking(v))
            result.append(*r);
    }
    return result;
}

std::optional<DDUpdateMeshDescriptor> ConvertToBackingContext::convertToBacking(const WebCore::DDModel::DDUpdateMeshDescriptor& desc)
{
    auto base = convertToBacking(static_cast<const WebCore::DDModel::ObjectDescriptorBase&>(desc));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), desc.partCount, convertVectorToBacking(desc.parts, *this), desc.renderFlags, convertVectorToBacking(desc.vertices, *this), desc.indices, desc.transform, desc.instanceTransforms4x4, desc.materialIds } };
}

std::optional<WebCore::DDModel::DDUpdateMeshDescriptor> ConvertFromBackingContext::convertFromBacking(const DDUpdateMeshDescriptor& desc)
{
    auto base = convertFromBacking(static_cast<const ObjectDescriptorBase&>(desc));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), desc.partCount, convertVectorFromBacking(desc.parts, *this), desc.renderFlags, convertVectorFromBacking(desc.vertices, *this), desc.indices, desc.transform, desc.instanceTransforms4x4, desc.materialIds } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
