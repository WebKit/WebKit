/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

namespace WebKit::WebGPU {

RemoteCommandEncoderProxy::RemoteCommandEncoderProxy(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteCommandEncoderProxy::~RemoteCommandEncoderProxy()
{
}

Ref<PAL::WebGPU::RenderPassEncoder> RemoteCommandEncoderProxy::beginRenderPass(const PAL::WebGPU::RenderPassDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteRenderPassEncoderProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteCommandEncoder::BeginRenderPass(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteRenderPassEncoderProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<PAL::WebGPU::ComputePassEncoder> RemoteCommandEncoderProxy::beginComputePass(const std::optional<PAL::WebGPU::ComputePassDescriptor>& descriptor)
{
    std::optional<WebKit::WebGPU::ComputePassDescriptor> convertedDescriptor;
    if (descriptor) {
        convertedDescriptor = m_convertToBackingContext->convertToBacking(*descriptor);
        if (!convertedDescriptor) {
            // FIXME: Implement error handling.
            return RemoteComputePassEncoderProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
        }
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteCommandEncoder::BeginComputePass(convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteComputePassEncoderProxy::create(*this, m_convertToBackingContext, identifier);
}

void RemoteCommandEncoderProxy::copyBufferToBuffer(
    const PAL::WebGPU::Buffer& source,
    PAL::WebGPU::Size64 sourceOffset,
    const PAL::WebGPU::Buffer& destination,
    PAL::WebGPU::Size64 destinationOffset,
    PAL::WebGPU::Size64 size)
{
    auto convertedSource = m_convertToBackingContext->convertToBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedSource || !convertedDestination)
        return;

    auto sendResult = send(Messages::RemoteCommandEncoder::CopyBufferToBuffer(convertedSource, sourceOffset, convertedDestination, destinationOffset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::copyBufferToTexture(
    const PAL::WebGPU::ImageCopyBuffer& source,
    const PAL::WebGPU::ImageCopyTexture& destination,
    const PAL::WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_convertToBackingContext->convertToBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = m_convertToBackingContext->convertToBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    auto sendResult = send(Messages::RemoteCommandEncoder::CopyBufferToTexture(*convertedSource, *convertedDestination, *convertedCopySize));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::copyTextureToBuffer(
    const PAL::WebGPU::ImageCopyTexture& source,
    const PAL::WebGPU::ImageCopyBuffer& destination,
    const PAL::WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_convertToBackingContext->convertToBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = m_convertToBackingContext->convertToBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    auto sendResult = send(Messages::RemoteCommandEncoder::CopyTextureToBuffer(*convertedSource, *convertedDestination, *convertedCopySize));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::copyTextureToTexture(
    const PAL::WebGPU::ImageCopyTexture& source,
    const PAL::WebGPU::ImageCopyTexture& destination,
    const PAL::WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_convertToBackingContext->convertToBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = m_convertToBackingContext->convertToBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    auto sendResult = send(Messages::RemoteCommandEncoder::CopyTextureToTexture(*convertedSource, *convertedDestination, *convertedCopySize));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::clearBuffer(
    const PAL::WebGPU::Buffer& buffer,
    PAL::WebGPU::Size64 offset,
    std::optional<PAL::WebGPU::Size64> size)
{
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    auto sendResult = send(Messages::RemoteCommandEncoder::ClearBuffer(convertedBuffer, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    auto sendResult = send(Messages::RemoteCommandEncoder::PushDebugGroup(WTFMove(groupLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::popDebugGroup()
{
    auto sendResult = send(Messages::RemoteCommandEncoder::PopDebugGroup());
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    auto sendResult = send(Messages::RemoteCommandEncoder::InsertDebugMarker(WTFMove(markerLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::writeTimestamp(const PAL::WebGPU::QuerySet& querySet, PAL::WebGPU::Size32 queryIndex)
{
    auto convertedQuerySet = m_convertToBackingContext->convertToBacking(querySet);
    ASSERT(convertedQuerySet);
    if (!convertedQuerySet)
        return;

    auto sendResult = send(Messages::RemoteCommandEncoder::WriteTimestamp(convertedQuerySet, queryIndex));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCommandEncoderProxy::resolveQuerySet(
    const PAL::WebGPU::QuerySet& querySet,
    PAL::WebGPU::Size32 firstQuery,
    PAL::WebGPU::Size32 queryCount,
    const PAL::WebGPU::Buffer& destination,
    PAL::WebGPU::Size64 destinationOffset)
{
    auto convertedQuerySet = m_convertToBackingContext->convertToBacking(querySet);
    ASSERT(convertedQuerySet);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedQuerySet || !convertedDestination)
        return;

    auto sendResult = send(Messages::RemoteCommandEncoder::ResolveQuerySet(convertedQuerySet, firstQuery, queryCount, convertedDestination, destinationOffset));
    UNUSED_VARIABLE(sendResult);
}

Ref<PAL::WebGPU::CommandBuffer> RemoteCommandEncoderProxy::finish(const PAL::WebGPU::CommandBufferDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteCommandBufferProxy::create(m_parent, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteCommandEncoder::Finish(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteCommandBufferProxy::create(m_parent, m_convertToBackingContext, identifier);
}

void RemoteCommandEncoderProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteCommandEncoder::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
