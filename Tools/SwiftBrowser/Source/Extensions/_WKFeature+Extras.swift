// Copyright (C) 2025 Apple Inc. All rights reserved.
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
import WebKit_Private.WKPreferencesPrivate

// MARK: _WKFeature

// FIXME: These extensions should probably just be in WebKit proper since
// there is nothing SwiftBrowser-specific about them.

// This conformance is safe since there is no mutable subclass of _WKFeature,
// and all properties are readonly or copied.
// swift-format-ignore: AvoidRetroactiveConformances
extension _WKFeature: @unchecked @retroactive Sendable {
}

// swift-format-ignore: AvoidRetroactiveConformances
extension _WKFeature: @retroactive Identifiable {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var id: String {
        // This is needed because `_WKFeature` lacks nullability annotations; this is never actually `nil`.
        // swift-format-ignore: NeverForceUnwrap
        self.key!
    }
}

extension _WKFeature: FeatureFlag {
}

// MARK: WebFeatureStatus

// swift-format-ignore: AvoidRetroactiveConformances
extension WebFeatureStatus: @retroactive CaseIterable {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static var allCases: [Self] {
        let cases = sequence(state: UInt.zero) { rawValue in
            defer { rawValue += 1 }
            return Self.init(rawValue: rawValue)
        }
        return Array(cases)
    }
}

// swift-format-ignore: AvoidRetroactiveConformances
extension WebFeatureStatus: @retroactive CustomStringConvertible {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var description: String {
        switch self {
        case .embedder: "Embedder"
        case .unstable: "Unstable"
        case .internal: "Internal"
        case .developer: "Developer"
        case .testable: "Testable"
        case .preview: "Preview"
        case .stable: "Stable"
        case .mature: "Mature"
        @unknown default: "Unknown"
        }
    }
}

// MARK: WebFeatureCategory

// swift-format-ignore: AvoidRetroactiveConformances
extension WebFeatureCategory: @retroactive CaseIterable {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static var allCases: [Self] {
        let cases = sequence(state: UInt.zero) { rawValue in
            defer { rawValue += 1 }
            return Self.init(rawValue: rawValue)
        }
        return Array(cases)
    }
}

// swift-format-ignore: AvoidRetroactiveConformances
extension WebFeatureCategory: @retroactive Comparable {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func < (lhs: WebFeatureCategory, rhs: WebFeatureCategory) -> Bool {
        lhs.rawValue < rhs.rawValue
    }
}

// swift-format-ignore: AvoidRetroactiveConformances
extension WebFeatureCategory: @retroactive CustomStringConvertible {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var description: String {
        switch self {
        case .none: "None"
        case .animation: "Animation"
        case .CSS: "CSS"
        case .DOM: "DOM"
        case .javascript: "JavaScript"
        case .media: "Media"
        case .networking: "Networking"
        case .privacy: "Privacy"
        case .security: "Security"
        case .HTML: "HTML"
        case .extensions: "Extensions"
        @unknown default: "Unknown"
        }
    }
}
