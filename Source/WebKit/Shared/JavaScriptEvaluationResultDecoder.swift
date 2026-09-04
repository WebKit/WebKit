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

struct JavaScriptEvaluationResultDecoder {
    func decode<T: CommonDecodable>(
        _ type: T.Type,
        from result: consuming WebKit.JavaScriptEvaluationResult
    ) throws(CodingError.Decoding) -> T {
        let rootID = unsafe WebKit.CxxInteropSupport.jsObjectIDRawValue(result.root())

        let owned = unsafe WebKit.JavaScriptEvaluationOwnedResult(consuming: result)
        let graph = unsafe JavaScriptEvaluationDecodingGraph<UInt64>(owned)

        var decoder = JavaScriptEvaluationGraphDecoder(
            storage: Ref(graph),
            id: rootID,
            codingPath: .init([])
        )

        return try T.decode(from: &decoder)
    }
}

#endif // HAVE_NEW_CODABLE
