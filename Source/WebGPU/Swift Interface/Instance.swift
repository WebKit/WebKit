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

public class Instance {
    internal let instance: __WGPUInstance
    private let scheduleWorkBlock: WGPUScheduleWorkBlock

    public init?(_ descriptor: WGPUInstanceDescriptor) {
        guard descriptor.nextInChain != nil,
            descriptor.nextInChain.pointee.sType == WGPUSType(rawValue: WGPUSTypeExtended_InstanceCocoaDescriptor.rawValue),
            descriptor.nextInChain.pointee.next == nil else {
            return nil
        }
        scheduleWorkBlock = descriptor.nextInChain.withMemoryRebound(to: WGPUInstanceCocoaDescriptor.self, capacity: 1) { (pointer) -> WGPUScheduleWorkBlock in
            return pointer.pointee.scheduleWorkBlock
        }
        var localDescriptor = descriptor
        instance = __wgpuCreateInstance(&localDescriptor)
    }

    deinit {
        __wgpuInstanceRelease(instance)
    }

    public func createSurface(_ descriptor: WGPUSurfaceDescriptor) -> Surface {
        var localDescriptor = descriptor
        return Surface(__wgpuInstanceCreateSurface(instance, &localDescriptor))
    }

    public func processEvents() {
        __wgpuInstanceProcessEvents(instance)
    }

    public func requestAdapter(_ options: WGPURequestAdapterOptions) async -> (WGPURequestAdapterStatus, Adapter, String?) {
        var localOptions = options
        return await withCheckedContinuation { continuation in
            scheduleWork { [self] in
                __wgpuInstanceRequestAdapterWithBlock(instance, &localOptions) { (status, adapter, message) in
                    continuation.resume(returning: (status, Adapter(adapter!, instance: self), (message != nil) ? String(cString: message!) : nil))
                }
            }
        }
    }

    internal func scheduleWork(workItem: @escaping WGPUWorkItem) {
        scheduleWorkBlock(workItem)
    }
}
