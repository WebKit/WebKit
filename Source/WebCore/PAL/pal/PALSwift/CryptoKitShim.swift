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

import CryptoKit
import Foundation

import PALSwift

public typealias VectorUInt8 = Cpp.VectorUInt8
public typealias SpanConstUInt8 = Cpp.SpanConstUInt8
public typealias OptionalVectorUInt8 = Cpp.OptionalVectorUInt8

public enum ErrorCodes: Int {
    case success = 0
    case wrongTagSize = 1
    case encryptionFailed = 2
    case encryptionResultNil = 3
    case invalidArgument = 4
    case tooBigArguments = 5
    case decryptionFailed = 6
    case hashingFailed = 7
}

private class Utils {
    static let zeroArray = [UInt8](repeating: 0, count: 0)
}
public struct AesGcmRV {
    public var cipherText: OptionalVectorUInt8 = OptionalVectorUInt8()
    public var errorCode: ErrorCodes = .success
}

public class AesGcm {
    public static func encrypt(
        key: SpanConstUInt8, iv: SpanConstUInt8, ad: SpanConstUInt8, message: SpanConstUInt8,
        desiredTagLengthInBytes: Int
    ) -> AesGcmRV {
        var rv = AesGcmRV()
        do {
            if iv.size() == 0 {
                rv.errorCode = .invalidArgument
                return rv
            }
            let sealedBox: AES.GCM.SealedBox = try AES.GCM.seal(message, key: key, iv: iv, ad: ad)
            if desiredTagLengthInBytes > sealedBox.tag.count {
                rv.errorCode = .invalidArgument
                return rv
            }
            var result = sealedBox.ciphertext
            result.append(
                sealedBox.tag[
                    sealedBox.tag.startIndex..<(sealedBox.tag.startIndex + desiredTagLengthInBytes)]
            )
            rv.errorCode = .success
            rv.cipherText = Cpp.makeOptional(result.copyToVectorUInt8())
            return rv
        } catch {
            rv.errorCode = .encryptionFailed
        }
        return rv
    }
}

public struct AesKwRV {
    public var errCode: ErrorCodes = ErrorCodes.success
    public var result: OptionalVectorUInt8 = OptionalVectorUInt8()
}

public class AesKw {
    public static func wrap(keyToWrap: SpanConstUInt8, using: SpanConstUInt8) -> AesKwRV {
        var rv = AesKwRV()
        do {
            let result = try AES.KeyWrap.wrap(keyToWrap, using: using)
            rv.errCode = .success
            rv.result = Cpp.makeOptional(
                result)
        } catch {
            rv.errCode = .encryptionFailed
        }
        return rv
    }

    public static func unwrap(wrappedKey: SpanConstUInt8, using: SpanConstUInt8) -> AesKwRV {
        var rv = AesKwRV()
        do {
            let result = try AES.KeyWrap.unwrap(
                wrappedKey, using: using)
            rv.errCode = .success
            rv.result = Cpp.makeOptional(
                result.copyToVectorUInt8())
        } catch {
            rv.errCode = .encryptionFailed
        }
        return rv
    }

}  // AesKw

public class Digest {
    public static func sha1(_ data: SpanConstUInt8) -> VectorUInt8 {
        return digest(data, Insecure.SHA1.self)
    }
    public static func sha256(_ data: SpanConstUInt8) -> VectorUInt8 {
        return digest(data, SHA256.self)
    }
    public static func sha384(_ data: SpanConstUInt8) -> VectorUInt8 {
        return digest(data, SHA384.self)
    }
    public static func sha512(_ data: SpanConstUInt8) -> VectorUInt8 {
        return digest(data, SHA512.self)
    }
    private static func digest<T: HashFunction>(_ data: SpanConstUInt8, _: T.Type)
        -> VectorUInt8
    {
        var hasher = T()
        hasher.update(data: data)
        return hasher.finalize().copyToVectorUInt8()
    }
}

#endif
