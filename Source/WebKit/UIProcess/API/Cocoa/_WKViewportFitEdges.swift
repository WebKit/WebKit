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

#if WTF_PLATFORM_IOS_FAMILY

import Foundation
import WebKit_Internal

@objc
@implementation
extension _WKViewportFitEdges {
    let top: _WKViewportFitEdge
    let left: _WKViewportFitEdge
    let bottom: _WKViewportFitEdge
    let right: _WKViewportFitEdge

    init(
        top: _WKViewportFitEdge,
        left: _WKViewportFitEdge,
        bottom: _WKViewportFitEdge,
        right: _WKViewportFitEdge
    ) {
        self.top = top
        self.left = left
        self.bottom = bottom
        self.right = right
    }

    @_implementationOnly
    public override var hash: Int {
        var hasher = Swift.Hasher()
        hasher.combine(top)
        hasher.combine(left)
        hasher.combine(bottom)
        hasher.combine(right)
        return hasher.finalize()
    }

    @_implementationOnly
    public override func isEqual(_ object: Any?) -> Bool {
        guard let other = object as? _WKViewportFitEdges else {
            return false
        }
        return top == other.top && left == other.left && bottom == other.bottom && right == other.right
    }
}

#endif // WTF_PLATFORM_IOS_FAMILY
