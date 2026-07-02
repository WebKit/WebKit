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

import wtf

extension WTF.String: LosslessStringConvertible {
    // Protocol requirement; no documentation needed.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public init(_ string: Swift.String) {
        let ns = string as NSString
        let cf: CFString = ns as CFString
        self = WTF.String(cf)
    }

    // Protocol requirement; no documentation needed.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var description: Swift.String {
        unsafe createNSString().get()
    }
}

extension WTF.String: ExpressibleByStringLiteral {
    // Protocol requirement; no documentation needed.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public init(stringLiteral: Swift.String) {
        self.init(stringLiteral)
    }
}

extension WTF.String: ExpressibleByNilLiteral {
    // Protocol requirement; no documentation needed.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public init(nilLiteral: ()) {
        self = unsafe WTF.nullString().pointee
    }
}

#endif // ENABLE_CXX_INTEROP
