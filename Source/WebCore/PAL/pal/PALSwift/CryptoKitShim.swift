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

public class AesGcm {
    public static func encrypt(
        _ key: UnsafePointer<UInt8>,
        keySize: UInt,
        iv: UnsafePointer<UInt8>,
        ivSize: UInt,
        additionalData: UnsafePointer<UInt8>,
        additionalDataSize: UInt,
        plainText: UnsafePointer<UInt8>,
        plainTextSize: UInt,
        cipherText: UnsafeMutablePointer<UInt8>
    ) -> ErrorCodes {
        do {
            if keySize > Int.max
                || ivSize > Int.max
                || plainTextSize > Int.max
                || additionalDataSize > Int.max
            {
                return .tooBigArguments
            }
            if ivSize == 0 {
                return .invalidArgument
            }
            let nonce = try AES.GCM.Nonce(
                data: UnsafeBufferPointer(start: iv, count: Int(ivSize)))
            var message = UnsafeBufferPointer(start: plainText, count: Int(plainTextSize))
            if plainTextSize == 0 {
                Utils.zeroArray.withUnsafeBufferPointer { ptr in
                    message = ptr
                }
            }
            let symKey = SymmetricKey(
                data: UnsafeBufferPointer(start: key, count: Int(keySize)))
            let sealedBox: AES.GCM.SealedBox
            if additionalDataSize > 0 {
                sealedBox = try AES.GCM.seal(
                    message,
                    using: symKey,
                    nonce: nonce,
                    authenticating: UnsafeBufferPointer(
                        start: additionalData, count: Int(additionalDataSize))
                )
            } else {
                sealedBox = try AES.GCM.seal(
                    message,
                    using: symKey,
                    nonce: nonce
                )
            }
            sealedBox.ciphertext.copyBytes(to: cipherText, count: Int(plainTextSize))
            sealedBox.tag.copyBytes(
                to: cipherText + Int(plainTextSize), count: sealedBox.tag.count)
        } catch {
            return .encryptionFailed
        }
        return .success
    }
}

public struct AesKwRV {
    public var errCode: ErrorCodes = ErrorCodes.success
    public var outputSize: UInt64 = 0
}

public class AesKw {
    public static func wrap(
        _ key: UnsafePointer<UInt8>,
        keySize: UInt,
        data: UnsafePointer<UInt8>,
        dataSize: UInt,
        cipherText: UnsafeMutablePointer<UInt8>,
        cipherTextSize: UInt64
    ) -> AesKwRV {
        var rv = AesKwRV()
        rv.errCode = .success
        if keySize > Int.max
            || dataSize > Int.max
            || cipherTextSize > Int.max
            || keySize == 0
            || dataSize == 0
            || cipherTextSize == 0
        {
            rv.errCode = .invalidArgument
            return rv
        }
        do {
            let cipher = try AES.KeyWrap.wrap(
                SymmetricKey(data: UnsafeBufferPointer(start: data, count: Int(dataSize))),
                using: SymmetricKey(
                    data: UnsafeBufferPointer(start: key, count: Int(keySize))))
            if cipher.count > Int(cipherTextSize) {
                rv.errCode = .encryptionFailed
                return rv
            }
            cipher.copyBytes(to: cipherText, count: cipher.count)
            rv.errCode = .success
            rv.outputSize = UInt64(cipher.count)
        } catch {
            rv.errCode = .encryptionFailed
            return rv
        }
        return rv
    }

    public static func unwrap(
        _ key: UnsafePointer<UInt8>,
        keySize: UInt,
        cipherText: UnsafePointer<UInt8>,
        cipherTextSize: UInt,
        data: UnsafeMutablePointer<UInt8>,
        dataSize: UInt64
    ) -> AesKwRV {
        var rv = AesKwRV()
        rv.errCode = ErrorCodes.success
        if keySize > Int.max
            || dataSize > Int.max
            || cipherTextSize > Int.max
            || keySize == 0
            || dataSize == 0
            || cipherTextSize == 0
        {
            rv.errCode = .invalidArgument
            return rv
        }
        do {
            let unwrappedKey = try AES.KeyWrap.unwrap(
                UnsafeBufferPointer(start: cipherText, count: Int(cipherTextSize)),
                using: SymmetricKey(
                    data: UnsafeBufferPointer(start: key, count: Int(keySize))))
            if (unwrappedKey.bitCount / 8) > Int(dataSize) {
                rv.errCode = .encryptionFailed
                return rv
            }
            unwrappedKey.withUnsafeBytes { buf in
                let mutable = UnsafeMutableRawBufferPointer(
                    start: data, count: Int(dataSize))
                mutable.copyBytes(from: buf)
            }
            rv.errCode = .success
            rv.outputSize = UInt64(unwrappedKey.bitCount / 8)
        } catch {
            rv.errCode = .decryptionFailed
            return rv
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
        let forceDestructorLink = VectorUInt8()
        var hasher = T()
        hasher.update(data: data)
        let digest = hasher.finalize()
        var returnValue = VectorUInt8(T.Digest.byteCount)
        var index = 0
        digest.forEach { val in
            returnValue[index] = val
            index += 1
        }
        return returnValue
    }
}

#endif
