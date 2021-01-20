/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

import WebKit
import SwiftUI

extension View {
    /// Provides a block to be executed on navigation milestones, and allows for customized handling
    /// of navigation policy.
    ///
    /// When providing a custom policy decider you **must** ensure that the coresponding decider's
    /// completion method is invoked exactly once. Failure to do so will result in a
    /// runtime exception.
    ///
    /// - Parameters:
    ///   - actionDecider: Decides whether to allow or cancel a navigation.
    public func webViewNavigationPolicy(
        onAction actionDecider: @escaping (NavigationAction, WebViewState) -> Void
    ) -> some View {
        environment(\.navigationActionDecider, actionDecider)
    }

    /// Provides a block to be executed on navigation milestones, and allows for customized handling
    /// of navigation policy.
    ///
    /// When providing a custom policy decider you **must** ensure that the coresponding decider's
    /// completion method is invoked exactly once. Failure to do so will result in a
    /// runtime exception.
    ///
    /// - Parameters:
    ///   - responseDecider: Decides whether to allow or cancel a navigation, after its response
    ///   is known.
    public func webViewNavigationPolicy(
        onResponse responseDecider: @escaping (NavigationResponse, WebViewState) -> Void
    ) -> some View {
        environment(\.navigationResponseDecider, responseDecider)
    }

    /// Provides a block to be executed on navigation milestones, and allows for customized handling
    /// of navigation policy.
    ///
    /// When providing a custom policy decider you **must** ensure that the coresponding decider's
    /// completion method is invoked exactly once. Failure to do so will result in a
    /// runtime exception.
    ///
    /// - Parameters:
    ///   - actionDecider: Decides whether to allow or cancel a navigation.
    ///   - responseDecider: Decides whether to allow or cancel a navigation, after its response
    ///   is known.
    public func webViewNavigationPolicy(
        onAction actionDecider: @escaping (NavigationAction, WebViewState) -> Void,
        onResponse responseDecider: @escaping (NavigationResponse, WebViewState) -> Void
    ) -> some View {
        environment(\.navigationActionDecider, actionDecider)
            .environment(\.navigationResponseDecider, responseDecider)
    }
}

/// Contains information about an action that may cause a navigation, used for making
/// policy decisions.
@dynamicMemberLookup
public struct NavigationAction {
    public typealias Policy = WKNavigationActionPolicy

    private var action: WKNavigationAction
    private var reply: (WKNavigationActionPolicy, WKWebpagePreferences) -> Void

    /// The default set of webpage preferences. This may be changed by passing modified preferences
    /// to `decidePolicy(_:webpagePreferences:)`.
    public private(set) var webpagePreferences: WKWebpagePreferences

    // FIXME: `webpagePreferences` lacks value semantics, which breaks the API contract. This
    // instance should not be mutable, but that would require an Objective-C type bridge. We could
    // also conform it to `NSCopying` and mark this property as `@NSCopying`.

    init(
        _ action: WKNavigationAction,
        webpagePreferences: WKWebpagePreferences,
        reply: @escaping (WKNavigationActionPolicy, WKWebpagePreferences) -> Void
    ) {
        self.action = action
        self.reply = reply
        self.webpagePreferences = webpagePreferences
    }

    /// Decides what action to take for this navigation. This method **must** be invoked exactly
    /// once, otherwise WebKit will raise an exception.
    /// - Parameters:
    ///   - policy: The desired policy for this navigation.
    ///   - webpagePreferences: The preferences to use for this navigation, or `nil` to use the
    ///   default preferences.
    public func decidePolicy(_ policy: Policy, webpagePreferences: WKWebpagePreferences? = nil) {
        reply(policy, webpagePreferences ?? self.webpagePreferences)
    }

    public subscript<T>(dynamicMember keyPath: KeyPath<WKNavigationAction, T>) -> T {
        action[keyPath: keyPath]
    }

    fileprivate struct DeciderKey : EnvironmentKey {
        static let defaultValue: ((NavigationAction, WebViewState) -> Void)? = nil
    }
}

/// Contains information about a navigation response, used for making policy decisions.
@dynamicMemberLookup
public struct NavigationResponse {
    public typealias Policy = WKNavigationResponsePolicy

    private var response: WKNavigationResponse
    private var reply: (Policy) -> Void

    init(_ response: WKNavigationResponse, reply: @escaping (Policy) -> Void) {
        self.response = response
        self.reply = reply
    }

    /// Decides what action to take for this navigation. This method **must** be invoked exactly
    /// once, otherwise WebKit will raise an exception.
    /// - Parameters:
    ///   - policy: The desired policy for this navigation.
    public func decidePolicy(_ policy: Policy) {
        reply(policy)
    }

    public subscript<T>(dynamicMember keyPath: KeyPath<WKNavigationResponse, T>) -> T {
        response[keyPath: keyPath]
    }

    fileprivate struct DeciderKey : EnvironmentKey {
        static let defaultValue: ((NavigationResponse, WebViewState) -> Void)? = nil
    }
}

extension EnvironmentValues {
    var navigationActionDecider: ((NavigationAction, WebViewState) -> Void)? {
        get { self[NavigationAction.DeciderKey.self] }
        set { self[NavigationAction.DeciderKey.self] = newValue }
    }

    var navigationResponseDecider: ((NavigationResponse, WebViewState) -> Void)? {
        get { self[NavigationResponse.DeciderKey.self] }
        set { self[NavigationResponse.DeciderKey.self] = newValue }
    }
}
