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
private import Synchronization

actor SwiftTestsController: TestRunner {
    nonisolated struct Storage: ~Copyable {
        var tests: [SwiftTestingABI.EncodedTest.ID: SwiftTestingABI.EncodedTest] = [:]
        var events: [SwiftTestingABI.EncodedTest.ID: [SwiftTestingABI.EncodedEvent]] = [:]
    }

    static let shared = SwiftTestsController()

    private static let entryPoint = unsafe unsafeBitCast(swt_abiv0_getEntryPoint(), to: SwiftTestingABI.EntryPoint.self)

    private let decoder = JSONDecoder()
    private let encoder = JSONEncoder()

    private let storage = Mutex<Storage>(.init())

    nonisolated private func saveRecord(_ record: SwiftTestingABI.Record) {
        switch record.kind {
        case .test(let encodedTest):
            storage.withLock { $0.tests[encodedTest.id] = encodedTest }

        case .event(let encodedEvent):
            guard let testID = encodedEvent.testID else {
                return
            }

            storage.withLock { $0.events[testID, default: []].append(encodedEvent) }
        }
    }

    nonisolated private func writeOutputIfNeeded(_ record: SwiftTestingABI.Record) {
        guard case .event(let eventRecord) = record.kind, eventRecord.kind == .testEnded else {
            return
        }

        guard let testID = eventRecord.testID else {
            return
        }

        let (test, events) = storage.withLock {
            ($0.tests[testID], $0.events[testID])
        }

        guard let test, let events, test.kind == .function else {
            return
        }

        let passed = events.allSatisfy { $0.kind != .issueRecorded }
        if passed {
            print("**PASS** \(testID.canonicalizedRepresentation)")
        } else {
            let failureDescriptions = events
                .lazy
                .filter { $0.kind == .issueRecorded }
                .flatMap(\.messages)
                .map { message in
                    "\(message.symbol.canonicalizedRepresentation) \(message.text)"
                }
                .joined(separator: "\n")

            print("**FAIL** \(testID.canonicalizedRepresentation)\n\(failureDescriptions)")
        }
    }

    nonisolated private func handleRecord(_ record: SwiftTestingABI.Record) {
        saveRecord(record)
        writeOutputIfNeeded(record)
    }

    func run(with configuration: Configuration) async throws -> Bool {
        let abiConfiguration = SwiftTestingABI.__CommandLineArguments_v0(configuration)
        let encodedConfiguration = try encoder.encode(abiConfiguration)

        // Copies the data of `encodedConfiguration` into a new buffer that is guaranteed to be valid for the scope of this function.

        let mutableBuffer = UnsafeMutableRawBufferPointer.allocate(byteCount: encodedConfiguration.count, alignment: 1)
        defer {
            unsafe mutableBuffer.deallocate()
        }
        unsafe encodedConfiguration.copyBytes(to: mutableBuffer)
        let buffer = UnsafeRawBufferPointer(mutableBuffer)

        return unsafe try await Self.entryPoint(buffer) { [self, abiConfiguration] recordJSON in
            // Either use custom logging, or Swift Testing logging, not both
            guard abiConfiguration.verbosity == .min else {
                return
            }

            let data = unsafe Data(recordJSON)
            // The program should terminate if this `try` fails, any other behavior would lead to unexpected results.
            // swift-format-ignore: NeverUseForceTry
            let decoded = try! decoder.decode(SwiftTestingABI.Record.self, from: data)
            handleRecord(decoded)
        }
    }
}

extension SwiftTestingABI.EncodedTest.ID {
    /// The stable representation of a test's identifier, not subject to changes based on source location.
    ///
    /// For example, this converts
    ///
    /// ```
    /// TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()/WKWebViewSwiftOverlayTests.swift:40:6
    /// ```
    ///
    /// to
    ///
    /// ```
    /// TestWebKitAPI.WKWebViewSwiftOverlayTests/evaluateJavaScriptWithNilResponse()
    /// ```
    fileprivate var canonicalizedRepresentation: some StringProtocol {
        let rawValue = stringValue
        let indexOfLastDelimiter = rawValue.lastIndex(of: "/") ?? rawValue.endIndex
        return rawValue[..<indexOfLastDelimiter]
    }
}

extension SwiftTestingABI.EncodedMessage.Symbol {
    fileprivate var canonicalizedRepresentation: some StringProtocol {
        switch self {
        case .fail: "✘"
        case .details: "↪︎"
        default: ""
        }
    }
}

extension SwiftTestingABI.__CommandLineArguments_v0 {
    fileprivate init(_ runnerConfiguration: TestRunner.Configuration) {
        self.verbosity =
            if runnerConfiguration.listTests {
                0 // Default Swift Testing logging level
            } else if runnerConfiguration.pretty {
                0 // Verbose Swift Testing logging level
            } else {
                Int.min // None Swift Testing logging level
            }

        self.filter = runnerConfiguration.filter.map(NSRegularExpression.escapedPattern(for:))
        self.repetitions = runnerConfiguration.repetitions
        self.skip = runnerConfiguration.skip
        self.listTests = runnerConfiguration.listTests
        self.parallel = runnerConfiguration.parallel
    }
}
