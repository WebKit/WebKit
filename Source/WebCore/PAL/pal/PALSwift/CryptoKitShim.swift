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
        var returnValue = CryptoOperationReturnValue()
        do {
            if iv.size() == 0 {
                 returnValue.errorCode = .InvalidArgument
                return returnValue
            }
            let sealedBox: AES.GCM.SealedBox = try AES.GCM.seal(message, key: key, iv: iv, ad: ad)
            if desiredTagLengthInBytes > sealedBox.tag.count {
                 returnValue.errorCode = .InvalidArgument
                return returnValue
            }
            var result = sealedBox.ciphertext
            result.append(
                sealedBox.tag[
                    sealedBox.tag.startIndex..<(sealedBox.tag.startIndex + desiredTagLengthInBytes)]
            )
             returnValue.errorCode = .Success
             returnValue.result = result.copyToVectorUInt8()
            return returnValue
        } catch {
             returnValue.errorCode = .EncryptionFailed
        }
        return returnValue
    }
}

public class AesKw {
    public static func wrap(keyToWrap: SpanConstUInt8, using: SpanConstUInt8)
        -> CryptoOperationReturnValue
    {
        var returnValue = CryptoOperationReturnValue()
        do {
            let result = try AES.KeyWrap.wrap(keyToWrap, using: using)
             returnValue.errorCode = .Success
             returnValue.result = result
        } catch {
             returnValue.errorCode = .EncryptionFailed
        }
        return returnValue
    }

    public static func unwrap(wrappedKey: SpanConstUInt8, using: SpanConstUInt8)
        -> CryptoOperationReturnValue
    {
        var returnValue = CryptoOperationReturnValue()
        do {
            let result = try AES.KeyWrap.unwrap(
                wrappedKey, using: using)
             returnValue.errorCode = .Success
             returnValue.result = result.copyToVectorUInt8()
        } catch {
             returnValue.errorCode = .EncryptionFailed
        }
        return returnValue
    }
}  // AesKw

public enum HashFunction {
    case sha1
    case sha256
    case sha384
    case sha512
}

public class Digest {
    var ctx: any CryptoKit.HashFunction

    public required init<T: CryptoKit.HashFunction>(_: T.Type) {
        ctx = T()
    }

    public static func sha1Init() -> Digest {
        return Self(Insecure.SHA1.self)
    }

    public static func sha256Init() -> Digest {
        return Self(SHA256.self)
    }

    public static func sha384Init() -> Digest {
        return Self(SHA384.self)
    }

    public static func sha512Init() -> Digest {
        return Self(SHA512.self)
    }

    public func update(_ data: SpanConstUInt8) {
        ctx.update(data: data)
    }

    public func finalize() -> VectorUInt8 {
        ctx.finalize().copyToVectorUInt8()
    }

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
    case privateKey(ECPrivateKey)
    case publicKey(ECPublicKey)
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
            key = .privateKey(.p256(P256.Signing.PrivateKey(compactRepresentable: true)))
        case .p384:
            key = .privateKey(.p384(P384.Signing.PrivateKey(compactRepresentable: true)))
        case .p521:
            key = .privateKey(.p521(P521.Signing.PrivateKey(compactRepresentable: true)))
        }
    }
    private init(publicKey: ECPublicKey) {
        key = .publicKey(publicKey)
    }
    private init(privateKey: ECPrivateKey) {
        key = .privateKey(privateKey)
    }
    private init(internalKey: ECKeyInternal) {
        key = internalKey
    }
    public func toPub() -> ECKey {
        switch key {
        case .publicKey:
            return self
        case let .privateKey(v):
            switch v {
            case let .p256(u):
                return ECKey(publicKey: .p256(u.publicKey))
            case let .p384(u):
                return ECKey(publicKey: .p384(u.publicKey))
            case let .p521(u):
                return ECKey(publicKey: .p521(u.publicKey))
            }
        }
    }

    public static func importX963Pub(data: SpanConstUInt8, curve: ECCurve) -> ECImportReturnValue {
        var returnValue = ECImportReturnValue()
        do {
            switch curve {
            case .p256:
                 returnValue.key = ECKey(internalKey: .publicKey(.p256(try P256.Signing.PublicKey(span: data))))
            case .p384:
                 returnValue.key = ECKey(internalKey: .publicKey(.p384(try P384.Signing.PublicKey(span: data))))
            case .p521:
                 returnValue.key = ECKey(internalKey: .publicKey(.p521(try P521.Signing.PublicKey(span: data))))
            }
             returnValue.errorCode = .success
        } catch {
             returnValue.errorCode = .importFailed
        }
        return returnValue
    }

    public func exportX963Pub() -> CryptoOperationReturnValue {
        var returnValue = CryptoOperationReturnValue()
        do {
            switch try getInternalPublic() {
            case .p256(let k):
                 returnValue.result = k.x963Representation.copyToVectorUInt8()
            case .p384(let k):
                 returnValue.result = k.x963Representation.copyToVectorUInt8()
            case .p521(let k):
                 returnValue.result = k.x963Representation.copyToVectorUInt8()
            }
             returnValue.errorCode = .Success
        } catch {
             returnValue.errorCode = .FailedToExport
        }
        return returnValue
    }
    public static func importCompressedPub(data: SpanConstUInt8, curve: ECCurve)
        -> ECImportReturnValue
    {
        var returnValue = ECImportReturnValue()
        do {
            switch curve {
            case .p256:
                 returnValue.key = ECKey(publicKey: .p256(try P256.Signing.PublicKey(spanCompressed: data)))
            case .p384:
                 returnValue.key = ECKey(publicKey: .p384(try P384.Signing.PublicKey(spanCompressed: data)))
            case .p521:
                 returnValue.key = ECKey(publicKey: .p521(try P521.Signing.PublicKey(spanCompressed: data)))
            }
             returnValue.errorCode = .success
        } catch {
             returnValue.errorCode = .importFailed
        }
        return returnValue
    }
    public static func importX963Private(data: SpanConstUInt8, curve: ECCurve)
        -> ECImportReturnValue
    {
        var returnValue = ECImportReturnValue()
        do {
            switch curve {
            case .p256:
                 returnValue.key = ECKey(privateKey: .p256(try P256.Signing.PrivateKey(span: data)))
            case .p384:
                 returnValue.key = ECKey(privateKey: .p384(try P384.Signing.PrivateKey(span: data)))
            case .p521:
                 returnValue.key = ECKey(privateKey: .p521(try P521.Signing.PrivateKey(span: data)))
            }
             returnValue.errorCode = .success
        } catch {
             returnValue.errorCode = .importFailed
        }
        return returnValue
    }
    public func exportX963Private() -> CryptoOperationReturnValue {
        var returnValue = CryptoOperationReturnValue()
        do {
            switch try getInternalPrivate() {
            case .p256(let k):
                 returnValue.result = k.x963Representation.copyToVectorUInt8()
            case .p384(let k):
                 returnValue.result = k.x963Representation.copyToVectorUInt8()
            case .p521(let k):
                 returnValue.result = k.x963Representation.copyToVectorUInt8()
            }
             returnValue.errorCode = .Success
        } catch {
             returnValue.errorCode = .FailedToExport
        }
        return returnValue
    }
    public func sign(message: SpanConstUInt8, hashFunction: HashFunction)
        -> CryptoOperationReturnValue
    {
        var returnValue = CryptoOperationReturnValue()
        do {
            switch try getInternalPrivate() {
            case .p256(let cryptoKey):
                 returnValue.result =
                    try cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                    .rawRepresentation.copyToVectorUInt8()
            case .p384(let cryptoKey):
                 returnValue.result =
                    try cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                    .rawRepresentation.copyToVectorUInt8()
            case .p521(let cryptoKey):
                 returnValue.result =
                    try cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                    .rawRepresentation.copyToVectorUInt8()
            }
             returnValue.errorCode = .Success
        } catch {
             returnValue.errorCode = .FailedToSign
        }
        return returnValue
    }

    public func verify(
        message: SpanConstUInt8, signature: SpanConstUInt8, hashFunction: HashFunction
    ) -> CryptoOperationReturnValue {
        var returnValue = CryptoOperationReturnValue()
        do {
            let internalPublic = try getInternalPublic()
            switch internalPublic {
            case .p256(let cryptoKey):
                 returnValue.errorCode =
                    cryptoKey.isValidSignature(
                        try P256.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction))
                    ? .Success : .FailedToVerify
            case .p384(let cryptoKey):
                 returnValue.errorCode =
                    cryptoKey.isValidSignature(
                        try P384.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction))
                    ? .Success : .FailedToVerify
            case .p521(let cryptoKey):
                 returnValue.errorCode =
                    cryptoKey.isValidSignature(
                        try P521.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction))
                    ? .Success : .FailedToVerify
            }
        } catch {
             returnValue.errorCode = .FailedToVerify
        }
        return returnValue
    }
    private func getInternalPrivate() throws -> ECPrivateKey {
        switch key {
        case .publicKey:
            throw LocalErrors.invalidArgument
        case .privateKey(let privateKey):
            return privateKey
        }
    }
    private func getInternalPublic() throws -> ECPublicKey {
        switch key {
        case .privateKey:
            throw LocalErrors.invalidArgument
        case .publicKey(let publicKey):
            return publicKey
        }
    }

    public func deriveBits(publicKey: ECKey) -> CryptoOperationReturnValue {
        var returnValue = CryptoOperationReturnValue()
        do {
            let internalPrivate = try getInternalPrivate()
            let internalPub = try publicKey.getInternalPublic()
            switch internalPrivate {
            case .p256(let signing):
                let scalar = try P256.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation)
                if case let .p256(publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P256.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation))
                     returnValue.result = derived.copyToVectorUInt8()
                    break
                }
                 returnValue.errorCode = .InvalidArgument
            case .p384(let signing):
                let scalar = try P384.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation)
                if case let .p384(publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P384.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation))
                     returnValue.result = derived.copyToVectorUInt8()
                    break
                }
                 returnValue.errorCode = .InvalidArgument
            case .p521(let signing):
                let scalar = try P521.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation)
                if case let .p521(publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P521.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation))
                     returnValue.result = derived.copyToVectorUInt8()
                    break
                }
                 returnValue.errorCode = .InvalidArgument
            }
             returnValue.errorCode = .Success
        } catch {
             returnValue.errorCode = .FailedToDerive
        }
        return returnValue
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
    public static func generatePrivateKey(algo: EdSigningAlgorithm) -> VectorUInt8 {
        switch algo {
        case .ed25519:
            return Curve25519.Signing.PrivateKey().rawRepresentation.copyToVectorUInt8()
        case .ed448:
            return Data(count: 0).copyToVectorUInt8()
        }
    }

    public static func generatePrivateKeyKeyAgreement(algo: EdKeyAgreementAlgorithm) -> VectorUInt8
    {
        switch algo {
        case .x25519:
            return Curve25519.KeyAgreement.PrivateKey().rawRepresentation.copyToVectorUInt8()
        case .x448:
            return Data(count: 0).copyToVectorUInt8()
        }
    }

    public static func privateToPublic(algo: EdSigningAlgorithm, privateKey: SpanConstUInt8)
        -> CryptoOperationReturnValue
    {
        var returnValue = CryptoOperationReturnValue()
        do {
            if privateKey.size() != 32 {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .ed25519:
                 returnValue.result = try Curve25519.Signing.PrivateKey(span: privateKey).publicKey
                    .rawRepresentation.copyToVectorUInt8()
                if  returnValue.result.size() != 32 {
                    throw LocalErrors.invalidArgument
                }
                 returnValue.errorCode = .Success
            case .ed448:
                 returnValue.errorCode = .UnsupportedAlgorithm
            }
        } catch {
             returnValue.errorCode = .FailedToImport
        }
        return returnValue
    }

    public static func privateToPublicKeyAgreement(
        algo: EdKeyAgreementAlgorithm, privateKey: SpanConstUInt8
    ) -> CryptoOperationReturnValue {
        var returnValue = CryptoOperationReturnValue()
        do {
            if privateKey.size() != 32 {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .x25519:
                 returnValue.result = try Curve25519.KeyAgreement.PrivateKey(span: privateKey).publicKey
                    .rawRepresentation.copyToVectorUInt8()
                if  returnValue.result.size() != 32 {
                    throw LocalErrors.invalidArgument
                }
                 returnValue.errorCode = .Success
            case .x448:
                 returnValue.errorCode = .UnsupportedAlgorithm
            }
        } catch {
             returnValue.errorCode = .FailedToImport
        }
        return returnValue
    }
    public static func validateKeyPair(
        algo: EdSigningAlgorithm, privateKey: SpanConstUInt8, publicKey: SpanConstUInt8
    ) -> Bool {
        do {
            if privateKey.size() != 32 || publicKey.size() != 32 {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .ed25519:
                let derivedPublicKey = try Curve25519.Signing.PrivateKey(span: privateKey).publicKey.rawRepresentation
                let importedPublicKey = try Curve25519.Signing.PublicKey(span: publicKey).rawRepresentation
                return derivedPublicKey == importedPublicKey
            case .ed448:
                return false
            }
        } catch {
            return false
        }

    }

    public static func validateKeyPairKeyAgreement(
        algo: EdKeyAgreementAlgorithm, privateKey: SpanConstUInt8, publicKey: SpanConstUInt8
    ) -> Bool {
        do {
            if privateKey.size() != 32 || publicKey.size() != 32 {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .x25519:
                let derivedPublicKey = try Curve25519.KeyAgreement.PrivateKey(span: privateKey).publicKey.rawRepresentation
                let importedPublicKey = try Curve25519.KeyAgreement.PublicKey(span: publicKey).rawRepresentation
                return derivedPublicKey == importedPublicKey
            case .x448:
                return false
            }
        } catch {
            return false
        }

    }

    public static func sign(algo: EdSigningAlgorithm, privateKey: SpanConstUInt8, data: SpanConstUInt8)
        -> CryptoOperationReturnValue
    {
        var returnValue = CryptoOperationReturnValue()
        do {
            switch algo {
            case .ed25519:
                let privateKeyImported = try Curve25519.Signing.PrivateKey(span: privateKey)
                 returnValue.result = try privateKeyImported.signature(span: data)
                 returnValue.errorCode = .Success
            case .ed448:
                 returnValue.errorCode = .UnsupportedAlgorithm
            }
        } catch {
             returnValue.errorCode = .FailedToSign
        }
        return returnValue
    }
    public static func verify(
        algo: EdSigningAlgorithm, publicKey: SpanConstUInt8, signature: SpanConstUInt8,
        data: SpanConstUInt8
    ) -> CryptoOperationReturnValue {
        var returnValue = CryptoOperationReturnValue()
        do {
            switch algo {
            case .ed25519:
                let publicKeyImported = try Curve25519.Signing.PublicKey(span: publicKey)
                 returnValue.errorCode =
                    publicKeyImported.isValidSignature(signature: signature, data: data)
                    ? .Success : .FailedToVerify
            case .ed448:
                 returnValue.errorCode = .UnsupportedAlgorithm
            }
        } catch {
             returnValue.errorCode = .FailedToSign
        }
        return returnValue
    }

    public static func deriveBits(
        algo: EdKeyAgreementAlgorithm, privateKey: SpanConstUInt8, publicKey: SpanConstUInt8
    ) -> CryptoOperationReturnValue {
        var returnValue = CryptoOperationReturnValue()
        do {
            switch algo {
            case .x25519:
                let privateKeyImported = try Curve25519.KeyAgreement.PrivateKey(span: privateKey)
                 returnValue.result = try privateKeyImported.sharedSecretFromKeyAgreement(pubSpan: publicKey)
                 returnValue.errorCode = .Success
            case .x448:
                 returnValue.errorCode = .UnsupportedAlgorithm
            }
        } catch {
             returnValue.errorCode = .FailedToDerive
        }
        return returnValue

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
        var returnValue = CryptoOperationReturnValue()
        if outputBitCount <= 0 || outputBitCount % 8 != 0 {
             returnValue.errorCode = .InvalidArgument
            return returnValue
        } else {
             returnValue.errorCode = .Success
        }
        switch hashFunction {
        case .sha1:
            if outputBitCount > hkdfInputSizeLimitSHA1 {
                 returnValue.errorCode = .InvalidArgument
                break
            }
             returnValue.result =
                CryptoKit.HKDF<Insecure.SHA1>.deriveKey(
                    inputKeyMaterial: key, salt: salt, info: info,
                    outputByteCount: outputBitCount / 8)

        case .sha256:
            if outputBitCount > hkdfInputSizeLimitSHA256 {
                 returnValue.errorCode = .InvalidArgument
                break
            }
             returnValue.result =
                CryptoKit.HKDF<SHA256>.deriveKey(
                    inputKeyMaterial: key, salt: salt, info: info,
                    outputByteCount: outputBitCount / 8)

        case .sha384:
            if outputBitCount > hkdfInputSizeLimitSHA384 {
                 returnValue.errorCode = .InvalidArgument
                break
            }
             returnValue.result =
                CryptoKit.HKDF<SHA384>.deriveKey(
                    inputKeyMaterial: key, salt: salt, info: info,
                    outputByteCount: outputBitCount / 8)

        case .sha512:
            if outputBitCount > hkdfInputSizeLimitSHA512 {
                 returnValue.errorCode = .InvalidArgument
                break
            }
             returnValue.result =
                CryptoKit.HKDF<SHA512>.deriveKey(
                    inputKeyMaterial: key, salt: salt, info: info,
                    outputByteCount: outputBitCount / 8)

        }
        return returnValue
    }
}
#endif
