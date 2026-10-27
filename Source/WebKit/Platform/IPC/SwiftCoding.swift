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

#if ENABLE_BACK_FORWARD_LIST_SWIFT || (ENABLE_IPC_TESTING_API && ENABLE_IPC_TESTING_SWIFT)

import WebKit_Internal

/// A malformed message argument, which makes C++ mark the message invalid.
struct IPCDecodingError: Error {}

/// The Swift counterpart of the C++ IPC::ArgumentCoder of a type.
///
/// A coder is separate from the type it decodes because one Swift type can stand for C++ types with
/// different encodings: `String?` is both a WTF::String and a std::optional<String>.
protocol IPCCoder {
    associatedtype Value: Sendable

    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Value
    static func encode(_ value: Value, to encoder: IPC.SwiftReplyEncoder)
}

/// Swift types generated from .serialization.in types marked [Swift].
enum IPCTypes {}

extension Bool: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Bool {
        var value = false
        guard decoder.decodeBool(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: Bool, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeBool(value)
    }
}

extension UInt8: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> UInt8 {
        var value: UInt8 = 0
        guard decoder.decodeUInt8(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: UInt8, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeUInt8(value)
    }
}

extension UInt16: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> UInt16 {
        var value: UInt16 = 0
        guard decoder.decodeUInt16(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: UInt16, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeUInt16(value)
    }
}

extension UInt32: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> UInt32 {
        var value: UInt32 = 0
        guard decoder.decodeUInt32(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: UInt32, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeUInt32(value)
    }
}

extension UInt64: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> UInt64 {
        var value: UInt64 = 0
        guard decoder.decodeUInt64(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: UInt64, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeUInt64(value)
    }
}

extension Int8: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Int8 {
        var value: Int8 = 0
        guard decoder.decodeInt8(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: Int8, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeInt8(value)
    }
}

extension Int16: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Int16 {
        var value: Int16 = 0
        guard decoder.decodeInt16(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: Int16, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeInt16(value)
    }
}

extension Int32: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Int32 {
        var value: Int32 = 0
        guard decoder.decodeInt32(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: Int32, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeInt32(value)
    }
}

extension Int64: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Int64 {
        var value: Int64 = 0
        guard decoder.decodeInt64(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: Int64, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeInt64(value)
    }
}

extension Float: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Float {
        var value: Float = 0
        guard decoder.decodeFloat(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: Float, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeFloat(value)
    }
}

extension Double: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Double {
        var value: Double = 0
        guard decoder.decodeDouble(&value) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: Double, to encoder: IPC.SwiftReplyEncoder) {
        encoder.encodeDouble(value)
    }
}

/// The Swift form of WTF::ObjectIdentifier and WTF::AtomicObjectIdentifier.
///
/// Each identifier type in IPCTypes is an alias of this with its own tag.
struct IPCObjectIdentifier<Tag>: Sendable, Hashable {
    let rawValue: UInt64

    init?(rawValue: UInt64) {
        guard rawValue != 0 && rawValue != UInt64.max else {
            return nil
        }
        self.rawValue = rawValue
    }
}

extension IPCObjectIdentifier: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Self {
        guard let identifier = Self(rawValue: try UInt64.decode(from: &decoder)) else {
            throw IPCDecodingError()
        }
        return identifier
    }

    static func encode(_ value: Self, to encoder: IPC.SwiftReplyEncoder) {
        UInt64.encode(value.rawValue, to: encoder)
    }
}

/// Coders for the C++ templates and WTF types that have Swift forms.
enum IPCCoding {
    /// std::optional<T>, and RefPtr<T>.
    enum StdOptional<Wrapped: IPCCoder>: IPCCoder {
        static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Wrapped.Value? {
            guard try Bool.decode(from: &decoder) else {
                return nil
            }
            return try Wrapped.decode(from: &decoder)
        }

        static func encode(_ value: Wrapped.Value?, to encoder: IPC.SwiftReplyEncoder) {
            Bool.encode(value != nil, to: encoder)
            if let value {
                Wrapped.encode(value, to: encoder)
            }
        }
    }

    /// Markable<T>, whose flag is set when the value is absent.
    enum Markable<Wrapped: IPCCoder>: IPCCoder {
        static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> Wrapped.Value? {
            if try Bool.decode(from: &decoder) {
                return nil
            }
            return try Wrapped.decode(from: &decoder)
        }

        static func encode(_ value: Wrapped.Value?, to encoder: IPC.SwiftReplyEncoder) {
            Bool.encode(value == nil, to: encoder)
            if let value {
                Wrapped.encode(value, to: encoder)
            }
        }
    }

    /// Vector<T>.
    ///
    /// Decoding one element at a time reads the same bytes as the block C++ sends for arithmetic types.
    enum Vector<Element: IPCCoder>: IPCCoder {
        static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> [Element.Value] {
            let count = try UInt64.decode(from: &decoder)
            var result: [Element.Value] = []
            // Each element takes at least a byte.
            result.reserveCapacity(Int(min(count, UInt64(decoder.remainingByteCount()))))
            for _ in 0..<count {
                result.append(try Element.decode(from: &decoder))
            }
            return result
        }

        static func encode(_ value: [Element.Value], to encoder: IPC.SwiftReplyEncoder) {
            UInt64.encode(UInt64(value.count), to: encoder)
            for element in value {
                Element.encode(element, to: encoder)
            }
        }
    }

    /// WTF::String, whose null string is nil.
    ///
    /// A Swift String cannot hold an unpaired UTF-16 surrogate, so one decodes as U+FFFD.
    enum WTFString: IPCCoder {
        private static let nullStringLength = UInt32.max

        static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> String? {
            let length = try UInt32.decode(from: &decoder)
            if length == nullStringLength {
                return nil
            }
            let is8Bit = try Bool.decode(from: &decoder)
            guard let count = Int(exactly: length), decoder.beginArray(count, is8Bit ? 1 : 2) else {
                throw IPCDecodingError()
            }
            if is8Bit {
                var scalars = String.UnicodeScalarView()
                scalars.reserveCapacity(count)
                for _ in 0..<count {
                    scalars.append(Unicode.Scalar(try UInt8.decode(from: &decoder)))
                }
                return String(scalars)
            }
            var units: [UInt16] = []
            units.reserveCapacity(count)
            for _ in 0..<count {
                units.append(try UInt16.decode(from: &decoder))
            }
            return String(decoding: units, as: Unicode.UTF16.self)
        }

        static func encode(_ value: String?, to encoder: IPC.SwiftReplyEncoder) {
            guard let value else {
                encoder.encodeUInt32(nullStringLength)
                return
            }
            if value.unicodeScalars.allSatisfy({ $0.value <= 0xFF }) {
                encoder.encodeUInt32(UInt32(value.unicodeScalars.count))
                encoder.encodeBool(true)
                for scalar in value.unicodeScalars {
                    encoder.encodeUInt8(UInt8(scalar.value))
                }
                return
            }
            encoder.encodeUInt32(UInt32(value.utf16.count))
            encoder.encodeBool(false)
            for unit in value.utf16 {
                encoder.encodeUInt16(unit)
            }
        }
    }
}

#endif // ENABLE_BACK_FORWARD_LIST_SWIFT || (ENABLE_IPC_TESTING_API && ENABLE_IPC_TESTING_SWIFT)
