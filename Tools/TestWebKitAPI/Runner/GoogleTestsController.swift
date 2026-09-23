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

import Foundation

@MainActor
final class GoogleTestsController: TestRunner {
    static let shared = GoogleTestsController()

    func run(with configuration: Configuration) async throws -> Bool {
        let arguments = Self.parseArguments(configuration: configuration)
        return unsafe withUnsafeMutableCStyleArguments(arguments) { argc, argv in
            unsafe TestWebKitAPIRunTests(argc, argv)
        }
    }
}

extension GoogleTestsController {
    private static func parseArguments(configuration: TestRunner.Configuration) -> [String] {
        var result: [String] = []

        if let programName = configuration.programName {
            result.append(programName)
        }

        if configuration.listTests {
            result.append("--gtest_list_tests")
        }

        if configuration.pretty {
            result.append("--gtest_brief=0")
        }

        if configuration.force {
            result.append("--gtest_also_run_disabled_tests")
        }

        var filterArgument = configuration.filter.joined(separator: ":")
        if !configuration.skip.isEmpty {
            filterArgument += "-"
            filterArgument += configuration.skip.joined(separator: ":")
        }

        if !filterArgument.isEmpty {
            result.append("--gtest_filter=\(filterArgument)")
        }

        if let repetitions = configuration.repetitions {
            result.append("--gtest_repeat=\(repetitions)")
        }

        return result
    }
}

func withUnsafeMutableCStyleArguments<R>(
    _ arguments: some Sequence<String>,
    _ body: (_ argc: Int32, _ argv: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>) throws -> R
) rethrows -> R {
    let pointers = unsafe arguments.map { unsafe strdup($0) }
    unsafe precondition(!pointers.isEmpty)

    defer {
        for unsafe pointer in unsafe pointers {
            unsafe free(pointer)
        }
    }

    var mutablePointers = unsafe pointers
    return try unsafe mutablePointers.withUnsafeMutableBufferPointer { buffer in
        // Guaranteed to be non-nil because of the precondition at the beginning.
        // swift-format-ignore: NeverForceUnwrap
        unsafe try body(Int32(buffer.count), buffer.baseAddress!)
    }
}
