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

private enum LocalErrors: Error {
    case invalidArgument
}

public enum ErrorCodes: Int {
    case success = 0
    case wrongTagSize = 1
    case encryptionFailed = 2
    case encryptionResultNil = 3
    case invalidArgument = 4
    case tooBigArguments = 5
    case decryptionFailed = 6
    case hashingFailed = 7
    case publicKeyProvidedToSign = 8
    case failedToSign = 9
    case failedToVerify = 10
    case privateKeyProvidedForVerification = 11
    case failedToImport = 12
    case failedToDerive = 13
    case failedToExport = 14
    case defaultValue = 15
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

public enum HashFunction {
    case sha256
    case sha384
    case sha512
    case sha1
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

public struct ECRv {
    public var errCode: ErrorCodes = .defaultValue
    public var signature: OptionalVectorUInt8 = OptionalVectorUInt8()
    public var keyBytes: OptionalVectorUInt8 = OptionalVectorUInt8()
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

    public static func importX963Pub(data: SpanConstUInt8, curve: ECCurve) -> ECRv {
        var rv = ECRv()
        do {
            switch curve {
            case .p256:
                rv.key = ECKey(internalKey: .pub(.p256(try P256.Signing.PublicKey(span: data))))
            case .p384:
                rv.key = ECKey(internalKey: .pub(.p384(try P384.Signing.PublicKey(span: data))))
            case .p521:
                rv.key = ECKey(internalKey: .pub(.p521(try P521.Signing.PublicKey(span: data))))
            }
            rv.errCode = .success
        } catch {
            rv.errCode = .failedToImport
        }
        return rv
    }

    public func exportX963Pub() -> ECRv {
        var rv = ECRv()
        do {
            switch try getInternalPublic() {
            case .p256(let k):
                rv.keyBytes = Cpp.makeOptional(k.x963Representation.copyToVectorUInt8())
            case .p384(let k):
                rv.keyBytes = Cpp.makeOptional(k.x963Representation.copyToVectorUInt8())
            case .p521(let k):
                rv.keyBytes = Cpp.makeOptional(k.x963Representation.copyToVectorUInt8())
            }
            rv.errCode = .success
        } catch {
            rv.errCode = .failedToExport
        }
        return rv
    }
    public static func importCompressedPub(data: SpanConstUInt8, curve: ECCurve) -> ECRv {
        var rv = ECRv()
        do {
            switch curve {
            case .p256:
                rv.key = ECKey(pub: .p256(try P256.Signing.PublicKey(spanCompressed: data)))
            case .p384:
                rv.key = ECKey(pub: .p384(try P384.Signing.PublicKey(spanCompressed: data)))
            case .p521:
                rv.key = ECKey(pub: .p521(try P521.Signing.PublicKey(spanCompressed: data)))
            }
            rv.errCode = .success
        } catch {
            rv.errCode = .failedToImport
        }
        return rv
    }
    public static func importX963Private(data: SpanConstUInt8, curve: ECCurve) -> ECRv {
        var rv = ECRv()
        do {
            switch curve {
            case .p256:
                rv.key = ECKey(priv: .p256(try P256.Signing.PrivateKey(span: data)))
            case .p384:
                rv.key = ECKey(priv: .p384(try P384.Signing.PrivateKey(span: data)))
            case .p521:
                rv.key = ECKey(priv: .p521(try P521.Signing.PrivateKey(span: data)))
            }
            rv.errCode = .success
        } catch {
            rv.errCode = .failedToImport
        }
        return rv
    }
    public func exportX963Private() -> ECRv {
        var rv = ECRv()
        do {
            switch try getInternalPrivate() {
            case .p256(let k):
                rv.keyBytes = Cpp.makeOptional(k.x963Representation.copyToVectorUInt8())
            case .p384(let k):
                rv.keyBytes = Cpp.makeOptional(k.x963Representation.copyToVectorUInt8())
            case .p521(let k):
                rv.keyBytes = Cpp.makeOptional(k.x963Representation.copyToVectorUInt8())
            }
            rv.errCode = .success
        } catch {
            rv.errCode = .failedToExport
        }
        return rv
    }
    public func sign(message: SpanConstUInt8, hashFunction: HashFunction) -> ECRv {
        var rv = ECRv()
        do {
            switch try getInternalPrivate() {
            case .p256(let cryptoKey):
                rv.signature = Cpp.makeOptional(
                    try cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                        .rawRepresentation.copyToVectorUInt8())
            case .p384(let cryptoKey):
                rv.signature = Cpp.makeOptional(
                    try cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                        .rawRepresentation.copyToVectorUInt8())
            case .p521(let cryptoKey):
                rv.signature = Cpp.makeOptional(
                    try cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                        .rawRepresentation.copyToVectorUInt8())
            }
            rv.errCode = .success
        } catch {
            rv.errCode = .failedToSign
        }
        return rv
    }

    public func verify(
        message: SpanConstUInt8, signature: SpanConstUInt8, hashFunction: HashFunction
    ) -> ECRv {
        var rv = ECRv()
        do {
            let internalPublic = try getInternalPublic()
            switch internalPublic {
            case .p256(let cryptoKey):
                rv.errCode =
                    cryptoKey.isValidSignature(
                        try P256.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction))
                    ? .success : .failedToVerify
            case .p384(let cryptoKey):
                rv.errCode =
                    cryptoKey.isValidSignature(
                        try P384.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction))
                    ? .success : .failedToVerify
            case .p521(let cryptoKey):
                rv.errCode =
                    cryptoKey.isValidSignature(
                        try P521.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction))
                    ? .success : .failedToVerify
            }
        } catch {
            rv.errCode = .failedToVerify
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

    public func deriveBits(pub: ECKey) -> ECRv {
        var rv = ECRv()
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
                    rv.keyBytes = Cpp.makeOptional(derived.copyToVectorUInt8())
                    break
                }
                rv.errCode = .invalidArgument
            case .p384(let signing):
                let scalar = try P384.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation)
                if case let .p384(publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P384.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation))
                    rv.keyBytes = Cpp.makeOptional(derived.copyToVectorUInt8())
                    break
                }
                rv.errCode = .invalidArgument
            case .p521(let signing):
                let scalar = try P521.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation)
                if case let .p521(publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P521.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation))
                    rv.keyBytes = Cpp.makeOptional(derived.copyToVectorUInt8())
                    break
                }
                rv.errCode = .invalidArgument
            }
            rv.errCode = .success
        } catch {
            rv.errCode = .failedToDerive
        }
        return rv
    }
}

#endif
