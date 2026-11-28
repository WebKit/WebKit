// Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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

import CoreGraphics
import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import UniformTypeIdentifiers
import WebKit
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKWebViewConfigurationPrivate
private import WebKit_Private.WKWebViewPrivate

import struct Swift.String

#if WTF_PLATFORM_IOS_FAMILY
private import UIKit
#else
private import AppKit
#endif

@MainActor
struct PasteHTMLTests {
    private func writeHTMLToPasteboard(_ html: String) {
        #if WTF_PLATFORM_MAC
        NSPasteboard.general.declareTypes([.legacyHTML], owner: nil)
        NSPasteboard.general.setString(html, forType: .legacyHTML)
        #else
        UIPasteboard.general.setItems([[UTType.html.identifier: html]])
        #endif
    }

    private func testResourceString(
        _ name: String,
        fileID: String = #fileID,
        filePath: String = #filePath,
        line: Int = #line,
        column: Int = #column
    ) throws -> String {
        let url = try requireNonNil(
            Bundle.testResources.url(forResource: name, withExtension: "html"),
            fileID: fileID,
            filePath: filePath,
            line: line,
            column: column
        )
        return try String(contentsOf: url, encoding: .utf8)
    }

    // rdar://159421461
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://159421461"))
    #else
    @Test
    #endif
    func exposesHTMLTypeInDataTransfer() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")

        writeHTMLToPasteboard("<!DOCTYPE html><html><body><p><u>hello</u>, world</p></body></html>")
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) { "editor.innerHTML = clipboardData.values[0]; editor.focus()" }
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return document.queryCommandState('underline')" })
        try await webView.callJavaScript(userGesture: false) { "getSelection().modify('move', 'forward', 'lineboundary')" }
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return document.queryCommandState('underline')" })
        expectEqual(try await webView.callJavaScript(returning: String.self) { "return editor.textContent" }, "hello, world")
    }

    // rdar://138144869
    // rdar://159421461 (Release)
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://159421461"))
    #else
    @Test
    #endif
    func sanitizesHTML() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")

        writeHTMLToPasteboard(
            #"<!DOCTYPE html><meta content="secret"><b onmouseover="dangerousCode()">hello</b>"#
                + "<!-- secret-->, world<script>dangerousCode()</script>';"
        )
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('hello')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('world')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('secret')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('dangerousCode')" })
    }

    #if WTF_PLATFORM_IOS
    @Test(.disabled())
    #else
    @Test
    #endif
    func sanitizerKeepsPresentationIntentAttributes() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")

        writeHTMLToPasteboard(
            #"<!DOCTYPE html><ol start="3"><li value="5">item</li></ol><pre><code class="language-swift">code</code></pre>"#
                + #"<div role="heading" aria-level="2">heading</div>"#
        )
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) { "editor.innerHTML = clipboardData.values[0]" }
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return editor.querySelector('ol').getAttribute('start')" },
            "3"
        )
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return editor.querySelector('li').getAttribute('value')" },
            "5"
        )
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return editor.querySelector('code').getAttribute('class')" },
            "language-swift"
        )
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return editor.querySelector('[role]').getAttribute('role')" },
            "heading"
        )
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return editor.querySelector('[role]').getAttribute('aria-level')" },
            "2"
        )
    }

    // rdar://138144869
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://138144869"))
    #else
    @Test
    #endif
    func doesNotSanitizeHTMLWhenCustomPasteboardDataIsDisabled() async throws {
        let webView = makeWebViewWithCustomPasteboardData(enabled: false)
        try await webView.load(testPageNamed: "paste-rtfd")

        writeHTMLToPasteboard(
            #"<!DOCTYPE html><meta content="secret"><b onmouseover="dangerousCode()">hello</b>"#
                + "<!-- secret-->, world<script>dangerousCode()</script>';"
        )
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('hello')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('world')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('secret')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('dangerousCode')" })
    }

    // rdar://138144869
    // rdar://159421461 (Release)
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://159421461"))
    #else
    @Test
    #endif
    func stripsFileAndJavaScriptURLs() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")

        writeHTMLToPasteboard(
            """
            <!DOCTYPE html><html><body>\
            <a alt='hello' href='file:///private/var/folders/secret/files/'>world</a>\
            <a href='rdar://101878956'>Radar Link</a>\
            <a href='javascript:runCode()'>JavaScript Link</a>
            """
        )
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('hello')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('world')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('secret')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('rdar://101878956')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('Radar Link')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('runCode()')" })
    }

    // rdar://138144869
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://138144869"))
    #else
    @Test
    #endif
    func doesNotStripFileURLsWhenCustomPasteboardDataIsDisabled() async throws {
        let webView = makeWebViewWithCustomPasteboardData(enabled: false)
        try await webView.load(testPageNamed: "paste-rtfd")

        writeHTMLToPasteboard("<!DOCTYPE html><html><body><a alt='hello' href='file:///private/var/folders/secret/files/'>world</a>")
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('hello')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('world')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('secret')" })
    }

    // rdar://138144869
    // rdar://159421461 (Release)
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://159421461"))
    #else
    @Test
    #endif
    func keepsHTTPURLs() async throws {
        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")

        writeHTMLToPasteboard(
            "<!DOCTYPE html><html><body><a title='hello' href='https://svn.webkit.org/repository/webkit/trunk/LayoutTests/editing/resources/abe.png'>world</a>"
        )
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('hello')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('world')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.values[0].includes('abe.png')" })
    }

    // rdar://138144869
    // rdar://159421461 (Release)
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://159421461"))
    #else
    @Test
    #endif
    func preservesMSOList() async throws {
        writeHTMLToPasteboard(try testResourceString("mso-list"))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) {
            "window.htmlInDataTransfer = clipboardData.values[clipboardData.types.indexOf('text/html')]"
        }
        try await webView.callJavaScript(userGesture: false) { "window.pastedHTML = editor.innerHTML" }

        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.startsWith('<html xmlns:o="urn:schemas-microsoft-com:office:office"')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes('<style class="WebKit-mso-list-quirks-style">\n<!--\n')"#
            }
        )
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/* Style Definitions */')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/* List Definitions */')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('@list l0:level1')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { #"return pastedHTML.includes('}\n\n-->\n</style>')"# })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('[if !supportLists]')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('[endif]')" })
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes(' style="text-indent:-.25in;mso-list:l0 level1 lfo1">')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes('<p class="MsoNormal" style="margin-left:0cm;text-indent:0cm;mso-pagination:none;\n"#
                    + #"mso-list:l1 level1 lfo1;mso-layout-grid-align:none;text-autospace:none">')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes('<p class="MsoNormal" style="mso-list:l1 level1 lfo1;margin-left:0cm">')"#
            }
        )
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/Users/webkitten/Library/')" })

        try await webView.callJavaScript(userGesture: false) {
            "getSelection().setPosition(document.querySelector('.MsoListParagraphCxSpLast'));"
        }
        try await webView.callJavaScript(userGesture: false) { "getSelection().modify('move', 'forward', 'lineboundary');" }
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return document.queryCommandValue('foreColor')" },
            "rgb(255, 0, 0)"
        )

        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.startsWith('<html xmlns:o="urn:schemas-microsoft-com:office:office"')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.includes('<head><style class="WebKit-mso-list-quirks-style">\n<!--\n')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/* Style Definitions */')" }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/* List Definitions */')" }
        )
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('@list l0:level1')" })
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) { #"return htmlInDataTransfer.includes('}\n\n-->\n</style></head>')"# }
        )
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('[if !supportLists]')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('[endif]')" })
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.includes(' style="text-indent:-.25in;mso-list:l0 level1 lfo1">')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.includes('<p class="MsoNormal" style="margin-left:0cm;text-indent:0cm;mso-pagination:none;\n"#
                    + #"mso-list:l1 level1 lfo1;mso-layout-grid-align:none;text-autospace:none">')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.includes('<p class="MsoNormal" style="mso-list:l1 level1 lfo1;margin-left:0cm">')"#
            }
        )
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/Users/webkitten/Library/')" }
        )

        try await webView.callJavaScript(userGesture: false) { "editor.innerHTML = ''; editor.focus();" }
        try await webView.callJavaScript(userGesture: false) { "document.execCommand('insertHTML', false, htmlInDataTransfer);" }
        try await webView.callJavaScript(userGesture: false) {
            "getSelection().setPosition(document.querySelector('.MsoListParagraphCxSpLast'));"
        }
        try await webView.callJavaScript(userGesture: false) { "getSelection().modify('move', 'forward', 'lineboundary');" }
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return document.queryCommandValue('foreColor')" },
            "rgb(255, 0, 0)"
        )
    }

    // rdar://159421461
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://159421461"))
    #else
    @Test
    #endif
    func preservesMSOListInCompatibilityMode() async throws {
        writeHTMLToPasteboard(try testResourceString("mso-list-compat-mode"))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) {
            "window.htmlInDataTransfer = clipboardData.values[clipboardData.types.indexOf('text/html')]"
        }
        try await webView.callJavaScript(userGesture: false) { "window.pastedHTML = editor.innerHTML" }

        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.startsWith('<html xmlns:o="urn:schemas-microsoft-com:office:office"')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes('<style class="WebKit-mso-list-quirks-style">\n<!--\n')"#
            }
        )
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/* Style Definitions */')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/* List Definitions */')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('@list l0:level1')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { #"return pastedHTML.includes('}\n\n-->\n</style>')"# })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('[if !supportLists]')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('[endif]')" })
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes('<p style="margin-left:.5in;text-indent:-.25in;mso-list:l0 level1 lfo1">')"#
            }
        )
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/Users/webkitten/Library/')" })

        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.startsWith('<html xmlns:o="urn:schemas-microsoft-com:office:office"')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.includes('<head><style class="WebKit-mso-list-quirks-style">\n<!--\n')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/* Style Definitions */')" }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/* List Definitions */')" }
        )
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('@list l0:level1')" })
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) { #"return htmlInDataTransfer.includes('}\n\n-->\n</style></head>')"# }
        )
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('[if !supportLists]')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('[endif]')" })
        // This checks `pastedHTML` where `htmlInDataTransfer` was probably intended.
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes('<p style="margin-left:.5in;text-indent:-.25in;mso-list:l0 level1 lfo1">')"#
            }
        )
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/Users/webkitten/Library/')" }
        )
    }

    // rdar://138144869
    // rdar://159421461 (Release)
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://159421461"))
    #else
    @Test
    #endif
    func preservesMSOListOnH4() async throws {
        writeHTMLToPasteboard(try testResourceString("mso-list-on-h4"))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) {
            "window.htmlInDataTransfer = clipboardData.values[clipboardData.types.indexOf('text/html')]"
        }
        try await webView.callJavaScript(userGesture: false) { "window.pastedHTML = editor.innerHTML" }

        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.startsWith('<html xmlns:o="urn:schemas-microsoft-com:office:office"')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes('<style class="WebKit-mso-list-quirks-style">\n<!--\n')"#
            }
        )
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/* Style Definitions */')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/* List Definitions */')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('@list l0:level1')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { #"return pastedHTML.includes('}\n\n-->\n</style>')"# })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('[if !supportLists]')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('[endif]')" })
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes('<h4 style="margin-left:67.5pt;text-indent:-.25in;mso-list:l0 level1 lfo4">')"#
            }
        )
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/Users/webkitten/Library/')" })

        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.startsWith('<html xmlns:o="urn:schemas-microsoft-com:office:office"')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.includes('<head><style class="WebKit-mso-list-quirks-style">\n<!--\n')"#
            }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/* Style Definitions */')" }
        )
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/* List Definitions */')" }
        )
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('@list l0:level1')" })
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) { #"return htmlInDataTransfer.includes('}\n\n-->\n</style></head>')"# }
        )
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('[if !supportLists]')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('[endif]')" })
        expectTrue(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.includes('<h4 style="margin-left:67.5pt;text-indent:-.25in;mso-list:l0 level1 lfo4">')"#
            }
        )
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/Users/webkitten/Library/')" }
        )
    }

    // rdar://138144869
    // rdar://159421461 (Release)
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://159421461"))
    #else
    @Test
    #endif
    func stripsMSOListWhenMissingMSOHTMLElement() async throws {
        let markup = try testResourceString("mso-list")

        writeHTMLToPasteboard(String(markup[markup.index(after: try requireNonNil(markup.firstIndex(of: ">")))...]))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) {
            "window.htmlInDataTransfer = clipboardData.values[clipboardData.types.indexOf('text/html')]"
        }
        try await webView.callJavaScript(userGesture: false) { "window.pastedHTML = editor.innerHTML" }

        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.startsWith('<html xmlns:o="urn:schemas-microsoft-com:office:office"')"#
            }
        )
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes('<style class="WebKit-mso-list-quirks-style">\n<!--\n')"#
            }
        )
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/* Style Definitions */')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/* List Definitions */')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('@list l0:level1')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { #"return pastedHTML.includes('}\n\n-->\n</style>')"# })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('[if !supportLists]')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('[endif]')" })
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return pastedHTML.includes(' style="text-indent:-.25in;mso-list:l0 level1 lfo1">')"#
            }
        )
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('/Users/webkitten/Library/')" })

        try await webView.callJavaScript(userGesture: false) {
            "getSelection().setPosition(document.querySelector('.MsoListParagraphCxSpLast'));"
        }
        try await webView.callJavaScript(userGesture: false) { "getSelection().modify('move', 'forward', 'lineboundary');" }
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return document.queryCommandValue('foreColor')" },
            "rgb(255, 0, 0)"
        )

        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.startsWith('<html xmlns:o="urn:schemas-microsoft-com:office:office"')"#
            }
        )
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.includes('<head><style class="WebKit-mso-list-quirks-style">\n<!--\n')"#
            }
        )
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/* Style Definitions */')" }
        )
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/* List Definitions */')" }
        )
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('@list l0:level1')" })
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) { #"return htmlInDataTransfer.includes('}\n\n-->\n</style></head>')"# }
        )
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('[if !supportLists]')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('[endif]')" })
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) {
                #"return htmlInDataTransfer.includes(' style="text-indent:-.25in;mso-list:l0 level1 lfo1">')"#
            }
        )
        expectFalse(
            try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('/Users/webkitten/Library/')" }
        )

        try await webView.callJavaScript(userGesture: false) { "editor.innerHTML = ''; editor.focus();" }
        try await webView.callJavaScript(userGesture: false) { "document.execCommand('insertHTML', false, htmlInDataTransfer);" }
        try await webView.callJavaScript(userGesture: false) {
            "getSelection().setPosition(document.querySelector('.MsoListParagraphCxSpLast'));"
        }
        try await webView.callJavaScript(userGesture: false) { "getSelection().modify('move', 'forward', 'lineboundary');" }
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return document.queryCommandValue('foreColor')" },
            "rgb(255, 0, 0)"
        )
    }

    // rdar://138144869
    // rdar://159421461 (Release)
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://159421461"))
    #else
    @Test
    #endif
    func stripsSystemFontNames() async throws {
        writeHTMLToPasteboard(try testResourceString("cocoa-writer-markup-with-system-fonts"))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) {
            "window.htmlInDataTransfer = clipboardData.values[clipboardData.types.indexOf('text/html')]"
        }
        try await webView.callJavaScript(userGesture: false) { "window.pastedHTML = editor.innerHTML" }

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('Hello Cocoa')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('font-weight: bold')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return !pastedHTML.includes('.AppleSystemUIFont')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return !pastedHTML.includes('.SFUI')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return !pastedHTML.includes('.SF')" })

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('Hello Cocoa')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('font-weight: bold')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return !htmlInDataTransfer.includes('.AppleSystemUIFont')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return !htmlInDataTransfer.includes('.SFUI')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return !htmlInDataTransfer.includes('.SF')" })

        expectEqual(
            try await webView.callJavaScript(returning: String.self) {
                "return getComputedStyle(document.querySelector('.s2')).fontFamily"
            },
            try await webView.callJavaScript(returning: String.self) { "return getComputedStyle(document.body).fontFamily" }
        )
        expectEqual(
            try await webView.callJavaScript(returning: String.self) {
                "return getComputedStyle(document.querySelector('.s4')).fontFamily"
            },
            try await webView.callJavaScript(returning: String.self) { "return getComputedStyle(document.body).fontFamily" }
        )
    }

    // rdar://138144869
    #if WTF_PLATFORM_IOS
    @Test(.disabled("rdar://138144869"))
    #else
    @Test
    #endif
    func doesNotAddStandardFontFamily() async throws {
        writeHTMLToPasteboard(try testResourceString("cocoa-writer-markup-with-lists"))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        try await webView.callJavaScript(userGesture: false) { "document.body.style.fontFamily = 'Arial'" }
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) {
            "window.htmlInDataTransfer = clipboardData.values[clipboardData.types.indexOf('text/html')]"
        }
        try await webView.callJavaScript(userGesture: false) { "window.pastedHTML = editor.innerHTML" }

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('Hello')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return pastedHTML.includes('font-weight: bold')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return !pastedHTML.includes('-webkit-standard')" })

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('Hello')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return htmlInDataTransfer.includes('font-weight: bold')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return !htmlInDataTransfer.includes('-webkit-standard')" })

        expectEqual(
            try await webView.callJavaScript(returning: String.self) {
                "return getComputedStyle(document.querySelector('.s2')).fontFamily"
            },
            try await webView.callJavaScript(returning: String.self) { "return getComputedStyle(document.body).fontFamily" }
        )
        expectEqual(
            try await webView.callJavaScript(returning: String.self) {
                "return getComputedStyle(document.querySelector('.s4')).fontFamily"
            },
            try await webView.callJavaScript(returning: String.self) { "return getComputedStyle(document.body).fontFamily" }
        )
    }

    #if WTF_PLATFORM_MAC

    @Test
    func readSelectionFromPasteboard() async throws {
        let generalPasteboard = NSPasteboard.general
        generalPasteboard.clearContents()
        generalPasteboard.setString("Hello world", forType: .string)

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(html: "<input>")
        try await webView.callJavaScript(userGesture: false) { "document.querySelector('input').focus()" }
        webView.readSelection(from: generalPasteboard)

        let inputValue = try await webView.callJavaScript(returning: String.self) { "return document.querySelector('input').value" }
        expectEqual(inputValue, "Hello world")
    }

    @Test
    func readSelectionFromPasteboardDispatchesPaste() async throws {
        let pasteboard = NSPasteboard.withUniqueName()
        let string = NSAttributedString(string: "Bold text", attributes: [.font: NSFont.boldSystemFont(ofSize: 12)])
        pasteboard.writeObjects([string])

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "DataTransfer")
        webView.readSelection(from: pasteboard)

        let textData = try await webView.callJavaScript(returning: String.self) { "return document.getElementById('textData').textContent" }
        let types = try await webView.callJavaScript(returning: String.self) { "return document.getElementById('types').textContent" }

        expectEqual(textData, string.string)
        expectTrue(types.contains("text/html"))
        expectTrue(types.contains("text/plain"))
    }

    #endif // WTF_PLATFORM_MAC

    #if ENABLE_DARK_MODE_CSS

    @Test
    func transformColorsOfDarkContent() async throws {
        let webView = makeWebViewWithCustomPasteboardData(colorFilterEnabled: true)
        webView.forceDarkMode()

        try await webView.load(testPageNamed: "rich-color-filtered")

        writeHTMLToPasteboard(#"<span style="color: rgb(238, 238, 238); background-color: rgb(51, 51, 51)">Hello</span>"#)

        try await webView.callJavaScript(userGesture: false) { "selectRichText()" }
        webView.paste(nil)

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('span').style.color" },
            "rgb(21, 21, 21)"
        )
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('span').style.backgroundColor" },
            "rgb(255, 255, 255)"
        )
    }

    @Test
    func doesNotTransformColorsOfLightContent() async throws {
        let webView = makeWebViewWithCustomPasteboardData(colorFilterEnabled: true)
        webView.forceDarkMode()

        try await webView.load(testPageNamed: "rich-color-filtered")

        writeHTMLToPasteboard(#"<span style="color: rgb(101, 101, 101)">Hello</span>"#)

        try await webView.callJavaScript(userGesture: false) { "selectRichText()" }
        webView.paste(nil)

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('span').style.color" },
            "rgb(101, 101, 101)"
        )
    }

    @Test
    func transformColorsOfDarkContentButNotSemanticColor() async throws {
        let webView = makeWebViewWithCustomPasteboardData(colorFilterEnabled: true)
        webView.forceDarkMode()

        try await webView.load(testPageNamed: "rich-color-filtered")

        try await webView.callJavaScript(userGesture: false) { "document.body.style.color = '-apple-system-label';" }
        writeHTMLToPasteboard("<span style='color: -apple-system-label'>hello</span><b style='color: #eee'>world</b>")

        try await webView.callJavaScript(userGesture: false) { "selectRichText()" }
        webView.paste(nil)

        #if USE_APPKIT
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return rich.innerHTML" },
            #"hello<b style="color: rgb(21, 21, 21);">world</b>"#
        )
        #else
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return rich.innerHTML" },
            #"<span style="-webkit-text-size-adjust: auto;">hello</span><b style="-webkit-text-size-adjust: auto; color: rgb(21, 21, 21);">world</b>"#
        )
        #endif
    }

    @Test
    func doesNotTransformColorsOfLightContentDuringOutdent() async throws {
        let webView = makeWebViewWithCustomPasteboardData(colorFilterEnabled: true)
        webView.forceDarkMode()

        try await webView.load(testPageNamed: "rich-color-filtered")

        try await webView.callJavaScript(userGesture: false) {
            "document.body.style = `color: -apple-system-label; caret-color: -apple-system-secondary-label; background-color: -apple-system-text-background;`;"
        }
        try await webView.callJavaScript(userGesture: false) {
            "rich.innerHTML = `<ul><li>hello</li><ul><li id='target'>world</li></ul></ul>`;"
        }

        try await webView.callJavaScript(userGesture: false) { "getSelection().setPosition(target, 0); document.execCommand('outdent');" }

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return rich.innerHTML" },
            "<ul><li>hello</li><li>world</li></ul>"
        )
    }

    @Test
    func transformColorsDependsOnUsedInlineStyle() async throws {
        let webView = makeWebViewWithCustomPasteboardData(colorFilterEnabled: true)
        webView.forceDarkMode()

        try await webView.load(testPageNamed: "rich-color-filtered")

        writeHTMLToPasteboard(
            "<ul style='color: black'><li style='color: white'>Coffee</li><li style='color: white'>Milk</li><li style='color: white'>Tea</li></ul>"
        )

        try await webView.callJavaScript(userGesture: false) { "selectRichText()" }
        webView.paste(nil)

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelector('li').style.color" },
            "rgb(0, 0, 0)"
        )
    }

    @Test
    func pasteDarkTextOnWhiteBackgroundIntoDarkModeEditor() async throws {
        do {
            let configuration = WKWebViewConfiguration()
            let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400), configuration: configuration)
            webView._isEditable = true
            try await webView.load(html: "<span style='background-color: white; color: rgb(5, 5, 5);'>Hello World</span>")
            try await webView.callJavaScript(userGesture: false) { "getSelection().selectAllChildren(document.querySelector('span'))" }
            _ = await webView._executeEditCommand("Copy", argument: nil)
        }

        let configuration = WKWebViewConfiguration()
        configuration._colorFilterEnabled = false
        configuration.preferences._punchOutWhiteBackgroundsInDarkMode = true

        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400), configuration: configuration)
        webView._isEditable = true
        webView.forceDarkMode()
        try await webView.load(html: "<html><head><style>:root { color-scheme: light dark; }</style></head><body></body></html>")

        try await webView.callJavaScript(userGesture: false) { "document.body.focus()" }
        webView.paste(nil)
        await webView.nextPresentationUpdate()

        let computedColor = try await webView.callJavaScript(returning: String.self) {
            "return getComputedStyle(document.querySelector('span')).color"
        }
        expectEqual(computedColor, "rgb(255, 255, 255)")

        let computedCaretColor = try await webView.callJavaScript(returning: String.self) {
            "return getComputedStyle(document.querySelector('span')).caretColor"
        }
        expectEqual(computedCaretColor, "rgb(255, 255, 255)")
    }

    @Test
    func dropsCanvasLikeBackgroundWhenPastingIntoPunchOutEditor() async throws {
        let configuration = WKWebViewConfiguration()
        configuration._colorFilterEnabled = true
        configuration.preferences._punchOutWhiteBackgroundsInDarkMode = true
        configuration.setFeature("DataTransferItemsEnabled", enabled: true)
        configuration.setFeature("CustomPasteboardDataEnabled", enabled: true)

        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400), configuration: configuration)
        webView._isEditable = true
        webView.forceDarkMode()
        try await webView.load(testPageNamed: "rich-color-filtered")

        writeHTMLToPasteboard(
            #"<p style="color: rgb(23, 43, 77); background-color: rgb(244, 245, 247);">Overview</p>"#
                + #"<p style="color: rgb(23, 43, 77); background-color: rgb(255, 193, 7);">At risk</p>"#
        )

        try await webView.callJavaScript(userGesture: false) { "selectRichText()" }
        webView.paste(nil)

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelectorAll('p')[0].style.backgroundColor" },
            ""
        )
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return rich.querySelectorAll('p')[1].style.backgroundColor" },
            "rgb(255, 193, 7)"
        )
    }

    /// The `CGImage` of a snapshot, as `TestWebKitAPI::Util::convertToCGImage()` makes it.
    private func cgImage(of image: CocoaImage) -> CGImage? {
        #if WTF_PLATFORM_IOS_FAMILY
        image.cgImage
        #else
        unsafe image.cgImage(forProposedRect: nil, context: nil, hints: nil)
        #endif
    }

    /// The relative luminance of each pixel of an image, row by row from the top.
    ///
    /// Draws the image into an 8-bit sRGB bitmap like `CGImagePixelReader`, and computes the luminance like
    /// `WebCore::Color::luminance()`, to within rounding.
    private func relativeLuminances(of image: CGImage) -> [Double]? {
        guard let colorSpace = CGColorSpace(name: CGColorSpace.sRGB) else {
            return nil
        }

        let width = image.width
        let height = image.height
        var pixels = [UInt8](repeating: 0, count: 4 * width * height)
        let drew = pixels.withUnsafeMutableBytes { buffer in
            guard
                let context = unsafe CGContext(
                    data: buffer.baseAddress,
                    width: width,
                    height: height,
                    bitsPerComponent: 8,
                    bytesPerRow: 4 * width,
                    space: colorSpace,
                    bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue).union(.byteOrder32Big).rawValue
                )
            else {
                return false
            }
            context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
            return true
        }
        guard drew else {
            return nil
        }

        func linearized(_ component: UInt8) -> Double {
            let value = Double(component) / 255
            return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4)
        }

        return stride(from: 0, to: pixels.count, by: 4)
            .map { offset in
                0.2126 * linearized(pixels[offset]) + 0.7152 * linearized(pixels[offset + 1]) + 0.0722 * linearized(pixels[offset + 2])
            }
    }

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "UIWindow(frame:) is deprecated, but the test needs a window of its own on iOS")
    #endif
    @Test
    func quotedContentOnPunchedOutBackgroundRemainsLegible() async throws {
        let webViewFrame = CGRect(x: 0, y: 0, width: 400, height: 400)

        let configuration = WKWebViewConfiguration()
        configuration._colorFilterEnabled = false
        configuration.preferences._punchOutWhiteBackgroundsInDarkMode = true

        #if WTF_PLATFORM_IOS_FAMILY
        let webView = TestWKWebView(frame: webViewFrame, configuration: configuration, addToWindow: false)
        let window = UIWindow(frame: webViewFrame)
        window.addSubview(webView)
        // Nothing else keeps the window alive.
        defer { withExtendedLifetime(window) {} }
        #else
        let webView = TestWKWebView(frame: webViewFrame, configuration: configuration)
        #endif

        webView._isEditable = true
        webView.forceDarkMode()

        // Punching out requires dark appearance, which requires the document to opt into dark mode.
        try await webView.load(html: "<html><head><style>:root { color-scheme: light dark; }</style></head><body dir=auto></body></html>")

        // Mimics -[BodyFieldIOS addMarkupString:] / _addNode: for a reply.
        try await webView.callJavaScript(userGesture: false) {
            """
            (() => {\
                let node = document.createElement('div');\
                node.innerHTML = "<div id='quoted' style='background-color: #fff; color: #1d1d1f; font: 32px Helvetica;'>HHHHHHHH</div>";\
                let blockquote = document.createElement('blockquote');\
                blockquote.setAttribute('type', 'cite');\
                blockquote.appendChild(node);\
                document.body.insertBefore(blockquote, document.body.firstChild);\
            })()
            """
        }
        await webView.nextPresentationUpdate()

        // The DOM is deliberately left alone, so the sender's colors survive into the outgoing message.
        expectEqual(try await webView.callJavaScript(returning: String.self) { "return getComputedStyle(quoted).color" }, "rgb(29, 29, 31)")

        let snapshotConfiguration = WKSnapshotConfiguration()
        snapshotConfiguration.afterScreenUpdates = true
        let snapshot = try requireNonNil(cgImage(of: try await webView.takeSnapshot(configuration: snapshotConfiguration)))
        let luminances = try requireNonNil(relativeLuminances(of: snapshot))

        let rect = try await webView.callJavaScript(returning: [Double].self) {
            "return (() => { let r = quoted.getBoundingClientRect(); return [r.left, r.top, r.width, r.height]; })()"
        }
        let scaleX = Double(snapshot.width) / Double(webViewFrame.width)
        let scaleY = Double(snapshot.height) / Double(webViewFrame.height)
        let minX = Int(rect[0] * scaleX)
        let minY = Int(rect[1] * scaleY)
        let maxX = min(snapshot.width, Int(Double(minX) + rect[2] * scaleX))
        let maxY = min(snapshot.height, Int(Double(minY) + rect[3] * scaleY))

        var darkestLuminance = 1.0
        var brightestLuminance = 0.0
        for y in stride(from: minY, to: maxY, by: 1) {
            for x in stride(from: minX, to: maxX, by: 1) {
                let luminance = luminances[y * snapshot.width + x]
                darkestLuminance = min(darkestLuminance, luminance)
                brightestLuminance = max(brightestLuminance, luminance)
            }
        }

        // Fails if the background painted white, which would make the contrast check vacuous.
        expectTrue(darkestLuminance < 0.1, "darkest luminance \(darkestLuminance)")

        let wcagContrastRatio = (brightestLuminance + 0.05) / (darkestLuminance + 0.05)
        expectTrue(wcagContrastRatio >= 4.5, "contrast ratio \(wcagContrastRatio)")
    }

    #endif // ENABLE_DARK_MODE_CSS
}
