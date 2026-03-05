//
// Copyright (C) 2021-2023 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if ENABLE_SWIFT_TEST_CONDITION
internal import WebKit_Internal
#endif

#if compiler(>=6.2)

#if ENABLE_SWIFT_TEST_CONDITION
final class TestWithSwiftConditionallyWeakRef {
    private weak var target: TestWithSwiftConditionally?
    init(target: TestWithSwiftConditionally) {
        self.target = target
    }

    func getMessageTarget() -> TestWithSwiftConditionally? {
        target
    }
}

extension WebKit.TestWithSwiftConditionallyMessageForwarder {
    static func create(target: TestWithSwiftConditionally) -> RefTestWithSwiftConditionallyMessageForwarder {
        let weakRefContainer = TestWithSwiftConditionallyWeakRef(target: target)
        // Safety: we're creating a pointer which will immediately be stored in a
        // proper ref-counted reference on the C++ side before this call returns.
        // Workaround for rdar://163107752.
        return unsafe WebKit.TestWithSwiftConditionallyMessageForwarder.createFromWeak(
            OpaquePointer(
                Unmanaged.passRetained(weakRefContainer).toOpaque()
            )
        )
    }
}
#endif

#else

#if ENABLE_SWIFT_TEST_CONDITION
final class TestWithSwiftConditionallyWeakRef {
    private weak var target: TestWithSwiftConditionally?
    init(target: TestWithSwiftConditionally) {
        self.target = target
    }

    func getMessageTarget() -> TestWithSwiftConditionally? {
        target
    }
}

extension WebKit.TestWithSwiftConditionallyMessageForwarder {
    static func create(target: TestWithSwiftConditionally) -> RefTestWithSwiftConditionallyMessageForwarder {
        let weakRefContainer = TestWithSwiftConditionallyWeakRef(target: target)
        // Safety: we're creating a pointer which will immediately be stored in a
        // proper ref-counted reference on the C++ side before this call returns.
        // Workaround for rdar://163107752.
        return WebKit.TestWithSwiftConditionallyMessageForwarder.createFromWeak(
            OpaquePointer(
                Unmanaged.passRetained(weakRefContainer).toOpaque()
            )
        )
    }
}
#endif

#endif
