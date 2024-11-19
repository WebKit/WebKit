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
    var bufferContents: UnsafeMutableRawBufferPointer {
        UnsafeMutableRawBufferPointer(start: m_buffer.contents(), count: m_buffer.length)
    }

    func copy(from data: SpanConstUInt8, offset: Int) {
        let slice = bufferContents[offset...]
        // copyBytes(from:) checks bounds in debug builds only.
        // FIXME: Use a bounds-checking implementation when one is available.
        precondition(slice.count <= data.size_bytes())
        slice.copyBytes(from: data)
    }
}

// FIXME(emw): Find a way to generate thunks like these, maybe via a macro?
@_expose(Cxx)
public func Buffer_copyFrom_thunk(_ buffer: WebGPU.Buffer, from data: SpanConstUInt8, offset: Int) {
    buffer.copy(from: data, offset: offset)
}
