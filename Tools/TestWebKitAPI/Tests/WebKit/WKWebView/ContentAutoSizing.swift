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

#if WTF_PLATFORM_MAC

private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate

@MainActor
struct ContentAutoSizingTests {
    @Test
    func sizeToContentAutoSizeMaximumSizeGetterReturnsSetValue() {
        let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 1000, height: 1000))

        #expect(webView._sizeToContentAutoSizeMaximumSize == .zero)

        let maximumSize = CGSize(width: 400, height: 300)
        webView._sizeToContentAutoSizeMaximumSize = maximumSize
        #expect(webView._sizeToContentAutoSizeMaximumSize == maximumSize)

        webView._minimumLayoutWidth = 150
        #expect(webView._sizeToContentAutoSizeMaximumSize == maximumSize)
        #expect(webView._minimumLayoutWidth == 150)
    }
}

#endif // WTF_PLATFORM_MAC
