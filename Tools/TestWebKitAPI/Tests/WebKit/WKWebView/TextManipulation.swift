// Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
private import TestWebKitAPILibrary.Helpers.cocoa.WKWebViewConfigurationExtras
import Testing
import WebKit
private import WebKit_Private
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private._WKTextManipulationConfiguration
private import WebKit_Private._WKTextManipulationDelegate
private import WebKit_Private._WKTextManipulationExclusionRule
private import WebKit_Private._WKTextManipulationItem
private import WebKit_Private._WKTextManipulationToken

import struct Foundation.URL
import struct Swift.String

// MARK: Supporting helpers

@MainActor
private final class TextManipulationDelegate: NSObject, @preconcurrency _WKTextManipulationDelegate {
    private(set) var items: [_WKTextManipulationItem] = []
    private(set) var itemBatchCount = 0

    private var itemCountWaiter: (count: Int, continuation: CheckedContinuation<Void, Never>)?

    // swift-format-ignore: NoLeadingUnderscores
    func _webView(_ webView: WKWebView, didFind items: [_WKTextManipulationItem]) {
        self.items += items
        itemBatchCount += 1

        if let waiter = itemCountWaiter, self.items.count >= waiter.count {
            itemCountWaiter = nil
            waiter.continuation.resume()
        }
    }

    @discardableResult
    func waitForItems(count: Int) async -> [_WKTextManipulationItem] {
        if items.count < count {
            await withCheckedContinuation { itemCountWaiter = (count, $0) }
        }
        return items
    }
}

@MainActor
private final class LegacyTextManipulationDelegate: NSObject, @preconcurrency _WKTextManipulationDelegate {
    private(set) var items: [_WKTextManipulationItem] = []

    // swift-format-ignore: NoLeadingUnderscores
    func _webView(_ webView: WKWebView, didFind item: _WKTextManipulationItem) {
        items.append(item)
    }
}

extension _WKTextManipulationToken {
    fileprivate convenience init(identifier: String?, content: String?, excluded: Bool = false) {
        self.init()
        self.identifier = identifier
        self.isExcluded = excluded
        self.content = content
    }
}

private func proximity(
    forItemContaining text: String,
    in items: [_WKTextManipulationItem]
) -> _WKTextManipulationViewportProximityInfo? {
    items.first { $0.tokens.contains { $0.content?.contains(text) == true } }?.viewportProximityInfo
}

private let subscrollerHTML = """
    <!DOCTYPE html>\
    <meta name='viewport' content='width=800, initial-scale=1'>\
    <body style='margin: 0; font: 20px/20px monospace'>\
    <div id='scroller' style='overflow: scroll; width: 400px; height: 60px'>\
    <p style='margin: 0; height: 200px'>First paragraph</p>\
    <p style='margin: 0; height: 200px'>Second paragraph</p>\
    </div>\
    <div style='height: 2000px'></div>\
    <div>Offscreen text</div>\
    </body>
    """

// MARK: Tests

@MainActor
struct TextManipulationTests {
    private let delegate: TextManipulationDelegate
    private let webView: TestWKWebView

    init() {
        delegate = TextManipulationDelegate()
        webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))
        webView._textManipulationDelegate = delegate
    }

    private func bodyInnerHTML() async throws -> String {
        try await webView.callJavaScript(returning: String.self) { "return document.body.innerHTML" }
    }

    private func selectionDirectionProbe(elementID: String) async throws -> String {
        try await webView.callJavaScript(returning: String.self) {
            """
            var p = document.getElementById('\(elementID)');
            var t = p.firstChild;
            var sel = getSelection();
            var r = document.createRange();
            r.setStart(t, 10);
            r.collapse(true);
            sel.removeAllRanges();
            sel.addRange(r);
            sel.modify('extend', 'right', 'character');
            if (sel.focusNode !== t) return 'other';
            if (sel.focusOffset > 10) return 'forward';
            if (sel.focusOffset < 10) return 'backward';
            return 'none';
            """
        }
    }

    @Test
    func startTextManipulationExitEarlyWithoutDelegate() async throws {
        let delegate = TextManipulationDelegate()
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))

        try await webView.load(html: "<!DOCTYPE html><html><body>hello<br>world<div>WebKit</div></body></html>")

        await webView._startTextManipulations(with: nil)

        #expect(delegate.items.isEmpty)
    }

    @Test
    func startTextManipulationFindSimpleParagraphs() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body>hello<br>world<div>WebKit</div></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 3)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")

        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "world")

        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "WebKit")
    }

    @Test
    func startTextManipulationFindMultipleParagraphsInSingleTextNode() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><pre>hello\nworld\nWebKit</pre></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 5)
        #expect(items[0].tokens[0].content == "hello")
        #expect(items[0].tokens[1].content == "\n")
        #expect(items[0].tokens[2].content == "world")
        #expect(items[0].tokens[3].content == "\n")
        #expect(items[0].tokens[4].content == "WebKit")
    }

    @Test
    func startTextManipulationFindParagraphsWithMultipleTokens() async throws {
        try await webView.load(
            html: "<!DOCTYPE html><html><body>hello,  <b>world</b><br><div><em> <b>Web</b>Kit</em>  </div></body></html>"
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "hello, ")
        #expect(items[0].tokens[1].content == "world")

        try #require(items[1].tokens.count == 2)
        #expect(items[1].tokens[0].content == "Web")
        #expect(items[1].tokens[1].content == "Kit")
    }

    @Test
    func startTextManipulationFindAttributeContent() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html><html><head><title>hey</title></head>\
                <body><div><span aria-label="this is greet">hello</span><img src="apple.gif" alt="fruit"></body></html>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 4)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hey")
        #expect(!items[0].tokens[0].isExcluded)

        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "this is greet")
        #expect(!items[1].tokens[0].isExcluded)

        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "fruit")
        #expect(!items[2].tokens[0].isExcluded)

        try #require(items[3].tokens.count == 1)
        #expect(items[3].tokens[0].content == "hello")
        #expect(!items[3].tokens[0].isExcluded)
    }

    @Test
    func startTextManipulationSupportsLegacyDelegateCallback() async throws {
        let delegate = LegacyTextManipulationDelegate()
        webView._textManipulationDelegate = delegate

        try await webView.load(html: "<!DOCTYPE html><html><body><p>hello, <span>world</span></p><p>WebKit</p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "hello, ")
        #expect(items[0].tokens[1].content == "world")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "WebKit")
    }

    @Test
    func startTextManipulationFindNewlyInsertedParagraph() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>hello</p></body></html>")

        await webView._startTextManipulations(with: nil)

        var items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")

        try await webView.callJavaScript {
            "document.body.appendChild(document.createElement('div')).innerHTML = 'world<br><b>Web</b>Kit';"
        }
        items = await delegate.waitForItems(count: 3)

        try #require(items.count == 3)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "world")
        try #require(items[2].tokens.count == 2)
        #expect(items[2].tokens[0].content == "Web")
        #expect(items[2].tokens[1].content == "Kit")
    }

    @Test
    func startTextManipulationFindNewlyDisplayedParagraph() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html><html><body>\
                <style> .hidden { display: none; } </style>\
                <p>hello</p>\
                <div>\
                <span class='hidden'>Web</span>\
                <span class='hidden'>Kit</span>\
                </div>\
                <div class='hidden'>hey</div>\
                <section id='section' hidden='until-found'>there</section>\
                </body></html>
                """
        )

        await webView._startTextManipulations(with: nil)

        var items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")

        try await webView.callJavaScript {
            "document.querySelectorAll('span.hidden').forEach((span) => span.classList.remove('hidden'));"
        }
        items = await delegate.waitForItems(count: 2)

        try #require(items.count == 2)
        try #require(items[1].tokens.count == 2)
        #expect(items[1].tokens[0].content == "Web")
        #expect(items[1].tokens[1].content == "Kit")

        // These need to happen separately in order to have a deterministic ordering.
        try await webView.callJavaScript {
            "document.querySelectorAll('div.hidden').forEach((div) => div.classList.remove('hidden'));"
        }
        items = await delegate.waitForItems(count: 3)

        try #require(items.count == 3)
        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "hey")

        try await webView.callJavaScript { "document.getElementById('section').removeAttribute('hidden');" }
        items = await delegate.waitForItems(count: 4)

        try #require(items.count == 4)
        try #require(items[3].tokens.count == 1)
        #expect(items[3].tokens[0].content == "there")
    }

    @Test
    func startTextManipulationFindSameParagraphWithNewContent() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>hello</p></body></html>")

        await webView._startTextManipulations(with: nil)

        var items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")

        try await webView.callJavaScript {
            """
            b = document.createElement('b');
            b.textContent = ' world';
            document.querySelector('p').appendChild(b); ''
            """
        }

        items = await delegate.waitForItems(count: 2)

        try #require(items.count == 2)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == " world")
    }

    @Test
    func startTextManipulationApplySingleExcluionRuleForElement() async throws {
        try await webView.load(
            html: "<!DOCTYPE html><html><body>Here's some code:<code>function <span>F</span>() { }</code>.</body></html>"
        )

        let configuration = _WKTextManipulationConfiguration()
        configuration.exclusionRules = [
            _WKTextManipulationExclusionRule(exclusion: true, forElement: "code")
        ]

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 5)
        let tokens = items[0].tokens
        #expect(tokens[0].content == "Here's some code:")
        #expect(!tokens[0].isExcluded)
        #expect(tokens[1].content == "function ")
        #expect(tokens[1].isExcluded)
        #expect(tokens[2].content == "F")
        #expect(tokens[2].isExcluded)
        #expect(tokens[3].content == "() { }")
        #expect(tokens[3].isExcluded)
        #expect(tokens[4].content == ".")
        #expect(!tokens[4].isExcluded)
    }

    @Test
    func startTextManipulationApplyInclusionExclusionRulesForAttributes() async throws {
        try await webView.load(
            html: "<!DOCTYPE html><html><body><span data-exclude=Yes><b>hello, </b><span data-exclude=NO>world</span></span></body></html>"
        )

        let configuration = _WKTextManipulationConfiguration()
        configuration.exclusionRules = [
            _WKTextManipulationExclusionRule(exclusion: true, forAttribute: "data-exclude", value: "yes"),
            _WKTextManipulationExclusionRule(exclusion: false, forAttribute: "data-exclude", value: "no"),
        ]

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "world")
        #expect(!items[0].tokens[0].isExcluded)
    }

    @Test
    func startTextManipulationApplyInclusionExclusionRulesForClass() async throws {
        try await webView.load(
            html:
                "<!DOCTYPE html><html><body>Message: <span class='someClass exclude'><b>hello, </b><span>world</span></span></body></html>"
        )

        let configuration = _WKTextManipulationConfiguration()
        configuration.exclusionRules = [
            _WKTextManipulationExclusionRule(exclusion: true, forClass: "exclude")
        ]

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "Message: ")
        #expect(!items[0].tokens[0].isExcluded)
    }

    @Test
    func startTextManipulationApplyInclusionExclusionRulesForClassAndAttribute() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html><body><span class='someClass exclude'>Message: <b data-exclude=no>hello, </b><span>world</span></span></body></html>
                """
        )

        let configuration = _WKTextManipulationConfiguration()
        configuration.exclusionRules = [
            _WKTextManipulationExclusionRule(exclusion: true, forAttribute: "data-exclude", value: "yes"),
            _WKTextManipulationExclusionRule(exclusion: false, forAttribute: "data-exclude", value: "no"),
            _WKTextManipulationExclusionRule(exclusion: true, forClass: "exclude"),
        ]

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello, ")
        #expect(!items[0].tokens[0].isExcluded)
    }

    @Test
    func startTextManipulationBreaksParagraphInBetweenListItems() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <head>\
                    <style>\
                        li.block { display: block; }\
                        li.float-left { float: left; margin: 1em; }\
                        li.inline { display: inline; }\
                    </style>\
                </head>\
                <body>\
                    <ul><li class='block'>One</li><li class='block'>Two<span>-three</span></li></ul>\
                    <div><br></div>\
                    <ul><li class='block float-left'>Four</li><li class='block float-left'>Five<span>-six</span></li></ul>\
                    <div><br></div>\
                    <ol><li class='inline'>Seven</li><li class='inline'>Eight</li></ol>\
                    <div><br></div>\
                    <ul><li>Nine</li><li>Ten</li></ol>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 7)

        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "One")

        try #require(items[1].tokens.count == 2)
        #expect(items[1].tokens[0].content == "Two")
        #expect(items[1].tokens[1].content == "-three")

        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "Four")

        try #require(items[3].tokens.count == 2)
        #expect(items[3].tokens[0].content == "Five")
        #expect(items[3].tokens[1].content == "-six")

        try #require(items[4].tokens.count == 2)
        #expect(items[4].tokens[0].content == "Seven")
        #expect(items[4].tokens[1].content == "Eight")

        try #require(items[5].tokens.count == 1)
        #expect(items[5].tokens[0].content == "Nine")

        try #require(items[6].tokens.count == 1)
        #expect(items[6].tokens[0].content == "Ten")
    }

    @Test
    func startTextManipulationBreaksParagraphInBetweenFloatingListItems() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <style>\
                ul {\
                    margin: 0;\
                    padding: 0;\
                    list-style: none\
                }\
                li {\
                    text-align: center;\
                    float: left;\
                    border: 1px solid #e9e9e9;\
                    padding: 1rem;\
                }\
                </style>\
                <ul>\
                <li>hello</li>\
                <li>world</li>\
                <li>WebKit</li>\
                </ul>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 3)

        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")

        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "world")

        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "WebKit")
    }

    @Test
    func startTextManipulationIncludesFullyClippedText() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <head>\
                    <style>\
                        div { overflow: hidden; width: 200px; height: 0; }\
                        p { visibility: hidden; }\
                    </style>\
                </head>\
                <body>\
                    <div><span>Hello</span> world</div>\
                    <br>\
                    <p>More text</p>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "Hello")
        #expect(items[0].tokens[1].content == " world")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "More text")
    }

    @Test
    func startTextManipulationFindsInsertedClippedText() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <head>\
                    <style>\
                        span { overflow: hidden; width: 200px; height: 0; }\
                        p { visibility: hidden; }\
                    </style>\
                </head>\
                <body>\
                    <div>hello, world</div>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        #expect(delegate.items.count == 1)

        try await webView.callJavaScript {
            """
            var beforeElement = document.createElement('span');
            beforeElement.innerHTML='before';
            document.querySelector('div').before(beforeElement);
            """
        }
        await delegate.waitForItems(count: 2)

        try await webView.callJavaScript {
            """
            var afterElement = document.createElement('p');
            afterElement.innerHTML='after';
            document.querySelector('div').after(afterElement)
            """
        }
        let items = await delegate.waitForItems(count: 3)

        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello, world")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "before")
        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "after")
    }

    @Test
    func viewportProximity() async throws {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600))
        webView._textManipulationDelegate = delegate

        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <meta name='viewport' content='width=800, initial-scale=1'>\
                <body style='margin: 0'>\
                <div>Onscreen text</div>\
                <div style='height: 2000px'></div>\
                <div>Offscreen text</div>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)
        let items = delegate.items

        let onscreen = try #require(proximity(forItemContaining: "Onscreen text", in: items))
        #expect(onscreen.relation == .intersecting)
        #expect(onscreen.viewportSizedDistance == 0)
        #expect(onscreen.viewportCoverage > 0)

        let offscreen = try #require(proximity(forItemContaining: "Offscreen text", in: items))
        #expect(offscreen.relation == .offscreen)
        #expect(offscreen.viewportSizedDistance > 0)
    }

    @Test
    func viewportProximityHorizontalDistance() async throws {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600))
        webView._textManipulationDelegate = delegate

        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <meta name='viewport' content='width=800, initial-scale=1'>\
                <body style='margin: 0'>\
                <div>Onscreen text</div>\
                <div style='position: absolute; top: 0; left: 2000px; white-space: nowrap'>Right of viewport</div>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)
        let items = delegate.items

        let onscreen = try #require(proximity(forItemContaining: "Onscreen text", in: items))
        #expect(onscreen.relation == .intersecting)
        #expect(onscreen.viewportCoverage > 0)

        let toTheRight = try #require(proximity(forItemContaining: "Right of viewport", in: items))
        #expect(toTheRight.relation == .offscreen)
        #expect(abs(toTheRight.viewportSizedDistance - 1.5) <= 0.1)
        #expect(toTheRight.viewportCoverage == 0)
    }

    @Test
    func viewportProximityCoverageBounds() async throws {
        do {
            let delegate = TextManipulationDelegate()
            let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600))
            webView._textManipulationDelegate = delegate

            try await webView.load(
                html: """
                    <!DOCTYPE html>\
                    <meta name='viewport' content='width=800, initial-scale=1'>\
                    <body style='margin: 0'>\
                    <div style='font: 1000px/1000px monospace'>WWWW</div>\
                    </body>
                    """
            )

            await webView._startTextManipulations(with: nil)
            let items = delegate.items

            let largerThanViewport = try #require(proximity(forItemContaining: "WWWW", in: items))
            #expect(largerThanViewport.relation == .intersecting)
            #expect(largerThanViewport.viewportCoverage == 1)
        }

        do {
            let delegate = TextManipulationDelegate()
            let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600))
            webView._textManipulationDelegate = delegate

            try await webView.load(
                html: """
                    <!DOCTYPE html>\
                    <meta name='viewport' content='width=800, initial-scale=1'>\
                    <body style='margin: 0; font: 100px/100px monospace'>\
                    <div>AAAA</div>\
                    <div style='height: 470px'></div>\
                    <div>BBBB</div>\
                    </body>
                    """
            )

            await webView._startTextManipulations(with: nil)
            let items = delegate.items

            let fullyVisible = try #require(proximity(forItemContaining: "AAAA", in: items))
            #expect(fullyVisible.relation == .intersecting)
            #expect(fullyVisible.viewportCoverage > 0)
            #expect(fullyVisible.viewportCoverage < 1)

            let straddlingBottomEdge = try #require(proximity(forItemContaining: "BBBB", in: items))
            #expect(straddlingBottomEdge.relation == .intersecting)
            #expect(straddlingBottomEdge.viewportCoverage > 0)
            #expect(straddlingBottomEdge.viewportCoverage < fullyVisible.viewportCoverage / 2)
        }
    }

    @Test
    func viewportProximityWithSubscroller() async throws {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600))
        webView._textManipulationDelegate = delegate

        try await webView.load(html: subscrollerHTML)

        await webView._startTextManipulations(with: nil)
        let items = delegate.items

        let first = try #require(proximity(forItemContaining: "First paragraph", in: items))
        #expect(first.relation == .intersecting)
        #expect(first.viewportSizedDistance == 0)
        #expect(first.viewportCoverage > 0)

        let second = try #require(proximity(forItemContaining: "Second paragraph", in: items))
        #expect(second.relation == .clippedByAncestor)
        #expect(second.viewportCoverage == 0)

        let offscreen = try #require(proximity(forItemContaining: "Offscreen text", in: items))
        #expect(offscreen.relation == .offscreen)
        #expect(offscreen.viewportSizedDistance > 0)
        #expect(offscreen.viewportCoverage == 0)
    }

    @Test
    func viewportProximityAfterScrollingSubscroller() async throws {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600))
        webView._textManipulationDelegate = delegate

        try await webView.load(html: subscrollerHTML)
        try await webView.callJavaScript { "document.getElementById('scroller').scrollTop = 200; document.body.offsetHeight" }

        await webView._startTextManipulations(with: nil)
        let items = delegate.items

        let first = try #require(proximity(forItemContaining: "First paragraph", in: items))
        #expect(first.relation == .clippedByAncestor)
        #expect(first.viewportCoverage == 0)

        let second = try #require(proximity(forItemContaining: "Second paragraph", in: items))
        #expect(second.relation == .intersecting)
        #expect(second.viewportSizedDistance == 0)
        #expect(second.viewportCoverage > 0)
    }

    @Test
    func viewportProximityWithClippingAncestor() async throws {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600))
        webView._textManipulationDelegate = delegate

        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <meta name='viewport' content='width=800, initial-scale=1'>\
                <body style='margin: 0'>\
                <div style='overflow: hidden; width: 200px; height: 0'><p style='margin: 0'>Clipped text</p></div>\
                <div>Onscreen text</div>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)
        let items = delegate.items

        let clipped = try #require(proximity(forItemContaining: "Clipped text", in: items))
        #expect(clipped.relation == .clippedByAncestor)
        #expect(clipped.viewportCoverage == 0)
        #expect(clipped.viewportSizedDistance == 0)

        let onscreen = try #require(proximity(forItemContaining: "Onscreen text", in: items))
        #expect(onscreen.relation == .intersecting)
        #expect(onscreen.viewportCoverage > 0)
    }

    @Test
    func startTextManipulationTreatsInlineBlockLinksAndButtonsAndSpansAsParagraphs() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <head>\
                    <style>\
                        a, span {\
                            display: inline-block;\
                            border: 1px blue solid;\
                            margin-left: 1em;\
                        }\
                    </style>\
                </head>\
                <body>\
                    <button>One</button><button>Two</button>\
                    <div><br></div>\
                    <a href='#'>Three</a><a href='#'>Four</a>\
                    <span role='button'>Five</span>\
                    <span>Six</span>\
                    <b>End</b>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 7)
        try #require(items[0].tokens.count == 1)
        try #require(items[1].tokens.count == 1)
        try #require(items[2].tokens.count == 1)
        try #require(items[3].tokens.count == 1)
        try #require(items[4].tokens.count == 1)
        try #require(items[5].tokens.count == 1)
        try #require(items[6].tokens.count == 1)
        #expect(items[0].tokens[0].content == "One")
        #expect(items[1].tokens[0].content == "Two")
        #expect(items[2].tokens[0].content == "Three")
        #expect(items[3].tokens[0].content == "Four")
        #expect(items[4].tokens[0].content == "Five")
        #expect(items[5].tokens[0].content == "Six")
        #expect(items[6].tokens[0].content == "End")
    }

    @Test
    func startTextManipulationTreatsLinksInNavigationElementsAsParagraphs() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <head>\
                    <style>\
                        li { display: inline; }\
                    </style>\
                </head>\
                <body>\
                    <div role='navigation'>\
                        <ul>\
                            <li><a href='#'>Foo</a></li>\
                            <li><a href='#'>Bar</a></li>\
                        </ul>\
                    </div>\
                    <nav>\
                        <ul>\
                            <li><a href='#'>Baz</a></li>\
                            <li><a href='#'>Garply</a></li>\
                        </ul>\
                    </nav>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 4)
        try #require(items[0].tokens.count == 1)
        try #require(items[1].tokens.count == 1)
        try #require(items[2].tokens.count == 1)
        try #require(items[3].tokens.count == 1)
        #expect(items[0].tokens[0].content == "Foo")
        #expect(items[1].tokens[0].content == "Bar")
        #expect(items[2].tokens[0].content == "Baz")
        #expect(items[3].tokens[0].content == "Garply")
    }

    @Test
    func startTextManipulationTreatsNestedInlineBlockListItemsAndLinksAsParagraphs() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <head>\
                    <style>\
                        li, a { display: inline-block; margin: 2em; }\
                    </style>\
                </head>\
                <body>\
                    <ol>\
                        <li>One<a href='#'>Two</a><a href='#'>Three</a>Four</li>\
                    </ol>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 4)
        try #require(items[0].tokens.count == 1)
        try #require(items[1].tokens.count == 1)
        try #require(items[2].tokens.count == 1)
        try #require(items[3].tokens.count == 1)
        #expect(items[0].tokens[0].content == "One")
        #expect(items[1].tokens[0].content == "Two")
        #expect(items[2].tokens[0].content == "Three")
        #expect(items[3].tokens[0].content == "Four")
    }

    @Test
    func startTextManipulationExtractsUserInfo() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                    <title>This is a test</title>\
                    <p>First</p>\
                    <div role='button'>Second</div>\
                    <span>Third</span>\
                    <div style='margin-top: 2000px;'>Fourth</div>\
                    <script>scrollTo(0, 2000);</script>\
                </body>
                """
        )

        await webView.nextPresentationUpdate()

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 5)
        try #require(items[0].tokens.count == 1)
        try #require(items[1].tokens.count == 1)
        try #require(items[2].tokens.count == 1)
        try #require(items[3].tokens.count == 1)
        try #require(items[4].tokens.count == 1)
        #expect(items[0].tokens[0].content == "This is a test")
        #expect(items[1].tokens[0].content == "First")
        #expect(items[2].tokens[0].content == "Second")
        #expect(items[3].tokens[0].content == "Third")
        #expect(items[4].tokens[0].content == "Fourth")
        do {
            let userInfo = try #require(items[0].tokens[0].userInfo)
            #if WTF_PLATFORM_MAC
            #expect((userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] as? URL)?.lastPathComponent == "Resources")
            #else
            #expect(
                (userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] as? URL)?.lastPathComponent == "TestWebKitAPIResources.bundle"
            )
            #endif
            #expect(userInfo[_WKTextManipulationTokenUserInfoTagNameKey] as? String == "TITLE")
            #expect(userInfo[_WKTextManipulationTokenUserInfoVisibilityKey] as? Bool == false)
        }
        do {
            let userInfo = try #require(items[1].tokens[0].userInfo)
            #if WTF_PLATFORM_MAC
            #expect((userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] as? URL)?.lastPathComponent == "Resources")
            #else
            #expect(
                (userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] as? URL)?.lastPathComponent == "TestWebKitAPIResources.bundle"
            )
            #endif
            #expect(userInfo[_WKTextManipulationTokenUserInfoTagNameKey] as? String == "P")
            #expect(userInfo[_WKTextManipulationTokenUserInfoVisibilityKey] as? Bool == false)
        }
        do {
            let userInfo = try #require(items[2].tokens[0].userInfo)
            #if WTF_PLATFORM_MAC
            #expect((userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] as? URL)?.lastPathComponent == "Resources")
            #else
            #expect(
                (userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] as? URL)?.lastPathComponent == "TestWebKitAPIResources.bundle"
            )
            #endif
            #expect(userInfo[_WKTextManipulationTokenUserInfoTagNameKey] as? String == "DIV")
            #expect(userInfo[_WKTextManipulationTokenUserInfoRoleAttributeKey] as? String == "button")
            #expect(userInfo[_WKTextManipulationTokenUserInfoVisibilityKey] as? Bool == false)
        }
        do {
            let userInfo = try #require(items[3].tokens[0].userInfo)
            #if WTF_PLATFORM_MAC
            #expect((userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] as? URL)?.lastPathComponent == "Resources")
            #else
            #expect(
                (userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] as? URL)?.lastPathComponent == "TestWebKitAPIResources.bundle"
            )
            #endif
            #expect(userInfo[_WKTextManipulationTokenUserInfoTagNameKey] as? String == "SPAN")
            #expect(userInfo[_WKTextManipulationTokenUserInfoVisibilityKey] as? Bool == false)
        }
        do {
            let userInfo = try #require(items[4].tokens[0].userInfo)
            #if WTF_PLATFORM_MAC
            #expect((userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] as? URL)?.lastPathComponent == "Resources")
            #else
            #expect(
                (userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] as? URL)?.lastPathComponent == "TestWebKitAPIResources.bundle"
            )
            #endif
            #expect(userInfo[_WKTextManipulationTokenUserInfoTagNameKey] as? String == "DIV")
            #expect(userInfo[_WKTextManipulationTokenUserInfoVisibilityKey] as? Bool == true)
        }
    }

    @Test
    func startTextManipulationExtractsValuesFromButtonInputs() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <input type='button' value='One'>\
                <input type='submit' value='Two'>\
                <input type='password' value='Three'>\
                <input type='range' value='4'>\
                <input type='date' value='2020-05-01'>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        try #require(items[1].tokens.count == 1)
        #expect(items[0].tokens[0].content == "One")
        #expect(items[1].tokens[0].content == "Two")
    }

    @Test
    func startTextManipulationExtractsValuesFromTextInputs() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <input type='search' value='One'>\
                <input type='text' value='Two'>\
                <input id='a' type='search'>\
                <input id='b' type='text'>\
                <input type='number' value='6'>\
                <input type='email' value='foo@bar.com'>\
                <input type='url' value='https://www.apple.com'>\
                <script>\
                document.getElementById('a').focus();\
                document.execCommand('InsertText', true, 'Three');\
                document.getElementById('b').focus();\
                document.execCommand('InsertText', true, 'Four');\
                </script>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        try #require(items[1].tokens.count == 1)
        #expect(items[0].tokens[0].content == "One")
        #expect(items[1].tokens[0].content == "Two")
    }

    @Test
    func startTextManipulationDoesNotExtractUserModifiedText() async throws {
        try await webView.load(html: "<!DOCTYPE html><body><input id='one'><input id='two'></body>")

        await webView._startTextManipulations(with: nil)

        #expect(delegate.items.isEmpty)

        try await webView.callJavaScript {
            """
            document.getElementById('one').focus();
            document.execCommand('InsertText', true, 'foo');
            document.getElementById('two').value = 'bar';
            """
        }

        let items = await delegate.waitForItems(count: 1)

        try #require(items.count == 1)

        let tokens = items[0].tokens
        #expect(tokens.count == 1)
        #expect(tokens.first?.content == "bar")
    }

    @Test
    func startTextManipulationExtractsVisibleLineBreaksInTextAsExcludedTokens() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <span style='white-space:pre;'>&#10; one&#10;  two</span>\
                <span style='white-space:pre;'>&#10;</span>\
                <span>&#10; three&#10;  four&#10;</span>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 4)
        #expect(items[0].tokens[0].content == "\n ")
        #expect(items[0].tokens[0].isExcluded)
        #expect(items[0].tokens[1].content == "one")
        #expect(!items[0].tokens[1].isExcluded)
        #expect(items[0].tokens[2].content == "\n  ")
        #expect(items[0].tokens[2].isExcluded)
        #expect(items[0].tokens[3].content == "two")
        #expect(!items[0].tokens[3].isExcluded)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "three four")
    }

    @Test
    func startTextManipulationExtractsPrivateUseCharactersAsExcludedTokens() async throws {
        try await webView.load(html: "<body>foo\u{E607}bar\u{E607}baz</body>")
        let configuration = _WKTextManipulationConfiguration()

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 1)

        let item = try #require(items.first)
        try #require(item.tokens.count == 5)
        #expect(item.tokens[0].content == "foo")
        #expect(!item.tokens[0].isExcluded)
        #expect(item.tokens[1].content == "\u{E607}")
        #expect(item.tokens[1].isExcluded)
        #expect(item.tokens[2].content == "bar")
        #expect(!item.tokens[2].isExcluded)
        #expect(item.tokens[3].content == "\u{E607}")
        #expect(item.tokens[3].isExcluded)
        #expect(item.tokens[4].content == "baz")
        #expect(!item.tokens[4].isExcluded)
    }

    @Test
    func startTextManipulationExtractsValuesByNode() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <span>one</span><span style='white-space:pre;'>two &#10; three</span><span>four</span>\
                <span style='white-space:pre;'>&#10;five</span>\
                <span>   six</span>        <span>seven</span>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 5)
        #expect(items[0].tokens[0].content == "one")
        #expect(items[0].tokens[1].content == "two")
        #expect(items[0].tokens[2].content == " \n ")
        #expect(items[0].tokens[3].content == "three")
        #expect(items[0].tokens[4].content == "four")
        try #require(items[1].tokens.count == 5)
        #expect(items[1].tokens[0].content == "\n")
        #expect(items[1].tokens[1].content == "five")
        #expect(items[1].tokens[2].content == " six")
        #expect(items[1].tokens[3].content == " ")
        #expect(items[1].tokens[4].content == "seven")
    }

    @Test
    func startTextManipulationExcludesTextRenderedAsIcons() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <head>\
                <style>\
                @font-face {\
                    font-family: Ahem;\
                    src: url(Ahem.ttf);\
                }\
                @font-face {\
                    font-family: SpaceOnly;\
                    src: url(SpaceOnly.otf);\
                }\
                .Ahem { font-family: Ahem; }\
                .SpaceOnly { font-family: SpaceOnly; }\
                .Missing { font-family: Missing; }\
                .SystemUI { font-family: system-ui; }\
                </style>\
                </head>\
                <body>\
                <span class='Ahem'>one</span><span class='SpaceOnly'>two</span><span class='Missing'>three</span><span class='SystemUI'>four</span>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        let item = try #require(items.first)
        try #require(item.tokens.count == 4)
        #expect(item.tokens[0].content == "one")
        #expect(!item.tokens[0].isExcluded)
        #expect(item.tokens[1].content == "two")
        #expect(item.tokens[1].isExcluded)
        #expect(item.tokens[2].content == "three")
        #expect(!item.tokens[2].isExcluded)
        #expect(item.tokens[3].content == "four")
        #expect(!item.tokens[3].isExcluded)
    }

    @Test
    func startTextManipulationAvoidCrashWhenExtractingOrphanedPositions() async throws {
        try await webView.load(html: "<p>hello world</p>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello world")

        try await webView.callJavaScript {
            """
            (() => {
                const objectElement = document.createElement('object');
                document.body.appendChild(objectElement);
                document.body.scrollTop;
                objectElement.remove();
                const text = document.createTextNode('testing');
                const container = document.createElement('div');
                container.appendChild(text);
                document.body.appendChild(container);
            })();
            """
        }

        await delegate.waitForItems(count: 2)
    }

    @Test
    func removedElements() async throws {
        try await webView.load(html: "<p>hello world</p>")

        await webView._startTextManipulations(with: nil)

        var items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello world")

        try await webView.callJavaScript {
            """
            (() => {
                for (let i = 0; i < 1024; ++i) {
                    const objectElement = document.createElement('object');
                    document.body.appendChild(objectElement);
                    objectElement.remove();
                }
                const text = document.createTextNode('testing');
                const container = document.createElement('div');
                container.appendChild(text);
                document.body.appendChild(container);
            })();
            """
        }

        items = await delegate.waitForItems(count: 2)

        try #require(items.count == 2)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "testing")
    }

    @Test
    func startTextManipulationExtractsHeadingElementsAsSeparateItems() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html>\
                  <body>\
                    <div style='float: left; width: 300px; height: 150px;'></div>\
                    <p style='float: left; width: 600px;'>Hello world</p>\
                    <h4 style='float: left; width: 600px;'>This is a heading</h4>\
                  </body>\
                </html>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "Hello world")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "This is a heading")
    }

    @Test
    func startTextManipulationIgnoresSpaces() async throws {
        try await webView.load(html: "<!DOCTYPE html>Hello<div style='background-color: lightblue;'>&nbsp;</div>World")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "Hello")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "World")
    }

    @Test
    func startTextManipulationExtractsTableCellsAsSeparateItems() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html>\
                <head>\
                  <style>\
                    td { border: solid 1px tomato; }\
                    a { border: solid 1px black; display: table-cell; }\
                  </style>\
                </head>\
                <body>\
                  <table>\
                    <tbody>\
                      <tr>\
                        <a><span>Hello</span></a>\
                        <a>World</a>\
                        <td>Foo</td>\
                        <td>Bar</td>\
                        <td>Baz</td>\
                      </tr>\
                    </tbody>\
                  </table>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 5)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "Hello")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "World")
        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "Foo")
        try #require(items[3].tokens.count == 1)
        #expect(items[3].tokens[0].content == "Bar")
        try #require(items[4].tokens.count == 1)
        #expect(items[4].tokens[0].content == "Baz")
    }

    @Test
    func startTextManipulationDoesNotFindContentInIframeIfIncludeSubframeIsNotSet() async throws {
        let configuration = _WKTextManipulationConfiguration()
        configuration.includeSubframes = true
        try await webView.load(html: "<!DOCTYPE html><div>hello</div><div>world</div><iframe src='data:text/html,<p>WebKit</p>'>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        #expect(!items[0].isSubframe)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")
        #expect(!items[1].isSubframe)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "world")
    }

    @Test
    func startTextManipulationFindsContentInIframeIfIncludeSubframeIsSet() async throws {
        let configuration = _WKTextManipulationConfiguration()
        configuration.includeSubframes = true

        try await webView.load(html: "<!DOCTYPE html><div>hello</div><div>world</div><iframe src='data:text/html,<p>WebKit</p>'>")

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 3)
        #expect(!items[0].isSubframe)
        #expect(!items[0].isCrossSiteSubframe)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")
        #expect(!items[1].isSubframe)
        #expect(!items[1].isCrossSiteSubframe)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "world")
        #expect(items[2].isSubframe)
        #expect(items[2].isCrossSiteSubframe)
        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "WebKit")
    }

    @Test
    func startTextManipulationFindsContentInIframeInsertedLater() async throws {
        let configuration = _WKTextManipulationConfiguration()
        configuration.includeSubframes = true

        try await webView.load(html: "<!DOCTYPE html><div>hello</div><div>world</div>")

        await webView._startTextManipulations(with: configuration)

        var items = delegate.items
        try #require(items.count == 2)
        #expect(!items[0].isSubframe)
        #expect(!items[0].isCrossSiteSubframe)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")
        #expect(!items[1].isSubframe)
        #expect(!items[1].isCrossSiteSubframe)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "world")

        try await webView.callJavaScript {
            """
            frame = document.createElement('iframe');
            frame.srcdoc = '<!DOCTYPE html><div>WebKit</div>'; document.body.appendChild(frame); true
            """
        }

        items = await delegate.waitForItems(count: 3)

        try #require(items.count == 3)
        #expect(!items[0].isSubframe)
        #expect(!items[0].isCrossSiteSubframe)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")
        #expect(!items[1].isSubframe)
        #expect(!items[1].isCrossSiteSubframe)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "world")
        #expect(items[2].isSubframe)
        #expect(!items[2].isCrossSiteSubframe)
        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "WebKit")
    }

    @Test
    func startTextManipulationDoesNotFindContentInNewMainFrame() async throws {
        let configuration = _WKTextManipulationConfiguration()
        configuration.includeSubframes = true

        try await webView.load(testPageNamed: "simple")
        try await webView.callJavaScript { "document.body.innerHTML = '<p>hey, <em>earth</em></p>'" }

        await webView._startTextManipulations(with: configuration)

        var items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "hey, ")
        #expect(items[0].tokens[1].content == "earth")

        try await webView.load(testPageNamed: "copy-html")

        try await Task.sleep(for: .milliseconds(50))

        #expect(delegate.items.count == 1)

        await webView._startTextManipulations(with: configuration)

        items = delegate.items
        try #require(items.count == 2)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "some text")
    }

    @Test
    func completeTextManipulationReplaceSimpleSingleParagraph() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>helllo, wooorld</p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "helllo, wooorld")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello, world")]
            )
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<p>hello, world</p>")
    }

    @Test
    func completeTextManipulationTranslatedRTLParagraphUsesStaleSelectionDirection() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html><body>\
                <p id='translated' dir='rtl'>الثعلب البني السريع يقفز فوق الكلب الكسول</p>\
                <p id='control' dir='ltr'>The quick brown fox jumps over the lazy dog</p>\
                </body></html>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)

        #expect(try await selectionDirectionProbe(elementID: "translated") == "backward")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(
                        identifier: items[0].tokens[0].identifier,
                        content: "The quick brown fox jumps over the lazy dog"
                    )
                ]
            )
        ])
        #expect(errors == nil)

        let translatedText = try await webView.callJavaScript(returning: String.self) {
            "return document.getElementById('translated').textContent"
        }
        #expect(translatedText == "The quick brown fox jumps over the lazy dog")
        #expect(try await selectionDirectionProbe(elementID: "control") == "forward")
        #expect(try await selectionDirectionProbe(elementID: "translated") == "forward")
    }

    @Test
    func legacyCompleteTextManipulationReplaceSimpleSingleParagraph() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>helllo, wooorld</p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "helllo, wooorld")

        let success = await webView._completeTextManipulation(
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello, world")]
            )
        )
        #expect(success)
        #expect(try await bodyInnerHTML() == "<p>hello, world</p>")
    }

    @Test
    func completeTextManipulationDisgardsTokens() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>hello, <b>world</b>. <i>WebKit</i></p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 4)
        #expect(items[0].tokens[0].content == "hello, ")
        #expect(items[0].tokens[1].content == "world")
        #expect(items[0].tokens[2].content == ". ")
        #expect(items[0].tokens[3].content == "WebKit")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello, "),
                    _WKTextManipulationToken(identifier: items[0].tokens[3].identifier, content: "WebKit"),
                ]
            )
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<p>hello, <i>WebKit</i></p>")
    }

    @Test
    func completeTextManipulationReplaceTwoSimpleParagraphs() async throws {
        try await webView.load(html: "<p>hello</p>world")
        let configuration = _WKTextManipulationConfiguration()

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "world")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "World")]
            ),
        ])
        #expect(errors == nil)

        #expect(try await bodyInnerHTML() == "<p>Hello</p>World")
    }

    @Test
    func completeTextManipulationReplaceMultipleSimpleParagraphs() async throws {
        try await webView.load(
            html: "<!DOCTYPE html><html><body><p>helllo, wooorld</p><p> hey, <b> Kits</b> is <em>cuuute</em></p></body></html>"
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "helllo, wooorld")

        try #require(items[1].tokens.count == 4)
        #expect(items[1].tokens[0].content == "hey, ")
        #expect(items[1].tokens[1].content == "Kits")
        #expect(items[1].tokens[2].content == " is ")
        #expect(items[1].tokens[3].content == "cuuute")

        var errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "Hello, "),
                    _WKTextManipulationToken(identifier: items[1].tokens[1].identifier, content: "kittens"),
                    _WKTextManipulationToken(identifier: items[1].tokens[2].identifier, content: " are "),
                    _WKTextManipulationToken(identifier: items[1].tokens[3].identifier, content: "cute"),
                ]
            )
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<p>helllo, wooorld</p><p>Hello, <b>kittens</b> are <em>cute</em></p>")

        errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello, world.")]
            )
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<p>Hello, world.</p><p>Hello, <b>kittens</b> are <em>cute</em></p>")
    }

    @Test
    func completeTextManipulationReplaceMultipleSimpleParagraphsAtOnce() async throws {
        try await webView.load(
            html: "<!DOCTYPE html><html><body><p>helllo, wooorld</p><p> hey, <b> Kits</b> is <em>cuuute</em></p></body></html>"
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "helllo, wooorld")

        try #require(items[1].tokens.count == 4)
        #expect(items[1].tokens[0].content == "hey, ")
        #expect(items[1].tokens[1].content == "Kits")
        #expect(items[1].tokens[2].content == " is ")
        #expect(items[1].tokens[3].content == "cuuute")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "Hello, "),
                    _WKTextManipulationToken(identifier: items[1].tokens[1].identifier, content: "kittens"),
                    _WKTextManipulationToken(identifier: items[1].tokens[2].identifier, content: " are "),
                    _WKTextManipulationToken(identifier: items[1].tokens[3].identifier, content: "cute"),
                ]
            ),
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello, world.")]
            ),
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<p>Hello, world.</p><p>Hello, <b>kittens</b> are <em>cute</em></p>")
    }

    @Test
    func completeTextManipulationReplaceMultipleSimpleParagraphsSeparatedByBR() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>helllo, wooorld<br>webKit</p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "helllo, wooorld")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "webKit")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello, World")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "WebKit")]
            ),
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<p>Hello, World<br>WebKit</p>")
    }

    @Test
    func completeTextManipulationReplaceParagraphsSeparatedByWrappedBR() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>earth, <b>hey<br></b>webKit</p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "earth, ")
        #expect(items[0].tokens[1].content == "hey")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "webKit")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: "Hello, "),
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "World"),
                ]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "WebKit")]
            ),
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<p><b>Hello, </b>World<b><br></b>WebKit</p>")
    }

    @Test
    func completeTextManipulationPreservesWhitespacesBetweenItems() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html><html><head><style> a { white-space: nowrap; } div { width: 10px; } </style></head>\
                <body><div><a>helllo</a> <a>worrld</a></div></body></html>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "helllo")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "worrld")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "world")]
            ),
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<div><a>hello</a> <a>world</a></div>")
    }

    @Test
    func completeTextManipulationFailWhenBRIsInserted() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>helllo, <b>worrld</b></p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "helllo, ")
        #expect(items[0].tokens[1].content == "worrld")

        try await webView.callJavaScript { "document.querySelector('b').before(document.createElement('br'))" }

        let item = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [
                _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello, "),
                _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: "World"),
            ]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [item]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.contentChanged.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === item)
        #expect(try await bodyInnerHTML() == "<p>helllo, <br><b>worrld</b></p>")
    }

    @Test
    func completeTextManipulationAvoidCrashingWhenContentIsRemoved() async throws {
        try await webView.load(testPageNamed: "simple")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        let tokens = items[0].tokens
        try #require(tokens.count == 1)

        await withCheckedContinuation { continuation in
            webView.perform(afterReceivingMessage: "DoneRemovingParagraph") {
                continuation.resume()
            }

            webView.evaluateJavaScript(
                """
                const paragraph = document.createElement('p');
                paragraph.textContent = 'Hello world';
                document.body.appendChild(paragraph);
                setTimeout(() => { paragraph.remove(); webkit.messageHandlers.testHandler.postMessage('DoneRemovingParagraph') })
                """
            )
        }

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: tokens[0].identifier, content: "Simple HTML file!")]
            )
        ])
        #expect(errors == nil)

        let bodyText = try await webView.callJavaScript(returning: String.self) { "return document.body.textContent" }
        #expect(bodyText == "Simple HTML file!")
    }

    @Test
    func completeTextManipulationShouldPreserveImagesAsExcludedTokens() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><div>hello, <img src=\"apple.gif\"> world</div></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        let tokens = items[0].tokens
        try #require(tokens.count == 3)
        #expect(tokens[0].content == "hello, ")
        #expect(!tokens[0].isExcluded)
        #expect(tokens[1].content == "[]")
        #expect(tokens[1].isExcluded)
        #expect(tokens[2].content == " world")
        #expect(!tokens[2].isExcluded)

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: tokens[0].identifier, content: "this is "),
                    _WKTextManipulationToken(identifier: tokens[1].identifier, content: nil),
                    _WKTextManipulationToken(identifier: tokens[2].identifier, content: " a test"),
                ]
            )
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<div>this is <img src=\"apple.gif\"> a test</div>")
    }

    @Test
    func completeTextManipulationShouldPreserveSVGAsExcludedTokens() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html><html><body>\
                <section><div style="display: inline-block;">hello\
                <span style="display: inline-flex;"><svg viewBox="0 0 20 20" width="20" height="20"><rect width="20" height="20" fill="#06f"></rect></svg></span>\
                webkit</div></section>\
                <p>world</p></body></html>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        let tokens = items[0].tokens
        try #require(tokens.count == 3)
        #expect(tokens[0].content == "hello")
        #expect(!tokens[0].isExcluded)
        #expect(tokens[1].content == "[]")
        #expect(tokens[1].isExcluded)
        #expect(tokens[2].content == "webkit")
        #expect(!tokens[2].isExcluded)

        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "world")
        #expect(!items[1].tokens[0].isExcluded)

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: tokens[0].identifier, content: "hey"),
                    _WKTextManipulationToken(identifier: tokens[1].identifier, content: nil),
                    _WKTextManipulationToken(identifier: tokens[2].identifier, content: "WebKit"),
                ]
            )
        ])
        #expect(errors == nil)
        let expectedHTML = """
            <section><div style="display: inline-block;">hey<span style="display: inline-flex;">\
            <svg viewBox="0 0 20 20" width="20" height="20"><rect width="20" height="20" fill="#06f"></rect></svg>\
            </span>WebKit</div></section><p>world</p>
            """
        #expect(try await bodyInnerHTML() == expectedHTML)
    }

    @Test
    func completeTextManipulationShouldPreserveOrderOfBlockImage() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html><html><body><svg viewBox="0 0 10 10" width="100" height="100">\
                <rect width="10" height="10" fill="red"></rect></svg><img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAE\
                AAAABCAIAAACQd1PeAAAAAXNSR0IArs4c6QAAAAxJREFUCNdjYKhnAAABAgCAbV7tZwAAAABJRU5ErkJggg=="\
                style="display: block; width: 100px;"><section>helllo world</section></body></html>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "helllo world")
        #expect(!items[0].tokens[0].isExcluded)

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello, world")]
            )
        ])
        #expect(errors == nil)
        let expectedHTML = """
            <svg viewBox="0 0 10 10" width="100" height="100"><rect width="10" height="10" fill="red"></rect></svg>\
            <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAAAXNSR0IArs4c6QAAAAxJREFUCN\
            djYKhnAAABAgCAbV7tZwAAAABJRU5ErkJggg==" style="display: block; width: 100px;"><section>hello, world</section>
            """
        #expect(try await bodyInnerHTML() == expectedHTML)
    }

    @Test
    func completeTextManipulationShouldReplaceAttributeContent() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html><html><head><title>hey</title></head>\
                <body><div><span aria-label="this is greet">hello</span><img src="apple.gif" alt="fruit"></div></body></html>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 4)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hey")

        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "this is greet")

        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "fruit")

        try #require(items[3].tokens.count == 1)
        #expect(items[3].tokens[0].content == "hello")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "This is a greeting")]
            ),
            _WKTextManipulationItem(
                identifier: items[2].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[2].tokens[0].identifier, content: "Apple")]
            ),
        ])
        #expect(errors == nil)
        let documentHTML = try await webView.callJavaScript(returning: String.self) { "return document.documentElement.innerHTML" }
        let expectedHTML = """
            <head><title>Hello</title></head><body><div><span aria-label="This is a greeting">hello</span>\
            <img src="apple.gif" alt="Apple"></div></body>
            """
        #expect(documentHTML == expectedHTML)
    }

    @Test
    func completeTextManipulationShouldReplaceContentFollowedAfterImageInCSSTable() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html><html><body>\
                <div style="display: table"><div style="float: left;"><img src="apple.gif" style="display: flex;"></div>\
                <div><span style="display: block">hello world</span></div></body></html>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello world")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello, world")]
            )
        ])
        #expect(errors == nil)
        let expectedHTML = """
            <div style="display: table"><div style="float: left;"><img src="apple.gif" style="display: flex;"></div>\
            <div><span style="display: block">hello, world</span></div></div>
            """
        #expect(try await bodyInnerHTML() == expectedHTML)
    }

    @Test
    func completeTextManipulationShouldReplaceTextContentWithMultipleTokens() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html><html>\
                <title>This is a test</title>\
                <select>\
                    <option selected>Hello world</option>\
                    <option>Should not be replaced</option>\
                </select>\
                <span aria-label='label'>Text</span>\
                <img src='apple.gif' alt='image'>\
                </html>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 6)
        try #require(items[0].tokens.count == 1)
        try #require(items[1].tokens.count == 1)
        try #require(items[2].tokens.count == 1)
        try #require(items[3].tokens.count == 1)
        try #require(items[4].tokens.count == 1)
        try #require(items[5].tokens.count == 1)
        #expect(items[0].tokens[0].content == "This is a test")
        #expect(items[1].tokens[0].content == "Hello world")
        #expect(items[2].tokens[0].content == "Should not be replaced")
        #expect(items[3].tokens[0].content == "label")
        #expect(items[4].tokens[0].content == "image")
        #expect(items[5].tokens[0].content == "Text")

        let replacementItems = [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Replacement"),
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "title"),
                ]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "Replacement"),
                    _WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "option"),
                ]
            ),
            _WKTextManipulationItem(
                identifier: items[2].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[2].tokens[0].identifier, content: "Failed replacement"),
                    _WKTextManipulationToken(identifier: "12345", content: "option"),
                ]
            ),
            _WKTextManipulationItem(
                identifier: items[3].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[3].tokens[0].identifier, content: "Replacement"),
                    _WKTextManipulationToken(identifier: items[3].tokens[0].identifier, content: "label"),
                ]
            ),
            _WKTextManipulationItem(
                identifier: items[4].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[4].tokens[0].identifier, content: "Replacement"),
                    _WKTextManipulationToken(identifier: items[4].tokens[0].identifier, content: "image"),
                ]
            ),
        ]

        let errors = try #require(await webView._completeTextManipulation(for: replacementItems))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.invalidToken.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === replacementItems[2])

        let options = try await webView.callJavaScript(returning: [String].self) {
            "return Array.from(document.querySelector('select').options).map(option => option.textContent)"
        }
        #expect(options.first == "Replacement option")
        #expect(options.last == "Should not be replaced")
        let title = try await webView.callJavaScript(returning: String.self) { "return document.title" }
        #expect(title == "Replacement title")
        let ariaLabel = try await webView.callJavaScript(returning: String.self) {
            "return document.querySelector('span').getAttribute('aria-label')"
        }
        #expect(ariaLabel == "Replacement label")
        let altText = try await webView.callJavaScript(returning: String.self) {
            "return document.querySelector('img').getAttribute('alt')"
        }
        #expect(altText == "Replacement image")
    }

    @Test
    func completeTextManipulationShouldReplaceContentsAroundParagraphWithJustImage() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><div>heeey</div><div><img src=\"apple.gif\"></div><span>woorld</span>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "heeey")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "woorld")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "world")]
            ),
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<div>hello</div><div><img src=\"apple.gif\"></div><span>world</span>")
    }

    @Test
    func completeTextManipulationShouldBatchItemCallback() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body></body></html>")

        try await webView.callJavaScript {
            "html = ''; for (let i = 0; i < 2000; ++i) html += `<p>hello ${i}</p>`; document.body.innerHTML = html;"
        }

        await webView._startTextManipulations(with: nil)
        #expect(delegate.itemBatchCount >= 2)

        let items = delegate.items
        try #require(items.count == 2000)
        for i in 0..<2000 {
            try #require(items[i].tokens.count == 1)
            #expect(items[i].tokens[0].content == "hello \(i)")
        }
    }

    @Test
    func completeTextManipulationReordersContent() async throws {
        try await webView.load(
            html: "<!DOCTYPE html><html><body><p><a href=\"https://en.wikipedia.org/wiki/Cat\">cats</a>, <i>I</i> are</p></body></html>"
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 4)
        #expect(items[0].tokens[0].content == "cats")
        #expect(items[0].tokens[1].content == ", ")
        #expect(items[0].tokens[2].content == "I")
        #expect(items[0].tokens[3].content == " are")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[2].identifier, content: "I"),
                    _WKTextManipulationToken(identifier: items[0].tokens[3].identifier, content: "'m a "),
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "cat"),
                ]
            )
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<p><i>I</i>'m a <a href=\"https://en.wikipedia.org/wiki/Cat\">cat</a></p>")
    }

    @Test
    func completeTextManipulationCanSplitContent() async throws {
        try await webView.load(
            html: "<!DOCTYPE html><html><body><p id=\"paragraph\"><b class=\"hello-world\">hello world</b> WebKit</p></body></html>"
        )
        try await webView.callJavaScript { "paragraph.firstChild.addEventListener('click', () => window.didClick = true)" }

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "hello world")
        #expect(items[0].tokens[1].content == " WebKit")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello"),
                    _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: " WebKit "),
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "world"),
                ]
            )
        ])
        #expect(errors == nil)
        #expect(
            try await bodyInnerHTML()
                == "<p id=\"paragraph\"><b class=\"hello-world\">hello</b> WebKit <b class=\"hello-world\">world</b></p>"
        )
        let didClickFirstChild = try await webView.callJavaScript(returning: Bool.self) {
            "didClick = false; paragraph.firstChild.click(); return didClick"
        }
        #expect(didClickFirstChild)
        let didClickLastChild = try await webView.callJavaScript(returning: Bool.self) {
            "didClick = false; paragraph.lastChild.click(); return didClick"
        }
        #expect(didClickLastChild)
    }

    @Test
    func completeTextManipulationCanMergeContent() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p><b>hello <i>world</i> WebKit</b></p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 3)
        #expect(items[0].tokens[0].content == "hello ")
        #expect(items[0].tokens[1].content == "world")
        #expect(items[0].tokens[2].content == " WebKit")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello "),
                    _WKTextManipulationToken(identifier: items[0].tokens[2].identifier, content: "world"),
                ]
            )
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<p><b>hello world</b></p>")
    }

    @Test
    func completeTextManipulationFailWhenItemIdentifierIsDuplicated() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>hello, <b>world</b></p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "hello, ")
        #expect(items[0].tokens[1].content == "world")

        let firstItem = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "done")]
        )
        let secondItem = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "bad")]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [firstItem, secondItem]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.invalidItem.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === secondItem)
        #expect(try await bodyInnerHTML() == "<p>done</p>")
    }

    @Test
    func completeTextManipulationCanHandleSubsetOfItemsToFail() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>hey, <b>dude</b></p><p>this is <b>bad</b></p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "hey, ")
        #expect(items[0].tokens[1].content == "dude")
        try #require(items[1].tokens.count == 2)
        #expect(items[1].tokens[0].content == "this is ")
        #expect(items[1].tokens[1].content == "bad")

        let firstItem = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "bad")]
        )
        let secondItem = _WKTextManipulationItem(
            identifier: items[1].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "good")]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [firstItem, secondItem]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.invalidToken.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === firstItem)
        #expect(try await bodyInnerHTML() == "<p>hey, <b>dude</b></p><p>good</p>")
    }

    @Test
    func completeTextManipulationReplaceContentInIframe() async throws {
        let configuration = _WKTextManipulationConfiguration()
        configuration.includeSubframes = true

        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html><body><div id='container'><p>helllo</p></div>\
                <script>frame = document.createElement('iframe'); document.body.appendChild(frame);\
                frame.contentDocument.body.innerHTML = '<p>worrld</p>';</script>
                """
        )

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 2)
        #expect(!items[0].isSubframe)
        #expect(!items[0].isCrossSiteSubframe)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "helllo")
        #expect(items[1].isSubframe)
        #expect(!items[1].isCrossSiteSubframe)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "worrld")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "world")]
            ),
        ])
        #expect(errors == nil)
        let containerHTML = try await webView.callJavaScript(returning: String.self) { "return container.innerHTML" }
        #expect(containerHTML == "<p>hello</p>")
        let frameBodyHTML = try await webView.callJavaScript(returning: String.self) { "return frame.contentDocument.body.innerHTML" }
        #expect(frameBodyHTML == "<p>world</p>")
    }

    @Test
    func completeTextManipulationFailWhenContentIsChanged() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p> what <em>time</em> are they now?</p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 3)
        #expect(items[0].tokens[0].content == "what ")
        #expect(items[0].tokens[1].content == "time")
        #expect(items[0].tokens[2].content == " are they now?")

        try await webView.callJavaScript { "document.querySelector('em').nextSibling.data = ' is it now in London?'" }

        let item = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [
                _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "What "),
                _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: "time"),
                _WKTextManipulationToken(identifier: items[0].tokens[2].identifier, content: " is it now?"),
            ]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [item]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.contentChanged.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === item)
        #expect(try await bodyInnerHTML() == "<p> what <em>time</em> is it now in London?</p>")
    }

    @Test
    func completeTextManipulationFailWhenContentIsRemoved() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>hello, world</p></body></html>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello, world")

        try await webView.callJavaScript { "document.body.innerHTML = 'new content'" }

        let item = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hey")]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [item]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.contentChanged.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === item)
        #expect(try await bodyInnerHTML() == "new content")
    }

    @Test
    func completeTextManipulationFailWhenContentIsAdded() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>hello, world</p></body></html>")

        await webView._startTextManipulations(with: nil)

        var items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello, world")

        try await webView.callJavaScript {
            """
            document.querySelector('p').innerHTML = 'hello, world &#10; bye';
            document.body.appendChild(document.createElement('div')).innerHTML = 'end'
            """
        }
        items = await delegate.waitForItems(count: 3)

        let item = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello, World")]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [item]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.contentChanged.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === item)

        #expect(try await bodyInnerHTML() == "<p>hello, world \n bye</p><div>end</div>")
    }

    @Test
    func completeTextManipulationFailWhenContentIsChangedInIframe() async throws {
        let configuration = _WKTextManipulationConfiguration()
        configuration.includeSubframes = true

        try await webView.load(
            html: """
                <!DOCTYPE html><html><body><p>helllo</p>\
                <script>frame = document.createElement('iframe'); document.body.appendChild(frame);\
                frame.contentDocument.body.innerHTML = '<p>worrld</p>';</script></body></html>
                """
        )

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 2)
        #expect(!items[0].isSubframe)
        #expect(!items[0].isCrossSiteSubframe)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "helllo")
        #expect(items[1].isSubframe)
        #expect(!items[1].isCrossSiteSubframe)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "worrld")

        try await webView.callJavaScript { "frame.contentDocument.body.innerHTML = 'new content'" }

        let item0 = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hey")]
        )
        let item1 = _WKTextManipulationItem(
            identifier: items[1].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "dude")]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [item0, item1]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.contentChanged.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === item1)
        let frameBodyHTML = try await webView.callJavaScript(returning: String.self) { "return frame.contentDocument.body.innerHTML" }
        #expect(frameBodyHTML == "new content")
    }

    @Test
    func completeTextManipulationSuccedsWhenContentOutOfParagraphIsAdded() async throws {
        try await webView.load(html: "<p style='white-space:pre;background-color:blue;'><span>hello world</span><u>   </u></p>")

        let configuration = _WKTextManipulationConfiguration()
        await webView._startTextManipulations(with: configuration)

        var items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello world")

        try await webView.callJavaScript {
            """
            var element = document.createElement('span');
            element.innerHTML='inserted';
            document.querySelector('u').before(element)
            """
        }
        items = await delegate.waitForItems(count: 2)

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello World")]
            )
        ])
        #expect(errors == nil)

        #expect(
            try await bodyInnerHTML()
                == "<p style=\"white-space:pre;background-color:blue;\"><span>Hello World</span><span>inserted</span><u>   </u></p>"
        )
    }

    @Test
    func completeTextManipulationFailWhenDocumentHasBeenNavigatedAway() async throws {
        try await webView.load(testPageNamed: "simple")
        try await webView.callJavaScript { "document.body.innerHTML = '<p>hey, <em>earth</em></p>'" }

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "hey, ")
        #expect(items[0].tokens[1].content == "earth")

        try await webView.load(testPageNamed: "copy-html")
        try await webView.callJavaScript { "document.body.innerHTML = '<p>hey, <em>earth</em></p>'" }

        await webView._startTextManipulations(with: nil)

        let item = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [
                _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello, "),
                _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: "world"),
            ]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [item]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.invalidItem.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === item)
    }

    @Test
    func completeTextManipulationFailWhenExclusionIsViolated() async throws {
        try await webView.load(testPageNamed: "simple")
        try await webView.callJavaScript { "document.body.innerHTML = '<p>hi, <em>WebKitten</em> bye</p>'" }

        let configuration = _WKTextManipulationConfiguration()
        configuration.exclusionRules = [
            _WKTextManipulationExclusionRule(exclusion: true, forElement: "em")
        ]

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 3)
        #expect(items[0].tokens[0].content == "hi, ")
        #expect(items[0].tokens[1].content == "WebKitten")
        #expect(items[0].tokens[2].content == " bye")

        let item = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [
                _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello,"),
                _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: "WebKit"),
                _WKTextManipulationToken(identifier: items[0].tokens[2].identifier, content: "Bye"),
            ]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [item]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.exclusionViolation.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === item)

        #expect(try await bodyInnerHTML() == "<p>hi, <em>WebKitten</em> bye</p>")
    }

    @Test
    func completeTextManipulationFailWhenExcludedContentAppearsMoreThanOnce() async throws {
        try await webView.load(testPageNamed: "simple")
        try await webView.callJavaScript { "document.body.innerHTML = '<p>hi, <em>WebKitten</em> bye</p>'" }

        let configuration = _WKTextManipulationConfiguration()
        configuration.exclusionRules = [
            _WKTextManipulationExclusionRule(exclusion: true, forElement: "em")
        ]

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 3)
        #expect(items[0].tokens[0].content == "hi, ")
        #expect(items[0].tokens[1].content == "WebKitten")
        #expect(items[0].tokens[2].content == " bye")

        let item = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [
                _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: nil),
                _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello,"),
                _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: nil),
                _WKTextManipulationToken(identifier: items[0].tokens[2].identifier, content: "Bye"),
            ]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [item]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.exclusionViolation.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === item)

        #expect(try await bodyInnerHTML() == "<p>hi, <em>WebKitten</em> bye</p>")
    }

    @Test
    func completeTextManipulationPreservesExcludedContent() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>hi, <em>WebKitten</em> bye</p></body></html>")

        let configuration = _WKTextManipulationConfiguration()
        configuration.exclusionRules = [
            _WKTextManipulationExclusionRule(exclusion: true, forElement: "em")
        ]

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 3)
        #expect(items[0].tokens[0].content == "hi, ")
        #expect(items[0].tokens[1].content == "WebKitten")
        #expect(items[0].tokens[2].content == " bye")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello, "),
                    _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: nil),
                    _WKTextManipulationToken(identifier: items[0].tokens[2].identifier, content: " Bye"),
                ]
            )
        ])
        #expect(errors == nil)

        #expect(try await bodyInnerHTML() == "<p>Hello, <em>WebKitten</em> Bye</p>")
    }

    @Test
    func completeTextManipulationDoesNotCreateMoreTextManipulationItems() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p>Foo <strong>bar</strong> baz</p></body></html>")

        let configuration = _WKTextManipulationConfiguration()

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 1)

        let firstItem = try #require(items.first)
        try #require(firstItem.tokens.count == 3)

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: firstItem.identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: firstItem.tokens[0].identifier, content: "bar "),
                    _WKTextManipulationToken(identifier: firstItem.tokens[1].identifier, content: "garply"),
                    _WKTextManipulationToken(identifier: firstItem.tokens[2].identifier, content: " foo"),
                ]
            )
        ])
        #expect(errors == nil)

        await webView.nextPresentationUpdate()

        #expect(delegate.items.count == items.count)
        #expect(try await bodyInnerHTML() == "<p>bar <strong>garply</strong> foo</p>")
    }

    @Test
    func completeTextManipulationCorrectParagraphRange() async throws {
        try await webView.load(
            html: """
                <head><style>ul{display:block}li{display:inline-block}.inline {float: left;}.subframe {height: 42px;}\
                .frame {position: absolute;top: -9999px;}</style></head><body><div class='frame'><div class='subframe'></div></div>\
                <style></style><div class='inline'><div><li><a href='#'>holle</a></li><li><a href='#'>wdrlo</a></li></div></div>\
                <div class='frame'><div class='subframe'></div></div></body>
                """
        )

        let configuration = _WKTextManipulationConfiguration()
        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        try #require(items[1].tokens.count == 1)
        #expect(items[0].tokens[0].content == "holle")
        #expect(items[1].tokens[0].content == "wdrlo")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "hello")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "world")]
            ),
        ])
        #expect(errors == nil)

        let expectedHTML = """
            <div class="frame"><div class="subframe"></div></div><style></style><div class="inline"><div>\
            <li><a href="#">hello</a></li><li><a href="#">world</a></li></div></div>\
            <div class="frame"><div class="subframe"></div></div>
            """
        #expect(try await bodyInnerHTML() == expectedHTML)
    }

    @Test
    func completeTextManipulationCanMergeContentAndPreserveLineBreaks() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <span style='white-space:pre;'>one &#10; two &#10;</span>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 4)
        #expect(items[0].tokens[0].content == "one")
        #expect(items[0].tokens[1].content == " \n ")
        #expect(items[0].tokens[2].content == "two")
        #expect(items[0].tokens[3].content == " \n")

        let replacementItems = [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "ONE"),
                    _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: items[0].tokens[1].content),
                    _WKTextManipulationToken(identifier: items[0].tokens[2].identifier, content: "TWO"),
                    _WKTextManipulationToken(identifier: items[0].tokens[3].identifier, content: items[0].tokens[3].content),
                ]
            )
        ]

        let errors = await webView._completeTextManipulation(for: replacementItems)
        #expect(errors == nil)

        #expect(try await bodyInnerHTML() == "<span style=\"white-space:pre;\">ONE \n TWO \n</span>")
    }

    @Test
    func completeTextManipulationIgnoreWhiteSpacesBetweenParagraphs() async throws {
        try await webView.load(
            html: """
                <style>\
                .inline-block { display: inline-block; }\
                .list-item { display: list-item; }\
                .hide-absolute { display: none; position: absolute; }\
                .float-relative { float: left; position: relative; }\
                </style>\
                <ul class='float-relative'>\
                   <li class='list-item float-relative'><a class='inline-block'>hello</a>           \
                <div class='hide-absolute'><a class='inline-block float-relative'>hide</a></div></li>\
                   <li class='list-item float-relative'><a class='inline-block'>world</a></li>\
                </ul>
                """
        )
        let configuration = _WKTextManipulationConfiguration()

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "world")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "World")]
            ),
        ])
        #expect(errors == nil)
    }

    @Test
    func completeTextManipulationDoesNotSkipTabCharacterAtLineWrap() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html lang=en-US>\
                <div style='width: 32rem;'>\
                <p>This is another set of text to be translated. (1) \
                <strong>If this text is to be translated, then it should be something noteworthy</strong>\
                 (2) A monthly subscription is just&#9;$10.</p>\
                </div>
                """
        )
        let configuration = _WKTextManipulationConfiguration()

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 3)
        #expect(items[0].tokens[0].content == "This is another set of text to be translated. (1) ")
        #expect(items[0].tokens[1].content == "If this text is to be translated, then it should be something noteworthy")
        #expect(items[0].tokens[2].content == " (2) A monthly subscription is just $10.")

        try await webView.callJavaScript { "document.documentElement.setAttribute('lang', 'zh-CN')" }

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello")]
            )
        ])
        #expect(errors == nil)
    }

    @Test
    func completeTextManipulationShouldPreserveNodesAfterParagraphRange() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <p>hello<b>world<br><br><i>from</i></b></p>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "hello")
        #expect(items[0].tokens[1].content == "world")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "from")

        let replacementItems = [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello"),
                    _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: "World"),
                ]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "From WebKit")]
            ),
        ]

        let errors = await webView._completeTextManipulation(for: replacementItems)
        #expect(errors == nil)

        #expect(try await bodyInnerHTML() == "<p>Hello<b>World<br><br><i>From WebKit</i></b></p>")
    }

    @Test
    func completeTextManipulationSPreserveNodesBeforeParagraphRange() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <p><b><br><br>hello<i>world</i></b>from</p>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 3)
        #expect(items[0].tokens[0].content == "hello")
        #expect(items[0].tokens[1].content == "world")
        #expect(items[0].tokens[2].content == "from")

        let replacementItems = [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello"),
                    _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: "World"),
                    _WKTextManipulationToken(identifier: items[0].tokens[2].identifier, content: "From WebKit"),
                ]
            )
        ]

        let errors = await webView._completeTextManipulation(for: replacementItems)
        #expect(errors == nil)

        #expect(try await bodyInnerHTML() == "<p><b><br><br>Hello<i>World</i></b>From WebKit</p>")
    }

    @Test
    func insertingContentIntoAlreadyManipulatedContentCreatesTextManipulationItem() async throws {
        try await webView.load(html: "<!DOCTYPE html><html><body><p><i><b>hey</b></i> dude</p></body></html>")

        let configuration = _WKTextManipulationConfiguration()

        await webView._startTextManipulations(with: configuration)

        var items = delegate.items
        try #require(items.count == 1)

        let firstItem = try #require(items.first)
        try #require(firstItem.tokens.count == 2)
        #expect(firstItem.tokens[0].content == "hey")
        #expect(firstItem.tokens[1].content == " dude")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: firstItem.identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: firstItem.tokens[0].identifier, content: "hello,"),
                    _WKTextManipulationToken(identifier: firstItem.tokens[1].identifier, content: " world"),
                ]
            )
        ])
        #expect(errors == nil)

        try await webView.callJavaScript {
            "span = document.createElement('span'); span.textContent = ' WebKit!'; document.querySelector('b').after(span);"
        }
        items = await delegate.waitForItems(count: 2)

        #expect(items.count == 2)
        #expect(try await bodyInnerHTML() == "<p><i><b>hello,</b><span> WebKit!</span></i> world</p>")
    }

    @Test
    func completeTextManipulationInButtonsAndTextFields() async throws {
        try await webView.load(html: "<input type='text' value='hello1'><input type='submit' value='hello2'>")
        let configuration = _WKTextManipulationConfiguration()

        await webView._startTextManipulations(with: configuration)

        let items = delegate.items
        try #require(items.count == 2)

        let firstItem = try #require(items.first)
        let lastItem = try #require(items.last)
        try #require(firstItem.tokens.count == 1)
        try #require(lastItem.tokens.count == 1)
        #expect(firstItem.tokens[0].content == "hello1")
        #expect(lastItem.tokens[0].content == "hello2")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: firstItem.identifier,
                tokens: [_WKTextManipulationToken(identifier: firstItem.tokens[0].identifier, content: "world1")]
            ),
            _WKTextManipulationItem(
                identifier: lastItem.identifier,
                tokens: [_WKTextManipulationToken(identifier: lastItem.tokens[0].identifier, content: "world2")]
            ),
        ])
        #expect(errors == nil)

        let textFieldValue = try await webView.callJavaScript(returning: String.self) {
            "return document.querySelector('input[type=text]').value"
        }
        #expect(textFieldValue == "world1")
        let submitButtonValue = try await webView.callJavaScript(returning: String.self) {
            "return document.querySelector('input[type=submit]').value"
        }
        #expect(submitButtonValue == "world2")
    }

    @Test
    func completeTextManipulationForNewlyDisplayedParagraph() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html><html><body>\
                <style> .hidden { display: none; } </style>\
                <div>\
                <b>webkit!</b>\
                <span>\
                hello world\
                <i class='hidden'>bye</i>\
                </span>\
                </div>\
                </body></html>
                """
        )

        await webView._startTextManipulations(with: nil)

        var items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "webkit!")
        #expect(items[0].tokens[1].content == "hello world")

        var errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "WebKit!"),
                    _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: "Hello World"),
                ]
            )
        ])
        #expect(errors == nil)

        try await webView.callJavaScript { "document.querySelector('i').removeAttribute('class');" }
        items = await delegate.waitForItems(count: 2)

        try #require(items.count == 2)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "bye")

        errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "Bye")]
            )
        ])
        #expect(errors == nil)

        let divHTML = try await webView.callJavaScript(returning: String.self) { "return document.querySelector('div').innerHTML" }
        #expect(divHTML == "<b>WebKit!</b><span>Hello World<i>Bye</i></span>")
    }

    @Test
    func completeTextManipulationForNewlyDisplayedText() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html>\
                <body>\
                <a>hello world</a>\
                </body>\
                </html>
                """
        )

        await webView._startTextManipulations(with: nil)

        var items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello world")

        var errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello World")]
            )
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<a>Hello World</a>")

        try await webView.callJavaScript {
            "document.querySelector('a').innerHTML=''; document.querySelector('a').innerHTML='hello world again';"
        }
        items = await delegate.waitForItems(count: 2)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "hello world again")

        errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "Hello World Again")]
            )
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<a>Hello World Again</a>")
    }

    @Test
    func completeTextManipulationForManipulatedTextWithNewContent() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html><html>\
                <body>\
                <span>hello world</span>\
                <p>hello webkit</p>\
                </body></html>
                """
        )

        await webView._startTextManipulations(with: nil)

        var items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello world")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "hello webkit")

        var errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello World")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "Hello WebKit")]
            ),
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<span>Hello World</span><p>Hello WebKit</p>")

        try await webView.callJavaScript { "document.querySelector('span').innerHTML='hello world again';" }
        try await webView.callJavaScript { "document.querySelector('p').childNodes[0].nodeValue='hello webkit again';" }
        items = await delegate.waitForItems(count: 4)

        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "hello world again")
        try #require(items[3].tokens.count == 1)
        #expect(items[3].tokens[0].content == "hello webkit again")

        errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[2].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[2].tokens[0].identifier, content: "Hello World Again")]
            ),
            _WKTextManipulationItem(
                identifier: items[3].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[3].tokens[0].identifier, content: "Hello WebKit Again")]
            ),
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<span>Hello World Again</span><p>Hello WebKit Again</p>")
    }

    @Test
    func completeTextManipulationForTitleElement() async throws {
        try await webView.load(html: "<!DOCTYPE html><html></html>")

        await webView._startTextManipulations(with: nil)
        var items = delegate.items
        #expect(items.isEmpty)

        try await webView.callJavaScript { "document.title = 'page title'" }
        items = await delegate.waitForItems(count: 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "page title")

        var errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Page Title")]
            )
        ])
        #expect(errors == nil)
        let title = try await webView.callJavaScript(returning: String.self) { "return document.title" }
        #expect(title == "Page Title")
        let documentHTML = try await webView.callJavaScript(returning: String.self) { "return document.documentElement.innerHTML" }
        #expect(documentHTML == "<head><title>Page Title</title></head><body></body>")

        try await webView.callJavaScript { "document.title = 'new page title'" }
        items = await delegate.waitForItems(count: 2)
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "new page title")

        errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "New Page Title")]
            )
        ])
        #expect(errors == nil)
        let newTitle = try await webView.callJavaScript(returning: String.self) { "return document.title" }
        #expect(newTitle == "New Page Title")
        let newDocumentHTML = try await webView.callJavaScript(returning: String.self) { "return document.documentElement.innerHTML" }
        #expect(newDocumentHTML == "<head><title>New Page Title</title></head><body></body>")
    }

    @Test
    func completeTextManipulationAvoidExtractingManipulatedTextAfterManipulation() async throws {
        try await webView.load(html: "<p>foo<br>bar</p>")

        await webView._startTextManipulations(with: nil)

        var items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "foo")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == "bar")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "FOO")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "BAR")]
            ),
        ])
        #expect(errors == nil)

        #expect(try await bodyInnerHTML() == "<p>FOO<br>BAR</p>")

        try await webView.callJavaScript { "document.querySelector('p').style.display = 'none'" }
        try await webView.callJavaScript { "document.querySelector('p').style.display = ''" }
        try await webView.callJavaScript {
            """
            var element = document.createElement('span');
            element.innerHTML='end';
            document.querySelector('p').after(element)
            """
        }

        items = await delegate.waitForItems(count: 3)

        try #require(items.count == 3)
        try #require(items[2].tokens.count == 1)
        #expect(items[2].tokens[0].content == "end")
    }

    @Test
    func completeTextManipulationAddsOverflowHiddenToAvoidBreakingLayout() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <html>\
                <head>\
                  <style>\
                    span {\
                      display: inline-block;\
                      width: 28px;\
                      height: 40px;\
                      margin: 0;\
                      word-break: break-all;\
                      font-size: 32px;\
                    }\
                    a {\
                      display: block;\
                      width: 100px;\
                      height: 100px;\
                      overflow: hidden;\
                    }\
                  </style>\
                </head>\
                <body>\
                  <a>\
                    <span>A</span> Hello world\
                  </a>\
                </body>\
                </html>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "A")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == " Hello world")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "This is a long translation")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: " Foo bar")]
            ),
        ])
        #expect(errors == nil)

        let linkText = try await webView.callJavaScript(returning: String.self) { "return document.querySelector('a').textContent.trim()" }
        #expect(linkText == "This is a long translation Foo bar")
        let overflowX = try await webView.callJavaScript(returning: String.self) {
            "return getComputedStyle(document.querySelector('span')).overflowX"
        }
        #expect(overflowX == "hidden")
        let overflowY = try await webView.callJavaScript(returning: String.self) {
            "return getComputedStyle(document.querySelector('span')).overflowY"
        }
        #expect(overflowY == "auto")
    }

    @Test
    func completeTextManipulationParagraphsContainCollapsedSpaces() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <head>\
                <style>\
                span { display: inline-block; }\
                </style>\
                </head>\
                <body>\
                <span>  hello</span>\
                <span>  world</span>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")
        try #require(items[1].tokens.count == 1)
        #expect(items[1].tokens[0].content == " world")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello")]
            ),
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [_WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "World")]
            ),
        ])
        #expect(errors == nil)

        #expect(try await bodyInnerHTML() == "<span>Hello</span><span>World</span>")
    }

    @Test
    func completeTextManipulationParagraphContainsCollapsedSpaces() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <div id='div' style='width: 15ch;'>\
                <a href='https://www.webkit.org'>webkit</a>\
                 and \
                <a href='https://www.apple.com'>apple</a>\
                </div>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 3)
        #expect(items[0].tokens[0].content == "webkit")
        #expect(items[0].tokens[1].content == " and ")
        #expect(items[0].tokens[2].content == "apple")

        try await webView.callJavaScript { "document.getElementById('div').style.width='9ch';" }

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "WebKit"),
                    _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: " And "),
                    _WKTextManipulationToken(identifier: items[0].tokens[2].identifier, content: "Apple"),
                ]
            )
        ])
        #expect(errors == nil)

        let expectedHTML = """
            <div id="div" style="width: 9ch;"><a href="https://www.webkit.org">WebKit</a> And \
            <a href="https://www.apple.com">Apple</a></div>
            """
        #expect(try await bodyInnerHTML() == expectedHTML)
    }

    @Test
    func completeTextManipulationShouldReplaceContentIgnoredByEditing() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <body>\
                <div role='img'>\
                images\
                <a>link</a>\
                <img src='hello.png'>\
                <img src='webkit.png'>\
                </div>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "images")
        #expect(items[0].tokens[1].content == "link")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[0].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Images"),
                    _WKTextManipulationToken(identifier: items[0].tokens[1].identifier, content: "Link"),
                ]
            )
        ])
        #expect(errors == nil)

        #expect(try await bodyInnerHTML() == "<div role=\"img\">Images<a>Link</a><img src=\"hello.png\"><img src=\"webkit.png\"></div>")
    }

    @Test
    func completeTextManipulationShouldOnlyChangeNodesInParagraphRange() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <head>\
                <style>\
                span { white-space:pre; }\
                </style>\
                </head>\
                <body>\
                <span>zero &#10;<b>two four</b></span>\
                one\
                <i>three</i>\
                </body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 2)
        try #require(items[0].tokens.count == 2)
        #expect(items[0].tokens[0].content == "zero")
        #expect(items[0].tokens[1].content == " \n")
        try #require(items[1].tokens.count == 3)
        #expect(items[1].tokens[0].content == "two four")
        #expect(items[1].tokens[1].content == "one")
        #expect(items[1].tokens[2].content == "three")

        let errors = await webView._completeTextManipulation(for: [
            _WKTextManipulationItem(
                identifier: items[1].identifier,
                tokens: [
                    _WKTextManipulationToken(identifier: items[1].tokens[1].identifier, content: "one"),
                    _WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "two"),
                    _WKTextManipulationToken(identifier: items[1].tokens[2].identifier, content: "three"),
                    _WKTextManipulationToken(identifier: items[1].tokens[0].identifier, content: "four"),
                ]
            )
        ])
        #expect(errors == nil)
        #expect(try await bodyInnerHTML() == "<span>zero \n</span>one<span><b>two</b></span><i>three</i><span><b>four</b></span>")
    }

    @Test
    func completeTextManipulationParagraphBecomesHidden() async throws {
        try await webView.load(
            html: """
                <!DOCTYPE html>\
                <head><style> .hidden { display: none; } </style></head>\
                <body><span>hello</span></body>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")

        let result = try await webView.evaluateJavaScript("document.querySelector('span').classList.add('hidden');")
        #expect(result == nil)

        let item = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Hello")]
        )
        let errors = try #require(await webView._completeTextManipulation(for: [item]))
        #expect(errors.count == 1)
        let error = try #require(errors.first) as NSError
        #expect(error.domain == _WKTextManipulationItemErrorDomain)
        #expect(error.code == _WKTextManipulationItemErrorCode.contentChanged.rawValue)
        #expect(error.userInfo[_WKTextManipulationItemErrorItemKey] as? _WKTextManipulationItem === item)

        #expect(try await bodyInnerHTML() == "<span class=\"hidden\">hello</span>")
    }

    @Test
    func completeTextManipulationSkipsEmptyContainers() async throws {
        try await webView.load(
            html: """
                <section><a href='#'>Guten<img width='50' src='icon.png'></section>\
                <section><div id='target'></div><div><a href='#'>Tag</a></div></section>
                """
        )

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 3)
        #expect(items[0].tokens[0].content == "Guten")
        #expect(items[0].tokens[1].content == "[]")
        #expect(items[0].tokens[2].content == "Tag")

        let replacement = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [
                _WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "Good"),
                _WKTextManipulationToken(identifier: items[0].tokens[2].identifier, content: "Day"),
            ]
        )

        let errors = await webView._completeTextManipulation(for: [replacement])
        #expect((errors ?? []).isEmpty)

        let targetParentTagName = try await webView.callJavaScript(returning: String.self) {
            "return document.getElementById('target').parentElement.tagName"
        }
        #expect(targetParentTagName == "SECTION")
        let textContents = try await webView.callJavaScript(returning: [String].self) {
            "return Array.from(document.links).map(a => a.textContent)"
        }
        #expect(textContents.count == 2)
        #expect(textContents.first == "Good")
        #expect(textContents.last == "Day")
    }

    @Test
    func completeTextManipulationReplacesShadowDOMContent() async throws {
        try await webView.load(html: "<!DOCTYPE html><body><span id='host'><template shadowrootmode='open'>hello</template></span></body>")

        await webView._startTextManipulations(with: nil)

        let items = delegate.items
        try #require(items.count == 1)
        try #require(items[0].tokens.count == 1)
        #expect(items[0].tokens[0].content == "hello")

        let item = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "world")]
        )
        let errors = await webView._completeTextManipulation(for: [item])
        #expect((errors ?? []).isEmpty)

        let shadowRootText = try await webView.callJavaScript(returning: String.self) {
            "return document.getElementById('host').shadowRoot.textContent"
        }
        #expect(shadowRootText == "world")
    }

    @Test
    func completeTextManipulationDoesNotFillAutoFilledField() async throws {
        let configuration = WKWebViewConfiguration._test_configurationWithTestPlugInClassName(
            "WebProcessPlugInWithInternals",
            configureJSCForTesting: true
        )
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400), configuration: configuration)
        webView._textManipulationDelegate = delegate
        try await webView.load(html: "<!DOCTYPE html><body><input id='textField'></body>")

        await webView._startTextManipulations(with: nil)

        try await webView.callJavaScript { "document.getElementById('textField').value = 'foo'" }
        let items = await delegate.waitForItems(count: 1)

        try await webView.callJavaScript { "internals.setAutofilled(document.getElementById('textField'), true)" }

        let item = _WKTextManipulationItem(
            identifier: items[0].identifier,
            tokens: [_WKTextManipulationToken(identifier: items[0].tokens[0].identifier, content: "bar")]
        )
        _ = await webView._completeTextManipulation(for: [item])

        let textFieldValue = try await webView.callJavaScript(returning: String.self) {
            "return document.getElementById('textField').value"
        }
        #expect(textFieldValue == "foo")
    }

    @Test
    func textManipulationTokenDebugDescription() {
        let token = _WKTextManipulationToken()
        token.identifier = "foo_is_the_identifier"
        token.content = "bar_is_the_content"

        let description = token.description
        #expect(description.contains("foo_is_the_identifier"))
        #expect(description.contains("bar_is_the_content") == false)

        let debugDescription = token.debugDescription
        #expect(debugDescription.contains("foo_is_the_identifier"))
        #expect(debugDescription.contains("bar_is_the_content"))
    }

    @Test
    func textManipulationTokenNotEqualToNil() {
        let tokenA = _WKTextManipulationToken()
        tokenA.identifier = "A"
        tokenA.content = "A"

        #expect(tokenA.isEqual(to: nil, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: nil, includingContentEquality: false) == false)

        tokenA.identifier = nil
        tokenA.content = nil
        #expect(tokenA.isEqual(to: nil, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: nil, includingContentEquality: false) == false)
    }

    @Test
    func textManipulationTokenEqualityWithEqualIdentifiers() {
        let token1 = _WKTextManipulationToken()
        token1.identifier = "A"
        let token2 = _WKTextManipulationToken()
        token2.identifier = "A"

        #expect(token1.isEqual(to: token2, includingContentEquality: true))
        #expect(token1.isEqual(to: token2, includingContentEquality: false))

        // Same identifiers, different content.
        token1.content = "1"
        token2.content = "2"
        #expect(token1.isEqual(to: token2, includingContentEquality: true) == false)
        #expect(token1.isEqual(to: token2, includingContentEquality: false))

        // Same identifiers, different exclusion.
        token1.isExcluded = false
        token2.isExcluded = true
        token1.content = nil
        token2.content = nil
        #expect(token1.isEqual(to: token2, includingContentEquality: true) == false)
        #expect(token1.isEqual(to: token2, includingContentEquality: false) == false)

        // Same identifiers, different exclusion and different content.
        token1.content = "1"
        token2.content = "2"
        #expect(token1.isEqual(to: token2, includingContentEquality: true) == false)
        #expect(token1.isEqual(to: token2, includingContentEquality: false) == false)
    }

    @Test
    func textManipulationTokenEqualityWithDifferentIdentifiers() {
        let tokenA = _WKTextManipulationToken()
        tokenA.identifier = "A"
        let tokenB = _WKTextManipulationToken()
        tokenB.identifier = "B"

        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false) == false)

        // Different identifiers, same content.
        tokenA.content = "content"
        tokenB.content = "content"
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false) == false)

        // Different identifiers, same exclusion.
        tokenA.content = nil
        tokenB.content = nil
        tokenA.isExcluded = true
        tokenB.isExcluded = true
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false) == false)

        // Different identifiers, same content and same exclusion.
        tokenA.content = "content"
        tokenB.content = "content"
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false) == false)
    }

    @Test
    func textManipulationTokenEqualityWithNilIdentifiers() {
        let tokenA = _WKTextManipulationToken()
        #expect(tokenA.identifier == nil)
        let tokenB = _WKTextManipulationToken()
        tokenB.identifier = "B"
        let tokenC = _WKTextManipulationToken()
        #expect(tokenC.identifier == nil)

        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false) == false)
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: true))
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: false))

        // Equal content.
        tokenA.content = "content"
        tokenB.content = "content"
        tokenC.content = "content"
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false) == false)
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: true))
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: false))

        // Different content.
        tokenA.content = "contentA"
        tokenB.content = "contentB"
        tokenC.content = "contentC"
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false) == false)
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: false))
    }

    @Test
    func textManipulationTokenEqualityWithEmptyIdentifiers() {
        let tokenA = _WKTextManipulationToken()
        tokenA.identifier = ""
        let tokenB = _WKTextManipulationToken()
        tokenB.identifier = "B"
        let tokenC = _WKTextManipulationToken()
        tokenC.identifier = ""

        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false) == false)
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: true))
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: false))

        // Equal content.
        tokenA.content = "content"
        tokenB.content = "content"
        tokenC.content = "content"
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false) == false)
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: true))
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: false))

        // Different content.
        tokenA.content = "contentA"
        tokenB.content = "contentB"
        tokenC.content = "contentC"
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false) == false)
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: false))
    }

    @Test
    func textManipulationTokenWithNilContent() {
        let tokenA = _WKTextManipulationToken()
        tokenA.identifier = "A"
        let tokenB = _WKTextManipulationToken()
        tokenB.identifier = "A"

        #expect(tokenA.content == nil)
        #expect(tokenB.content == nil)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true))
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false))

        tokenB.content = ""
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false))

        tokenB.content = "B"
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false))
    }

    @Test
    func textManipulationTokenWithEmptyContent() {
        let tokenA = _WKTextManipulationToken()
        tokenA.identifier = "A"
        tokenA.content = ""
        let tokenB = _WKTextManipulationToken()
        tokenB.identifier = "A"
        tokenB.content = ""

        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true))
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false))

        tokenB.content = nil
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false))

        tokenB.content = "B"
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false))
    }

    @Test
    func textManipulationTokenWithIdenticalContent() {
        let tokenA = _WKTextManipulationToken()
        tokenA.identifier = "A"
        tokenA.content = "content"
        let tokenB = _WKTextManipulationToken()
        tokenB.identifier = "A"
        tokenB.content = "content"

        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true))
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false))
    }

    @Test
    func textManipulationTokenWithPointerEqualContent() {
        let tokenA = _WKTextManipulationToken()
        tokenA.identifier = "A"
        let tokenB = _WKTextManipulationToken()
        tokenB.identifier = "A"

        let contentString = "content"
        tokenA.content = contentString
        tokenB.content = contentString

        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true))
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false))
    }

    @Test
    func textManipulationTokenWithTrailingSpace() {
        let tokenA = _WKTextManipulationToken()
        tokenA.identifier = "A"
        tokenA.content = "content"
        let tokenB = _WKTextManipulationToken()
        tokenB.identifier = "A"
        tokenB.content = "content "

        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: false))
    }

    @Test
    func textManipulationTokenEqualToSelf() {
        let token = _WKTextManipulationToken()
        token.identifier = "A"
        token.content = "content"

        #expect(token.isEqual(to: token, includingContentEquality: true))
        #expect(token.isEqual(to: token, includingContentEquality: false))
        #expect(token.isEqual(token))
    }

    @Test
    func textManipulationTokenNSObjectEqualityWithOtherToken() {
        let tokenA = _WKTextManipulationToken()
        tokenA.identifier = "A"
        tokenA.content = "content"
        let tokenB = _WKTextManipulationToken()
        tokenB.identifier = "A"
        tokenB.content = "content"
        let tokenC = _WKTextManipulationToken()
        tokenC.identifier = "A"
        tokenC.content = "content "

        #expect(tokenA.isEqual(to: tokenB, includingContentEquality: true))
        #expect(tokenA.isEqual(tokenB))

        #expect(tokenA.isEqual(to: tokenC, includingContentEquality: true) == false)
        #expect(tokenA.isEqual(tokenC) == false)
    }

    @Test
    func textManipulationTokenNSObjectEqualityWithNonToken() {
        let token = _WKTextManipulationToken()
        token.identifier = "A"
        token.content = "content"
        let string = "content"

        #expect(token.isEqual(string) == false)
        #expect(token.isEqual(nil) == false)
    }

    @Test
    func textManipulationItemDebugDescription() {
        let tokenA = _WKTextManipulationToken(identifier: "public_identifier_A", content: "private_content_A")
        let tokenB = _WKTextManipulationToken(identifier: "public_identifier_B", content: "private_content_B")
        let item = _WKTextManipulationItem(identifier: "public_item_identifier", tokens: [tokenA, tokenB])

        let debugDescription = item.debugDescription
        #expect(debugDescription.contains("public_identifier_A"))
        #expect(debugDescription.contains("public_identifier_B"))
        #expect(debugDescription.contains("private_content_A"))
        #expect(debugDescription.contains("private_content_B"))
        #expect(debugDescription.contains("public_item_identifier"))

        let description = item.description
        #expect(description.contains("public_identifier_A"))
        #expect(description.contains("public_identifier_B"))
        #expect(description.contains("private_content_A") == false)
        #expect(description.contains("private_content_B") == false)
        #expect(description.contains("public_item_identifier"))
    }

    @Test
    func textManipulationItemEqualityToNilItem() {
        let item = _WKTextManipulationItem(identifier: "A", tokens: [])

        #expect(item.isEqual(to: nil, includingContentEquality: true) == false)
        #expect(item.isEqual(to: nil, includingContentEquality: false) == false)
    }

    @Test
    func textManipulationItemEqualityToSelf() {
        let token = _WKTextManipulationToken(identifier: "A", content: "token")
        let item = _WKTextManipulationItem(identifier: "B", tokens: [token])

        #expect(item.isEqual(to: item, includingContentEquality: true))
        #expect(item.isEqual(to: item, includingContentEquality: false))
        #expect(item.isEqual(item))
    }

    @Test
    func textManipulationItemBasicEquality() {
        let token1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let token2 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let itemA = _WKTextManipulationItem(identifier: "A", tokens: [token1])
        let itemB = _WKTextManipulationItem(identifier: "A", tokens: [token2])

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true))
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false))
    }

    @Test
    func textManipulationItemBasicEqualityWithMultipleTokens() {
        let tokenA1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let tokenA2 = _WKTextManipulationToken(identifier: "2", content: "token2")
        let tokenB1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let tokenB2 = _WKTextManipulationToken(identifier: "2", content: "token2")

        let itemA = _WKTextManipulationItem(identifier: "A", tokens: [tokenA1, tokenA2])
        let itemB = _WKTextManipulationItem(identifier: "A", tokens: [tokenB1, tokenB2])

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true))
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false))
    }

    @Test
    func textManipulationItemEqualitySimilarTokensWithDifferentContent() {
        let token1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let token2 = _WKTextManipulationToken(identifier: "1", content: "token2")
        let itemA = _WKTextManipulationItem(identifier: "A", tokens: [token1])
        let itemB = _WKTextManipulationItem(identifier: "A", tokens: [token2])

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true) == false)
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false))
    }

    @Test
    func textManipulationItemEqualityWithOutOfOrderTokens() {
        let tokenA1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let tokenA2 = _WKTextManipulationToken(identifier: "2", content: "token2")
        let tokenB1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let tokenB2 = _WKTextManipulationToken(identifier: "2", content: "token2")

        let itemA = _WKTextManipulationItem(identifier: "A", tokens: [tokenA1, tokenA2])
        let itemB = _WKTextManipulationItem(identifier: "A", tokens: [tokenB2, tokenB1])

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true) == false)
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false) == false)
    }

    @Test
    func textManipulationItemEqualityWithPointerEqualTokens() {
        let token1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let token2 = _WKTextManipulationToken(identifier: "2", content: "token2")

        let itemA = _WKTextManipulationItem(identifier: "A", tokens: [token1, token2])
        let itemB = _WKTextManipulationItem(identifier: "A", tokens: [token1, token2])

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true))
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false))
    }

    @Test
    func textManipulationItemEqualityWithPointerEqualTokenArrays() {
        let token1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let token2 = _WKTextManipulationToken(identifier: "2", content: "token2")
        let tokens = [token1, token2]

        let itemA = _WKTextManipulationItem(identifier: "A", tokens: tokens)
        let itemB = _WKTextManipulationItem(identifier: "A", tokens: tokens)

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true))
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false))
    }

    @Test
    func textManipulationItemEqualityWithMismatchedTokenCounts() {
        let tokenA1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let tokenA2 = _WKTextManipulationToken(identifier: "2", content: "token2")
        let tokenA3 = _WKTextManipulationToken(identifier: "3", content: "token3")
        let tokenB1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let tokenB2 = _WKTextManipulationToken(identifier: "2", content: "token2")

        let itemA = _WKTextManipulationItem(identifier: "A", tokens: [tokenA1, tokenA2, tokenA3])
        let itemB = _WKTextManipulationItem(identifier: "A", tokens: [tokenB1, tokenB2])

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true) == false)
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false) == false)

        #expect(itemB.isEqual(to: itemA, includingContentEquality: true) == false)
        #expect(itemB.isEqual(to: itemA, includingContentEquality: false) == false)
    }

    @Test
    func textManipulationItemEqualityWithDifferentTokenIdentifiers() {
        let tokenA = _WKTextManipulationToken(identifier: "A", content: "token")
        let tokenB = _WKTextManipulationToken(identifier: "B", content: "token")
        let itemA = _WKTextManipulationItem(identifier: "A", tokens: [tokenA])
        let itemB = _WKTextManipulationItem(identifier: "A", tokens: [tokenB])

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true) == false)
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false) == false)
    }

    @Test
    func textManipulationItemEqualityWithNilIdentifiers() {
        let token = _WKTextManipulationToken(identifier: "A", content: "token")
        let itemA = _WKTextManipulationItem(identifier: nil, tokens: [token])
        let itemB = _WKTextManipulationItem(identifier: nil, tokens: [token])

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true))
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false))
    }

    @Test
    func textManipulationItemEqualityWithDifferentTokenExclusions() {
        let tokenA = _WKTextManipulationToken(identifier: "1", content: "token", excluded: false)
        let tokenB = _WKTextManipulationToken(identifier: "1", content: "token", excluded: true)
        let itemA = _WKTextManipulationItem(identifier: "A", tokens: [tokenA])
        let itemB = _WKTextManipulationItem(identifier: "A", tokens: [tokenB])

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true) == false)
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false) == false)
    }

    @Test
    func textManipulationItemNSObjectEqualityWithOtherItem() {
        let tokenA1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let tokenA2 = _WKTextManipulationToken(identifier: "2", content: "token2")
        let tokenB1 = _WKTextManipulationToken(identifier: "1", content: "token1")
        let tokenB2 = _WKTextManipulationToken(identifier: "2", content: "token2")

        let itemA = _WKTextManipulationItem(identifier: "A", tokens: [tokenA1, tokenA2])
        let itemB = _WKTextManipulationItem(identifier: "A", tokens: [tokenB1, tokenB2])

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true))
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false))
        #expect(itemA.isEqual(itemB))

        tokenB2.content = "something else"

        #expect(itemA.isEqual(to: itemB, includingContentEquality: true) == false)
        #expect(itemA.isEqual(to: itemB, includingContentEquality: false))
        #expect(itemA.isEqual(itemB) == false)
    }

    @Test
    func textManipulationItemNSObjectEqualityWithNonToken() {
        let token = _WKTextManipulationToken(identifier: "1", content: "token1")
        _ = _WKTextManipulationItem(identifier: "A", tokens: [token])
        let string = "content"

        #expect(token.isEqual(string) == false)
        #expect(token.isEqual(nil) == false)
    }
}
