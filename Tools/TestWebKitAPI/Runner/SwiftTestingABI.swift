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
import Testing
import struct Swift.String

enum SwiftTestingABI {
    typealias EntryPoint =
        @convention(thin) @Sendable (
            _ configurationJSON: UnsafeRawBufferPointer?,
            _ recordHandler: @escaping @Sendable (_ recordJSON: UnsafeRawBufferPointer) -> Void
        ) async throws -> Bool
}

// MARK: __CommandLineArguments_v0

extension SwiftTestingABI {
    // swift-format-ignore: NoLeadingUnderscores
    struct __CommandLineArguments_v0 {
        var verbosity: Int = 0
        var listTests: Bool? = nil
        var filter: [String]? = nil
        var skip: [String]? = nil
        var repetitions: Int? = nil
        var parallel: Bool? = nil

        init() {
        }
    }
}

extension SwiftTestingABI.__CommandLineArguments_v0: Codable {
}

// MARK: EncodedInstant

extension SwiftTestingABI {
    struct EncodedInstant {
        let absolute: Double
        let since1970: Double
    }
}

extension SwiftTestingABI.EncodedInstant: Codable {
}

// MARK: EncodedSourceLocation

extension SwiftTestingABI {
    struct EncodedSourceLocation {
        let fileID: String?
        let filePath: String? // ABI 6.3
        let line: Int
        let column: Int
    }
}

extension SwiftTestingABI.EncodedSourceLocation: Codable {
}

// MARK: EncodedIssue

extension SwiftTestingABI {
    struct EncodedIssue {
        enum Severity: String, Codable {
            case warning
            case error
        }

        let severity: Severity? // ABI 6.3
        let isFailure: Bool? // ABI 6.3
        let isKnown: Bool
        let sourceLocation: EncodedSourceLocation?
    }
}

extension SwiftTestingABI.EncodedIssue: Codable {
}

// MARK: EncodedTest

extension SwiftTestingABI {
    struct EncodedTest {
        enum Kind: String, Codable {
            case suite
            case function
        }

        struct ID: Codable, Hashable {
            var stringValue: String

            func encode(to encoder: any Encoder) throws {
                try stringValue.encode(to: encoder)
            }

            init(from decoder: any Decoder) throws {
                stringValue = try String(from: decoder)
            }
        }

        let kind: Kind
        let name: String
        let displayName: String?
        let sourceLocation: EncodedSourceLocation
        let id: ID
        let isParameterized: Bool?
        let tags: [String]? // ABI 6.4
        let bugs: [Bug]? // ABI 6.4
        let timeLimit: Double? // ABI 6.4
    }
}

extension SwiftTestingABI.EncodedTest: Codable {
}

// MARK: EncodedMessage

extension SwiftTestingABI {
    struct EncodedMessage {
        enum Symbol: String, Codable {
            case `default`
            case skip
            case pass
            case passWithKnownIssue
            case fail
            case difference
            case warning
            case details
        }

        let symbol: Symbol
        let text: String
    }
}

extension SwiftTestingABI.EncodedMessage: Codable {
}

// MARK: EncodedEvent

extension SwiftTestingABI {
    struct EncodedEvent {
        enum Kind: String, Codable {
            case runStarted
            case testStarted
            case testCaseStarted
            case issueRecorded
            case valueAttached
            case testCaseEnded
            case testCaseCancelled
            case testEnded
            case testSkipped
            case testCancelled
            case runEnded
        }

        let kind: Kind
        let instant: EncodedInstant
        let issue: EncodedIssue?
        let messages: [EncodedMessage]
        let testID: EncodedTest.ID?
    }
}

extension SwiftTestingABI.EncodedEvent: Codable {
}

// MARK: Record

extension SwiftTestingABI {
    struct Record {
        enum Kind {
            case test(EncodedTest)
            case event(EncodedEvent)
        }

        let kind: Kind
    }
}

extension SwiftTestingABI.Record: Decodable {
    private enum CodingKeys: String, CodingKey {
        case kind
        case payload
    }

    init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)

        switch try container.decode(String.self, forKey: .kind) {
        case "test":
            let test = try container.decode(SwiftTestingABI.EncodedTest.self, forKey: .payload)
            self.kind = .test(test)
        case "event":
            let event = try container.decode(SwiftTestingABI.EncodedEvent.self, forKey: .payload)
            self.kind = .event(event)
        case let kind:
            throw DecodingError.dataCorrupted(
                .init(
                    codingPath: decoder.codingPath + [CodingKeys.kind as any CodingKey],
                    debugDescription: "Unrecognized record kind '\(kind)'"
                )
            )
        }
    }
}
