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

extension WebGPU.Buffer {
    func copy(from data: Span<UInt8>, offset: Int) {
        // FIXME: Use a bounds-checking implementation when one is available.
        var bufferContents = unsafe MutableSpan<UInt8>(_unsafeCxxSpan: getBufferContents());
        precondition(bufferContents.count >= offset + data.count)
        for i in 0..<data.count {
            bufferContents[offset + i] = data[i]
        }
    }
}

// FIXME(emw): Find a way to generate thunks like these, maybe via a macro?
@_expose(Cxx)
public func Buffer_copyFrom_thunk(_ buffer: WebGPU.Buffer, from data: SpanConstUInt8, offset: Int) {
    buffer.copy(from: unsafe Span<UInt8>(_unsafeCxxSpan: data), offset: offset)
}

@_expose(Cxx)
public func Buffer_getMappedRange_thunk(_ buffer: WebGPU.Buffer, offset: Int, size: Int) -> SpanUInt8 {
    return buffer.getMappedRange(offset: offset, size: size)
}

internal func computeRangeSize(size: Int, offset: Int) -> Int
{
    let result = WebGPU_Internal.checkedDifferenceSizeT(size, offset)
    if result.hasOverflowed() {
        return 0
    }
    return result.value()
}

extension WebGPU.Buffer {
    public func getMappedRange(offset: Int, size: Int) -> SpanUInt8
    {
        if !isValid() {
            return unsafe SpanUInt8()
        }

        var rangeSize = size
        if size == WGPU_WHOLE_MAP_SIZE {
            rangeSize = computeRangeSize(size: Int(currentSize()), offset: offset)
        }

        if !validateGetMappedRange(offset, rangeSize) {
            return unsafe SpanUInt8()
        }

        m_mappedRanges.add(WTFRangeSizeT(UInt(offset), UInt(offset + rangeSize)))
        m_mappedRanges.compact()

        if m_buffer.storageMode == .private || m_buffer.storageMode == .memoryless || m_buffer.length == 0 {
            return unsafe SpanUInt8()
        }

        return unsafe getBufferContents().subspan(offset, rangeSize)
    }
}
