// Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE_SWIFTUI && compiler(>=6.0)

import Foundation
internal import WebKit_Internal

extension WebPage_v0 {
    @MainActor
    @_spi(Private)
    public struct NavigationPreferences: Sendable {
        public enum ContentMode: Sendable {
            case recommended
            case mobile
            case desktop
        }

        public enum UpgradeToHTTPSPolicy: Sendable {
            case keepAsRequested
            case automaticFallbackToHTTP
            case userMediatedFallbackToHTTP
            case errorOnFailure
        }

        public init() {
        }

        public var preferredContentMode: ContentMode = .recommended

        public var allowsContentJavaScript: Bool = true

        public var preferredHTTPSNavigationPolicy: UpgradeToHTTPSPolicy = .keepAsRequested

        fileprivate var _isLockdownModeEnabled: Bool? = nil
        public var isLockdownModeEnabled: Bool {
            get { _isLockdownModeEnabled ?? false }
            set { _isLockdownModeEnabled = newValue }
        }
    }
}

// MARK: Adapters

extension WKWebpagePreferences.ContentMode {
    init(_ wrapped: WebPage_v0.NavigationPreferences.ContentMode) {
        self = switch wrapped {
        case .recommended: .recommended
        case .mobile: .mobile
        case .desktop: .desktop
        }
    }
}

extension WKWebpagePreferences.UpgradeToHTTPSPolicy {
    init(_ wrapped: WebPage_v0.NavigationPreferences.UpgradeToHTTPSPolicy) {
        self = switch wrapped {
        case .keepAsRequested: .keepAsRequested
        case .automaticFallbackToHTTP: .automaticFallbackToHTTP
        case .userMediatedFallbackToHTTP: .userMediatedFallbackToHTTP
        case .errorOnFailure: .errorOnFailure
        }
    }
}

extension WKWebpagePreferences {
    convenience init(_ wrapped: WebPage_v0.NavigationPreferences) {
        self.init()

        self.preferredContentMode = .init(wrapped.preferredContentMode)
        self.preferredHTTPSNavigationPolicy = .init(wrapped.preferredHTTPSNavigationPolicy)
        self.allowsContentJavaScript = wrapped.allowsContentJavaScript

        if let isLockdownModeEnabled = wrapped._isLockdownModeEnabled, self.isLockdownModeEnabled != isLockdownModeEnabled {
            self.isLockdownModeEnabled = isLockdownModeEnabled
        }
    }
}

extension WebPage_v0.NavigationPreferences.ContentMode {
    init(_ wrapped: WKWebpagePreferences.ContentMode) {
        self = switch wrapped {
        case .recommended: .recommended
        case .mobile: .mobile
        case .desktop: .desktop
        @unknown default:
            fatalError()
        }
    }
}

extension WebPage_v0.NavigationPreferences.UpgradeToHTTPSPolicy {
    init(_ wrapped: WKWebpagePreferences.UpgradeToHTTPSPolicy) {
        self = switch wrapped {
        case .keepAsRequested: .keepAsRequested
        case .automaticFallbackToHTTP: .automaticFallbackToHTTP
        case .userMediatedFallbackToHTTP: .userMediatedFallbackToHTTP
        case .errorOnFailure: .errorOnFailure
        @unknown default:
            fatalError()
        }
    }
}

extension WebPage_v0.NavigationPreferences {
    init(_ wrapped: WKWebpagePreferences) {
        self.init()

        self.preferredContentMode = .init(wrapped.preferredContentMode)
        self.preferredHTTPSNavigationPolicy = .init(wrapped.preferredHTTPSNavigationPolicy)

        self.allowsContentJavaScript = wrapped.allowsContentJavaScript
        self.isLockdownModeEnabled = wrapped.isLockdownModeEnabled
    }
}

#endif
