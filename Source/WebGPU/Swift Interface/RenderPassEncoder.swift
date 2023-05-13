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

public class RenderPassEncoder {
    internal let renderPassEncoder: __WGPURenderPassEncoder
    private let ownership: Ownership

    internal init(_ renderPassEncoder: __WGPURenderPassEncoder, ownership: Ownership = .Owned) {
        self.renderPassEncoder = renderPassEncoder
        self.ownership = ownership
    }

    deinit {
        if ownership == .Owned {
            __wgpuRenderPassEncoderRelease(renderPassEncoder)
        }
    }

    public func beginOcclusionQuery(queryIndex: UInt32) {
        __wgpuRenderPassEncoderBeginOcclusionQuery(renderPassEncoder, queryIndex)
    }

    public func beginPipelineStatisticsQuery(querySet: QuerySet, queryIndex: UInt32) {
        __wgpuRenderPassEncoderBeginPipelineStatisticsQuery(renderPassEncoder, querySet.querySet, queryIndex)
    }
    
    public func draw(vertexCount: UInt32, instanceCount: UInt32, firstVertex: UInt32, firstInstance: UInt32) {
        __wgpuRenderPassEncoderDraw(renderPassEncoder, vertexCount, instanceCount, firstVertex, firstInstance)
    }

    public func drawIndexed(indexCount: UInt32, instanceCount: UInt32, firstIndex: UInt32, baseVertex: Int32, firstInstance: UInt32) {
        __wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexCount, instanceCount, firstIndex, baseVertex, firstInstance)
    }

    public func drawIndexedIndirect(_ indirectBuffer: Buffer, indirectOffset: UInt64) {
        __wgpuRenderPassEncoderDrawIndexedIndirect(renderPassEncoder, indirectBuffer.buffer, indirectOffset)
    }

    public func drawIndirect(_ indirectBuffer: Buffer, indirectOffset: UInt64) {
        __wgpuRenderPassEncoderDrawIndirect(renderPassEncoder, indirectBuffer.buffer, indirectOffset)
    }

    public func end() {
        __wgpuRenderPassEncoderEnd(renderPassEncoder)
    }

    public func endOcclusionQuery() {
        __wgpuRenderPassEncoderEndOcclusionQuery(renderPassEncoder)
    }

    public func endPipelineStatisticsQuery() {
        __wgpuRenderPassEncoderEndPipelineStatisticsQuery(renderPassEncoder)
    }

    public func executeBundles(bundles: [RenderBundle]) {
        bundles.map({ $0.renderBundle }).withUnsafeBufferPointer { pointer in
            __wgpuRenderPassEncoderExecuteBundles(renderPassEncoder, UInt32(bundles.count), pointer.baseAddress)
        }
    }

    public func insertDebugMarker(_ markerLabel: String) {
        __wgpuRenderPassEncoderInsertDebugMarker(renderPassEncoder, markerLabel)
    }

    public func popDebugGroup() {
        __wgpuRenderPassEncoderPopDebugGroup(renderPassEncoder)
    }

    public func pushDebugGroup(_ groupLabel: String) {
        __wgpuRenderPassEncoderPushDebugGroup(renderPassEncoder, groupLabel)
    }

    public func setBindGroup(groupIndex: UInt32, group: BindGroup, dynamicOffsets: [UInt32]) {
        dynamicOffsets.withUnsafeBufferPointer { pointer in
            __wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, groupIndex, group.bindGroup, UInt32(dynamicOffsets.count), pointer.baseAddress)
        }
    }

    public func setBlendConstant(_ color: WGPUColor) {
        var localColor = color
        __wgpuRenderPassEncoderSetBlendConstant(renderPassEncoder, &localColor)
    }

    public func setIndexBuffer(_ buffer: Buffer, format: WGPUIndexFormat, offset: UInt64, size: UInt64) {
        __wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, buffer.buffer, format, offset, size)
    }

    public func setLabel(_ label: String) {
        __wgpuRenderPassEncoderSetLabel(renderPassEncoder, label)
    }

    public func setPipeline(_ pipeline: ComputePipeline) {
        __wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipeline.computePipeline)
    }

    public func setScissorRect(x: UInt32, y: UInt32, width: UInt32, height: UInt32) {
        __wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, x, y, width, height)
    }

    public func setStencilReference(reference: UInt32) {
        __wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, reference)
    }

    public func setVertexBuffer(slot: UInt32, buffer: Buffer, offset: UInt64, size: UInt64) {
        __wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, slot, buffer.buffer, offset, size)
    }

    public func setViewport(x: Float, y: Float, width: Float, height: Float, minDepth: Float, maxDepth: Float) {
        __wgpuRenderPassEncoderSetViewport(renderPassEncoder, x, y, width, height, minDepth, maxDepth)
    }
}
