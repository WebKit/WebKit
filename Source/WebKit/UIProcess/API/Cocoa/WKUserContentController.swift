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

#if compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

import Foundation
import WebKit_Internal

@available(anyAppleOSAndDownlevels 27.0, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
extension WKUserContentController {
    /// Adds a data buffer that will be available to JavaScript through the `window.webkit.buffers` object.
    ///
    /// - Parameters:
    ///   - buffer: The buffer to add.
    ///   - name: The name of the buffer to be referenced from JavaScript.
    ///     e.g. with a `name` parameter of `"mybuffer"`, JavaScript can reference the buffer via `window.webkit.buffers.mybuffer`.
    ///   - contentWorld: The `WKContentWorld` to add the buffer to.
    ///     The buffer will only be visible to JavaScript executing in that content world.
    ///
    public func addBuffer(_ buffer: RawSpan, name: Swift.String, to contentWorld: WKContentWorld) {
        // Safety: This is safe because it's just the pre-API version of the safe `Span(viewing:)` API in Swift 6.4.
        // FIXME: (rdar://181879532) Adopt `Span(viewing:)` initializer instead of `Span(_bytes:)` in `WKUserContentController/addBuffer` implementation.
        let typedSpan = unsafe Span<UInt8>(_bytes: buffer)

        // This use of unsafe is necessary to wrap the RawSpan for immediate processing by WebKit,
        // unknowledging that the safety of the RawSpan passed in by the client cannot be guaranteed.
        // This is fine becuase WebKit is going to immediately make a copy of the passed-in bytes
        // into a safely managed object.
        // rdar://181746505
        unsafe _addDataSpan(.init(typedSpan), name: name, contentWorld: contentWorld)
    }

    /// Removes a previously added data buffer from the given `WKContentWorld`.
    ///
    /// - Parameters:
    ///   - name: The name of the buffer to remove.
    ///   - contentWorld: The `WKContentWorld` from which to remove the buffer.
    ///
    public func removeBuffer(named name: Swift.String, from contentWorld: WKContentWorld) {
        __removeBuffer(withName: name, contentWorld: contentWorld)
    }
}

#endif // compiler(>=6.4)
