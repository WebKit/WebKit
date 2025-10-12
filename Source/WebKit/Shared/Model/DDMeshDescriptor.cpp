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
#include "DDMeshDescriptor.h"

#if ENABLE(GPU_PROCESS)

#include "DDVertexAttributeFormat.h"
#include "DDVertexLayout.h"
#include "ModelConvertFromBackingContext.h"
#include "ModelConvertToBackingContext.h"
#include <WebCore/DDMeshDescriptor.h>
#include <WebCore/DDVertexAttributeFormat.h>
#include <WebCore/DDVertexLayout.h>

namespace WebKit::DDModel {

std::optional<DDVertexLayout> ConvertToBackingContext::convertToBacking(const WebCore::DDModel::DDVertexLayout& format)
{
    auto base = convertToBacking(static_cast<const WebCore::DDModel::ObjectDescriptorBase&>(format));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), format.bufferIndex, format.bufferOffset, format.bufferStride } };
}

static Vector<DDVertexLayout> convertVectorToBacking(const Vector<WebCore::DDModel::DDVertexLayout>& formats, ConvertToBackingContext& context)
{
    Vector<DDVertexLayout> result;
    for (auto& format : formats) {
        if (auto r = context.convertToBacking(format))
            result.append(*r);
    }
    return result;
}

std::optional<WebCore::DDModel::DDVertexLayout> ConvertFromBackingContext::convertFromBacking(const DDVertexLayout& format)
{
    auto base = convertFromBacking(static_cast<const ObjectDescriptorBase&>(format));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), format.bufferIndex, format.bufferOffset, format.bufferStride } };
}

static Vector<WebCore::DDModel::DDVertexLayout> convertVectorFromBacking(const Vector<DDVertexLayout>& formats, ConvertFromBackingContext& context)
{
    Vector<WebCore::DDModel::DDVertexLayout> result;
    for (auto& format : formats) {
        if (auto r = context.convertFromBacking(format))
            result.append(*r);
    }
    return result;
}

std::optional<DDVertexAttributeFormat> ConvertToBackingContext::convertToBacking(const WebCore::DDModel::DDVertexAttributeFormat& format)
{
    auto base = convertToBacking(static_cast<const WebCore::DDModel::ObjectDescriptorBase&>(format));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), format.semantic, format.format, format.layoutIndex, format.offset } };
}

static Vector<DDVertexAttributeFormat> convertVectorToBacking(const Vector<WebCore::DDModel::DDVertexAttributeFormat>& formats, ConvertToBackingContext& context)
{
    Vector<DDVertexAttributeFormat> result;
    for (auto& format : formats) {
        if (auto r = context.convertToBacking(format))
            result.append(*r);
    }
    return result;
}

std::optional<WebCore::DDModel::DDVertexAttributeFormat> ConvertFromBackingContext::convertFromBacking(const DDVertexAttributeFormat& format)
{
    auto base = convertFromBacking(static_cast<const ObjectDescriptorBase&>(format));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), format.semantic, format.format, format.layoutIndex, format.offset } };
}

static Vector<WebCore::DDModel::DDVertexAttributeFormat> convertVectorFromBacking(const Vector<DDVertexAttributeFormat>& formats, ConvertFromBackingContext& context)
{
    Vector<WebCore::DDModel::DDVertexAttributeFormat> result;
    for (auto& format : formats) {
        if (auto r = context.convertFromBacking(format))
            result.append(*r);
    }
    return result;
}

std::optional<DDMeshDescriptor> ConvertToBackingContext::convertToBacking(const WebCore::DDModel::DDMeshDescriptor& ddMeshDescriptor)
{
    auto base = convertToBacking(static_cast<const WebCore::DDModel::ObjectDescriptorBase&>(ddMeshDescriptor));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), ddMeshDescriptor.indexCapacity, ddMeshDescriptor.indexType, ddMeshDescriptor.vertexBufferCount, ddMeshDescriptor.vertexCapacity, convertVectorToBacking(ddMeshDescriptor.vertexAttributes, *this), convertVectorToBacking(ddMeshDescriptor.vertexLayouts, *this) } };
}

std::optional<WebCore::DDModel::DDMeshDescriptor> ConvertFromBackingContext::convertFromBacking(const DDMeshDescriptor& ddMeshDescriptor)
{
    auto base = convertFromBacking(static_cast<const ObjectDescriptorBase&>(ddMeshDescriptor));
    if (!base)
        return std::nullopt;

    return { { WTFMove(*base), ddMeshDescriptor.indexCapacity, ddMeshDescriptor.indexType, ddMeshDescriptor.vertexBufferCount, ddMeshDescriptor.vertexCapacity, convertVectorFromBacking(ddMeshDescriptor.vertexAttributes, *this), convertVectorFromBacking(ddMeshDescriptor.vertexLayouts, *this) } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
