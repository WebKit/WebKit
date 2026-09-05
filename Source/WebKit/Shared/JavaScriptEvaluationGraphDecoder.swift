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

struct JavaScriptEvaluationGraphDecoder<ID: Hashable & Sendable>: CommonDecoder, ~Escapable {
    typealias StructDecoder = JavaScriptEvaluationObjectDecoder<ID>
    typealias DictionaryDecoder = JavaScriptEvaluationObjectDecoder<ID>
    typealias ArrayDecoder = JavaScriptEvaluationArrayDecoder<ID>
    typealias FieldDecoder = JavaScriptEvaluationFieldDecoder

    private let storage: Ref<JavaScriptEvaluationDecodingGraph<ID>>
    private let id: ID

    let codingPath: CodingPath
    let supportsDecodeAny = true

    @_lifetime(copy storage)
    init(
        storage: Ref<JavaScriptEvaluationDecodingGraph<ID>>,
        id: ID,
        codingPath: CodingPath,
    ) {
        self.storage = storage
        self.id = id
        self.codingPath = codingPath
    }

    // Intentionally does not implement the `mutating func decode<T: CommonDecodable>(_: T.Type) throws(CodingError.Decoding) -> T` requirement; the default implementation is sound.

    @_lifetime(self: copy self)
    mutating func decodeStruct<T: ~Copyable>(
        _ closure: (inout StructDecoder) throws(CodingError.Decoding) -> T
    ) throws(CodingError.Decoding) -> T {
        let object = try decodePrimitive([JavaScriptEvaluationCodableValue<ID>.ObjectEntry].self) {
            guard case .object(let value) = $0 else {
                return nil
            }

            return value
        }

        var decoder = StructDecoder(storage: storage, entries: object, codingPath: codingPath)
        return try closure(&decoder)
    }

    @_lifetime(self: copy self)
    mutating func decodeDictionary<T: ~Copyable>(
        _ closure: (inout DictionaryDecoder) throws(CodingError.Decoding) -> T
    ) throws(CodingError.Decoding) -> T {
        try decodeStruct(closure)
    }

    @_lifetime(self: copy self)
    mutating func decodeArray<T: ~Copyable>(
        _ closure: (inout ArrayDecoder) throws(CodingError.Decoding) -> T
    ) throws(CodingError.Decoding) -> T {
        let array = try decodePrimitive([ID].self) {
            guard case .array(let value) = $0 else {
                return nil
            }

            return value
        }

        var decoder = ArrayDecoder(storage: storage, ids: array, codingPath: codingPath)
        return try closure(&decoder)
    }

    @_lifetime(self: copy self)
    mutating func decodeEnumCase<T: ~Copyable>(
        _ caseDecoder: (inout FieldDecoder) throws(CodingError.Decoding) -> Void,
        associatedValues valueDecoder: (inout StructDecoder) throws(CodingError.Decoding) -> T
    ) throws(CodingError.Decoding) -> T {
        let entries = try decodePrimitive([JavaScriptEvaluationCodableValue<ID>.ObjectEntry].self) {
            guard case .object(let value) = $0 else {
                return nil
            }

            return value
        }

        guard entries.count == 1, let entry = entries.first else {
            throw CodingError.dataCorrupted(
                at: codingPath,
                debugDescription: "Expected exactly one entry for an enum case, found \(entries.count)."
            )
        }

        let name = try storage.value.keyName(for: entry.key, at: codingPath)

        var fieldDecoder = FieldDecoder(string: name)
        try caseDecoder(&fieldDecoder)

        var associatedValues = Self(storage: storage, id: entry.value, codingPath: codingPath.appending(name))
        return try associatedValues.decodeStruct(valueDecoder)
    }

    // Intentionally does not implement the `mutating func decode<Key: CodingStringKeyRepresentable, Value: CommonDecodable>(_: [Key: Value].Type, sizeHint: Int) throws(CodingError.Decoding) -> [Key: Value]` requirement; the default implementation is sound.

    // Intentionally does not implement the `mutating func decode<Element: CommonDecodable>(_: [Element].Type, sizeHint: Int) throws(CodingError.Decoding) -> [Element]` requirement; the default implementation is sound.

    @_lifetime(self: copy self)
    mutating func decode(_: Bool.Type) throws(CodingError.Decoding) -> Bool {
        try decodePrimitive {
            guard case .bool(let value) = $0 else {
                return nil
            }

            return value
        }
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: Int.Type) throws(CodingError.Decoding) -> Int {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: Int8.Type) throws(CodingError.Decoding) -> Int8 {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: Int16.Type) throws(CodingError.Decoding) -> Int16 {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: Int32.Type) throws(CodingError.Decoding) -> Int32 {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: Int64.Type) throws(CodingError.Decoding) -> Int64 {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: Int128.Type) throws(CodingError.Decoding) -> Int128 {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: UInt.Type) throws(CodingError.Decoding) -> UInt {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: UInt8.Type) throws(CodingError.Decoding) -> UInt8 {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: UInt16.Type) throws(CodingError.Decoding) -> UInt16 {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: UInt32.Type) throws(CodingError.Decoding) -> UInt32 {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: UInt64.Type) throws(CodingError.Decoding) -> UInt64 {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: UInt128.Type) throws(CodingError.Decoding) -> UInt128 {
        try decodeInteger()
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: Float.Type) throws(CodingError.Decoding) -> Float {
        let double = try decodeNumber(narrowingTo: Float.self)
        let float = Float(double)

        func isOrdinary(_ value: some FloatingPoint) -> Bool {
            value.isFinite && !value.isZero
        }

        // A finite, nonzero value is the only kind narrowing can damage; +/-0, +/-infinity and
        // NaN all convert exactly.
        guard isOrdinary(float) || !isOrdinary(double) else {
            throw CodingError.dataCorrupted(
                at: codingPath,
                debugDescription: "\(double) is not representable in Float."
            )
        }

        return float
    }

    @_lifetime(self: copy self)
    mutating func decode(_ hint: Double.Type) throws(CodingError.Decoding) -> Double {
        try decodeNumber(narrowingTo: Double.self)
    }

    @_lifetime(self: copy self)
    mutating func decode(_: String.Type) throws(CodingError.Decoding) -> String {
        try decodePrimitive {
            guard case .string(let value) = $0 else {
                return nil
            }

            return value
        }
    }

    @_lifetime(self: copy self)
    mutating func decodeString<V: DecodingStringVisitor & ~Copyable>(
        _ visitor: borrowing V
    ) throws(CodingError.Decoding) -> V.DecodedValue {
        try visitor.visitString(decode(String.self))
    }

    @_lifetime(self: copy self)
    mutating func decodeNil() throws(CodingError.Decoding) -> Bool {
        try decodePrimitive {
            guard case .empty = $0 else {
                return false
            }

            // `undefined` counts as nil alongside `null`.
            return true
        }
    }

    @_lifetime(self: copy self)
    mutating func decodeOptional(_ closure: (inout Self) throws(CodingError.Decoding) -> Void) throws(CodingError.Decoding) {
        if try decodeNil() {
            return
        }

        try closure(&self)
    }

    @_lifetime(self: copy self)
    mutating func decodeAny<V: CommonDecodingVisitor>(_ visitor: V) throws(CodingError.Decoding) -> V.DecodedValue {
        fatalError("\(#function) is not implemented")
    }

    @_lifetime(self: copy self)
    mutating func decodeBytes<V: DecodingBytesVisitor>(visitor: V) throws(CodingError.Decoding) -> V.DecodedValue {
        fatalError("\(#function) is not implemented")
    }

    @_disfavoredOverload
    @_lifetime(self: copy self)
    mutating func decode<D: Decodable>(_: D.Type) throws(CodingError.Decoding) -> D {
        fatalError("\(#function) is not implemented")
    }
}

extension JavaScriptEvaluationGraphDecoder {
    private func codableValue() throws(CodingError.Decoding) -> JavaScriptEvaluationCodableValue<ID> {
        guard let value = storage.value.map[id] else {
            throw CodingError.dataCorrupted(
                at: codingPath,
                debugDescription: "No entry for \(id)."
            )
        }

        return value
    }

    private func decodePrimitive<T>(
        _ type: T.Type = T.self,
        resolve: (JavaScriptEvaluationCodableValue<ID>) -> T?
    ) throws(CodingError.Decoding) -> T {
        let value = try codableValue()

        guard let resolvedValue = resolve(value) else {
            throw CodingError.typeMismatch(
                expectedTypeDescription: "\(T.self)",
                actualValueDescription: "\(value)",
                at: codingPath
            )
        }

        return resolvedValue
    }

    private func decodeNumber<T>(narrowingTo type: T.Type) throws(CodingError.Decoding) -> Double {
        let value = try codableValue()

        guard case .number(let number) = value else {
            throw CodingError.typeMismatch(
                expectedTypeDescription: "\(T.self)",
                actualValueDescription: "\(value)",
                at: codingPath
            )
        }

        return number
    }

    private func decodeInteger<T: FixedWidthInteger>(_ type: T.Type = T.self) throws(CodingError.Decoding) -> T {
        let value = try decodeNumber(narrowingTo: T.self)

        guard let integer = T(exactly: value) else {
            throw CodingError.dataCorrupted(
                at: codingPath,
                debugDescription: "\(value) is not representable in \(T.self)."
            )
        }

        return integer
    }
}

#endif // HAVE_NEW_CODABLE
