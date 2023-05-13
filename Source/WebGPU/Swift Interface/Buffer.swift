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

public class Buffer {
    internal let buffer: __WGPUBuffer
    private let instance: Instance
    private let ownership: Ownership

    internal init(_ buffer: __WGPUBuffer, instance: Instance, ownership: Ownership = .Owned) {
        self.buffer = buffer
        self.instance = instance
        self.ownership = ownership
    }

    deinit {
        if ownership == .Owned {
            __wgpuBufferRelease(buffer)
        }
    }

    public func destroy() {
        __wgpuBufferDestroy(buffer)
    }

    // FIXME: Make this more Swifty.
    public func getConstMappedRange(offset: Int, size: Int) -> UnsafeRawPointer? {
        return __wgpuBufferGetConstMappedRange(buffer, offset, size)
    }

    public func getMapState() -> WGPUBufferMapState {
        return __wgpuBufferGetMapState(buffer)
    }

    // FIXME: Make this more Swifty.
    public func getMappedRange(offset: Int, size: Int) -> UnsafeMutableRawPointer? {
        return __wgpuBufferGetMappedRange(buffer, offset, size)
    }

    public func getSize() -> UInt64 {
        return __wgpuBufferGetSize(buffer)
    }

    public func getUsage() -> WGPUBufferUsage {
        return __wgpuBufferGetUsage(buffer)
    }

    public func mapAsync(mode: WGPUMapMode, offset: Int, size: Int) async -> WGPUBufferMapAsyncStatus {
        return await withCheckedContinuation { continuation in
            instance.scheduleWork { [self] in
                __wgpuBufferMapAsyncWithBlock(buffer, mode.rawValue, offset, size) { status in
                    continuation.resume(returning: status)
                }
            }
        }
    }

    public func setLabel(_ label: String) {
        __wgpuBufferSetLabel(buffer, label)
    }

    public func unmap() {
        __wgpuBufferUnmap(buffer)
    }
}
