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

private import CxxStdlib
internal import WebGPU_Internal.Buffer

extension WebGPU.Buffer {
    func copy(from source: Span<UInt8>, offset: Int) {
        // FIXME (rdar://161274084): Swift doesn't have a lifetime-safe way to return a borrowed value from a refcounted object yet.
        let bufferContents = unsafe MutableSpan(_unsafeCxxSpan: getBufferContents())

        var destination = bufferContents._consumingExtracting(droppingFirst: offset)
        destination.copyMemory(from: source)
    }
}

// FIXME(emw): Find a way to generate thunks like these, maybe via a macro?
// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func Buffer_copyFrom_thunk(_ buffer: WebGPU.Buffer, from data: WebGPU.SpanConstUInt8, offset: Int) {
    buffer.copy(from: unsafe Span<UInt8>(_unsafeCxxSpan: data), offset: offset)
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func Buffer_getMappedRange_thunk(_ buffer: WebGPU.Buffer, offset: Int, size: Int) -> WebGPU.SpanUInt8 {
    unsafe buffer.getMappedRange(offset: offset, size: size)
}

extension WebGPU.Buffer {
    func getMappedRange(offset: Int, size: Int) -> WebGPU.SpanUInt8 {
        if !isValid() {
            return unsafe WebGPU.SpanUInt8()
        }

        var rangeSize = size
        if size == WGPU_WHOLE_MAP_SIZE {
            rangeSize = max(Int(currentSize()) - offset, 0)
        }

        if !validateGetMappedRange(offset, rangeSize) {
            return unsafe WebGPU.SpanUInt8()
        }

        m_mappedRanges.add(.init(UInt(offset), UInt(offset + rangeSize)))
        m_mappedRanges.compact()

        if m_buffer.storageMode == .private || m_buffer.storageMode == .memoryless || m_buffer.length == 0 {
            return unsafe WebGPU.SpanUInt8()
        }

        return unsafe getBufferContents().subspan(offset, rangeSize)
    }
}
