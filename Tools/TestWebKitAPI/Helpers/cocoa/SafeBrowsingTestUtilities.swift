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
private import TestWebKitAPILibrary.Helpers.cocoa.SafeBrowsingTestUtilities

import struct Swift.String

/// Namespaced Safe Browsing filter lists, keyed by `"<namespace>/domains"` and `"<namespace>/filter"`.
public typealias SafeBrowsingLists = [String: [String]]

/// Replaces the namespaced list lookup on `SSBLookupContext` for the duration of the given closure.
///
/// - Parameters:
///   - lists: The lists the stubbed lookup should report.
///   - body: The work to run while the stub is in effect.
/// - Returns: The value returned by `body`.
/// - Throws: Rethrows any error thrown by `body`.
@MainActor
@discardableResult
public func withStubbedSafeBrowsingLists<Result: ~Copyable, Failure: Error>(
    _ lists: SafeBrowsingLists,
    perform body: nonisolated(nonsending) () async throws(Failure) -> sending Result
) async throws(Failure) -> sending Result {
    typealias LookUpListsBlock =
        @convention(block) (
            AnyObject, String?, String?, @escaping @Sendable (SafeBrowsingLists?, (any Error)?) -> Void
        ) -> Void

    let lookUpLists: LookUpListsBlock = { _, _, _, completionHandler in
        Task.detached { @concurrent in
            completionHandler(lists, nil)
        }
    }

    return try await withSwizzledObjectiveCInstanceMethod(
        replacing: testSSBLookupContextClass(),
        name: Selector(("_getListsForNamespace:collectionId:completionHandler:")),
        with: lookUpLists
    ) { () throws(Failure) in
        try await body()
    }
}

/// Replaces `+[SSBLookupContext sharedLookupContext]` for the duration of the given closure.
///
/// - Parameters:
///   - lookupContext: The stand-in to return, such as `TestLookupContext.shared()`.
///   - body: The work to run while the stub is in effect.
/// - Returns: The value returned by `body`.
/// - Throws: Rethrows any error thrown by `body`.
@MainActor
@discardableResult
public func withStubbedSharedLookupContext<Result: ~Copyable, Failure: Error>(
    _ lookupContext: AnyObject,
    perform body: nonisolated(nonsending) () async throws(Failure) -> sending Result
) async throws(Failure) -> sending Result {
    typealias SharedLookupContextBlock = @convention(block) (AnyObject) -> AnyObject

    let sharedLookupContext: SharedLookupContextBlock = { _ in lookupContext }

    return try await withSwizzledObjectiveCClassMethod(
        class: testSSBLookupContextClass(),
        replacing: #selector(TestLookupContext.shared),
        with: sharedLookupContext
    ) { () throws(Failure) in
        try await body()
    }
}
