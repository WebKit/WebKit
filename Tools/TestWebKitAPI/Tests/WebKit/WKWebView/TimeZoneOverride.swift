// Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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

#if WTF_PLATFORM_COCOA

import CoreGraphics
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import WebKit
private import WebKit_Private._WKProcessPoolConfiguration

import struct Swift.String

@MainActor
struct TimeZoneOverrideTests {
    private let webView: TestWKWebView

    init() {
        let processPoolConfiguration = _WKProcessPoolConfiguration()
        processPoolConfiguration.timeZoneOverride = "Europe/Berlin"

        webView = TestWKWebView(
            frame: CGRect(x: 0, y: 0, width: 300, height: 300),
            configuration: WKWebViewConfiguration(),
            processPoolConfiguration: processPoolConfiguration
        )
    }

    @Test
    func timeZoneOverride() async throws {
        let offset = try await webView.callJavaScript(returning: Int.self) {
            "let now = new Date(1651511226050); return now.getTimezoneOffset()"
        }

        #expect(offset == -120)
    }

    @Test
    func timeZoneOverrideInWorkers() async throws {
        let timeZones = try await webView.callJavaScript(returning: String.self) {
            """
            return new Promise(fulfill => {
              const results = [Intl.DateTimeFormat().resolvedOptions().timeZone];
              for (let i = 0; i < 3; i++) {
                const worker = new Worker('data:text/javascript,self.postMessage(Intl.DateTimeFormat().resolvedOptions().timeZone)');
                worker.onmessage = message => {
                  results.push(message.data);
                  if (results.length === 4)
                    fulfill(results.join(', '));
                };
              }
            });
            """
        }

        #expect(timeZones == "Europe/Berlin, Europe/Berlin, Europe/Berlin, Europe/Berlin")
    }
}

#endif // WTF_PLATFORM_COCOA
