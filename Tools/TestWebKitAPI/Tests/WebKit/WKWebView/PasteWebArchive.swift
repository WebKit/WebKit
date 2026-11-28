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

#if WTF_PLATFORM_MAC

import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import UniformTypeIdentifiers
import WebKit
private import WebKit_Private.WKWebViewPrivate

import struct Foundation.URL
import struct Swift.String

private import AppKit

private let sampleMarkup = Data(
    (#"<!DOCTYPE html><html><body><meta content="secret"><b onmouseover="dangerousCode()">hello</b>"#
        + "<!-- secret-->, world<script>dangerousCode()</script></html>")
        .utf8
)

@MainActor
struct PasteWebArchiveTests {
    private func writeToPasteboard(_ archiveData: Data, type: NSPasteboard.PasteboardType = .legacyWebArchive) {
        NSPasteboard.general.declareTypes([type], owner: nil)
        NSPasteboard.general.setData(archiveData, forType: type)
    }

    private func msoListMarkupWithoutProperHTMLElement(
        fileID: String = #fileID,
        filePath: String = #filePath,
        line: Int = #line,
        column: Int = #column
    ) throws -> Data? {
        let url = try requireNonNil(
            Bundle.testResources.url(forResource: "mso-list", withExtension: "html"),
            fileID: fileID,
            filePath: filePath,
            line: line,
            column: column
        )
        let markup = try Data(contentsOf: url)
        guard let index = markup.firstIndex(of: UInt8(ascii: ">")) else {
            return nil
        }
        return Data(markup[(index + 1)...])
    }

    @Test
    func exposesHTMLTypeInDataTransfer() async throws {
        let url = try requireNonNil(URL(string: "file:///some-file.html"))
        writeToPasteboard(makeWebArchiveData(html: sampleMarkup, url: url))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) { "html = clipboardData.values[0]" }
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return html.includes('hello')" })
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return html.includes(', world')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return html.includes('secret')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return html.includes('dangerousCode')" })

        try await webView.callJavaScript(userGesture: false) {
            "editor.innerHTML = html; editor.focus(); getSelection().setPosition(editor, 0)"
        }
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return document.queryCommandState('bold')" })
        try await webView.callJavaScript(userGesture: false) { "getSelection().modify('move', 'forward', 'lineboundary')" }
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return document.queryCommandState('bold')" })
        expectEqual(try await webView.callJavaScript(returning: String.self) { "return editor.textContent" }, "hello, world")
    }

    @Test
    func sanitizesHTML() async throws {
        let url = try requireNonNil(URL(string: "file:///some-file.html"))
        writeToPasteboard(makeWebArchiveData(html: sampleMarkup, url: url))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectEqual(try await webView.callJavaScript(returning: String.self) { "return editor.textContent" }, "hello, world")
        try await webView.callJavaScript(userGesture: false) { "editor.focus(); getSelection().setPosition(editor, 0)" }
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return document.queryCommandState('bold')" })
        try await webView.callJavaScript(userGesture: false) { "getSelection().modify('move', 'forward', 'lineboundary')" }
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return document.queryCommandState('bold')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return editor.innerHTML.includes('secret')" })
        expectFalse(try await webView.callJavaScript(returning: Bool.self) { "return editor.innerHTML.includes('dangerousCode')" })
    }

    @Test
    func preservesMSOList() async throws {
        let url = try requireNonNil(URL(string: "file:///some-file.html"))
        let markupURL = try requireNonNil(Bundle.testResources.url(forResource: "mso-list", withExtension: "html"))
        writeToPasteboard(makeWebArchiveData(html: try Data(contentsOf: markupURL), url: url))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return JSON.stringify(clipboardData.types)" },
            "[\"text/html\"]"
        )
        try await webView.callJavaScript(userGesture: false) { "window.htmlInDataTransfer = clipboardData.values[0]" }
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

    @Test
    func preservesMSOListInCompatibilityMode() async throws {
        let url = try requireNonNil(URL(string: "file:///some-file.html"))
        let markupURL = try requireNonNil(Bundle.testResources.url(forResource: "mso-list-compat-mode", withExtension: "html"))
        writeToPasteboard(makeWebArchiveData(html: try Data(contentsOf: markupURL), url: url))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return JSON.stringify(clipboardData.types)" },
            "[\"text/html\"]"
        )
        try await webView.callJavaScript(userGesture: false) { "window.htmlInDataTransfer = clipboardData.values[0]" }
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

    @Test
    func stripsMSOListWhenMissingMSOHTMLElement() async throws {
        let url = try requireNonNil(URL(string: "file:///some-file.html"))
        let markup = try requireNonNil(try msoListMarkupWithoutProperHTMLElement())
        writeToPasteboard(makeWebArchiveData(html: markup, url: url))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return JSON.stringify(clipboardData.types)" },
            "[\"text/html\"]"
        )
        try await webView.callJavaScript(userGesture: false) { "window.htmlInDataTransfer = clipboardData.values[0]" }
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

    @Test
    func preservesPictureInsideSpan() async throws {
        let markupData = Data("<span class='s1'><picture><source srcset='1.png' type='image/png'><img src='2.gif'></picture></span>".utf8)
        let pngData = try Data(contentsOf: try requireNonNil(Bundle.testResources.url(forResource: "icon", withExtension: "png")))
        let gifData = try Data(contentsOf: try requireNonNil(Bundle.testResources.url(forResource: "apple", withExtension: "gif")))
        writeToPasteboard(
            makeWebArchiveData(
                html: markupData,
                url: try requireNonNil(URL(string: "foo.html")),
                subresources: [
                    (pngData, try requireNonNil(URL(string: "1.png")), "image/png"),
                    (gifData, try requireNonNil(URL(string: "2.gif")), "image/gif"),
                ]
            )
        )

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectEqual(try await webView.callJavaScript(returning: String.self) { "return editor.children[0].tagName" }, "SPAN")
        expectEqual(try await webView.callJavaScript(returning: String.self) { "return editor.children[0].children[0].tagName" }, "PICTURE")
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return editor.children[0].children[0].children[0].tagName" },
            "SOURCE"
        )
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return editor.children[0].children[0].children[1].tagName" },
            "IMG"
        )
    }

    @Test
    func webArchiveTypeIdentifier() async throws {
        let url = try requireNonNil(URL(string: "file:///some-file.html"))
        let markup = "<strong style='color: rgb(255, 0, 0);'>This is some text to copy.</strong>"
        writeToPasteboard(
            makeWebArchiveData(html: Data(markup.utf8), url: url),
            type: NSPasteboard.PasteboardType(UTType.webArchive.identifier)
        )

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return JSON.stringify(clipboardData.types)" },
            "[\"text/html\"]"
        )
        // The misspelled `docment` throws, so this script has no effect.
        webView.startEvaluatingJavaScript("docment.body.innerHTML = clipboardData.values[0]")
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return document.querySelector('strong').textContent" },
            "This is some text to copy."
        )
        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return getComputedStyle(document.querySelector('strong')).color" },
            "rgb(255, 0, 0)"
        )
    }

    @Test
    func destinationUsesDarkMode() async throws {
        let url = try requireNonNil(URL(string: "file:///some-file.html"))
        let markup = "<p class='p1'><span class='s1'>Test</span></p>"
        writeToPasteboard(
            makeWebArchiveData(html: Data(markup.utf8), url: url),
            type: NSPasteboard.PasteboardType(UTType.webArchive.identifier)
        )

        let webView = makeWebViewWithCustomPasteboardData()
        webView.forceDarkMode()
        webView._isEditable = true
        try await webView.load(
            html:
                "<html><head><style> :root { color-scheme: light dark; } </style></head><body><br><div id='AppleMailSignature' dir='ltr'>Sent from my iPhone</div></body></html>"
        )

        try await webView.callJavaScript(userGesture: false) { "document.body.focus()" }
        webView.paste(nil)

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return getComputedStyle(document.querySelector('.s1')).color" },
            "rgb(255, 255, 255)"
        )

        webView.forceLightMode()
        await webView.nextPresentationUpdate()

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return getComputedStyle(document.querySelector('.s1')).color" },
            "rgb(0, 0, 0)"
        )
    }

    #if ENABLE_DATA_DETECTION

    @Test
    func stripsDataDetectorsLinks() async throws {
        let url = try requireNonNil(URL(string: "file:///some-file.html"))
        let markup =
            #"<!DOCTYPE html><html><body><span>Meeting <a href="x-apple-data-detectors://0" dir="ltr" x-apple-data-detectors="true" x-apple-data-detectors-type="calendar-event" x-apple-data-detectors-result="0" style="color: currentcolor; text-decoration-color: rgba(128, 128, 128, 0.38); font-weight: bold;">on Friday 11/6 at 4pm</a> at <a href="x-apple-data-detectors://1" dir="ltr" x-apple-data-detectors="true" x-apple-data-detectors-type="address" x-apple-data-detectors-result="1" style="color: currentcolor; text-decoration-color: rgba(128, 128, 128, 0.38);">1 Apple Park Way, Cupertino CA</a></span></body></html>"#
        writeToPasteboard(makeWebArchiveData(html: Data(markup.utf8), url: url))

        let webView = makeWebViewWithCustomPasteboardData()
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectEqual(
            try await webView.callJavaScript(returning: String.self) { "return editor.innerHTML" },
            #"Meeting&nbsp;<span dir="ltr" style="font-weight: bold;">on Friday 11/6 at 4pm</span>&nbsp;at&nbsp;<span dir="ltr">1 Apple Park Way, Cupertino CA</span>"#
        )
    }

    #endif // ENABLE_DATA_DETECTION

    @Test
    func destinationUsesLockdownMode() async throws {
        let archiveURL = try requireNonNil(Bundle.testResources.url(forResource: "opentype_test", withExtension: "webarchive"))
        writeToPasteboard(try Data(contentsOf: archiveURL), type: NSPasteboard.PasteboardType(UTType.webArchive.identifier))

        let webView = makeWebViewWithCustomPasteboardData(lockdownModeEnabled: true)
        try await webView.load(testPageNamed: "paste-rtfd")
        webView.paste(nil)

        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return clipboardData.types.includes('text/html')" })
        try await webView.callJavaScript(userGesture: false) { "html = clipboardData.values[0]" }
        expectTrue(try await webView.callJavaScript(returning: Bool.self) { "return html.includes('test case')" })

        try await webView.callJavaScript(userGesture: false) {
            "editor.innerHTML = html; editor.focus(); getSelection().setPosition(editor, 0)"
        }
        try await webView.callJavaScript(userGesture: false) { "getSelection().modify('move', 'forward', 'lineboundary')" }
        expectEqual(try await webView.callJavaScript(returning: String.self) { "return editor.textContent" }, "test case")
    }
}

#endif // WTF_PLATFORM_MAC
