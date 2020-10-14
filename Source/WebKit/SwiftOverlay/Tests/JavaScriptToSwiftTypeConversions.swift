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

final class JavaScriptToSwiftConversions : XCTestCase {
    #if os(macOS)
    let window = NSWindow()
    #elseif os(iOS)
    let window = UIWindow()
    #endif

    let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 320, height: 480))

    override func setUp() {
        #if os(macOS)
        window.contentView?.addSubview(webView)
        #else
        window.isHidden = false
        window.addSubview(webView)
        #endif

        webView.load(URLRequest(url: URL(string: "about:blank")!))
    }

    override func tearDown() {
        #if os(macOS)
        window.orderOut(nil)
        #else
        window.isHidden = true
        #endif
    }

    func evaluateJavaScript<T : Equatable>(_ javaScript: String, andExpect expectedValue: T) {
        let evaluationExpectation = self.expectation(description: "Evaluation of \(javaScript.debugDescription)")
        webView.evaluateJavaScript(javaScript, in: nil, in: .defaultClient) { result in
            do {
                let actualValue = try result.get() as? T
                XCTAssertEqual(actualValue, expectedValue)
                evaluationExpectation.fulfill()
            } catch {
                XCTFail("Evaluating \(javaScript.debugDescription) failed with error: \(error)")
            }
        }

        wait(for: [evaluationExpectation], timeout: 30)
    }

    func testNull() {
        evaluateJavaScript("null", andExpect: NSNull())
    }

    func testInteger() {
        evaluateJavaScript("12", andExpect: 12 as Int)
    }

    func testDecimal() {
        evaluateJavaScript("12.0", andExpect: 12 as Float)
    }

    func testBoolean() {
        evaluateJavaScript("true", andExpect: true)
        evaluateJavaScript("false", andExpect: false)
    }

    func testString() {
        evaluateJavaScript(#""Hello, world!""#, andExpect: "Hello, world!")
    }

    func testArray() {
        // This uses [AnyHashable], instead of [Any], so we can perform an equality check for testing.
        evaluateJavaScript(#"[ 1, 2, "cat" ]"#, andExpect: [1, 2, "cat"] as [AnyHashable])
    }

    func testDictionary() {
        // This uses [AnyHashable:AnyHashable], instead of [AnyHashable:Any], so we can perform an
        // equality check for testing. An object’s keys are always converted to strings, so even
        // though we input `1:` we expect `"1":` back.
        let result: [AnyHashable:AnyHashable] = ["1": 2, "cat": "dog"]
        evaluateJavaScript(#"const value = { 1: 2, "cat": "dog" }; value"#, andExpect: result)
    }

    func testUndefined() {
        let evaluationExpectation = self.expectation(description: "Evaluation of \"undefined\" using deprecated API")
        webView.evaluateJavaScript("undefined", in: nil, in: .defaultClient) { (result: Result<Any, Error>) in
            do {
                let value = try result.get()
                if let optionalValue = value as? Any?, optionalValue == nil {
                    evaluationExpectation.fulfill()
                } else {
                    XCTFail("Value did not contain nil")
                }
            } catch {
                XCTFail("Evaluating \"undefined\" failed with error: \(error)")
            }
        }

        wait(for: [evaluationExpectation], timeout: 30)
    }
}
