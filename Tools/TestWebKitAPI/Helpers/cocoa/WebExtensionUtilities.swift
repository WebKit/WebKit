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

#if ENABLE_WK_WEB_EXTENSIONS

import CoreGraphics
import Foundation
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
public import WebKit

public import struct Swift.String

#if WTF_PLATFORM_MAC
import AppKit
#else
import UIKit
#endif

/// Creates a manager for an extension parsed from a manifest, without loading it.
///
/// - Parameters:
///   - manifest: The extension's manifest, as it would appear in `manifest.json`.
///   - resources: The extension's resources, keyed by path.
///   - configuration: The controller configuration to use. Defaults to a non-persistent one.
/// - Returns: A manager for the parsed extension.
@MainActor
public func parseWebExtension(
    manifest: [String: Any],
    resources: [String: Any] = [:],
    configuration: WKWebExtensionController.Configuration? = nil
) -> TestWebExtensionManager {
    let manager = TestWebExtensionManager(
        manifest: manifest,
        resources: resources,
        extensionControllerConfiguration: configuration
    )
    manager.collectsFailures = true
    return manager
}

/// Creates a manager for an extension parsed from a manifest, and loads it.
///
/// - Parameters:
///   - manifest: The extension's manifest, as it would appear in `manifest.json`.
///   - resources: The extension's resources, keyed by path.
///   - configuration: The controller configuration to use. Defaults to a non-persistent one.
/// - Returns: A manager for the loaded extension.
/// - Throws: the failure the extension reported if it could not be loaded.
@MainActor
public func loadWebExtension(
    manifest: [String: Any],
    resources: [String: Any] = [:],
    configuration: WKWebExtensionController.Configuration? = nil
) throws -> TestWebExtensionManager {
    let manager = parseWebExtension(manifest: manifest, resources: resources, configuration: configuration)

    manager.load()

    try manager.checkCollectedFailures()

    return manager
}

extension TestWebExtensionManager {
    /// Waits for the extension to present the popup for one of its actions.
    ///
    /// - Parameter body: Work that causes the popup to be presented, such as performing the action
    ///   for a tab. Omit it when the extension opens its own popup.
    /// - Returns: The action whose popup was presented.
    /// - Throws: if the extension's test harness reported a failure
    ///   while this call was waiting.
    @MainActor
    public func nextPresentedAction(triggeredBy body: () -> Void = {}) async throws -> WKWebExtension.Action {
        try await nextAction(reportedBy: \.presentPopupForAction, triggeredBy: body)
    }

    /// Waits for the extension to change the appearance or behavior of one of its actions.
    ///
    /// - Parameter body: Work that causes the action to be updated. Omit it when the extension's
    ///   background script updates the action on its own.
    /// - Returns: The action that was updated.
    /// - Throws: if the extension's test harness reported a failure
    ///   while this call was waiting.
    @MainActor
    public func nextUpdatedAction(triggeredBy body: () -> Void = {}) async throws -> WKWebExtension.Action {
        try await nextAction(reportedBy: \.didUpdateAction, triggeredBy: body)
    }

    @MainActor
    private func nextAction(
        reportedBy callback: ReferenceWritableKeyPath<TestWebExtensionsDelegate, ((WKWebExtension.Action) -> Void)?>,
        triggeredBy body: () -> Void
    ) async throws -> WKWebExtension.Action {
        let previousCallback = internalDelegate[keyPath: callback]
        defer { internalDelegate[keyPath: callback] = previousCallback }

        let action = await withCheckedContinuation { continuation in
            internalDelegate[keyPath: callback] = { action in
                continuation.resume(returning: action)
            }

            body()
        }

        try checkCollectedFailures()

        return action
    }

    /// Waits for the extension to ask for a new tab, and opens it.
    ///
    /// - Parameter body: Work that causes the tab to be requested.
    /// - Returns: The configuration the extension asked for.
    /// - Throws: if the extension's test harness reported a failure
    ///   while this call was waiting.
    @MainActor
    public func nextRequestedTab(triggeredBy body: () -> Void = {}) async throws -> WKWebExtension.TabConfiguration {
        let openNewTab = internalDelegate.openNewTab
        defer { internalDelegate.openNewTab = openNewTab }

        let configuration = await withCheckedContinuation { continuation in
            internalDelegate.openNewTab = { configuration, context, completionHandler in
                openNewTab?(configuration, context, completionHandler)
                continuation.resume(returning: configuration)
            }

            body()
        }

        try checkCollectedFailures()

        return configuration
    }

    #if WTF_PLATFORM_MAC
    /// Waits for the extension to ask for a new window, and opens it.
    ///
    /// - Parameter body: Work that causes the window to be requested.
    /// - Returns: The configuration the extension asked for.
    /// - Throws: if the extension's test harness reported a failure
    ///   while this call was waiting.
    @MainActor
    public func nextRequestedWindow(triggeredBy body: () -> Void = {}) async throws -> WKWebExtension.WindowConfiguration {
        let openNewWindow = internalDelegate.openNewWindow
        defer { internalDelegate.openNewWindow = openNewWindow }

        let configuration = await withCheckedContinuation { continuation in
            internalDelegate.openNewWindow = { configuration, context, completionHandler in
                openNewWindow?(configuration, context, completionHandler)
                continuation.resume(returning: configuration)
            }

            body()
        }

        try checkCollectedFailures()

        return configuration
    }
    #endif

    /// Grants every permission the extension asks for.
    @MainActor
    public func grantRequestedPermissions() {
        internalDelegate.promptForPermissions = { _, permissions, callback in
            callback(Set(permissions.map { WKWebExtension.Permission($0) }), nil)
        }

        internalDelegate.promptForPermissionMatchPatterns = { _, matchPatterns, callback in
            callback(matchPatterns, nil)
        }

        internalDelegate.promptForPermissionToAccessURLs = { _, urls, callback in
            callback(urls, nil)
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
