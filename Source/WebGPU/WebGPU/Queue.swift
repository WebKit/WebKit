// Copyright (C) 2021-2024 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

public import Metal
internal import WebGPU_Internal.Queue

private let largeBufferSize = 32 * 1024 * 1024

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func Queue_writeBuffer_thunk(queue: WebGPU.Queue, buffer: MTLBuffer, bufferOffset: UInt64, data: WebGPU.SpanUInt8) {
    // FIXME (rdar://161269480): We should be able to declare 'data' as MutableSpan<UInt8>, which will remove this use of 'unsafe'.
    queue.writeBuffer(buffer: buffer, bufferOffset: bufferOffset, data: unsafe MutableSpan(_unsafeCxxSpan: data))
}

extension WebGPU.Queue {
    func writeBuffer(buffer: MTLBuffer, bufferOffset: UInt64, data: consuming MutableSpan<UInt8>) {
        guard self.metalDevice() != nil else {
            return
        }

        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }

        let count = data.count
        let noCopy = data.count >= largeBufferSize
        // FIXME: 'bufferWithOffset' may extend the lifetime of 'data', but we drop that information here.
        let bufferWithOffset = unsafe newTemporaryBufferWithBytes(WebGPU.SpanUInt8(data), noCopy)
        let temporaryBuffer = unsafe bufferWithOffset.first
        let temporaryBufferOffset = unsafe bufferWithOffset.second

        guard let temporaryBuffer = temporaryBuffer else {
            assertionFailure("temporaryBuffer should not be nil")
            return
        }

        blitCommandEncoder.copy(
            from: temporaryBuffer,
            sourceOffset: Int(temporaryBufferOffset),
            to: buffer,
            destinationOffset: Int(bufferOffset),
            size: count
        )

        if noCopy {
            finalizeBlitCommandEncoder()
        }
    }
}
