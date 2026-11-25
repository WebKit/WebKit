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

// This is more-or-less duplicate of WebKit/StdLibExtras.swift. Remove this file once that is in WTF.

#if ENABLE_CXX_INTEROP

import CxxStdlib
import wtf

// WTF::StringImpl is a Swift shared reference type; get one from a RefPtr<StringImpl> with `.get()`.
extension WTF.StringImpl {
    /// Creates a `StringImpl` with the contents of `string`.
    ///
    /// Only `nil` if allocation fails; the optional mirrors C++ `RefPtr<StringImpl>`, and passes straight to
    /// `StringImpl*` parameters.
    static func create(from string: Swift.String) -> WTF.StringImpl? {
        // An empty Swift string may have no storage, and fromUTF8() returns null for a null pointer.
        if string.isEmpty {
            return WTF.StringImpl.empty()
        }
        var string = string
        return string.withUTF8 { characters in
            // Safety: `characters` is valid for the duration of the closure, and fromUTF8() copies it.
            // swift-format-ignore: NeverForceUnwrap
            unsafe WTF.StringImpl.fromUTF8(WTF.SpanUInt8(characters.baseAddress!, characters.count)).get()
        }
    }
}

extension Swift.String {
    /// Creates a `Swift.String` from a `StringImpl`; `nil` (a null WTF string) becomes the empty string.
    init(_ string: WTF.StringImpl?) {
        self.init(WTF.utf8StdString(string))
    }
}

#endif // ENABLE_CXX_INTEROP
