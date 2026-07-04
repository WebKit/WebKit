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

#if os(macOS)

public import AppKit
import Foundation
public import SwiftUI

extension NSWindow {
    /// Create a new NSWindow with the specified size, containing a SwiftUI view.
    ///
    /// - Parameters:
    ///   - size: The content size for the window.
    ///   - rootView: The root view for the window.
    public convenience init(size: NSSize, @ViewBuilder rootView: () -> some View) {
        let viewController = NSHostingController(rootView: rootView())
        self.init(contentViewController: viewController)
        setContentSize(size)
        layoutIfNeeded()
    }
}

extension NSApplication {
    /// Suspends execution until this application is active.
    ///
    /// - Note: This must be called prior to any test that depends on having a "real" NSApplication.
    @MainActor
    public func waitForActivation() async {
        // Activation is processed asynchronously by the AppKit event loop, so wait for it to take
        // effect before synthesizing events.
        var attempts = 0
        while !NSApp.isActive && attempts < 500 {
            try? await Task.sleep(for: .milliseconds(10))
            attempts += 1
        }

        guard NSApp.isActive else {
            fatalError("NSApp is not active; unable to properly run test")
        }
    }
}

#endif // os(macOS)
