// Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#if canImport(WritingTools)

import Foundation

class IntelligenceTextEffectChunk: PlatformIntelligenceTextEffectChunk {
    class Pondering: IntelligenceTextEffectChunk {
        override init(range: Range<Int>) {
            super.init(range: range)
        }
    }

    class Replacement: IntelligenceTextEffectChunk {
        let finished: Bool
        let characterDelta: Int
        let replacement: (() async -> Void)

        init(range: Range<Int>, finished: Bool, characterDelta: Int, replacement: @escaping (() async -> Void)) {
            self.finished = finished
            self.replacement = replacement
            self.characterDelta = characterDelta
            super.init(range: range)
        }
    }

    let id = UUID()

    var range: Range<Int>

    private init(range: Range<Int>) {
        self.range = range
    }
}

// MARK: WKIntelligenceTextEffectCoordinator.Chunk + Hashable & Equatable

extension IntelligenceTextEffectChunk: Hashable, Equatable {
    static func == (lhs: IntelligenceTextEffectChunk, rhs: IntelligenceTextEffectChunk) -> Bool {
        lhs.id == rhs.id
    }

    func hash(into hasher: inout Hasher) {
        self.id.hash(into: &hasher)
    }
}

#endif
