/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

import XCTest
import WebKit

class WebKitTests: XCTestCase {
    /// This is a compile-time test that ensures the function names are what we expect.
    func testAPI() {
        _ = WKContentWorld.world(name:)
        _ = WKWebView.callAsyncJavaScript(_:arguments:in:completionHandler:)
        _ = WKWebView.createPDF(configuration:completionHandler:)
        _ = WKWebView.createWebArchiveData(completionHandler:)
        _ = WKWebView.evaluateJavaScript(_:in:completionHandler:)
        _ = WKWebView.find(_:configuration:completionHandler:)
    }

    func testWKPDFConfigurationRect() {
        let configuration = WKPDFConfiguration()

        XCTAssert(type(of: configuration.rect) == Optional<CGRect>.self)

        configuration.rect = nil
        XCTAssertEqual(configuration.rect, nil)

        configuration.rect = .null
        XCTAssertEqual(configuration.rect, nil)

        configuration.rect = CGRect.zero
        XCTAssertEqual(configuration.rect, .zero)

        let originalPhoneBounds = CGRect(x: 0, y: 0, width: 320, height: 480)
        configuration.rect = originalPhoneBounds
        XCTAssertEqual(configuration.rect, originalPhoneBounds)
    }
}
