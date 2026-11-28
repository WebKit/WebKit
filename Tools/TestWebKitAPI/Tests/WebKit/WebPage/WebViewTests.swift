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

#if ENABLE_SWIFTUI && !WTF_PLATFORM_WATCHOS && !WTF_PLATFORM_APPLETV

import Observation
import SwiftUI
import Testing
@_spi(Testing) import WebKit
import WebKit_Private.WKWebViewPrivate
@_spi(Experimental) import _WebKit_SwiftUI
private import TestWebKitAPILibrary
import struct Swift.String
#if WTF_PLATFORM_MAC
import AppKit
#else
import UIKit
#endif

@Observable
@MainActor
private final class ViewModel {
    struct Attachments: Equatable {
        var inserted: Set<_WKAttachment> = []
        var removed: Set<_WKAttachment> = []
    }

    let page: WebPage = {
        var configuration = WebPage.Configuration()
        configuration.attachmentElementEnabled = true
        return WebPage(configuration: configuration)
    }()

    var isEditable: Bool = false

    // The `_WebKit_SwiftUI` qualifier is needed to disambiguate from WebKitLegacy's WebView.
    // FIXME: Consider alternative designs to avoid this requirement.
    var viewportWidth: _WebKit_SwiftUI.WebView.ViewportConfiguration_v0.Width? = nil
    var viewportInitialScale: Float? = nil

    var attachments = Attachments()

    var scrollPosition = ScrollPosition(edge: .top)
}

private struct TestView: View {
    @Environment(ViewModel.self)
    var model

    var body: some View {
        WebView(model.page)
            .webViewContentEnvironmentV0(model.isEditable ? .editable : .standard)
            .webViewViewportConfigurationV0(
                width: model.viewportWidth,
                initialScale: model.viewportInitialScale
            )
            .webViewOnAttachmentActivityPhase { phase in
                switch phase.kind {
                case .inserted(_):
                    model.attachments.inserted.insert(phase.attachment)

                case .removed:
                    model.attachments.removed.insert(phase.attachment)

                case .dataInvalidated:
                    break

                @unknown default:
                    fatalError()
                }
            }
    }
}

private struct ScrollPositionTestView: View {
    @Environment(ViewModel.self)
    var model

    var body: some View {
        @Bindable
        var model = model

        WebView(model.page)
            .webViewScrollPosition($model.scrollPosition)
    }
}

private final class WebViewGeometry {
    struct Placement: Equatable {
        var size: CGSize = .zero
        var safeAreaInsets = EdgeInsets()
    }

    var placement = Placement()
    var containerSize: CGSize = .zero
}

extension View {
    fileprivate func recordingGeometry(in geometry: WebViewGeometry) -> some View {
        self
            .webViewOnScrollGeometryChange(for: CGSize.self, of: \.containerSize) { _, containerSize in
                geometry.containerSize = containerSize
            }
            .onGeometryChange(for: WebViewGeometry.Placement.self) { proxy in
                .init(size: proxy.size, safeAreaInsets: proxy.safeAreaInsets)
            } action: { placement in
                geometry.placement = placement
            }
    }
}

#if WTF_PLATFORM_IOS_FAMILY
private struct SafeAreaInsetTestView: View {
    let page: WebPage
    let padding: CGFloat
    let layoutDirection: LayoutDirection
    let geometry: WebViewGeometry

    var body: some View {
        WebView(page)
            .recordingGeometry(in: geometry)
            .padding([.top, .leading], padding)
            .safeAreaInset(edge: .trailing, spacing: 0) {
                Color.clear
                    .frame(width: 84)
            }
            .safeAreaInset(edge: .bottom, spacing: 0) {
                Color.clear
                    .frame(height: 84)
            }
            .environment(\.layoutDirection, layoutDirection)
    }
}
#endif // WTF_PLATFORM_IOS_FAMILY

@MainActor
struct WebViewTests {
    @Test
    func applyingContentEnvironmentAffectsPageSemantics() async throws {
        func isContentEditable() async throws -> Bool {
            try await model.page.callJavaScript(returning: Bool.self) {
                """
                const element = document.getElementById("div");
                return element.isContentEditable;
                """
            }
        }

        let model = ViewModel()

        render {
            TestView()
                .environment(model)
        } observing: {
            model.isEditable
        }

        model.isEditable = true

        try await model.page.load(html: #"<div id="div">hello</div>"#).wait()
        await model.page.waitForNextPresentationUpdate()

        #expect(try await isContentEditable())

        model.isEditable = false

        await model.page.waitForNextPresentationUpdate()

        #expect(!(try await isContentEditable()))
    }

    @Test
    func onAttachmentActivityPhaseAffectsListeners() async throws {
        let html = """
            <meta name='viewport' content='width=device-width, initial-scale=1'>
            <script>
            focus = () => document.body.focus()
            </script>
            <body onload=focus() contenteditable></body>
            """

        let model = ViewModel()

        render {
            TestView()
                .environment(model)
        }

        try await model.page.load(html: html).wait()

        let testHTMLData = try #require("<a href='#'>This is some HTML data</a>".data(using: .utf8))
        let testImageFileURL = try #require(Bundle.testResources.url(forResource: "icon", withExtension: "png"))
        let testImageData = try Data(contentsOf: testImageFileURL)

        func insertAttachment(filename: String, contentType: String?, data: Data) async -> _WKAttachment? {
            let fileWrapper = FileWrapper(regularFileWithContents: data)
            fileWrapper.preferredFilename = filename

            return await model.page.insertAttachment(fileWrapper: fileWrapper, contentType: contentType) as? _WKAttachment
        }

        let firstAttachment = await insertAttachment(filename: "foo", contentType: "text/html", data: testHTMLData)

        #expect(model.attachments.removed == [])
        #expect(model.attachments.inserted == [firstAttachment])

        await model.page.executeEditCommand(.deleteBackward)

        #expect(model.attachments.removed == [firstAttachment])
        #expect(model.attachments.inserted == [firstAttachment])

        let secondAttachment = await insertAttachment(filename: "bar.png", contentType: "text/html", data: testImageData)

        #expect(model.attachments.removed == [firstAttachment])
        #expect(model.attachments.inserted == [firstAttachment, secondAttachment])
    }

    #if WTF_PLATFORM_MAC
    @Test
    func animatedScrollToEdgeUsesSpring() async throws {
        let model = ViewModel()

        render {
            ScrollPositionTestView()
                .frame(width: 500, height: 500)
                .environment(model)
        } observing: {
            model.scrollPosition
        }

        let html = """
            <body style='height: 10000px'>
            <script>
            window.scrollEndCount = 0;
            window.scrollEndTime = new Promise(resolve => {
                addEventListener('scrollend', () => {
                    ++window.scrollEndCount;
                    resolve(performance.now());
                });
            });
            </script>
            </body>
            """

        try await model.page.load(html: html).wait()
        await model.page.waitForNextPresentationUpdate()

        let startTime = try await model.page.callJavaScript(returning: Double.self) {
            "return performance.now();"
        }

        withAnimation {
            model.scrollPosition.scrollTo(edge: .bottom)
        }

        // The spring takes over a second to reach the bottom of this page, whereas other animated scrolls take at most 200ms.
        let endTime = try await model.page.callJavaScript(returning: Double.self) {
            "return await window.scrollEndTime;"
        }
        #expect(endTime - startTime > 400)

        let isAtBottom = try await model.page.callJavaScript(returning: Bool.self) {
            "return window.scrollY == document.scrollingElement.scrollHeight - innerHeight;"
        }
        #expect(isAtBottom)

        let scrollEndCount = try await model.page.callJavaScript(returning: Int.self) {
            "return window.scrollEndCount;"
        }
        #expect(scrollEndCount == 1)
    }
    #endif // WTF_PLATFORM_MAC

    #if WTF_PLATFORM_IOS_FAMILY
    @Test
    func overrideViewportArgumentsAffectsPageViewport() async throws {
        func bodyWidth() async throws -> Int {
            await model.page.waitForNextPresentationUpdate()

            return try await model.page.callJavaScript(returning: Int.self) {
                """
                return document.body.clientWidth;
                """
            }
        }

        let model = ViewModel()

        render {
            TestView()
                .frame(width: 20, height: 20)
                .environment(model)
        } observing: {
            (model.viewportWidth, model.viewportInitialScale)
        }

        let htmlWithInitialScale = """
            <meta name='viewport' content='initial-scale=1'>
            <div id='divWithViewportUnits' style='width: 100vw;'></div>
            """

        try await model.page.load(html: htmlWithInitialScale).wait()

        try await #expect(bodyWidth() == 20)

        model.viewportWidth = 1000
        try await #expect(bodyWidth() == 1000)

        model.viewportInitialScale = 1
        try await #expect(bodyWidth() == 1000)
        #expect(model.page.magnification == 1)

        model.viewportInitialScale = 5
        try await #expect(bodyWidth() == 1000)
        #expect(model.page.magnification == 5)

        model.viewportWidth = nil
        model.viewportInitialScale = nil

        try await #expect(bodyWidth() == 20)

        let htmlWithWidth = """
            <meta name='viewport' content='width=10'>
            <div id='divWithViewportUnits' style='width: 100vw;'></div>
            """

        try await model.page.load(html: htmlWithWidth).wait()

        try await #expect(bodyWidth() == 10)

        model.viewportWidth = 1000
        model.viewportInitialScale = 1
        try await #expect(bodyWidth() == 1000)

        model.viewportWidth = .deviceWidth
        model.viewportInitialScale = 1
        try await #expect(bodyWidth() == 20)
    }

    // A fractional padding gives the web view a fractional size, which it rounds down to whole points.
    @Test(arguments: [120, 120.5], [LayoutDirection.leftToRight, .rightToLeft])
    func webViewBorderingSafeAreaInsetsContentOnce(padding: CGFloat, layoutDirection: LayoutDirection) async throws {
        let page = WebPage()
        let geometry = WebViewGeometry()

        render {
            SafeAreaInsetTestView(page: page, padding: padding, layoutDirection: layoutDirection, geometry: geometry)
        }

        try await page.load(html: "<meta name='viewport' content='width=device-width, initial-scale=1'>").wait()
        await page.waitForNextPresentationUpdate()

        let size = geometry.placement.size
        let safeAreaInsets = geometry.placement.safeAreaInsets
        try #require(safeAreaInsets.trailing > 0 && safeAreaInsets.bottom > 0, "The web view should border the safe area.")

        #expect(geometry.containerSize.width == (size.width + safeAreaInsets.trailing).rounded(.down))
        #expect(geometry.containerSize.height == (size.height + safeAreaInsets.bottom).rounded(.down))

        let (viewportWidth, viewportHeight) = try await page.callJavaScript(returning: (CGFloat, CGFloat).self) {
            "return [innerWidth, innerHeight];"
        }
        #expect(viewportWidth == size.width.rounded(.down))
        #expect(viewportHeight == size.height.rounded(.down))
    }
    #endif // WTF_PLATFORM_IOS_FAMILY

    // A fractional height gives the web view a fractional size, which it rounds down to whole points.
    @Test(arguments: [300, 300.5])
    func shortPageDoesNotOverflowWebView(height: CGFloat) async throws {
        let page = WebPage()
        let geometry = WebViewGeometry()

        render {
            WebView(page)
                .frame(width: 300, height: height)
                .recordingGeometry(in: geometry)
        }

        try await page.load(html: "<meta name='viewport' content='width=device-width, initial-scale=1'>").wait()
        await page.waitForNextPresentationUpdate()

        #expect(geometry.containerSize.height == geometry.placement.size.height.rounded(.down))

        let pageHeight = try await page.callJavaScript(returning: CGFloat.self) { "return document.scrollingElement.scrollHeight;" }
        #expect(pageHeight <= geometry.containerSize.height)
    }
}

#endif // ENABLE_SWIFTUI && !WTF_PLATFORM_WATCHOS && !WTF_PLATFORM_APPLETV
