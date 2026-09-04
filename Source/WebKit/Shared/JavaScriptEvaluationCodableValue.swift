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
import WebKit_Internal

enum JavaScriptEvaluationCodableValue<ID: Hashable & Sendable>: Sendable, Hashable {
    enum EmptyType: Sendable, Hashable {
        case undefined
        case null
    }

    struct ObjectEntry: Hashable, Sendable {
        let key: ID
        let value: ID
    }

    case empty(EmptyType)
    case bool(Bool)
    case number(Double)
    case string(String)
    case seconds(Double)
    case array([ID])
    case object([ObjectEntry])
}

extension JavaScriptEvaluationCodableValue.EmptyType {
    fileprivate init(_ emptyType: WebKit.JavaScriptEvaluationResult.EmptyType) {
        self = emptyType == .Null ? .null : .undefined
    }
}

extension JavaScriptEvaluationCodableValue {
    init(_ value: borrowing WebKit.JavaScriptEvaluationResult.Value) {
        typealias Cxx = WebKit.CxxInteropSupport

        self =
            switch unsafe value.index() {
            case Cxx.alternativeIndexForJavaScriptEvaluationResultValue(Alternative: WebKit.JavaScriptEvaluationResult.EmptyType.self):
                unsafe .empty(.init(Cxx.alternativeForVariant(value)))
            case Cxx.alternativeIndexForJavaScriptEvaluationResultValue(Alternative: CBool.self):
                unsafe .bool(Cxx.alternativeForVariant(value))
            case Cxx.alternativeIndexForJavaScriptEvaluationResultValue(Alternative: CDouble.self):
                unsafe .number(Cxx.alternativeForVariant(value))
            case Cxx.alternativeIndexForJavaScriptEvaluationResultValue(Alternative: WTF.String.self):
                unsafe .string((Cxx.alternativeForVariant(value) as WTF.String).description)
            case 4: // Seconds
                fatalError("not yet implemented")
            case 5: // Vector<JSObjectID>
                fatalError("not yet implemented")
            case 6: // ObjectMap
                fatalError("not yet implemented")
            case 7: // UniqueRef<JSHandleInfo>
                fatalError("not yet implemented")
            case 8: // UniqueRef<WebCore::SerializedNode>
                fatalError("not yet implemented")
            default:
                fatalError("invalid index")
            }
    }
}

#endif // HAVE_NEW_CODABLE
