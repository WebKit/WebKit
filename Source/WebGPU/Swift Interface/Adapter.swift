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

public class Adapter {
    internal let adapter: __WGPUAdapter
    private let instance: Instance
    private let ownership: Ownership

    internal init(_ adapter: __WGPUAdapter, instance: Instance, ownership: Ownership = .Owned) {
        self.adapter = adapter
        self.instance = instance
        self.ownership = ownership
    }

    deinit {
        if ownership == .Owned {
            __wgpuAdapterRelease(adapter)
        }
    }

    public func enumerateFeatures() -> [WGPUFeatureName] {
        let count = __wgpuAdapterEnumerateFeatures(adapter, nil)
        var result = Array(repeating: WGPUFeatureName_Undefined, count: count)
        result.withUnsafeMutableBufferPointer { pointer in
            _ = __wgpuAdapterEnumerateFeatures(adapter, pointer.baseAddress)
        }
        return result
    }

    public func getLimits() -> WGPULimits {
        var result = WGPUSupportedLimits()
        assert(__wgpuAdapterGetLimits(adapter, &result) == true)
        return result.limits
    }

    public func getProperties() -> WGPUAdapterProperties {
        var result = WGPUAdapterProperties()
        __wgpuAdapterGetProperties(adapter, &result)
        return result
    }

    public func hasFeature(_ featureName: WGPUFeatureName) -> Bool {
        return __wgpuAdapterHasFeature(adapter, featureName)
    }

    public func requestDevice(_ options: WGPUDeviceDescriptor) async -> (WGPURequestDeviceStatus, Device, String?) {
        var localOptions = options
        return await withCheckedContinuation { continuation in
            instance.scheduleWork { [self] in
                __wgpuAdapterRequestDeviceWithBlock(adapter, &localOptions) { [self] (status, device, message) in
                    continuation.resume(returning: (status, Device(device!, instance: instance), (message != nil) ? String(cString: message!) : nil))
                }
            }
        }
    }
}
