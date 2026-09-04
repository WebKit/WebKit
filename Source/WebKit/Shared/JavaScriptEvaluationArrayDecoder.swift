// Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if HAVE_NEW_CODABLE

import Foundation
import NewCodable
import WebKit_Internal

struct JavaScriptEvaluationArrayDecoder<ID: Hashable & Sendable>: CommonArrayDecoder, ~Escapable {
    typealias ElementDecoder = JavaScriptEvaluationGraphDecoder<ID>

    private let storage: Ref<JavaScriptEvaluationDecodingGraph<ID>>
    private let ids: [ID]

    private var index = 0

    let sizeHint: Int?
    let codingPath: CodingPath

    @_lifetime(copy storage)
    init(storage: Ref<JavaScriptEvaluationDecodingGraph<ID>>, ids: [ID], codingPath: CodingPath) {
        self.storage = storage
        self.ids = ids
        self.codingPath = codingPath
        self.sizeHint = ids.count
    }

    @_lifetime(self: copy self)
    mutating func decodeNext<T: ~Copyable>(
        _ closure: (inout ElementDecoder) throws(CodingError.Decoding) -> T
    ) throws(CodingError.Decoding) -> T? {
        guard index < ids.count else {
            return nil
        }

        var decoder = createDecoder(index: index)
        index += 1
        return try closure(&decoder)
    }

    @_lifetime(self: copy self)
    mutating func decodeEachElement(
        _ closure: (inout ElementDecoder) throws(CodingError.Decoding) -> Void
    ) throws(CodingError.Decoding) {
        while index < ids.count {
            var decoder = createDecoder(index: index)
            index += 1
            try closure(&decoder)
        }
    }

    @_lifetime(copy self)
    private func createDecoder(index: Int) -> ElementDecoder {
        ElementDecoder(
            storage: storage,
            id: ids[index],
            codingPath: codingPath.appending(index: index)
        )
    }
}

#endif // HAVE_NEW_CODABLE
