/*
 * Copyright (c) 2021-2024 Apple Inc. All rights reserved.
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

import WebGPU_Internal

private let largeBufferSize = 32 * 1024 * 1024

public func writeBuffer(
    queue: WebGPU.Queue, buffer: WebGPU.Buffer, bufferOffset: UInt64, data: SpanUInt8
) {
    queue.writeBuffer(buffer, bufferOffset: bufferOffset, data: data)
}

extension WebGPU.Queue {
    public func writeBuffer(_ buffer: WebGPU.Buffer, bufferOffset: UInt64, data: SpanUInt8) {
        let device = self.device()
        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }
        let noCopy = data.size() >= largeBufferSize
        guard device.device() != nil else {
            return
        }
        guard
            let tempBuffer =
                noCopy
                ? device.newBufferWithBytesNoCopy(
                    data.__dataUnsafe(), data.size(), MTLResourceOptions.storageModeShared)
                : device.newBufferWithBytes(
                    data.__dataUnsafe(), data.size(), MTLResourceOptions.storageModeShared)
        else {
            return
        }
        blitCommandEncoder.copy(
            from: tempBuffer, sourceOffset: 0, to: buffer.buffer(),
            destinationOffset: Int(bufferOffset),
            size: data.size())
        if noCopy {
            finalizeBlitCommandEncoder()
        }
    }
}
