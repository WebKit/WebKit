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

@_exported import WebKit

@available(iOS 14.0, macOS 10.16, *)
extension WKPDFConfiguration {
    public var rect: CGRect? {
        get { __rect == .null ? nil : __rect }
        set { __rect = newValue == nil ? .null : newValue! }
    }
}

@available(iOS 14.0, macOS 10.16, *)
extension WKWebView {
    public func callAsyncJavaScript(_ functionBody: String, arguments: [String:Any] = [:], in frame: WKFrameInfo? = nil, in contentWorld: WKContentWorld, completionHandler: ((Result<Any, Error>) -> Void)? = nil) {
        __callAsyncJavaScript(functionBody, arguments: arguments, inFrame: frame, in: contentWorld, completionHandler: completionHandler.map(ObjCBlockConversion.boxingNilAsAnyForCompatibility))
    }

    public func createPDF(configuration: WKPDFConfiguration = .init(), completionHandler: @escaping (Result<Data, Error>) -> Void) {
        __createPDF(with: configuration, completionHandler: ObjCBlockConversion.exclusive(completionHandler))
    }

    public func createWebArchiveData(completionHandler: @escaping (Result<Data, Error>) -> Void) {
        __createWebArchiveData(completionHandler: ObjCBlockConversion.exclusive(completionHandler))
    }

    public func evaluateJavaScript(_ javaScript: String, in frame: WKFrameInfo? = nil, in contentWorld: WKContentWorld, completionHandler: ((Result<Any, Error>) -> Void)? = nil) {
        __evaluateJavaScript(javaScript, inFrame: frame, in: contentWorld, completionHandler: completionHandler.map(ObjCBlockConversion.boxingNilAsAnyForCompatibility))
    }

    public func find(_ string: String, configuration: WKFindConfiguration = .init(), completionHandler: @escaping (WKFindResult) -> Void) {
        __find(string, with: configuration, completionHandler: completionHandler)
    }
}
