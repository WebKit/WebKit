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

#if compiler(>=6.2.3)

#if WTF_PLATFORM_COCOA && !HAVE_SWIFT_STDLIB_6_2
#if USE_APPLE_INTERNAL_SDK
@_spi(MainActorUtilities) import _Concurrency
#else
import _Concurrency_SPI
#endif
#endif

extension Task where Failure == Never {
    /// `Task.immediate` for a main-actor operation: it runs inline until its first suspension, so that
    /// generated dispatch handles a message in order with C++-handled messages. `Task.immediate` needs
    /// the OS 26 concurrency runtime; on older OSes the `startOnMainActor` SPI does the same thing.
    @MainActor
    @discardableResult
    static func immediateOnMainActor(_ operation: sending @escaping @MainActor () async -> Success) -> Task<Success, Never> {
        #if HAVE_SWIFT_STDLIB_6_2 || !WTF_PLATFORM_COCOA
        Task.immediate(operation: operation)
        #else
        startOnMainActor(operation)
        #endif
    }
}

#endif // compiler(>=6.2.3)
