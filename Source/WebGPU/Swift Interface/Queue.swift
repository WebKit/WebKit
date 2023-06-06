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

public class Queue {
    internal let queue: __WGPUQueue
    private let instance: Instance
    private let ownership: Ownership

    internal init(_ queue: __WGPUQueue, instance: Instance, ownership: Ownership = .Owned) {
        self.queue = queue
        self.instance = instance
        self.ownership = ownership
    }

    deinit {
        if ownership == .Owned {
            __wgpuQueueRelease(queue)
        }
    }

    public func onSubmittedWorkDone() async -> WGPUQueueWorkDoneStatus {
        return await withCheckedContinuation { continuation in
            instance.scheduleWork { [self] in
                __wgpuQueueOnSubmittedWorkDoneWithBlock(queue) { status in
                    continuation.resume(returning: status)
                }
            }
        }
    }

    public func setLabel(_ label: String) {
        __wgpuQueueSetLabel(queue, label)
    }

    public func submit(_ commands: [CommandBuffer]) {
        commands.map({ $0.commandBuffer }).withUnsafeBufferPointer { pointer in
            __wgpuQueueSubmit(queue, UInt32(commands.count), pointer.baseAddress)
        }
    }

    public func writeBuffer(_ buffer: Buffer, bufferOffset: UInt64, data: UnsafeRawPointer, size: Int) {
        __wgpuQueueWriteBuffer(queue, buffer.buffer, bufferOffset, data, size)
    }

    public func writeTexture(destination: WGPUImageCopyTexture, data: UnsafeRawPointer, dataSize: Int, dataLayout: WGPUTextureDataLayout, writeSize: WGPUExtent3D) {
        var localDestination = destination
        var localDataLayout = dataLayout
        var localWriteSize = writeSize
        __wgpuQueueWriteTexture(queue, &localDestination, data, dataSize, &localDataLayout, &localWriteSize)
    }
}
