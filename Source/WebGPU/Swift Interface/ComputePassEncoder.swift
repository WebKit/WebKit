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

public class ComputePassEncoder {
    internal let computePassEncoder: __WGPUComputePassEncoder
    private let ownership: Ownership

    internal init(_ computePassEncoder: __WGPUComputePassEncoder, ownership: Ownership = .Owned) {
        self.computePassEncoder = computePassEncoder
        self.ownership = ownership
    }

    deinit {
        if ownership == .Owned {
            __wgpuComputePassEncoderRelease(computePassEncoder)
        }
    }

    public func beginPipelineStatisticsQuery(querySet: QuerySet, queryIndex: UInt32) {
        __wgpuComputePassEncoderBeginPipelineStatisticsQuery(computePassEncoder, querySet.querySet, queryIndex)
    }

    public func dispatchWorkgroups(workgroupCountX: UInt32, workgroupCountY: UInt32, workgroupCountZ: UInt32) {
        __wgpuComputePassEncoderDispatchWorkgroups(computePassEncoder, workgroupCountX, workgroupCountY, workgroupCountZ)
    }

    public func dispatchWorkgroupsIndirect(indirectBuffer: Buffer, indirectOffset: UInt64) {
        __wgpuComputePassEncoderDispatchWorkgroupsIndirect(computePassEncoder, indirectBuffer.buffer, indirectOffset)
    }

    public func end() {
        __wgpuComputePassEncoderEnd(computePassEncoder)
    }

    public func endPipelineStatisticsQuery() {
        __wgpuComputePassEncoderEndPipelineStatisticsQuery(computePassEncoder)
    }

    public func insertDebugMarker(_ markerLabel: String) {
        __wgpuComputePassEncoderInsertDebugMarker(computePassEncoder, markerLabel)
    }

    public func popDebugGroup() {
        __wgpuComputePassEncoderPopDebugGroup(computePassEncoder)
    }

    public func pushDebugGroup(_ groupLabel: String) {
        __wgpuComputePassEncoderPushDebugGroup(computePassEncoder, groupLabel)
    }

    public func setBindGroup(groupIndex: UInt32, group: BindGroup, dynamicOffsets: [UInt32]) {
        dynamicOffsets.withUnsafeBufferPointer { pointer in
            __wgpuComputePassEncoderSetBindGroup(computePassEncoder, groupIndex, group.bindGroup, UInt32(dynamicOffsets.count), pointer.baseAddress)
        }
    }

    public func setLabel(_ label: String) {
        __wgpuComputePassEncoderSetLabel(computePassEncoder, label)
    }

    public func setPipeline(_ pipeline: ComputePipeline) {
        __wgpuComputePassEncoderSetPipeline(computePassEncoder, pipeline.computePipeline)
    }
}
