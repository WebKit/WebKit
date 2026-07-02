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

#if JSC_SUPPORTS_SWIFT

import Foundation

// MARK: Test harness types

protocol TestSuite {
    init()

    static var tests: [TestCase<Self>] { get }
}

struct TestCase<T> {
    let name: String
    private let function: (T) -> () throws -> Void

    init(_ name: String, _ function: @escaping (T) -> () throws -> Void) {
        self.name = name
        self.function = function
    }

    func run(on instance: T) throws {
        try function(instance)()
    }
}

struct ExpectationError: Error, CustomStringConvertible {
    let fileID: String
    let filePath: String
    let line: Int
    let column: Int
    let message: String

    var description: String {
        """
        \(fileID):\(line):\(column): "\(message)"
        """
    }
}

struct TestCondition {
    let message: String
    let evaluate: () -> Bool
}

@inline(__always)
func ~= <T>(_ lhs: T, _ rhs: T) -> TestCondition where T: Equatable {
    TestCondition(message: "lhs \(lhs) is not equal to rhs \(rhs)") {
        lhs == rhs
    }
}

func expect(
    _ condition: TestCondition,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) throws(ExpectationError) {
    guard condition.evaluate() else {
        throw ExpectationError(
            fileID: fileID,
            filePath: filePath,
            line: line,
            column: column,
            message: condition.message
        )
    }
}

func require<T>(
    _ value: T?,
    fileID: String = #fileID,
    filePath: String = #filePath,
    line: Int = #line,
    column: Int = #column
) throws(ExpectationError) -> T {
    guard let value else {
        throw ExpectationError(
            fileID: fileID,
            filePath: filePath,
            line: line,
            column: column,
            message: "expected value of type \(T.self) to not be nil"
        )
    }

    return value
}

// MARK: Entry point

@c
@implementation
func testSwiftAPI(_ filter: UnsafePointer<CChar>?) -> CInt {
    let filterString = unsafe filter.map { unsafe String(cString: $0) }

    var failed: CInt = 0

    func runSuite(_ suite: (some TestSuite).Type) {
        let instance = suite.init()

        for test in suite.tests {
            if let filterString, !filterString.contains(test.name) {
                continue
            }

            do {
                try test.run(on: instance)
                print("PASS: \(test.name)")
            } catch {
                print("FAIL: \(test.name) (\(error))")
                failed += 1
            }
        }
    }

    runSuite(BasicFunctionality.self)

    return failed
}

#endif // JSC_SUPPORTS_SWIFT
