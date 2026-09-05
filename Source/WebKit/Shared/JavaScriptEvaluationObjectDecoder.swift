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

struct JavaScriptEvaluationObjectDecoder<ID: Hashable & Sendable>: ~Escapable {
    typealias FieldDecoder = JavaScriptEvaluationFieldDecoder
    typealias ValueDecoder = JavaScriptEvaluationGraphDecoder<ID>
    typealias KeyDecoder = JavaScriptEvaluationGraphDecoder<ID>

    private let storage: Ref<JavaScriptEvaluationDecodingGraph<ID>>
    private let entries: [JavaScriptEvaluationCodableValue<ID>.ObjectEntry]

    private var index = 0

    let codingPath: CodingPath
    let sizeHint: Int?

    @_lifetime(copy storage)
    init(
        storage: Ref<JavaScriptEvaluationDecodingGraph<ID>>,
        entries: [JavaScriptEvaluationCodableValue<ID>.ObjectEntry],
        codingPath: CodingPath
    ) {
        self.storage = storage
        self.entries = entries
        self.sizeHint = entries.count
        self.codingPath = codingPath
    }

    private func keyName(entry: JavaScriptEvaluationCodableValue<ID>.ObjectEntry) throws(CodingError.Decoding) -> String {
        try storage.value.keyName(for: entry.key, at: codingPath)
    }

    @_lifetime(copy self)
    private func valueDecoder(entry: JavaScriptEvaluationCodableValue<ID>.ObjectEntry, name: String) -> ValueDecoder {
        ValueDecoder(
            storage: storage,
            id: entry.value,
            codingPath: codingPath.appending(name)
        )
    }

    @_lifetime(copy self)
    private func keyDecoder(entry: JavaScriptEvaluationCodableValue<ID>.ObjectEntry, name: String) -> KeyDecoder {
        KeyDecoder(
            storage: storage,
            id: entry.key,
            codingPath: codingPath.appending(name)
        )
    }
}

extension JavaScriptEvaluationObjectDecoder: CommonStructDecoder {
    @_lifetime(self: copy self)
    mutating func decodeExpectedOrderField(
        required: Bool,
        matchingClosure: (UTF8Span) -> Bool,
        andValue valueDecoderClosure: (inout ValueDecoder) throws(CodingError.Decoding) -> Void
    ) throws(CodingError.Decoding) -> Bool {
        guard index < entries.count else {
            return !required
        }

        let entry = entries[index]
        let name = try keyName(entry: entry)
        guard matchingClosure(name.utf8Span) else {
            return !required
        }

        index += 1
        var value = valueDecoder(entry: entry, name: name)
        try valueDecoderClosure(&value)
        return true
    }

    @_lifetime(self: copy self)
    mutating func decodeEachField(
        _ fieldDecoderClosure: (inout FieldDecoder) throws(CodingError.Decoding) -> Void,
        andValue valueDecoderClosure: (inout ValueDecoder) throws(CodingError.Decoding) -> Void
    ) throws(CodingError.Decoding) {
        var fieldDecoder = JavaScriptEvaluationFieldDecoder(string: "")

        while index < entries.count {
            let entry = entries[index]
            index += 1

            let name = try keyName(entry: entry)
            fieldDecoder.string = name
            try fieldDecoderClosure(&fieldDecoder)

            var value = valueDecoder(entry: entry, name: name)
            try valueDecoderClosure(&value)
        }
    }

    @_lifetime(self: copy self)
    mutating func decodeEachKeyAndValue(
        _ closure: (String, inout ValueDecoder) throws(CodingError.Decoding) -> Bool
    ) throws(CodingError.Decoding) {
        while index < entries.count {
            let entry = entries[index]
            index += 1

            let name = try keyName(entry: entry)
            var value = valueDecoder(entry: entry, name: name)
            if try closure(name, &value) {
                break
            }
        }
    }
}

extension JavaScriptEvaluationObjectDecoder: CommonDictionaryDecoder {
    @_lifetime(self: copy self)
    mutating func decodeEachKey(
        _ keyDecodingClosure: (inout KeyDecoder) throws(CodingError.Decoding) -> Void,
        andValue valueDecoderClosure: (inout ValueDecoder) throws(CodingError.Decoding) -> Void
    ) throws(CodingError.Decoding) {
        while index < entries.count {
            let entry = entries[index]
            index += 1

            let name = try keyName(entry: entry)

            var key = keyDecoder(entry: entry, name: name)
            try keyDecodingClosure(&key)

            var value = valueDecoder(entry: entry, name: name)
            try valueDecoderClosure(&value)
        }
    }

    @_lifetime(self: copy self)
    mutating func decodeKey(
        _ keyDecodingClosure: (inout KeyDecoder) throws(CodingError.Decoding) -> Void,
        andValue valueDecoderClosure: (inout ValueDecoder) throws(CodingError.Decoding) -> Void
    ) throws(CodingError.Decoding) -> Bool {
        guard index < entries.count else {
            return false
        }

        let entry = entries[index]
        index += 1

        let name = try keyName(entry: entry)

        var key = keyDecoder(entry: entry, name: name)
        try keyDecodingClosure(&key)

        var value = valueDecoder(entry: entry, name: name)
        try valueDecoderClosure(&value)

        return true
    }
}

#endif // HAVE_NEW_CODABLE
