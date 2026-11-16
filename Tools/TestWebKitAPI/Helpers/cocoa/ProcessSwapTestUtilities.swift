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

// The Swift counterparts of the helpers in ProcessSwapOnNavigation.mm, for tests of which web content processes host a
// page as it navigates: process swaps, the back/forward cache, the process cache, and the browsing context groups that
// Cross-Origin-Opener-Policy creates.

import Foundation
public import WebKit
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private._WKProcessPoolConfiguration

#if hasAttribute(diagnose)
@diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses a custom process pool")
#endif
extension WKProcessPool {
    /// Creates a process pool that swaps web content processes on navigation, like the process pools of the tests in
    /// ProcessSwapOnNavigation.mm.
    ///
    /// The pool also prewarms processes, and swaps processes for cross-site navigations within a protocol outside the
    /// HTTP family, such as a custom URL scheme.
    ///
    /// - Parameters:
    ///   - backForwardCacheEnabled: Whether the pool keeps the pages a web view navigates away from in the
    ///     back/forward cache, which keeps their processes alive as suspended pages.
    ///   - webProcessCacheEnabled: Whether the pool keeps processes that no longer host a page in its process cache, to
    ///     host a later page from the same site.
    /// - Returns: The process pool.
    public static func processSwapping(
        backForwardCacheEnabled: Bool = true,
        webProcessCacheEnabled: Bool = true
    ) -> WKProcessPool {
        let configuration = _WKProcessPoolConfiguration()
        configuration.processSwapsOnNavigation = true
        configuration.usesWebProcessCache = webProcessCacheEnabled
        configuration.prewarmsProcessesAutomatically = true
        configuration.processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol = true
        configuration.pageCacheEnabled = backForwardCacheEnabled
        // WKProcessPoolPrivate.h has no nullability annotations, but this never returns nil.
        return WKProcessPool._processPool(with: configuration)
    }

    /// Creates a process pool with the default configuration, except that it doesn't keep pages in the back/forward
    /// cache.
    ///
    /// Use this when a test needs a history navigation to load its page again, or a process to exit once its frames
    /// are gone.
    ///
    /// - Returns: The process pool.
    public static func withBackForwardCacheDisabled() -> WKProcessPool {
        let configuration = _WKProcessPoolConfiguration()
        configuration.pageCacheEnabled = false
        return WKProcessPool._processPool(with: configuration)
    }

    /// The identifiers of the web content processes of the pool that are cross-origin isolated.
    ///
    /// A process is cross-origin isolated when it hosts documents of a browsing context group whose documents are
    /// cross-origin isolated, such as one created for a page that sends `Cross-Origin-Opener-Policy: same-origin` and
    /// `Cross-Origin-Embedder-Policy: require-corp`. Such a process only ever hosts documents of that group.
    public var crossOriginIsolatedProcessIdentifiers: Set<pid_t> {
        // WKProcessPoolPrivate.h has no nullability annotations, but the set is never nil.
        let identifiers: Set<NSNumber> = _crossOriginIsolatedProcessIdentifiersForTesting()
        return Set(identifiers.map(\.int32Value))
    }

    /// Returns whether a web content process of the pool is cross-origin isolated.
    ///
    /// - Parameter processIdentifier: The identifier of the process, such as a web view's `_webProcessIdentifier`.
    /// - Returns: Whether the process is cross-origin isolated, which is never the case for an identifier of 0.
    public func isCrossOriginIsolatedProcess(_ processIdentifier: pid_t) -> Bool {
        processIdentifier != 0 && crossOriginIsolatedProcessIdentifiers.contains(processIdentifier)
    }
}

extension WKWebViewConfiguration {
    /// Turns on the `Cross-Origin-Opener-Policy` and `Cross-Origin-Embedder-Policy` headers for the web views created
    /// with the configuration.
    ///
    /// A page that sends `Cross-Origin-Opener-Policy` can then get a browsing context group of its own, and one that
    /// also sends `Cross-Origin-Embedder-Policy: require-corp` is cross-origin isolated.
    public func enableCrossOriginOpenerAndEmbedderPolicies() {
        setFeature("CrossOriginOpenerPolicyEnabled", enabled: true)
        setFeature("CrossOriginEmbedderPolicyEnabled", enabled: true)
    }
}
