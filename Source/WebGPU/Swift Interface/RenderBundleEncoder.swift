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

public class RenderBundleEncoder {
    internal let renderBundleEncoder: __WGPURenderBundleEncoder
    private let ownership: Ownership

    internal init(_ renderBundleEncoder: __WGPURenderBundleEncoder, ownership: Ownership = .Owned) {
        self.renderBundleEncoder = renderBundleEncoder
        self.ownership = ownership
    }

    deinit {
        if ownership == .Owned {
            __wgpuRenderBundleEncoderRelease(renderBundleEncoder)
        }
    }

    public func draw(vertexCount: UInt32, instanceCount: UInt32, firstVertex: UInt32, firstInstance: UInt32) {
        __wgpuRenderBundleEncoderDraw(renderBundleEncoder, vertexCount, instanceCount, firstVertex, firstInstance)
    }

    public func drawIndexed(indexCount: UInt32, instanceCount: UInt32, firstIndex: UInt32, baseVertex: Int32, firstInstance: UInt32) {
        __wgpuRenderBundleEncoderDrawIndexed(renderBundleEncoder, indexCount, instanceCount, firstIndex, baseVertex, firstInstance)
    }

    public func drawIndexedIndirect(_ indirectBuffer: Buffer, indirectOffset: UInt64) {
        __wgpuRenderBundleEncoderDrawIndexedIndirect(renderBundleEncoder, indirectBuffer.buffer, indirectOffset)
    }

    public func drawIndirect(_ indirectBuffer: Buffer, indirectOffset: UInt64) {
        __wgpuRenderBundleEncoderDrawIndirect(renderBundleEncoder, indirectBuffer.buffer, indirectOffset)
    }

    public func finish(_ descriptor: WGPURenderBundleDescriptor) -> RenderBundle {
        var localDescriptor = descriptor
        return RenderBundle(__wgpuRenderBundleEncoderFinish(renderBundleEncoder, &localDescriptor))
    }

    public func insertDebugMarker(_ markerLabel: String) {
        __wgpuRenderBundleEncoderInsertDebugMarker(renderBundleEncoder, markerLabel)
    }

    public func popDebugGroup() {
        __wgpuRenderBundleEncoderPopDebugGroup(renderBundleEncoder)
    }

    public func pushDebugGroup(_ groupLabel: String) {
        __wgpuRenderBundleEncoderPushDebugGroup(renderBundleEncoder, groupLabel)
    }

    public func setBindGroup(groupIndex: UInt32, group: BindGroup, dynamicOffsets: [UInt32]) {
        dynamicOffsets.withUnsafeBufferPointer { pointer in
            __wgpuRenderBundleEncoderSetBindGroup(renderBundleEncoder, groupIndex, group.bindGroup, UInt32(dynamicOffsets.count), pointer.baseAddress)
        }
    }

    public func setIndexBuffer(_ buffer: Buffer, format: WGPUIndexFormat, offset: UInt64, size: UInt64) {
        __wgpuRenderBundleEncoderSetIndexBuffer(renderBundleEncoder, buffer.buffer, format, offset, size)
    }

    public func setLabel(_ label: String) {
        __wgpuRenderBundleEncoderSetLabel(renderBundleEncoder, label)
    }

    public func setPipeline(_ pipeline: ComputePipeline) {
        __wgpuRenderBundleEncoderSetPipeline(renderBundleEncoder, pipeline.computePipeline)
    }

    public func setVertexBuffer(slot: UInt32, buffer: Buffer, offset: UInt64, size: UInt64) {
        __wgpuRenderBundleEncoderSetVertexBuffer(renderBundleEncoder, slot, buffer.buffer, offset, size)
    }
}
