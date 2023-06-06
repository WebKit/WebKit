/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import Foundation

public class CommandEncoder {
    internal let commandEncoder: __WGPUCommandEncoder
    private let ownership: Ownership

    internal init(_ commandEncoder: __WGPUCommandEncoder, ownership: Ownership = .Owned) {
        self.commandEncoder = commandEncoder
        self.ownership = ownership
    }

    deinit {
        if ownership == .Owned {
            __wgpuCommandEncoderRelease(commandEncoder)
        }
    }

    public func beginComputePass(_ descriptor: WGPUComputePassDescriptor) -> ComputePassEncoder {
        var localDescriptor = descriptor
        return ComputePassEncoder(__wgpuCommandEncoderBeginComputePass(commandEncoder, &localDescriptor))
    }

    public func beginRenderPass(_ descriptor: WGPURenderPassDescriptor) -> RenderPassEncoder {
        var localDescriptor = descriptor
        return RenderPassEncoder(__wgpuCommandEncoderBeginRenderPass(commandEncoder, &localDescriptor))
    }

    public func clearBuffer(_ buffer: Buffer, offset: UInt64, size: UInt64) {
        __wgpuCommandEncoderClearBuffer(commandEncoder, buffer.buffer, offset, size)
    }

    public func copyBufferToBuffer(source: Buffer, sourceOffset: UInt64, destination: Buffer, destinationOffset: UInt64, size: UInt64) {
        __wgpuCommandEncoderCopyBufferToBuffer(commandEncoder, source.buffer, sourceOffset, destination.buffer, destinationOffset, size)
    }

    public func copyBufferToTexture(source: WGPUImageCopyBuffer, destination: WGPUImageCopyTexture, copySize: WGPUExtent3D) {
        var localSource = source
        var localDestination = destination
        var localCopySize = copySize
        __wgpuCommandEncoderCopyBufferToTexture(commandEncoder, &localSource, &localDestination, &localCopySize)
    }

    public func copyBufferToTexture(source: WGPUImageCopyTexture, destination: WGPUImageCopyBuffer, copySize: WGPUExtent3D) {
        var localSource = source
        var localDestination = destination
        var localCopySize = copySize
        __wgpuCommandEncoderCopyTextureToBuffer(commandEncoder, &localSource, &localDestination, &localCopySize)
    }

    public func copyTextureToTexture(source: WGPUImageCopyTexture, destination: WGPUImageCopyTexture, copySize: WGPUExtent3D) {
        var localSource = source
        var localDestination = destination
        var localCopySize = copySize
        __wgpuCommandEncoderCopyTextureToTexture(commandEncoder, &localSource, &localDestination, &localCopySize)
    }

    public func finish(_ descriptor: WGPUCommandBufferDescriptor) -> CommandBuffer {
        var localDescriptor = descriptor
        return CommandBuffer(__wgpuCommandEncoderFinish(commandEncoder, &localDescriptor))
    }

    public func insertDebugMarker(_ markerLable: String) {
        __wgpuCommandEncoderInsertDebugMarker(commandEncoder, markerLable)
    }

    public func popDebugGroup() {
        __wgpuCommandEncoderPopDebugGroup(commandEncoder)
    }

    public func pushDebugGroup(_ groupLabel: String) {
        __wgpuCommandEncoderPushDebugGroup(commandEncoder, groupLabel)
    }

    public func resolveQuerySet(_ querySet: QuerySet, firstQuery: UInt32, queryCount: UInt32, destination: Buffer, destinationOffset: UInt64) {
        __wgpuCommandEncoderResolveQuerySet(commandEncoder, querySet.querySet, firstQuery, queryCount, destination.buffer, destinationOffset)
    }

    public func setLabel(_ label: String) {
        __wgpuCommandEncoderSetLabel(commandEncoder, label)
    }

    public func writeTimestamp(querySet: QuerySet, queryIndex: UInt32) {
        __wgpuCommandEncoderWriteTimestamp(commandEncoder, querySet.querySet, queryIndex)
    }
}
