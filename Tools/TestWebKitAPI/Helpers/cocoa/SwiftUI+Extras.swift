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

public import SwiftUI
#if WTF_PLATFORM_MAC
private import AppKit
#else
private import UIKit
#endif

#if ENABLE_SWIFTUI && !WTF_PLATFORM_WATCHOS

/// Renders a SwiftUI view, updating whenever its observable state changes.
///
/// For example, to render a view whose state depends on an `isEditable` property of some model:
///
/// ```swift
/// let model = ViewModel()
///
/// render {
///     TestView().environment(model)
/// } observing: {
///     model.isEditable
/// }
///
/// model.isEditable = true // -> updates TestView
/// model.isEditable = false // -> updates TestView
/// ```
///
/// - Parameters:
///   - rootView: The view to render.
///   - observable: A closure that contains properties to track. The properties that `rootView` depends on must be captured within the `observable` closure.
#if hasAttribute(diagnose)
@diagnose(DeprecatedDeclaration, as: ignored, reason: "rdar://183894032")
#endif
@MainActor
public func render(
    @ViewBuilder rootView: () -> some View,
    @_inheritActorContext observing observable: @escaping @isolated(any) @Sendable () -> some Sendable
) {
    let resolvedView = rootView()

    #if WTF_PLATFORM_MAC
    let viewController = NSHostingController(rootView: resolvedView)
    #else
    let viewController = UIHostingController(rootView: resolvedView)

    // The hosting controller must be installed in a window for a layout pass to run,
    // otherwise the web view's constraints are never resolved and it stays zero-sized.

    let window = UIWindow(frame: .init(x: 0, y: 0, width: 800, height: 600))
    window.rootViewController = viewController
    window.isHidden = false
    window.layoutIfNeeded()
    viewController.view.layoutIfNeeded()
    #endif

    viewController._render(seconds: 1.0 / 60.0)

    Task.immediate {
        for await _ in Observations(observable) {
            viewController._render(seconds: 1.0 / 60.0)
        }
    }
}

/// Renders a SwiftUI view.
///
/// - Parameter rootView: The view to render.
@MainActor
public func render(@ViewBuilder rootView: () -> some View) {
    render(rootView: rootView) { () }
}

#endif // ENABLE_SWIFTUI && !WTF_PLATFORM_WATCHOS

extension Font {
    #if WTF_PLATFORM_IOS_FAMILY
    /// The attributes corresponding to the Cocoa-equivalent type.
    public typealias CocoaAttributes = AttributeScopes.UIKitAttributes
    #else
    /// The attributes corresponding to the Cocoa-equivalent type.
    public typealias CocoaAttributes = AttributeScopes.AppKitAttributes
    #endif

    /// Converts a UIKit or AppKit font to a SwiftUI Font.
    ///
    /// - Parameter font: The font to convert.
    public init?(_ font: CocoaAttributes.FontAttribute.Value?) {
        guard let font else {
            return nil
        }

        self = Font(font as CTFont)
    }
}
