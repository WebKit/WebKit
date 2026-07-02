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

#if ENABLE_SWIFTUI

public import SwiftUI
@_spi(CrossImportOverlay) import WebKit
import WebKit_Private

/// A SwiftUI view that renders a specific website-provided immersive environment.
///
/// Place this view in your app's Immersive Space hierarchy. Initialize it with the
/// `WebPage.ImmersiveEnvironment` received from the presentation callback to render
/// that specific environment.
@MainActor
@available(TBA, *)
@available(iOS, unavailable)
@available(macOS, unavailable)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
public struct WebViewImmersiveEnvironmentView: View {
    private let environment: WKImmersiveEnvironment

    /// Creates an immersive environment view from a ``WebPage/ImmersiveEnvironment``.
    public init(_ environment: WebPage.ImmersiveEnvironment) {
        self.init(environment.wrapped)
    }

    /// Creates an immersive environment view from a `WKImmersiveEnvironment`.
    public init(_ environment: WKImmersiveEnvironment) {
        self.environment = environment
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var body: some View {
        #if os(visionOS)
        Representable(environment: environment)
        #endif
    }

    #if os(visionOS)
    @MainActor
    private struct Representable: UIViewRepresentable {
        let environment: WKImmersiveEnvironment

        func makeUIView(context: Context) -> UIView {
            environment._environmentView
        }

        func updateUIView(_ uiView: UIView, context: Context) {}
    }
    #endif
}

#endif
