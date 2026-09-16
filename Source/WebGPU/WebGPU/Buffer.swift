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

import WebGPU_Internal.Buffer
import WebGPU_Private.CxxBridgingPublic
import wtf

extension WebGPU.BufferBorrow {
    func copy(from source: WTF.ByteSpan, offset: Int) {
        var destination = bytes().subspan(offset, source.size())
        WTF.copyByteSpan(&destination, source)
    }
}

@_expose(Cxx)
func bufferCopyFrom(_ borrow: WebGPU.BufferBorrow, from data: WTF.ByteSpan, offset: Int) {
    borrow.copy(from: data, offset: offset)
}

@_expose(Cxx)
@_lifetime(copy borrow)
func bufferGetMappedRange(
    _ borrow: WebGPU.BufferBorrow,
    offset: Int,
    size: Int,
) -> WTF.MutableByteSpan {
    borrow.getMappedRange(offset: offset, size: size)
}

extension WebGPU.Buffer {
    // Validates a getMappedRange() request and records the range as mapped, returning the
    // size of the range to hand out, or nil if there is nothing to map. Separate from
    // BufferBorrow.getMappedRange() because it needs Buffer's private members.
    func recordMappedRange(offset: Int, size: Int) -> Int? {
        if !isValid() {
            return nil
        }

        var rangeSize = size
        if size == WGPU_WHOLE_MAP_SIZE {
            rangeSize = max(Int(currentSize()) - offset, 0)
        }

        if !validateGetMappedRange(offset, rangeSize) {
            return nil
        }

        m_mappedRanges.add(.init(UInt(offset), UInt(offset + rangeSize)))
        m_mappedRanges.compact()

        if m_buffer.storageMode == .private || m_buffer.storageMode == .memoryless || m_buffer.length == 0 {
            return nil
        }

        return rangeSize
    }
}

extension WebGPU.BufferBorrow {
    @_lifetime(copy self)
    func getMappedRange(offset: Int, size: Int) -> WTF.MutableByteSpan {
        guard let rangeSize = buffer().recordMappedRange(offset: offset, size: size) else {
            return WTF.MutableByteSpan()
        }

        return bytes().subspan(offset, rangeSize)
    }
}
