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


enum ModelAssetPolicy {
    private static let glbMagic: UInt32 = 0x4654_6C67
    private static let glbJSONChunkType: UInt32 = 0x4E4F_534A
    private static let glbJSONChunkStart = 20
    private static let maximumDocumentDepth = 32

    static func referencesExternalAssets(modelData data: Data) -> Bool {
        let json: Data
        if littleEndianUInt32(data, at: 0) == glbMagic {
            guard let chunk = glbJSONChunk(data) else {
                return true
            }
            json = chunk
        } else if looksLikeJSONObject(data) {
            json = data
        } else {
            return false
        }

        guard let document = try? JSONSerialization.jsonObject(with: json) else {
            return true
        }

        return referencesExternalAsset(document, depth: 0)
    }

    private static func littleEndianUInt32(_ data: Data, at offset: Int) -> UInt32? {
        guard offset >= 0, data.count >= 4, offset <= data.count - 4 else {
            return nil
        }

        let base = data.startIndex + offset
        return UInt32(data[base])
            | UInt32(data[base + 1]) << 8
            | UInt32(data[base + 2]) << 16
            | UInt32(data[base + 3]) << 24
    }

    private static func glbJSONChunk(_ data: Data) -> Data? {
        guard littleEndianUInt32(data, at: 16) == glbJSONChunkType,
            let chunkLength = littleEndianUInt32(data, at: 12),
            Int(chunkLength) <= data.count - glbJSONChunkStart
        else {
            return nil
        }

        let base = data.startIndex + glbJSONChunkStart
        return data[base..<(base + Int(chunkLength))]
    }

    private static func looksLikeJSONObject(_ data: Data) -> Bool {
        var index = data.startIndex
        if data.count >= 3, data[index] == 0xEF, data[index + 1] == 0xBB, data[index + 2] == 0xBF {
            index += 3
        }

        while index < data.endIndex {
            switch data[index] {
            case 0x09, 0x0A, 0x0D, 0x20:
                index += 1
            case UInt8(ascii: "{"):
                return true
            default:
                return false
            }
        }

        return false
    }

    private static func referencesExternalAsset(_ value: Any, depth: Int) -> Bool {
        guard depth < maximumDocumentDepth else {
            return true
        }

        if let object = value as? [String: Any] {
            for (key, member) in object {
                if key == "uri", let uri = member as? String, !uri.lowercased().hasPrefix("data:") {
                    return true
                }
                if referencesExternalAsset(member, depth: depth + 1) {
                    return true
                }
            }
            return false
        }

        if let array = value as? [Any] {
            return array.contains { referencesExternalAsset($0, depth: depth + 1) }
        }

        return false
    }
}

