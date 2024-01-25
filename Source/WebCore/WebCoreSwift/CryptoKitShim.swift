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

import Foundation
import CryptoKit

@objc(WebCryptoAesGcmErrorCodes)
public enum AesGcmErrorCodes: Int {
    case Success = 0
    case WrongTagSize = 1
    case EncryptionFailed = 2
    case EncryptionResultNil = 3
    case InvalidArgument = 4
    case TooBigArguments = 5
}

@objc(WebCryptoAesGcm)
public final class AesGcm: NSObject {
    
    @objc public static func encrypt(
        _ key: UnsafePointer<UInt8>,
        keySize: UInt,
        iv: UnsafePointer<UInt8>,
        ivSize: UInt,
        additionalData: UnsafePointer<UInt8>,
        additionalDataSize: UInt,
        plainText: UnsafePointer<UInt8>,
        plainTextSize: UInt,
        cipherText: UnsafeMutablePointer<UInt8>) -> AesGcmErrorCodes
    {
        do {
            if keySize > Int.max
                || ivSize > Int.max 
                || plainTextSize > Int.max
                || additionalDataSize > Int.max {
                return AesGcmErrorCodes.TooBigArguments
            }
            if ivSize == 0 {
                return AesGcmErrorCodes.InvalidArgument
            }
            let zeroArray = [UInt8](repeating: 0, count: 0)
            let nonce = try AES.GCM.Nonce(data: UnsafeBufferPointer(start: iv, count: Int(ivSize)))
            var message = UnsafeBufferPointer(start: plainText, count: Int(plainTextSize))
            if plainTextSize == 0 {
                zeroArray.withUnsafeBufferPointer { ptr in 
                    message = ptr
                }
            }
            let symKey = SymmetricKey(data: UnsafeBufferPointer(start: key, count: Int(keySize)))
            let sealedBox: AES.GCM.SealedBox
            if additionalDataSize > 0 {
                sealedBox = try AES.GCM.seal(
                    message,
                    using: symKey, 
                    nonce: nonce,
                    authenticating: UnsafeBufferPointer(start: additionalData, count: Int(additionalDataSize))
                )
            } else {
                sealedBox = try AES.GCM.seal(
                    message,
                    using: symKey, 
                    nonce: nonce
                )
            }
            sealedBox.ciphertext.copyBytes(to: cipherText, count: Int(plainTextSize));
            sealedBox.tag.copyBytes(to: cipherText + Int(plainTextSize), count: sealedBox.tag.count);
        } catch {
            return AesGcmErrorCodes.EncryptionFailed
        }
        return AesGcmErrorCodes.Success
    }
}
