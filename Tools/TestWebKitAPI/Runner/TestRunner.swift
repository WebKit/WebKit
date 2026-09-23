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

protocol TestRunner {
    typealias Configuration = TestRunnerConfiguration

    func run(with configuration: Configuration) async throws -> Bool
}

struct TestRunnerConfiguration {
    let programName: String?
    let pretty: Bool
    let listTests: Bool
    let filter: [String]
    let skip: [String]
    let repetitions: Int?
    let force: Bool
    let parallel: Bool
}

extension TestRunner.Configuration {
    private static func option(_ arguments: [String], for name: String) -> [String] {
        var result: [String] = []

        for index in arguments.indices where arguments[index] == name {
            let nextIndex = arguments.index(after: index)
            if nextIndex >= arguments.endIndex {
                continue
            }

            result.append(arguments[nextIndex])
        }

        return result
    }

    private static func flag(_ arguments: [String], for name: String) -> Bool {
        arguments.contains(name)
    }

    init(parsing arguments: [String]) {
        self.programName = arguments.first

        self.filter = Self.option(arguments, for: "--filter")
        self.skip = Self.option(arguments, for: "--skip")
        self.repetitions = Self.option(arguments, for: "--repetitions").first.flatMap { Int($0) }

        self.force = Self.flag(arguments, for: "--force")
        self.listTests = Self.flag(arguments, for: "--list-tests")
        self.pretty = Self.flag(arguments, for: "--pretty")
        self.parallel = Self.flag(arguments, for: "--parallel")
    }
}
