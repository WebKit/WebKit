/*
* Copyright (C) 2024 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#if swift(>=5.9)

import Foundation
import CryptoKit

import PALSwift

enum UnsafeErrors: Error {
    case invalidLength
    case emptySpan
}

extension HashFunction {
    mutating func update(data: SpanConstUInt8) {
        if data.empty() {
            self.update(data: Data.empty())
        } else {
            self.update(
                bufferPointer: UnsafeRawBufferPointer(
                    start: data.__dataUnsafe(), count: data.size()))
        }
    }
}

extension ContiguousBytes {
    public func copyToVectorUInt8() -> VectorUInt8 {
        return self.withUnsafeBytes { buf in
            let result = VectorUInt8(buf.count)
            buf.copyBytes(
                to: UnsafeMutableRawBufferPointer(
                    start: UnsafeMutableRawPointer(mutating: result.__dataUnsafe()),
                    count: result.size()), count: result.size())
            return result
        }
    }
}

extension Data {
    static let emptyData = Data()
    fileprivate static func temporaryDataFromSpan(spanNoCopy: SpanConstUInt8) -> Data {
        if spanNoCopy.empty() {
            return Data.empty()
        } else {
            return Data(
                bytesNoCopy: UnsafeMutablePointer(mutating: spanNoCopy.__dataUnsafe()),
                count: spanNoCopy.size(), deallocator: .none)
        }
    }

    // CryptoKit does not support a null pointer with zero length. We instead need to pass an empty Data. This class provides that.
    public static func empty() -> Data {
        return emptyData
    }
}

private class _WorkAroundRadar116406681 {
    // rdar://116406681
    private func forceLinkageForVectorDestructor() {
        let _ = VectorUInt8()
    }
}

extension AES.GCM {
    public static func seal(
        _ message: SpanConstUInt8, key: SpanConstUInt8, iv: SpanConstUInt8, ad: SpanConstUInt8
    ) throws -> AES.GCM.SealedBox {
        if ad.size() > 0 {
            return try AES.GCM.seal(
                Data.temporaryDataFromSpan(spanNoCopy: message),
                using: SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: key)),
                nonce: AES.GCM.Nonce(data: Data.temporaryDataFromSpan(spanNoCopy: iv)),
                authenticating: Data.temporaryDataFromSpan(spanNoCopy: ad))
        } else {
            return try AES.GCM.seal(
                Data.temporaryDataFromSpan(spanNoCopy: message),
                using: SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: key)),
                nonce: AES.GCM.Nonce(data: Data.temporaryDataFromSpan(spanNoCopy: iv))
            )
        }
    }
}

extension AES.KeyWrap {
    public static func unwrap(_ wrapped: SpanConstUInt8, using: SpanConstUInt8) throws
        -> SymmetricKey
    {
        return try AES.KeyWrap.unwrap(
            Data.temporaryDataFromSpan(spanNoCopy: wrapped),
            using: SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: using)))

    }
    public static func wrap(_ keyToWrap: SpanConstUInt8, using: SpanConstUInt8) throws
        -> VectorUInt8
    {
        return try AES.KeyWrap.wrap(
            SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: keyToWrap)),
            using: SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: using))
        ).copyToVectorUInt8()
    }
}
#endif
