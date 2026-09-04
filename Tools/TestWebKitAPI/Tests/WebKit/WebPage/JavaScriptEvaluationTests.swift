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

#if ENABLE_SWIFTUI && HAVE_NEW_CODABLE

@_spi(Experimental_NewCodable) import WebKit
import Testing
import NewCodable
import struct Swift.String

@MainActor
struct JavaScriptEvaluationTests {
    let page = WebPage()

    @Test
    func decodingBool() async throws {
        try await testDecoding(true) { "return true;" }
        try await testDecoding(false) { "return false;" }
        try await testDecoding(true) { "return 1 === 1;" }

        await testTypeMismatch(decoding: Bool.self) { "return 1;" }
        await testTypeMismatch(decoding: Bool.self) { "return 0;" }
        await testTypeMismatch(decoding: Bool.self) { #"return "true";"# }
    }

    @Test
    func decodingDouble() async throws {
        try await testDecoding(42.5) { "return 42.5;" }
        try await testDecoding(-0.25) { "return -0.25;" }
        try await testDecoding(0.0) { "return 0;" }
        try await testDecoding(3.0) { "return 1 + 2;" }
        try await testDecoding(9_007_199_254_740_991.0) { "return Number.MAX_SAFE_INTEGER;" }
        try await testDecoding(Double.greatestFiniteMagnitude) { "return Number.MAX_VALUE;" }
        try await testDecoding(Double.leastNonzeroMagnitude) { "return Number.MIN_VALUE;" }
        try await testDecoding(Double.infinity) { "return Infinity;" }
        try await testDecoding(-Double.infinity) { "return -Infinity;" }

        // NaN is not equal to itself, so it cannot use the equality-based helper.
        let nan = try await page.callJavaScriptv0(returning: Double.self) { "return NaN;" }
        #expect(nan.isNaN)

        await testTypeMismatch(decoding: Double.self) { #"return "42.5";"# }
        await testTypeMismatch(decoding: Double.self) { "return true;" }
    }

    @Test
    func decodingFloat() async throws {
        try await testDecoding(Float(0.5)) { "return 0.5;" }
        try await testDecoding(Float(-1.25)) { "return -1.25;" }
        try await testDecoding(Float(0)) { "return 0;" }

        // Rounding to the nearest Float loses precision, not the value, so it is allowed.
        try await testDecoding(Float(0.1)) { "return 0.1;" }

        // Infinity and NaN are exactly representable, so they are passed through rather than
        // treated as an overflow.
        try await testDecoding(Float.infinity) { "return Infinity;" }
        try await testDecoding(-Float.infinity) { "return -Infinity;" }

        let nan = try await page.callJavaScriptv0(returning: Float.self) { "return NaN;" }
        #expect(nan.isNaN)

        // The largest and smallest magnitudes Float can hold are fine, but a finite value
        // past either one keeps none of its magnitude, so it is rejected instead of becoming
        // infinity or zero.
        try await testDecoding(Float.greatestFiniteMagnitude) { "return 3.4028234663852886e38;" }
        try await testDecoding(-Float.greatestFiniteMagnitude) { "return -3.4028234663852886e38;" }
        try await testDecoding(Float.leastNonzeroMagnitude) { "return 1.401298464324817e-45;" }

        await testDataCorrupted(decoding: Float.self) { "return 1e39;" }
        await testDataCorrupted(decoding: Float.self) { "return -1e39;" }
        await testDataCorrupted(decoding: Float.self) { "return Number.MAX_VALUE;" }
        await testDataCorrupted(decoding: Float.self) { "return 1e-46;" }
        await testDataCorrupted(decoding: Float.self) { "return Number.MIN_VALUE;" }

        await testTypeMismatch(decoding: Float.self) { #"return "0.5";"# }
        await testTypeMismatch(decoding: Float.self) { "return false;" }
    }

    @Test
    func decodingInt() async throws {
        try await testDecoding(42) { "return 42;" }
        try await testDecoding(-42) { "return -42;" }
        try await testDecoding(0) { "return 0;" }
        try await testDecoding(3) { "return 1 + 2;" }
        try await testDecoding(9_007_199_254_740_991) { "return Number.MAX_SAFE_INTEGER;" }
        try await testDecoding(-9_007_199_254_740_991) { "return Number.MIN_SAFE_INTEGER;" }

        // A value which is not integral, or which is out of range, is rejected rather than
        // truncated or wrapped. The value arrived with the right type, so this is corrupt
        // data rather than a type mismatch.
        await testDataCorrupted(decoding: Int.self) { "return 1.5;" }
        await testDataCorrupted(decoding: Int.self) { "return -1.5;" }
        await testDataCorrupted(decoding: Int.self) { "return NaN;" }
        await testDataCorrupted(decoding: Int.self) { "return Infinity;" }
        await testDataCorrupted(decoding: Int.self) { "return Number.MAX_VALUE;" }

        await testTypeMismatch(decoding: Int.self) { #"return "42";"# }
        await testTypeMismatch(decoding: Int.self) { "return true;" }
    }

    @Test
    func decodingSignedIntegers() async throws {
        try await testDecoding(Int8.max) { "return 127;" }
        try await testDecoding(Int8.min) { "return -128;" }
        await testDataCorrupted(decoding: Int8.self) { "return 128;" }
        await testDataCorrupted(decoding: Int8.self) { "return -129;" }

        try await testDecoding(Int16.max) { "return 32767;" }
        try await testDecoding(Int16.min) { "return -32768;" }
        await testDataCorrupted(decoding: Int16.self) { "return 32768;" }

        try await testDecoding(Int32.max) { "return 2147483647;" }
        try await testDecoding(Int32.min) { "return -2147483648;" }
        await testDataCorrupted(decoding: Int32.self) { "return 2147483648;" }

        try await testDecoding(Int64(9_007_199_254_740_991)) { "return Number.MAX_SAFE_INTEGER;" }
        await testDataCorrupted(decoding: Int64.self) { "return 2 ** 63;" }

        try await testDecoding(Int128(-12345)) { "return -12345;" }
        await testDataCorrupted(decoding: Int128.self) { "return 1.5;" }
    }

    @Test
    func decodingUnsignedIntegers() async throws {
        try await testDecoding(UInt(42)) { "return 42;" }
        try await testDecoding(UInt(0)) { "return 0;" }
        await testDataCorrupted(decoding: UInt.self) { "return -1;" }

        try await testDecoding(UInt8.max) { "return 255;" }
        await testDataCorrupted(decoding: UInt8.self) { "return 256;" }

        try await testDecoding(UInt16.max) { "return 65535;" }
        await testDataCorrupted(decoding: UInt16.self) { "return 65536;" }

        try await testDecoding(UInt32.max) { "return 4294967295;" }
        await testDataCorrupted(decoding: UInt32.self) { "return 4294967296;" }

        try await testDecoding(UInt64(9_007_199_254_740_991)) { "return Number.MAX_SAFE_INTEGER;" }
        await testDataCorrupted(decoding: UInt64.self) { "return 2 ** 64;" }

        try await testDecoding(UInt128(12345)) { "return 12345;" }
        await testDataCorrupted(decoding: UInt128.self) { "return -12345;" }
    }

    @Test
    func decodingString() async throws {
        try await testDecoding("hello") { #"return "hello";"# }
        try await testDecoding("") { #"return "";"# }
        try await testDecoding("foo1") { #"return "foo" + 1;"# }
        try await testDecoding("42") { "return String(42);" }
        try await testDecoding("a\nb\tc\"d") { #"return "a\nb\tc\"d";"# }
        try await testDecoding(String(repeating: "x", count: 1000)) { #"return "x".repeat(1000);"# }

        await testTypeMismatch(decoding: String.self) { "return 42;" }
        await testTypeMismatch(decoding: String.self) { "return true;" }
    }

    @Test
    func decodingNonASCIIString() async throws {
        try await testDecoding("héllo wörld") { #"return "héllo wörld";"# }
        try await testDecoding("日本語") { #"return "日本語";"# }
        try await testDecoding("👩‍👩‍👧‍👦🇨🇦") { #"return "👩‍👩‍👧‍👦🇨🇦";"# }

        // Unpaired surrogates are not representable in UTF-8, and are replaced during conversion.
        try await testDecoding("\u{FFFD}") { #"return "\uD800";"# }
    }

    @Test
    func decodingOptional() async throws {
        try await testDecoding(Int?.none) { "return null;" }
        try await testDecoding(String?.none) { "return null;" }
        try await testDecoding(Bool?.none) { "return null;" }

        // `undefined` is a missing value too, however it was produced.
        try await testDecoding(Int?.none) { "return undefined;" }
        try await testDecoding(Int?.none) { "return ({}).missingProperty;" }
        try await testDecoding(Int?.none) { "let unused = 1;" }

        try await testDecoding(Int?.some(0)) { "return 0;" }
        try await testDecoding(String?.some("hello")) { #"return "hello";"# }
        try await testDecoding(String?.some("")) { #"return "";"# }
        try await testDecoding(Bool?.some(false)) { "return false;" }
        try await testDecoding(Double?.some(2.5)) { "return 2.5;" }

        // A value present but of the wrong type is still a mismatch, not a nil.
        await testTypeMismatch(decoding: Int?.self) { #"return "42";"# }

        // A missing value is only acceptable where the Swift type is optional.
        await testTypeMismatch(decoding: Int.self) { "return null;" }
        await testTypeMismatch(decoding: String.self) { "return null;" }
        await testTypeMismatch(decoding: Double.self) { "return undefined;" }
    }
}

extension JavaScriptEvaluationTests {
    private func testDecoding<T: CommonDecodable & Equatable>(
        _ expectedValue: T,
        sourceLocation: SourceLocation = #_sourceLocation,
        _ source: () -> String
    ) async throws {
        let actual = try await page.callJavaScriptv0(returning: T.self, source)
        #expect(actual == expectedValue, sourceLocation: sourceLocation)
    }

    private func testTypeMismatch<T: CommonDecodable>(
        decoding type: T.Type,
        sourceLocation: SourceLocation = #_sourceLocation,
        _ source: () -> String
    ) async {
        let error = await #expect(throws: CodingError.Decoding.self, sourceLocation: sourceLocation) {
            _ = try await page.callJavaScriptv0(returning: T.self, source)
        }

        guard case .typeMismatch = error?.kind else {
            Issue.record("Unexpected CodingError.Decoding type: \(error)", sourceLocation: sourceLocation)
            return
        }
    }

    private func testDataCorrupted<T: CommonDecodable>(
        decoding type: T.Type,
        sourceLocation: SourceLocation = #_sourceLocation,
        _ source: () -> String
    ) async {
        let error = await #expect(throws: CodingError.Decoding.self, sourceLocation: sourceLocation) {
            _ = try await page.callJavaScriptv0(returning: T.self, source)
        }

        guard case .dataCorrupted = error?.kind else {
            Issue.record("Unexpected CodingError.Decoding type: \(error)", sourceLocation: sourceLocation)
            return
        }
    }
}

#endif // ENABLE_SWIFTUI && HAVE_NEW_CODABLE
