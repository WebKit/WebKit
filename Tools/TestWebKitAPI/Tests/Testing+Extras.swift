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

import Testing

import struct Swift.String

// Use these instead of inline `#expect` and `#require` for equality, nil, and Boolean checks.
//
// Each `#expect` or `#require` is a round trip to the Swift Testing macro plugin, followed by type-checking the code it
// expands to. An inline `#expect(a == b)` is the worst case, taking roughly 20ms in this module because the expanded
// comparison is solved against every `==` overload visible at the call site. A test file with hundreds of checks can
// take seconds to compile. Routing checks through these helpers expands each macro once, here, leaving ordinary
// function calls at the call sites.
//
// The helpers take the caller's location as `#fileID`, `#filePath`, `#line`, and `#column` rather than as
// `SourceLocation = #_sourceLocation`, because `#_sourceLocation` is a plugin macro too, and as a default argument it
// would still be expanded at every call site. A helper that wraps these should take and forward the same four
// parameters.

func expectEqual<T: Equatable>(
    _ actual: T,
    _ expected: T,
    _ comment: Comment? = nil,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) {
    let sourceLocation = SourceLocation(fileID: fileID, filePath: filePath, line: line, column: column)
    #expect(actual == expected, comment, sourceLocation: sourceLocation)
}

func expectNotEqual<T: Equatable>(
    _ actual: T,
    _ unexpected: T,
    _ comment: Comment? = nil,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) {
    let sourceLocation = SourceLocation(fileID: fileID, filePath: filePath, line: line, column: column)
    #expect(actual != unexpected, comment, sourceLocation: sourceLocation)
}

func requireEqual<T: Equatable>(
    _ actual: T,
    _ expected: T,
    _ comment: Comment? = nil,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) throws {
    let sourceLocation = SourceLocation(fileID: fileID, filePath: filePath, line: line, column: column)
    try #require(actual == expected, comment, sourceLocation: sourceLocation)
}

func expectNil<T>(
    _ value: T?,
    _ comment: Comment? = nil,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) {
    let sourceLocation = SourceLocation(fileID: fileID, filePath: filePath, line: line, column: column)
    #expect(value == nil, comment, sourceLocation: sourceLocation)
}

func expectNotNil<T>(
    _ value: T?,
    _ comment: Comment? = nil,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) {
    let sourceLocation = SourceLocation(fileID: fileID, filePath: filePath, line: line, column: column)
    #expect(value != nil, comment, sourceLocation: sourceLocation)
}

func requireNonNil<T>(
    _ value: T?,
    _ comment: Comment? = nil,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) throws -> T {
    let sourceLocation = SourceLocation(fileID: fileID, filePath: filePath, line: line, column: column)
    return try #require(value, comment, sourceLocation: sourceLocation)
}

func expectTrue(
    _ condition: Bool,
    _ comment: Comment? = nil,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) {
    let sourceLocation = SourceLocation(fileID: fileID, filePath: filePath, line: line, column: column)
    #expect(condition, comment, sourceLocation: sourceLocation)
}

func expectFalse(
    _ condition: Bool,
    _ comment: Comment? = nil,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) {
    let sourceLocation = SourceLocation(fileID: fileID, filePath: filePath, line: line, column: column)
    #expect(!condition, comment, sourceLocation: sourceLocation)
}

func expectIdentical(
    _ actual: AnyObject?,
    _ expected: AnyObject?,
    _ comment: Comment? = nil,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) {
    let sourceLocation = SourceLocation(fileID: fileID, filePath: filePath, line: line, column: column)
    #expect(actual === expected, comment, sourceLocation: sourceLocation)
}
