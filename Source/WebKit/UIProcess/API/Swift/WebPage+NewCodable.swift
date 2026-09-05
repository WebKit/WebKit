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

#if ENABLE_SWIFTUI && HAVE_NEW_CODABLE

import WebKit_Internal
@_spiOnly public import NewCodable

extension WebPage {
    /// Experimental, do not use.
    ///
    /// - Parameters:
    ///   - outputType: The type to return.
    ///   - script: The script to evaluate.
    /// - Returns: The evaluated decoded value.
    /// - Throws: An error if evaluation fails.
    @_spi(Experimental_NewCodable)
    public func callJavaScriptv0<Output: CommonDecodable>(
        returning outputType: Output.Type,
        _ script: () -> Swift.String
    ) async throws -> Output {
        guard let page = backingWebView._protectedPage().get() else {
            fatalError()
        }

        guard let transferString = Optional(fromCxx: IPC.TransferString.create(script())) else {
            fatalError()
        }

        let parameters = unsafe WebKit.RunJavaScriptParameters(
            source: transferString,
            taintedness: .Untainted,
            sourceURL: .init(),
            runAsAsyncFunction: true,
            arguments: .init(),
            forceUserGesture: true,
            removeTransientActivation: false,
        )

        let result = unsafe try await page.runJavaScriptInMainFrame(parameters: parameters, wantsResult: true)

        let decoder = JavaScriptEvaluationResultDecoder()
        return try decoder.decode(Output.self, from: result)
    }
}

#endif // ENABLE_SWIFTUI && HAVE_NEW_CODABLE
