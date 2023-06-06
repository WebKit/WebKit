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
import WebGPU

var instanceCocoaDescriptor = WGPUInstanceCocoaDescriptor(chain: WGPUChainedStruct(next: nil, sType: WGPUSType(rawValue: WGPUSTypeExtended_InstanceCocoaDescriptor.rawValue)), scheduleWorkBlock: { workItem in
    guard let workItem else {
        return
    }
    DispatchQueue.main.async {
        workItem()
    }
})
let instance = Instance(WGPUInstanceDescriptor(nextInChain: &instanceCocoaDescriptor.chain))!

let (adapterStatus, adapter, _) = await instance.requestAdapter(WGPURequestAdapterOptions(nextInChain: nil, compatibleSurface: nil, powerPreference: WGPUPowerPreference_Undefined, forceFallbackAdapter: false))
assert(adapterStatus == WGPURequestAdapterStatus_Success)

let (deviceStatus, device, _) = await adapter.requestDevice(WGPUDeviceDescriptor(nextInChain: nil, label: nil, requiredFeaturesCount: 0, requiredFeatures: nil, requiredLimits: nil, defaultQueue: WGPUQueueDescriptor(nextInChain: nil, label: nil)))
assert(deviceStatus == WGPURequestDeviceStatus_Success)

device.pushErrorScope(WGPUErrorFilter_Validation)

let uploadBuffer = device.createBuffer(WGPUBufferDescriptor(nextInChain: nil, label: nil, usage: WGPUBufferUsage_MapWrite.rawValue | WGPUBufferUsage_CopySrc.rawValue, size: UInt64(MemoryLayout<Int32>.size), mappedAtCreation: false))
let downloadBuffer = device.createBuffer(WGPUBufferDescriptor(nextInChain: nil, label: nil, usage: WGPUBufferUsage_MapRead.rawValue | WGPUBufferUsage_CopyDst.rawValue, size: UInt64(MemoryLayout<Int32>.size), mappedAtCreation: false))

let mapUploadStatus = await uploadBuffer.mapAsync(mode: WGPUMapMode_Write, offset: 0, size: MemoryLayout<Int32>.size)
assert(mapUploadStatus == WGPUBufferMapAsyncStatus_Success)
let writePointer = uploadBuffer.getMappedRange(offset: 0, size: MemoryLayout<Int32>.size)
writePointer!.withMemoryRebound(to: Int32.self, capacity: 1) { pointer in
    pointer[0] = 17
}
uploadBuffer.unmap()

let commandEncoder = device.createCommandEncoder(WGPUCommandEncoderDescriptor(nextInChain: nil, label: nil))
commandEncoder.copyBufferToBuffer(source: uploadBuffer, sourceOffset: 0, destination: downloadBuffer, destinationOffset: 0, size: UInt64(MemoryLayout<Int32>.size))
let commandBuffer = commandEncoder.finish(WGPUCommandBufferDescriptor(nextInChain: nil, label: nil))

let queue: Queue = device.getQueue()
queue.submit([commandBuffer])

let copyStatus = await queue.onSubmittedWorkDone()
assert(copyStatus == WGPUQueueWorkDoneStatus_Success)

let mapDownloadStatus = await downloadBuffer.mapAsync(mode: WGPUMapMode_Read, offset: 0, size: MemoryLayout<Int32>.size)
assert(mapDownloadStatus == WGPUBufferMapAsyncStatus_Success)
let readPointer = downloadBuffer.getMappedRange(offset: 0, size: MemoryLayout<Int32>.size)
let result = readPointer!.withMemoryRebound(to: Int32.self, capacity: 1) { pointer in
    return pointer[0]
}
print("Result: \(result)")
downloadBuffer.unmap()

let (errorType, message) = await device.popErrorScope()
if errorType != WGPUErrorType_NoError {
    if message != nil {
        print("Message: \(message!)")
    } else {
        print("Empty message")
    }
} else {
    print("Success!")
}

print("Done")
