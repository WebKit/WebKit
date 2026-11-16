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

#if compiler(>=6.4) && HAVE_NEW_CODABLE

import WebKit_Internal

extension WebKit.RunJavaScriptParameters {
    struct JavaScriptArgument: ~Copyable {
        let key: String
        let value: WebKit.JavaScriptEvaluationResult
    }

    init(
        source: IPC.TransferString,
        taintedness: JSC.SourceTaintedOrigin,
        sourceURL: WTF.URL,
        runAsAsyncFunction: Bool,
        arguments: consuming UniqueArray<JavaScriptArgument>?,
        forceUserGesture: Bool,
        removeTransientActivation: Bool,
    ) {
        let argumentsBox = unsafe Self.makeCxxArguments(consume arguments)

        unsafe self.init(
            source: source,
            taintedness: taintedness,
            sourceURL: sourceURL,
            runAsAsyncFunction: runAsAsyncFunction ? .Yes : .No,
            arguments: argumentsBox,
            forceUserGesture: forceUserGesture ? .Yes : .No,
            removeTransientActivation: removeTransientActivation ? .Yes : .No
        )
    }
}

extension WebKit.RunJavaScriptParameters {
    fileprivate typealias Cxx = WebKit.CxxInteropSupport

    fileprivate static func makeCxxArguments(
        _ arguments: consuming UniqueArray<JavaScriptArgument>?
    ) -> Cxx.OptionalRunJavaScriptParametersArguments {
        guard var arguments else {
            return unsafe .init()
        }

        var vector = unsafe Cxx.RunJavaScriptParametersArguments()
        unsafe vector.reserveInitialCapacity(arguments.count)

        while let argument = arguments.popLast() {
            unsafe vector.append(consuming: .init(first: WTF.String(argument.key), second: argument.value))
        }

        unsafe vector.reverse()

        return unsafe Cxx.makeOptionalArguments(consuming: vector)
    }
}

#endif // compiler(>=6.4) && HAVE_NEW_CODABLE
