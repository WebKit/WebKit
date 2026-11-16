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

import Foundation

// FIXME: (rdar://184145864) Remove this file once Spatial is available everywhere.

struct Angle2D: Sendable, Codable, Hashable {
    var radians: Double

    var degrees: Double {
        radians * 180.0 / .pi
    }

    static func degrees(_ degrees: Double) -> Angle2D {
        .init(radians: degrees / 180.0 * .pi)
    }

    static func radians(_ radians: Double) -> Angle2D {
        .init(radians: radians)
    }

    static func atan(_ x: Double) -> Angle2D {
        .init(radians: Foundation.atan(x))
    }

    fileprivate init(radians: Double) {
        self.radians = radians
    }
}

extension Angle2D: AdditiveArithmetic {
    static var zero: Angle2D {
        .init(radians: 0)
    }

    static prefix func + (_ other: Angle2D) -> Angle2D {
        other
    }

    static func + (_ lhs: Angle2D, _ rhs: Angle2D) -> Angle2D {
        .init(radians: lhs.radians + rhs.radians)
    }

    static func += (_ lhs: inout Angle2D, _ rhs: Angle2D) {
        lhs.radians += rhs.radians
    }

    static prefix func - (_ other: Angle2D) -> Angle2D {
        .init(radians: -other.radians)
    }

    static func - (_ lhs: Angle2D, _ rhs: Angle2D) -> Angle2D {
        .init(radians: lhs.radians - rhs.radians)
    }

    static func -= (_ lhs: inout Angle2D, _ rhs: Angle2D) {
        lhs.radians -= rhs.radians
    }
}

extension Angle2D: Comparable {
    static func < (lhs: Angle2D, rhs: Angle2D) -> Bool {
        lhs.radians < rhs.radians
    }
}
