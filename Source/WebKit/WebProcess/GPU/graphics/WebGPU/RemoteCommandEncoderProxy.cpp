/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
#include "RemoteCommandEncoderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteCommandBufferProxy.h"
#include "RemoteCommandEncoderMessages.h"
#include "RemoteComputePassEncoderProxy.h"
#include "RemoteRenderPassEncoderProxy.h"
#include "WebGPUConvertToBackingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteCommandEncoderProxy);

RemoteCommandEncoderProxy::RemoteCommandEncoderProxy(RemoteGPUProxy& root, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_root(root)
{
}

RemoteCommandEncoderProxy::~RemoteCommandEncoderProxy()
{
    auto sendResult = send(Messages::RemoteCommandEncoder::Destruct());
    UNUSED_VARIABLE(sendResult);
}

RefPtr<::WebGPU::RenderPassEncoder> RemoteCommandEncoderProxy::beginRenderPass(const ::WebGPU::RenderPassDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);

    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteCommandEncoder::BeginRenderPass(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteRenderPassEncoderProxy::create(*this, m_convertToBackingContext, identifier);
    if (convertedDescriptor)
        result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

RefPtr<::WebGPU::ComputePassEncoder> RemoteCommandEncoderProxy::beginComputePass(const std::optional<::WebGPU::ComputePassDescriptor>& descriptor)
{
    std::optional<WebKit::WebGPU::ComputePassDescriptor> convertedDescriptor;

    if (descriptor) {
        convertedDescriptor = m_convertToBackingContext->convertToBacking(*descriptor);
        if (!convertedDescriptor)
            return nullptr;
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteCommandEncoder::BeginComputePass(convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteComputePassEncoderProxy::create(*this, m_convertToBackingContext, identifier);
    if (convertedDescriptor)
        result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

void RemoteCommandEncoderProxy::copyBufferToBuffer(
    const ::WebGPU::Buffer& source,
    uint64_t sourceOffset,
    const ::WebGPU::Buffer& destination,
    uint64_t destinationOffset,
    uint64_t size)
{
    auto convertedSource = m_convertToBackingContext->convertToBacking(source);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);

    auto sendResult = send(Messages::RemoteCommandEncoder::CopyBufferToBuffer(convertedSource, sourceOffset, convertedDestination, destinationOffset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::copyBufferToTexture(
    const ::WebGPU::TexelCopyBufferInfo& source,
    const ::WebGPU::TexelCopyTextureInfo& destination,
    const ::WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_convertToBackingContext->convertToBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedSource || !convertedDestination)
        return;

    auto sendResult = send(Messages::RemoteCommandEncoder::CopyBufferToTexture(*convertedSource, *convertedDestination, copySize));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::copyTextureToBuffer(
    const ::WebGPU::TexelCopyTextureInfo& source,
    const ::WebGPU::TexelCopyBufferInfo& destination,
    const ::WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_convertToBackingContext->convertToBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedSource || !convertedDestination)
        return;

    auto sendResult = send(Messages::RemoteCommandEncoder::CopyTextureToBuffer(*convertedSource, *convertedDestination, copySize));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::copyTextureToTexture(
    const ::WebGPU::TexelCopyTextureInfo& source,
    const ::WebGPU::TexelCopyTextureInfo& destination,
    const ::WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_convertToBackingContext->convertToBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedSource || !convertedDestination)
        return;

    auto sendResult = send(Messages::RemoteCommandEncoder::CopyTextureToTexture(*convertedSource, *convertedDestination, copySize));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::clearBuffer(
    const ::WebGPU::Buffer& buffer,
    uint64_t offset,
    std::optional<uint64_t> size)
{
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(buffer);

    auto sendResult = send(Messages::RemoteCommandEncoder::ClearBuffer(convertedBuffer, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    auto sendResult = send(Messages::RemoteCommandEncoder::PushDebugGroup(WTF::move(groupLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::popDebugGroup()
{
    auto sendResult = send(Messages::RemoteCommandEncoder::PopDebugGroup());
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    auto sendResult = send(Messages::RemoteCommandEncoder::InsertDebugMarker(WTF::move(markerLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::writeTimestamp(const ::WebGPU::QuerySet& querySet, uint32_t queryIndex)
{
    auto convertedQuerySet = m_convertToBackingContext->convertToBacking(querySet);

    auto sendResult = send(Messages::RemoteCommandEncoder::WriteTimestamp(convertedQuerySet, queryIndex));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::resolveQuerySet(
    const ::WebGPU::QuerySet& querySet,
    uint32_t firstQuery,
    uint32_t queryCount,
    const ::WebGPU::Buffer& destination,
    uint64_t destinationOffset)
{
    auto convertedQuerySet = m_convertToBackingContext->convertToBacking(querySet);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);

    auto sendResult = send(Messages::RemoteCommandEncoder::ResolveQuerySet(convertedQuerySet, firstQuery, queryCount, convertedDestination, destinationOffset));
    UNUSED_VARIABLE(sendResult);
}

RefPtr<::WebGPU::CommandBuffer> RemoteCommandEncoderProxy::finish(const ::WebGPU::CommandBufferDescriptor& descriptor)
{
    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteCommandEncoder::Finish(descriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteCommandBufferProxy::create(m_root, m_convertToBackingContext, identifier);
    result->setLabel(String { descriptor.label });
    return result;
}

void RemoteCommandEncoderProxy::setLabel(String&& label)
{
    auto sendResult = send(Messages::RemoteCommandEncoder::SetLabel(WTF::move(label)));
    UNUSED_VARIABLE(sendResult);
}

bool RemoteCommandEncoderProxy::isValid() const
{
    // The Web Process cannot know. RemoteGPU::isValid() answers it for tests.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
