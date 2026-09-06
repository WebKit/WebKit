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
import struct FoundationEssentials.Data
import struct Swift.String

// MARK: Supporting types

@CommonDecodable
private enum Shape: Equatable {
    case point
    case circle(radius: Double)
    case rectangle(width: Double, height: Double)
    case pair(Int, Int)
}

@CommonDecodable
private struct Person: Equatable {
    let name: String
    let age: Int
}

@CommonDecodable
private struct Item: Equatable {
    let name: String
    let rating: Double?
}

@CommonDecodable
private struct Post: Equatable {
    @CodingKey("date_published")
    let publishDate: String
    @DecodableAlias("headline")
    let title: String
    @CodableDefault(0)
    let views: Int
}

@CommonDecodable
private struct Team: Equatable {
    let lead: Person
    let members: [Person]
    let scoresByName: [String: Int]
}

@CommonDecodable
private struct Empty: Equatable {}

private struct Profile: Decodable, Equatable {
    let name: String
    let age: Int
    let tags: [String]
}

private struct CommonDecodableAdaptor<D: Decodable> {
    let value: D
}

extension CommonDecodableAdaptor: CommonDecodable {
    static func decode(from decoder: inout some (CommonDecoder & ~Escapable)) throws(CodingError.Decoding) -> Self {
        CommonDecodableAdaptor(value: try decoder.decode(D.self))
    }
}

extension CommonDecodableAdaptor: Equatable where D: Equatable {
}

// MARK: Tests

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

    @Test
    func decodingArray() async throws {
        try await testDecoding([1, 2, 3]) { "return [1, 2, 3];" }
        try await testDecoding([Int]()) { "return [];" }
        try await testDecoding(["a", "b"]) { #"return ["a", "b"];"# }
        try await testDecoding([true, false]) { "return [true, false];" }
        try await testDecoding([1.5, -0.25]) { "return [1.5, -0.25];" }
        try await testDecoding(Array(0..<100)) { "return Array.from({ length: 100 }, (_, i) => i);" }

        // The element type applies to every element, so a mixed array only decodes as a type
        // all of them share.
        try await testDecoding(["1", "two"]) { #"return ["1", "two"];"# }
        await testTypeMismatch(decoding: [Int].self) { #"return [1, "two", 3];"# }

        // A hole and an explicit null are both missing values, so both need an optional
        // element type.
        try await testDecoding([1, nil, 3]) { "return [1, , 3];" }
        try await testDecoding([1, nil, 3]) { "return [1, null, 3];" }
        await testTypeMismatch(decoding: [Int].self) { "return [1, null, 3];" }

        // Only a real array decodes as one; an array-like object does not.
        await testTypeMismatch(decoding: [Int].self) { "return { 0: 1, length: 1 };" }
        await testTypeMismatch(decoding: [Int].self) { "return 42;" }
        await testTypeMismatch(decoding: [Int].self) { #"return "abc";"# }
        await testTypeMismatch(decoding: [Int].self) { "return null;" }

        // Array decodes every element, but a type reading a fixed number of them pulls only
        // as many as it needs.
        try await testDecoding(1..<5) { "return [1, 5];" }
        await testDataCorrupted(decoding: Range<Int>.self) { "return [5, 1];" }
    }

    @Test
    func decodingDictionary() async throws {
        try await testDecoding(["a": 1, "b": 2]) { "return { a: 1, b: 2 };" }
        try await testDecoding([String: Int]()) { "return {};" }
        try await testDecoding(["greeting": "hello"]) { #"return { greeting: "hello" };"# }
        try await testDecoding(["yes": true, "no": false]) { "return { yes: true, no: false };" }

        // Any string is a usable key, including ones which are not identifiers.
        try await testDecoding(["with space": 1, "": 2]) { #"return { "with space": 1, "": 2 };"# }

        // A JavaScript key is always a string, so an Int key is parsed back out of one.
        try await testDecoding([1: "one", 2: "two"]) { #"return { 1: "one", 2: "two" };"# }
        await testDataCorrupted(decoding: [Int: String].self) { #"return { a: "one" };"# }

        // A null value keeps its key, rather than dropping the entry.
        try await testDecoding(["a": Int?.none, "b": 2]) { "return { a: null, b: 2 };" }
        await testTypeMismatch(decoding: [String: Int].self) { "return { a: null };" }
        await testTypeMismatch(decoding: [String: Int].self) { #"return { a: "1" };"# }

        await testTypeMismatch(decoding: [String: Int].self) { "return [1, 2];" }
        await testTypeMismatch(decoding: [String: Int].self) { "return 42;" }
        await testTypeMismatch(decoding: [String: Int].self) { "return null;" }
    }

    @Test
    func decodingNestedContainers() async throws {
        try await testDecoding([[1, 2], [3], []]) { "return [[1, 2], [3], []];" }
        try await testDecoding([["a": 1], ["b": 2]]) { "return [{ a: 1 }, { b: 2 }];" }
        try await testDecoding(["outer": ["inner": 1]]) { "return { outer: { inner: 1 } };" }
        try await testDecoding(["values": [1, 2, 3]]) { "return { values: [1, 2, 3] };" }
        try await testDecoding([["a": [1]], ["b": [2, 3]]]) { "return [{ a: [1] }, { b: [2, 3] }];" }
        try await testDecoding([[["deep"]]]) { #"return [[["deep"]]];"# }

        // The result is a graph rather than a tree, so one object reached twice is one entry
        // decoded twice.
        try await testDecoding([[1], [1]]) { "const shared = [1]; return [shared, shared];" }
        try await testDecoding(["x": ["n": 1], "y": ["n": 1]]) {
            "const shared = { n: 1 }; return { x: shared, y: shared };"
        }

        // A mismatch nested inside a container is reported like any other.
        await testTypeMismatch(decoding: [[Int]].self) { "return [[1], 2];" }
        await testTypeMismatch(decoding: [String: [Int]].self) { #"return { a: "x" };"# }
    }

    @Test
    func decodingEnum() async throws {
        try await testDecoding(Shape.point) { "return { point: {} };" }
        try await testDecoding(Shape.circle(radius: 2.5)) { "return { circle: { radius: 2.5 } };" }
        try await testDecoding(Shape.rectangle(width: 3, height: 4)) {
            "return { rectangle: { width: 3, height: 4 } };"
        }

        // Associated values without a label are keyed by position instead.
        try await testDecoding(Shape.pair(1, 2)) { "return { pair: { _0: 1, _1: 2 } };" }

        // Exactly one entry names the case, so neither none nor several will do.
        await testDataCorrupted(decoding: Shape.self) { "return {};" }
        await testDataCorrupted(decoding: Shape.self) { "return { point: {}, circle: { radius: 1 } };" }

        // The name has to be a case the enum actually has.
        await testUnknownKey(decoding: Shape.self) { "return { triangle: {} };" }

        // The associated values are an object, and every one the case declares is required.
        await testTypeMismatch(decoding: Shape.self) { "return { circle: 2.5 };" }
        await testDataCorrupted(decoding: Shape.self) { "return { circle: {} };" }
        await testDataCorrupted(decoding: Shape.self) { "return { rectangle: { width: 3 } };" }

        // The enum itself is an object, whatever a case may look like.
        await testTypeMismatch(decoding: Shape.self) { #"return "point";"# }
        await testTypeMismatch(decoding: Shape.self) { "return [1, 2];" }
        await testTypeMismatch(decoding: Shape.self) { "return null;" }
    }

    @Test
    func decodingStruct() async throws {
        let person = Person(name: "Ada", age: 36)

        try await testDecoding(person) { #"return { name: "Ada", age: 36 };"# }
        try await testDecoding(Empty()) { "return {};" }

        // Fields are matched by name, so the order the object happens to list them in does
        // not matter.
        try await testDecoding(person) { #"return { age: 36, name: "Ada" };"# }

        // A field the struct does not declare is ignored rather than rejected.
        try await testDecoding(person) { #"return { name: "Ada", age: 36, nickname: "A" };"# }
        try await testDecoding(Empty()) { "return { anything: 1 };" }

        // Every declared field has to be there, and has to be the right type.
        await testDataCorrupted(decoding: Person.self) { #"return { name: "Ada" };"# }
        await testDataCorrupted(decoding: Person.self) { "return {};" }
        await testTypeMismatch(decoding: Person.self) { #"return { name: "Ada", age: "36" };"# }

        // A struct is an object; nothing else stands in for one.
        await testTypeMismatch(decoding: Person.self) { #"return "Ada";"# }
        await testTypeMismatch(decoding: Person.self) { #"return ["Ada", 36];"# }
        await testTypeMismatch(decoding: Person.self) { "return null;" }
    }

    @Test
    func decodingStructWithOptionalField() async throws {
        try await testDecoding(Item(name: "book", rating: 4.5)) {
            #"return { name: "book", rating: 4.5 };"#
        }

        // An optional field is satisfied by an explicit null, by undefined, and by the key
        // being absent altogether.
        try await testDecoding(Item(name: "book", rating: nil)) {
            #"return { name: "book", rating: null };"#
        }
        try await testDecoding(Item(name: "book", rating: nil)) {
            #"return { name: "book", rating: undefined };"#
        }
        try await testDecoding(Item(name: "book", rating: nil)) { #"return { name: "book" };"# }

        // Optional means the value may be missing, not that it may be anything.
        await testTypeMismatch(decoding: Item.self) { #"return { name: "book", rating: "4.5" };"# }
    }

    @Test
    func decodingStructWithFieldAttributes() async throws {
        let post = Post(publishDate: "2026-01-01", title: "Hello", views: 10)

        try await testDecoding(post) {
            #"return { date_published: "2026-01-01", title: "Hello", views: 10 };"#
        }

        // @DecodableAlias accepts a second spelling of the same field.
        try await testDecoding(post) {
            #"return { date_published: "2026-01-01", headline: "Hello", views: 10 };"#
        }

        // @CodableDefault supplies the value when the field is absent.
        try await testDecoding(Post(publishDate: "2026-01-01", title: "Hello", views: 0)) {
            #"return { date_published: "2026-01-01", title: "Hello" };"#
        }

        // @CodingKey replaces the property name rather than adding to it.
        await testDataCorrupted(decoding: Post.self) {
            #"return { publishDate: "2026-01-01", title: "Hello" };"#
        }
    }

    @Test
    func decodingNestedStruct() async throws {
        let team = Team(
            lead: Person(name: "Ada", age: 36),
            members: [Person(name: "Grace", age: 45), Person(name: "Alan", age: 41)],
            scoresByName: ["Grace": 1, "Alan": 2]
        )

        try await testDecoding(team) {
            """
            return {
                lead: { name: "Ada", age: 36 },
                members: [{ name: "Grace", age: 45 }, { name: "Alan", age: 41 }],
                scoresByName: { Grace: 1, Alan: 2 },
            };
            """
        }

        try await testDecoding([Person(name: "Ada", age: 36)]) { #"return [{ name: "Ada", age: 36 }];"# }
        try await testDecoding(["lead": Person(name: "Ada", age: 36)]) {
            #"return { lead: { name: "Ada", age: 36 } };"#
        }

        // A failure inside a nested struct surfaces the same as one at the top level.
        await testDataCorrupted(decoding: Team.self) {
            #"return { lead: { name: "Ada" }, members: [], scoresByName: {} };"#
        }
        await testTypeMismatch(decoding: Team.self) {
            #"return { lead: { name: "Ada", age: 36 }, members: {}, scoresByName: {} };"#
        }
    }

    @Test
    func decodingData() async throws {
        let data = Data([1, 2, 255])

        // Bytes are either an array of numbers or the base64 string a script would produce for them.
        try await testDecoding(data) { "return [1, 2, 255];" }
        try await testDecoding(data) { "return btoa(String.fromCharCode(1, 2, 255));" }
        try await testDecoding(Data()) { "return [];" }
        try await testDecoding(Data()) { #"return "";"# }

        // Every element has to be a byte, so one out of range or not integral is corrupt data
        // rather than something to truncate.
        await testDataCorrupted(decoding: Data.self) { "return [256];" }
        await testDataCorrupted(decoding: Data.self) { "return [-1];" }
        await testDataCorrupted(decoding: Data.self) { "return [1.5];" }
        await testTypeMismatch(decoding: Data.self) { #"return ["1"];"# }

        // A string is read as base64, so one which is not is corrupt data too.
        await testDataCorrupted(decoding: Data.self) { #"return "not base64!";"# }

        // Anything which is neither shape is a mismatch. A typed array is among them: it is
        // not a JavaScript array, so it arrives keyed by index like any other object.
        await testTypeMismatch(decoding: Data.self) { "return { 0: 1, 1: 2 };" }
        await testTypeMismatch(decoding: Data.self) { "return 42;" }
        await testTypeMismatch(decoding: Data.self) { "return null;" }
    }

    @Test
    func decodingStandardDecodable() async throws {
        typealias ProfileAdaptor = CommonDecodableAdaptor<Profile>

        let box = ProfileAdaptor(value: Profile(name: "Ada", age: 36, tags: ["swift", "webkit"]))

        try await testDecoding(box) {
            #"return { name: "Ada", age: 36, tags: ["swift", "webkit"] };"#
        }

        // The standard decoding machinery classifies failures on this path, not the graph
        // decoder, so only the fact that one is raised is checked here.
        await testDecodingFailure(decoding: ProfileAdaptor.self) { #"return { name: "Ada", tags: [] };"# }
        await testDecodingFailure(decoding: ProfileAdaptor.self) { #"return { name: "Ada", age: "36", tags: [] };"# }
        await testDecodingFailure(decoding: ProfileAdaptor.self) { "return 42;" }
        await testDecodingFailure(decoding: ProfileAdaptor.self) { "return null;" }
    }
}

// MARK: Helpers

extension JavaScriptEvaluationTests {
    private func testDecoding<T: CommonDecodable & Equatable>(
        _ expectedValue: T,
        sourceLocation: SourceLocation = #_sourceLocation,
        _ source: () -> String
    ) async throws {
        let actual = try await page.callJavaScriptv0(returning: T.self, source)
        #expect(actual == expectedValue, sourceLocation: sourceLocation)
    }

    @discardableResult
    private func testDecodingFailure<T: CommonDecodable>(
        decoding type: T.Type,
        sourceLocation: SourceLocation = #_sourceLocation,
        _ source: () -> String
    ) async -> CodingError.Decoding? {
        await #expect(throws: CodingError.Decoding.self, sourceLocation: sourceLocation) {
            _ = try await page.callJavaScriptv0(returning: T.self, source)
        }
    }

    private func testTypeMismatch<T: CommonDecodable>(
        decoding type: T.Type,
        sourceLocation: SourceLocation = #_sourceLocation,
        _ source: () -> String
    ) async {
        let error = await testDecodingFailure(decoding: type, sourceLocation: sourceLocation, source)

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
        let error = await testDecodingFailure(decoding: type, sourceLocation: sourceLocation, source)

        guard case .dataCorrupted = error?.kind else {
            Issue.record("Unexpected CodingError.Decoding type: \(error)", sourceLocation: sourceLocation)
            return
        }
    }

    private func testUnknownKey<T: CommonDecodable>(
        decoding type: T.Type,
        sourceLocation: SourceLocation = #_sourceLocation,
        _ source: () -> String
    ) async {
        let error = await testDecodingFailure(decoding: type, sourceLocation: sourceLocation, source)

        guard case .unknownKey = error?.kind else {
            Issue.record("Unexpected CodingError.Decoding type: \(error)", sourceLocation: sourceLocation)
            return
        }
    }
}

#endif // ENABLE_SWIFTUI && HAVE_NEW_CODABLE
