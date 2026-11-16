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

enum JavaScriptEvaluationCodableValue<ID: Hashable & Sendable>: Hashable, Sendable {
    enum EmptyType: Hashable, Sendable {
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

extension JavaScriptEvaluationCodableValue<UInt64> {
    init(_ value: borrowing WebKit.JavaScriptEvaluationResult.Value) {
        typealias Cxx = WebKit.CxxInteropSupport

        self =
            switch value.index() {
            case Cxx.alternativeIndexForJavaScriptEvaluationResultValue(Alternative: WebKit.JavaScriptEvaluationResult.EmptyType.self):
                .empty(.init(Cxx.alternativeForVariant(value)))

            case Cxx.alternativeIndexForJavaScriptEvaluationResultValue(Alternative: CBool.self):
                .bool(Cxx.alternativeForVariant(value))

            case Cxx.alternativeIndexForJavaScriptEvaluationResultValue(Alternative: CDouble.self):
                .number(Cxx.alternativeForVariant(value))

            case Cxx.alternativeIndexForJavaScriptEvaluationResultValue(Alternative: WTF.String.self):
                .string((Cxx.alternativeForVariant(value) as WTF.String).description)

            case 4: // Seconds
                fatalError("not yet implemented")

            case Cxx.alternativeIndexForJavaScriptEvaluationResultValue(
                Alternative: WebKit.JavaScriptEvaluationResult.ObjectVector.self
            ):
                .array(
                    Array(Cxx.alternativeForVariant(value) as WebKit.JavaScriptEvaluationResult.ObjectVector).map(Cxx.jsObjectIDRawValue)
                )

            case Cxx.alternativeIndexForJavaScriptEvaluationResultValue(
                Alternative: WebKit.JavaScriptEvaluationResult.ObjectMap.self
            ):
                .object(
                    Array(Cxx.alternativeForVariant(value) as WebKit.JavaScriptEvaluationResult.ObjectMap)
                        .map {
                            ObjectEntry(key: Cxx.jsObjectIDRawValue($0.key), value: Cxx.jsObjectIDRawValue($0.value))
                        }
                )

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
