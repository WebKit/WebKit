// Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

private import CxxStdlib
private import wtf.Core.EscapableByteSpan
private import wtf.Core.text.CString
private import wtf.Core.text.StringView
public import wtf.Core.text.WTFString

extension WTF.String: LosslessStringConvertible {
    /// Construct a `WTF.String` from a `Swift.String`.
    ///
    /// - Parameter string: The string to copy.
    public init(_ string: Swift.String) {
        var string = string
        // rdar://167712240 prevents us from passing a Span to WTF.String.fromUTF8.
        // Safety: the buffer is valid for the duration of the closure, and the WTF::String takes a copy of it.
        #if compiler(>=6.4)
        self = string.withUTF8 { buffer in
            unsafe WTF.String.fromUTF8(WTF.SpanUInt8(buffer))
        }
        #else
        self = unsafe string.withUTF8 { buffer in
            unsafe WTF.String.fromUTF8(WTF.SpanUInt8(buffer))
        }
        #endif
    }

    /// Return a `Swift.String` from this `WTF.String`.
    public var description: Swift.String {
        // We could possibly make this quicker by treating a C++ span as
        // a Sequence. For now, we want to avoid unsafe as much as possible.
        Swift.String(WTF.makeUTF8CString(self).toStdString())
    }
}

extension WTF.String: ExpressibleByStringLiteral {
    /// Construct a `WTF.String` from a string literal.
    ///
    /// - Parameter stringLiteral: The string literal to copy.
    public init(stringLiteral: Swift.String) {
        self.init(stringLiteral)
    }
}
