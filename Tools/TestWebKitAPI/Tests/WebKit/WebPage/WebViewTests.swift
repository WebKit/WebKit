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

#if ENABLE_SWIFTUI && !os(watchOS) && !os(tvOS)

import Observation
import SwiftUI
import Testing
import WebKit
@_spi(Experimental) import _WebKit_SwiftUI
private import TestWebKitAPILibrary
import struct _Concurrency.Task
#if WTF_PLATFORM_MAC
import AppKit
#else
import UIKit
#endif

@Observable
@MainActor
private final class ViewModel {
    let page = WebPage()

    var isEditable: Bool = false
}

@MainActor
struct WebViewTests {
    @Test
    func applyingContentEnvironmentAffectsPageSemantics() async throws {
        struct TestView: View {
            @Environment(ViewModel.self)
            var model

            var body: some View {
                WebView(model.page)
                    .webViewContentEnvironmentV0(model.isEditable ? .editable : .standard)
            }
        }

        func isContentEditable() async throws -> Bool {
            try await model.page.callJavaScript(returning: Bool.self) {
                """
                const element = document.getElementById("div");
                return element.isContentEditable;
                """
            }
        }

        let model = ViewModel()

        render(observing: model) {
            TestView()
                .environment(model)
        }

        model.isEditable = true

        try await model.page.load(html: #"<div id="div">hello</div>"#).wait()
        await model.page.waitForNextPresentationUpdate()

        #expect(try await isContentEditable())

        model.isEditable = false

        await model.page.waitForNextPresentationUpdate()

        #expect(!(try await isContentEditable()))
    }
}

@MainActor
func render(
    observing observable: @escaping @isolated(any) @autoclosure @Sendable () -> some Observable & Sendable,
    @ViewBuilder rootView: () -> some View
) {
    let resolvedView = rootView()

    #if WTF_PLATFORM_MAC
    let viewController = NSHostingController(rootView: resolvedView)
    #else
    let viewController = UIHostingController(rootView: resolvedView)
    #endif

    Task.immediate {
        for await _ in Observations(observable) {
            viewController._render(seconds: 1.0 / 60.0)
        }
    }
}

#endif // ENABLE_SWIFTUI && !os(watchOS) && !os(tvOS)
