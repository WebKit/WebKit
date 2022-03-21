/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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
import WebGPU

func dispatchWork(callback: (() -> Void)?) {
    DispatchQueue.main.async(execute: callback!)
}
var instanceCocoaDescriptor = WGPUInstanceCocoaDescriptor(chain: WGPUChainedStruct(next: nil, sType: WGPUSType(WGPUSTypeExtended_InstanceCocoaDescriptor.rawValue)), scheduleWorkBlock: dispatchWork)
var instanceDescriptor = WGPUInstanceDescriptor(nextInChain: &instanceCocoaDescriptor.chain)
let instance = wgpuCreateInstance(&instanceDescriptor)
defer {
    wgpuInstanceRelease(instance)
}

var adapter: WGPUAdapter!
var requestAdapterOptions = WGPURequestAdapterOptions(nextInChain: nil, compatibleSurface: nil, powerPreference: WGPUPowerPreference_Undefined, forceFallbackAdapter: false)
wgpuInstanceRequestAdapterWithBlock(instance, &requestAdapterOptions) { (status: WGPURequestAdapterStatus, localAdapter: Optional<WGPUAdapter>, message: Optional<UnsafePointer<Int8>>) in
    assert(localAdapter != nil)
    adapter = localAdapter
}
defer {
    wgpuAdapterRelease(adapter)
}
print("Adapter: \(String(describing: adapter))")

var device: WGPUDevice!
var deviceDescriptor = WGPUDeviceDescriptor(nextInChain: nil, label: nil, requiredFeaturesCount: 0, requiredFeatures: nil, requiredLimits: nil)
wgpuAdapterRequestDeviceWithBlock(adapter, &deviceDescriptor) { (status: WGPURequestDeviceStatus, localDevice: Optional<WGPUDevice>, message: Optional<UnsafePointer<Int8>>) in
    assert(localDevice != nil)
    device = localDevice
}
defer {
    wgpuDeviceRelease(device)
}
print("Device: \(String(describing: device))")

wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation)

var uploadBufferDescriptor = WGPUBufferDescriptor(nextInChain: nil, label: nil, usage: WGPUBufferUsage_MapWrite.rawValue | WGPUBufferUsage_CopySrc.rawValue, size: UInt64(MemoryLayout<Int32>.size), mappedAtCreation: false)
let uploadBuffer = wgpuDeviceCreateBuffer(device, &uploadBufferDescriptor)
assert(uploadBuffer != nil)
defer {
    wgpuBufferRelease(uploadBuffer)
}

var downloadBufferDescriptor = WGPUBufferDescriptor(nextInChain: nil, label: nil, usage: WGPUBufferUsage_MapRead.rawValue | WGPUBufferUsage_CopyDst.rawValue, size: UInt64(MemoryLayout<Int32>.size), mappedAtCreation: false)
let downloadBuffer = wgpuDeviceCreateBuffer(device, &downloadBufferDescriptor)
assert(downloadBuffer != nil)
defer {
    wgpuBufferRelease(downloadBuffer)
}

wgpuBufferMapAsyncWithBlock(uploadBuffer, WGPUMapMode_Write.rawValue, 0, MemoryLayout<Int32>.size) { (status: WGPUBufferMapAsyncStatus) in
    assert(status == WGPUBufferMapAsyncStatus_Success);
    let writePointer = wgpuBufferGetMappedRange(uploadBuffer, 0, MemoryLayout<Int32>.size).bindMemory(to: Int32.self, capacity: 1)
    writePointer[0] = 17
    wgpuBufferUnmap(uploadBuffer)

    var commandEncoderDescriptor = WGPUCommandEncoderDescriptor(nextInChain: nil, label: nil)
    let commandEncoder = wgpuDeviceCreateCommandEncoder(device, &commandEncoderDescriptor)
    defer {
        wgpuCommandEncoderRelease(commandEncoder)
    }
    wgpuCommandEncoderCopyBufferToBuffer(commandEncoder, uploadBuffer, 0, downloadBuffer, 0, UInt64(MemoryLayout<Int32>.size))
    var commandBufferDescriptor = WGPUCommandBufferDescriptor(nextInChain: nil, label: nil)
    let commandBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDescriptor)
    defer {
        wgpuCommandBufferRelease(commandBuffer)
    }

    let commands: [WGPUCommandBuffer?] = [commandBuffer]
    wgpuQueueSubmit(wgpuDeviceGetQueue(device), UInt32(commands.count), commands)

    wgpuQueueOnSubmittedWorkDoneWithBlock(wgpuDeviceGetQueue(device), 0) { (status: WGPUQueueWorkDoneStatus) in
        assert(status == WGPUQueueWorkDoneStatus_Success)
        wgpuBufferMapAsyncWithBlock(downloadBuffer, WGPUMapMode_Read.rawValue, 0, MemoryLayout<Int32>.size) { (status: WGPUBufferMapAsyncStatus) in
            assert(status == WGPUBufferMapAsyncStatus_Success);
            let readPointer = wgpuBufferGetMappedRange(downloadBuffer, 0, MemoryLayout<Int32>.size).bindMemory(to: Int32.self, capacity: 1)
            print("Result: \(readPointer[0])")
            wgpuBufferUnmap(downloadBuffer)
            wgpuDevicePopErrorScopeWithBlock(device) { (type: WGPUErrorType, message: Optional<UnsafePointer<Int8>>) in
                if type != WGPUErrorType_NoError {
                    if message != nil {
                        print("Message: \(String(cString: message!))")
                    } else {
                        print("Empty message.")
                    }
                } else {
                    print("Success!")
                }
                CFRunLoopStop(CFRunLoopGetMain())
            }
        }
    }
}

CFRunLoopRun()
