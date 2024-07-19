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

public typealias CryptoOperationReturnValue = Cpp.CryptoOperationReturnValue
public typealias ErrorCodes = Cpp.ErrorCodes
public typealias VectorUInt8 = Cpp.VectorUInt8
public typealias SpanConstUInt8 = Cpp.SpanConstUInt8

private enum LocalErrors: Error {
    case invalidArgument
}

private class Utils {
    static let zeroArray = [UInt8](repeating: 0, count: 0)
}

public class AesGcm {
    public static func encrypt(
        key: SpanConstUInt8, iv: SpanConstUInt8, ad: SpanConstUInt8, message: SpanConstUInt8,
        desiredTagLengthInBytes: Int
    ) -> CryptoOperationReturnValue {
        var rv = CryptoOperationReturnValue()
        do {
            if iv.size() == 0 {
                rv.errorCode = .InvalidArgument
                return rv
            }
            let sealedBox: AES.GCM.SealedBox = try AES.GCM.seal(message, key: key, iv: iv, ad: ad)
            if desiredTagLengthInBytes > sealedBox.tag.count {
                rv.errorCode = .InvalidArgument
                return rv
            }
            var result = sealedBox.ciphertext
            result.append(
                sealedBox.tag[
                    sealedBox.tag.startIndex..<(sealedBox.tag.startIndex + desiredTagLengthInBytes)]
            )
            rv.errorCode = .Success
            rv.result = result.copyToVectorUInt8()
            return rv
        } catch {
            rv.errorCode = .EncryptionFailed
        }
        return rv
    }
}

public class AesKw {
    public static func wrap(keyToWrap: SpanConstUInt8, using: SpanConstUInt8)
        -> CryptoOperationReturnValue
    {
        var rv = CryptoOperationReturnValue()
        do {
            let result = try AES.KeyWrap.wrap(keyToWrap, using: using)
            rv.errorCode = .Success
            rv.result = result
        } catch {
            rv.errorCode = .EncryptionFailed
        }
        return rv
    }

    public static func unwrap(wrappedKey: SpanConstUInt8, using: SpanConstUInt8)
        -> CryptoOperationReturnValue
    {
        var rv = CryptoOperationReturnValue()
        do {
            let result = try AES.KeyWrap.unwrap(
                wrappedKey, using: using)
            rv.errorCode = .Success
            rv.result = result.copyToVectorUInt8()
        } catch {
            rv.errorCode = .EncryptionFailed
        }
        return rv
    }
}  // AesKw

public enum HashFunction {
    case sha1
    case sha256
    case sha384
    case sha512
}

public class Digest {
    public static func sha1(_ data: SpanConstUInt8) -> VectorUInt8 {
        return digest(data, t: Insecure.SHA1.self)
    }
    public static func sha256(_ data: SpanConstUInt8) -> VectorUInt8 {
        return digest(data, t: SHA256.self)
    }
    public static func sha384(_ data: SpanConstUInt8) -> VectorUInt8 {
        return digest(data, t: SHA384.self)
    }
    public static func sha512(_ data: SpanConstUInt8) -> VectorUInt8 {
        return digest(data, t: SHA512.self)
    }
    fileprivate static func digest<T: CryptoKit.HashFunction>(_ data: SpanConstUInt8, _: T.Type)
        -> T.Digest
    {
        var hasher = T()
        hasher.update(data: data)
        return hasher.finalize()
    }

    fileprivate static func digest<T: CryptoKit.HashFunction>(_ data: SpanConstUInt8, t: T.Type)
        -> VectorUInt8
    {
        return Self.digest(data, t).copyToVectorUInt8()
    }

    fileprivate static func digest(_ data: SpanConstUInt8, hashFunction: HashFunction)
        -> any CryptoKit.Digest
    {
        switch hashFunction {
        case .sha256:
            return digest(data, SHA256.self)
        case .sha384:
            return digest(data, SHA384.self)
        case .sha512:
            return digest(data, SHA512.self)
        case .sha1:
            return digest(data, Insecure.SHA1.self)
        }
    }
}

public enum ECCurve {
    case p256
    case p384
    case p521
}

enum ECPrivateKey {
    case p256(P256.Signing.PrivateKey)
    case p384(P384.Signing.PrivateKey)
    case p521(P521.Signing.PrivateKey)
}

enum ECPublicKey {
    case p256(P256.Signing.PublicKey)
    case p384(P384.Signing.PublicKey)
    case p521(P521.Signing.PublicKey)
}
enum ECKeyInternal {
    case priv(ECPrivateKey)
    case pub(ECPublicKey)
}
public enum ECImportReturnCode {
    case defaultValue
    case success
    case importFailed
}

public struct ECImportReturnValue {
    public var errorCode: ECImportReturnCode = .defaultValue
    public var key: ECKey? = nil
}

public struct ECKey {
    let key: ECKeyInternal
    public init(curve: ECCurve) {
        switch curve {
        case .p256:
            key = .priv(.p256(P256.Signing.PrivateKey(compactRepresentable: true)))
        case .p384:
            key = .priv(.p384(P384.Signing.PrivateKey(compactRepresentable: true)))
        case .p521:
            key = .priv(.p521(P521.Signing.PrivateKey(compactRepresentable: true)))
        }
    }
    private init(pub: ECPublicKey) {
        key = .pub(pub)
    }
    private init(priv: ECPrivateKey) {
        key = .priv(priv)
    }
    private init(internalKey: ECKeyInternal) {
        key = internalKey
    }
    public func toPub() -> ECKey {
        switch key {
        case .pub:
            return self
        case let .priv(v):
            switch v {
            case let .p256(u):
                return ECKey(pub: .p256(u.publicKey))
            case let .p384(u):
                return ECKey(pub: .p384(u.publicKey))
            case let .p521(u):
                return ECKey(pub: .p521(u.publicKey))
            }
        }
    }

    public static func importX963Pub(data: SpanConstUInt8, curve: ECCurve) -> ECImportReturnValue {
        var rv = ECImportReturnValue()
        do {
            switch curve {
            case .p256:
                rv.key = ECKey(internalKey: .pub(.p256(try P256.Signing.PublicKey(span: data))))
            case .p384:
                rv.key = ECKey(internalKey: .pub(.p384(try P384.Signing.PublicKey(span: data))))
            case .p521:
                rv.key = ECKey(internalKey: .pub(.p521(try P521.Signing.PublicKey(span: data))))
            }
            rv.errorCode = .success
        } catch {
            rv.errorCode = .importFailed
        }
        return rv
    }

    public func exportX963Pub() -> CryptoOperationReturnValue {
        var rv = CryptoOperationReturnValue()
        do {
            switch try getInternalPublic() {
            case .p256(let k):
                rv.result = k.x963Representation.copyToVectorUInt8()
            case .p384(let k):
                rv.result = k.x963Representation.copyToVectorUInt8()
            case .p521(let k):
                rv.result = k.x963Representation.copyToVectorUInt8()
            }
            rv.errorCode = .Success
        } catch {
            rv.errorCode = .FailedToExport
        }
        return rv
    }
    public static func importCompressedPub(data: SpanConstUInt8, curve: ECCurve)
        -> ECImportReturnValue
    {
        var rv = ECImportReturnValue()
        do {
            switch curve {
            case .p256:
                rv.key = ECKey(pub: .p256(try P256.Signing.PublicKey(spanCompressed: data)))
            case .p384:
                rv.key = ECKey(pub: .p384(try P384.Signing.PublicKey(spanCompressed: data)))
            case .p521:
                rv.key = ECKey(pub: .p521(try P521.Signing.PublicKey(spanCompressed: data)))
            }
            rv.errorCode = .success
        } catch {
            rv.errorCode = .importFailed
        }
        return rv
    }
    public static func importX963Private(data: SpanConstUInt8, curve: ECCurve)
        -> ECImportReturnValue
    {
        var rv = ECImportReturnValue()
        do {
            switch curve {
            case .p256:
                rv.key = ECKey(priv: .p256(try P256.Signing.PrivateKey(span: data)))
            case .p384:
                rv.key = ECKey(priv: .p384(try P384.Signing.PrivateKey(span: data)))
            case .p521:
                rv.key = ECKey(priv: .p521(try P521.Signing.PrivateKey(span: data)))
            }
            rv.errorCode = .success
        } catch {
            rv.errorCode = .importFailed
        }
        return rv
    }
    public func exportX963Private() -> CryptoOperationReturnValue {
        var rv = CryptoOperationReturnValue()
        do {
            switch try getInternalPrivate() {
            case .p256(let k):
                rv.result = k.x963Representation.copyToVectorUInt8()
            case .p384(let k):
                rv.result = k.x963Representation.copyToVectorUInt8()
            case .p521(let k):
                rv.result = k.x963Representation.copyToVectorUInt8()
            }
            rv.errorCode = .Success
        } catch {
            rv.errorCode = .FailedToExport
        }
        return rv
    }
    public func sign(message: SpanConstUInt8, hashFunction: HashFunction)
        -> CryptoOperationReturnValue
    {
        var rv = CryptoOperationReturnValue()
        do {
            switch try getInternalPrivate() {
            case .p256(let cryptoKey):
                rv.result =
                    try cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                    .rawRepresentation.copyToVectorUInt8()
            case .p384(let cryptoKey):
                rv.result =
                    try cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                    .rawRepresentation.copyToVectorUInt8()
            case .p521(let cryptoKey):
                rv.result =
                    try cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                    .rawRepresentation.copyToVectorUInt8()
            }
            rv.errorCode = .Success
        } catch {
            rv.errorCode = .FailedToSign
        }
        return rv
    }

    public func verify(
        message: SpanConstUInt8, signature: SpanConstUInt8, hashFunction: HashFunction
    ) -> CryptoOperationReturnValue {
        var rv = CryptoOperationReturnValue()
        do {
            let internalPublic = try getInternalPublic()
            switch internalPublic {
            case .p256(let cryptoKey):
                rv.errorCode =
                    cryptoKey.isValidSignature(
                        try P256.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction))
                    ? .Success : .FailedToVerify
            case .p384(let cryptoKey):
                rv.errorCode =
                    cryptoKey.isValidSignature(
                        try P384.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction))
                    ? .Success : .FailedToVerify
            case .p521(let cryptoKey):
                rv.errorCode =
                    cryptoKey.isValidSignature(
                        try P521.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction))
                    ? .Success : .FailedToVerify
            }
        } catch {
            rv.errorCode = .FailedToVerify
        }
        return rv
    }
    private func getInternalPrivate() throws -> ECPrivateKey {
        switch key {
        case .pub:
            throw LocalErrors.invalidArgument
        case .priv(let priv):
            return priv
        }
    }
    private func getInternalPublic() throws -> ECPublicKey {
        switch key {
        case .priv:
            throw LocalErrors.invalidArgument
        case .pub(let pub):
            return pub
        }
    }

    public func deriveBits(pub: ECKey) -> CryptoOperationReturnValue {
        var rv = CryptoOperationReturnValue()
        do {
            let internalPrivate = try getInternalPrivate()
            let internalPub = try pub.getInternalPublic()
            switch internalPrivate {
            case .p256(let signing):
                let scalar = try P256.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation)
                if case let .p256(publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P256.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation))
                    rv.result = derived.copyToVectorUInt8()
                    break
                }
                rv.errorCode = .InvalidArgument
            case .p384(let signing):
                let scalar = try P384.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation)
                if case let .p384(publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P384.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation))
                    rv.result = derived.copyToVectorUInt8()
                    break
                }
                rv.errorCode = .InvalidArgument
            case .p521(let signing):
                let scalar = try P521.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation)
                if case let .p521(publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P521.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation))
                    rv.result = derived.copyToVectorUInt8()
                    break
                }
                rv.errorCode = .InvalidArgument
            }
            rv.errorCode = .Success
        } catch {
            rv.errorCode = .FailedToDerive
        }
        return rv
    }
}

public enum EdSigningAlgorithm {
    case ed25519
    case ed448
}

public enum EdKeyAgreementAlgorithm {
    case x25519
    case x448
}

public class EdKey {
    public static func sign(algo: EdSigningAlgorithm, key: SpanConstUInt8, data: SpanConstUInt8)
        -> CryptoOperationReturnValue
    {
        var rv = CryptoOperationReturnValue()
        do {
            switch algo {
            case .ed25519:
                let priv = try Curve25519.Signing.PrivateKey(span: key)
                rv.result = try priv.signature(span: data)
                rv.errorCode = .Success
            case .ed448:
                rv.errorCode = .UnsupportedAlgorithm
            }
        } catch {
            rv.errorCode = .FailedToSign
        }
        return rv
    }
    public static func verify(
        algo: EdSigningAlgorithm, key: SpanConstUInt8, signature: SpanConstUInt8,
        data: SpanConstUInt8
    ) -> CryptoOperationReturnValue {
        var rv = CryptoOperationReturnValue()
        do {
            switch algo {
            case .ed25519:
                let pub = try Curve25519.Signing.PublicKey(span: key)
                rv.errorCode =
                    pub.isValidSignature(signature: signature, data: data)
                    ? .Success : .FailedToVerify
            case .ed448:
                rv.errorCode = .UnsupportedAlgorithm
            }
        } catch {
            rv.errorCode = .FailedToSign
        }
        return rv
    }

    public static func deriveBits(
        algo: EdKeyAgreementAlgorithm, priv: SpanConstUInt8, pub: SpanConstUInt8
    ) -> CryptoOperationReturnValue {
        var rv = CryptoOperationReturnValue()
        do {
            switch algo {
            case .x25519:
                let priv = try Curve25519.KeyAgreement.PrivateKey(span: priv)
                rv.result = try priv.sharedSecretFromKeyAgreement(pubSpan: pub)
                rv.errorCode = .Success
            case .x448:
                rv.errorCode = .UnsupportedAlgorithm
            }
        } catch {
            rv.errorCode = .FailedToDerive
        }
        return rv

    }
}

public class HMAC {
    public static func sign(key: SpanConstUInt8, data: SpanConstUInt8, hashFunction: HashFunction)
        -> VectorUInt8
    {
        switch hashFunction {
        case .sha1:
            return CryptoKit.HMAC<Insecure.SHA1>.authenticationCode(data: data, key: key)
        case .sha256:
            return CryptoKit.HMAC<SHA256>.authenticationCode(data: data, key: key)
        case .sha384:
            return CryptoKit.HMAC<SHA384>.authenticationCode(data: data, key: key)
        case .sha512:
            return CryptoKit.HMAC<SHA512>.authenticationCode(data: data, key: key)
        }
    }
    public static func verify(
        mac: SpanConstUInt8, key: SpanConstUInt8, data: SpanConstUInt8, hashFunction: HashFunction
    ) -> Bool {
        switch hashFunction {
        case .sha1:
            return CryptoKit.HMAC<Insecure.SHA1>.isValidAuthenticationCode(
                mac: mac, data: data, key: key)
        case .sha256:
            return CryptoKit.HMAC<SHA256>.isValidAuthenticationCode(mac: mac, data: data, key: key)
        case .sha384:
            return CryptoKit.HMAC<SHA384>.isValidAuthenticationCode(mac: mac, data: data, key: key)
        case .sha512:
            return CryptoKit.HMAC<SHA512>.isValidAuthenticationCode(mac: mac, data: data, key: key)
        }
    }
}

// https://www.ietf.org/rfc/rfc5869.txt
private let hkdfInputSizeLimitSHA1 = 255 * Insecure.SHA1.byteCount * 8
private let hkdfInputSizeLimitSHA256 = 255 * SHA256.byteCount * 8
private let hkdfInputSizeLimitSHA384 = 255 * SHA384.byteCount * 8
private let hkdfInputSizeLimitSHA512 = 255 * SHA512.byteCount * 8

public class HKDF {
    public static func deriveBits(
        key: SpanConstUInt8, salt: SpanConstUInt8, info: SpanConstUInt8, outputBitCount: Int,
        hashFunction: HashFunction
    ) -> CryptoOperationReturnValue {
        var rv = CryptoOperationReturnValue()
        if outputBitCount <= 0 || outputBitCount % 8 != 0 {
            rv.errorCode = .InvalidArgument
            return rv
        } else {
            rv.errorCode = .Success
        }
        switch hashFunction {
        case .sha1:
            if outputBitCount > hkdfInputSizeLimitSHA1 {
                rv.errorCode = .InvalidArgument
                break
            }
            rv.result =
                CryptoKit.HKDF<Insecure.SHA1>.deriveKey(
                    inputKeyMaterial: key, salt: salt, info: info,
                    outputByteCount: outputBitCount / 8)

        case .sha256:
            if outputBitCount > hkdfInputSizeLimitSHA256 {
                rv.errorCode = .InvalidArgument
                break
            }
            rv.result =
                CryptoKit.HKDF<SHA256>.deriveKey(
                    inputKeyMaterial: key, salt: salt, info: info,
                    outputByteCount: outputBitCount / 8)

        case .sha384:
            if outputBitCount > hkdfInputSizeLimitSHA384 {
                rv.errorCode = .InvalidArgument
                break
            }
            rv.result =
                CryptoKit.HKDF<SHA384>.deriveKey(
                    inputKeyMaterial: key, salt: salt, info: info,
                    outputByteCount: outputBitCount / 8)

        case .sha512:
            if outputBitCount > hkdfInputSizeLimitSHA512 {
                rv.errorCode = .InvalidArgument
                break
            }
            rv.result =
                CryptoKit.HKDF<SHA512>.deriveKey(
                    inputKeyMaterial: key, salt: salt, info: info,
                    outputByteCount: outputBitCount / 8)

        }
        return rv
    }
}
#endif
