// Copyright (C) 2024 Apple Inc. All rights reserved.
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

private import CryptoKit
import Foundation
public import pal.Core.crypto.CryptoTypes

private enum LocalErrors: Error {
    case invalidArgument
    case emptySpan
}

@_expose(Cxx)
final class AesGcm {
    @_expose(Cxx)
    static func encrypt(
        key: WTF.BorrowedBytes,
        iv: WTF.BorrowedBytes,
        ad: WTF.BorrowedBytes,
        message: WTF.BorrowedBytes,
        desiredTagLengthInBytes: Int
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            if iv.size() == 0 {
                returnValue.errorCode = .InvalidArgument
                return returnValue
            }
            let key = SymmetricKey(data: key.asNonNullBytes)
            let nonce = try AES.GCM.Nonce(data: iv.asNonNullBytes)
            let sealedBox: AES.GCM.SealedBox
            if ad.size() > 0 {
                sealedBox = try AES.GCM.seal(message.asNonNullBytes, using: key, nonce: nonce, authenticating: ad.asNonNullBytes)
            } else {
                sealedBox = try AES.GCM.seal(message.asNonNullBytes, using: key, nonce: nonce)
            }
            if desiredTagLengthInBytes > sealedBox.tag.count {
                returnValue.errorCode = .InvalidArgument
                return returnValue
            }
            var result = sealedBox.ciphertext
            result.append(
                sealedBox.tag[
                    sealedBox.tag.startIndex..<(sealedBox.tag.startIndex + desiredTagLengthInBytes)
                ]
            )
            returnValue.errorCode = .Success
            returnValue.result = makeVectorUInt8(copying: result)
            return returnValue
        } catch {
            returnValue.errorCode = .EncryptionFailed
        }
        return returnValue
    }
}

@_expose(Cxx)
final class AesKw {
    @_expose(Cxx)
    static func wrap(
        keyToWrap: WTF.BorrowedBytes,
        using: WTF.BorrowedBytes
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            let result = try AES.KeyWrap.wrap(
                SymmetricKey(data: keyToWrap.asNonNullBytes),
                using: SymmetricKey(data: using.asNonNullBytes)
            )
            returnValue.errorCode = .Success
            returnValue.result = makeVectorUInt8(copying: result)
        } catch {
            returnValue.errorCode = .EncryptionFailed
        }
        return returnValue
    }

    @_expose(Cxx)
    static func unwrap(
        wrappedKey: WTF.BorrowedBytes,
        using: WTF.BorrowedBytes
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            let result = try AES.KeyWrap.unwrap(
                wrappedKey.asNonNullBytes,
                using: SymmetricKey(data: using.asNonNullBytes)
            )
            returnValue.errorCode = .Success
            returnValue.result = makeVectorUInt8(copying: result)
        } catch {
            returnValue.errorCode = .EncryptionFailed
        }
        return returnValue
    }
}

@_expose(Cxx)
final class Digest {
    private var ctx: any CryptoKit.HashFunction

    private init<T: CryptoKit.HashFunction>(_: T.Type) {
        ctx = T()
    }

    @_expose(Cxx)
    static func sha1Init() -> Digest {
        Self(Insecure.SHA1.self)
    }

    @_expose(Cxx)
    static func sha256Init() -> Digest {
        Self(SHA256.self)
    }

    @_expose(Cxx)
    static func sha384Init() -> Digest {
        Self(SHA384.self)
    }

    @_expose(Cxx)
    static func sha512Init() -> Digest {
        Self(SHA512.self)
    }

    @_expose(Cxx)
    func update(_ data: WTF.BorrowedBytes) {
        ctx.update(data: data.asNonNullBytes)
    }

    @_expose(Cxx)
    func finalize() -> PAL.Crypto.VectorUInt8 {
        makeVectorUInt8(copying: ctx.finalize())
    }

    @_expose(Cxx)
    static func sha1(_ data: WTF.BorrowedBytes) -> PAL.Crypto.VectorUInt8 {
        digest(data, t: Insecure.SHA1.self)
    }

    @_expose(Cxx)
    static func sha256(_ data: WTF.BorrowedBytes) -> PAL.Crypto.VectorUInt8 {
        digest(data, t: SHA256.self)
    }

    @_expose(Cxx)
    static func sha384(_ data: WTF.BorrowedBytes) -> PAL.Crypto.VectorUInt8 {
        digest(data, t: SHA384.self)
    }

    @_expose(Cxx)
    static func sha512(_ data: WTF.BorrowedBytes) -> PAL.Crypto.VectorUInt8 {
        digest(data, t: SHA512.self)
    }

    fileprivate static func digest<T: CryptoKit.HashFunction>(_ data: WTF.BorrowedBytes, _: T.Type) -> T.Digest {
        var hasher = T()
        hasher.update(data: data.asNonNullBytes)
        return hasher.finalize()
    }

    fileprivate static func digest<T: CryptoKit.HashFunction>(_ data: WTF.BorrowedBytes, t: T.Type) -> PAL.Crypto.VectorUInt8 {
        makeVectorUInt8(copying: Self.digest(data, t))
    }

    fileprivate static func digest(
        _ data: WTF.BorrowedBytes,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> any CryptoKit.Digest {
        switch hashFunction {
        case .SHA_256:
            return digest(data, SHA256.self)
        case .SHA_384:
            return digest(data, SHA384.self)
        case .SHA_512:
            return digest(data, SHA512.self)
        case .SHA_1:
            return digest(data, Insecure.SHA1.self)
        case .DEPRECATED_SHA_224:
            fatalError("DEPRECATED_SHA_224 is not supported")
        @unknown default:
            fatalError("Unknown PAL.Crypto.CryptoDigestHashFunction enum case value: \(hashFunction.rawValue)")
        }
    }
}

private enum ECPrivateKey {
    case p256(P256.Signing.PrivateKey)
    case p384(P384.Signing.PrivateKey)
    case p521(P521.Signing.PrivateKey)
}

private enum ECPublicKey {
    case p256(P256.Signing.PublicKey)
    case p384(P384.Signing.PublicKey)
    case p521(P521.Signing.PublicKey)
}

private enum ECKeyInternal {
    case privateKey(ECPrivateKey)
    case publicKey(ECPublicKey)
}

@_expose(Cxx)
struct ECKey {
    private let key: ECKeyInternal

    @_expose(Cxx)
    init(curve: PAL.Crypto.ECNamedCurve) {
        switch curve {
        case .P256:
            key = .privateKey(.p256(P256.Signing.PrivateKey(compactRepresentable: true)))
        case .P384:
            key = .privateKey(.p384(P384.Signing.PrivateKey(compactRepresentable: true)))
        case .P521:
            key = .privateKey(.p521(P521.Signing.PrivateKey(compactRepresentable: true)))
        @unknown default:
            fatalError()
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

    @_expose(Cxx)
    func toPub() -> ECKey {
        switch key {
        case .publicKey:
            return self
        case .privateKey(let v):
            switch v {
            case .p256(let u):
                return ECKey(publicKey: .p256(u.publicKey))
            case .p384(let u):
                return ECKey(publicKey: .p384(u.publicKey))
            case .p521(let u):
                return ECKey(publicKey: .p521(u.publicKey))
            }
        }
    }

    @_expose(Cxx)
    static func importX963Pub(data: WTF.BorrowedBytes, curve: PAL.Crypto.ECNamedCurve) -> ECKey? {
        do {
            return switch curve {
            case .P256:
                ECKey(internalKey: .publicKey(.p256(try P256.Signing.PublicKey(span: data))))
            case .P384:
                ECKey(internalKey: .publicKey(.p384(try P384.Signing.PublicKey(span: data))))
            case .P521:
                ECKey(internalKey: .publicKey(.p521(try P521.Signing.PublicKey(span: data))))
            @unknown default:
                fatalError()
            }
        } catch {
            return nil
        }
    }

    @_expose(Cxx)
    func exportX963Pub() -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            switch try getInternalPublic() {
            case .p256(let k):
                returnValue.result = makeVectorUInt8(copying: k.x963Representation)
            case .p384(let k):
                returnValue.result = makeVectorUInt8(copying: k.x963Representation)
            case .p521(let k):
                returnValue.result = makeVectorUInt8(copying: k.x963Representation)
            }
            returnValue.errorCode = .Success
        } catch {
            returnValue.errorCode = .FailedToExport
        }
        return returnValue
    }

    @_expose(Cxx)
    static func importCompressedPub(data: WTF.BorrowedBytes, curve: PAL.Crypto.ECNamedCurve) -> ECKey? {
        do {
            return switch curve {
            case .P256:
                ECKey(publicKey: .p256(try P256.Signing.PublicKey(spanCompressed: data)))
            case .P384:
                ECKey(publicKey: .p384(try P384.Signing.PublicKey(spanCompressed: data)))
            case .P521:
                ECKey(publicKey: .p521(try P521.Signing.PublicKey(spanCompressed: data)))
            @unknown default:
                fatalError()
            }
        } catch {
            return nil
        }
    }

    @_expose(Cxx)
    static func importX963Private(data: WTF.BorrowedBytes, curve: PAL.Crypto.ECNamedCurve) -> ECKey? {
        do {
            return switch curve {
            case .P256:
                ECKey(privateKey: .p256(try P256.Signing.PrivateKey(span: data)))
            case .P384:
                ECKey(privateKey: .p384(try P384.Signing.PrivateKey(span: data)))
            case .P521:
                ECKey(privateKey: .p521(try P521.Signing.PrivateKey(span: data)))
            @unknown default:
                fatalError()
            }
        } catch {
            return nil
        }
    }

    @_expose(Cxx)
    func exportX963Private() -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            switch try getInternalPrivate() {
            case .p256(let k):
                returnValue.result = makeVectorUInt8(copying: k.x963Representation)
            case .p384(let k):
                returnValue.result = makeVectorUInt8(copying: k.x963Representation)
            case .p521(let k):
                returnValue.result = makeVectorUInt8(copying: k.x963Representation)
            }
            returnValue.errorCode = .Success
        } catch {
            returnValue.errorCode = .FailedToExport
        }
        return returnValue
    }

    @_expose(Cxx)
    func sign(
        message: WTF.BorrowedBytes,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            switch try getInternalPrivate() {
            case .p256(let cryptoKey):
                returnValue.result =
                    try makeVectorUInt8(
                        copying: cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction)).rawRepresentation
                    )
            case .p384(let cryptoKey):
                returnValue.result =
                    try makeVectorUInt8(
                        copying: cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction)).rawRepresentation
                    )
            case .p521(let cryptoKey):
                returnValue.result =
                    try makeVectorUInt8(
                        copying: cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction)).rawRepresentation
                    )
            }
            returnValue.errorCode = .Success
        } catch {
            returnValue.errorCode = .FailedToSign
        }
        return returnValue
    }

    @_expose(Cxx)
    func verify(
        message: WTF.BorrowedBytes,
        signature: WTF.BorrowedBytes,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            let internalPublic = try getInternalPublic()
            switch internalPublic {
            case .p256(let cryptoKey):
                returnValue.errorCode =
                    cryptoKey.isValidSignature(
                        try P256.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction)
                    )
                    ? .Success : .FailedToVerify
            case .p384(let cryptoKey):
                returnValue.errorCode =
                    cryptoKey.isValidSignature(
                        try P384.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction)
                    )
                    ? .Success : .FailedToVerify
            case .p521(let cryptoKey):
                returnValue.errorCode =
                    cryptoKey.isValidSignature(
                        try P521.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction)
                    )
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

    @_expose(Cxx)
    func deriveBits(publicKey: ECKey) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            let internalPrivate = try getInternalPrivate()
            let internalPub = try publicKey.getInternalPublic()
            switch internalPrivate {
            case .p256(let signing):
                let scalar = try P256.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation
                )
                if case .p256(let publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P256.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation
                        )
                    )
                    returnValue.result = makeVectorUInt8(copying: derived)
                    break
                }
                returnValue.errorCode = .InvalidArgument
            case .p384(let signing):
                let scalar = try P384.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation
                )
                if case .p384(let publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P384.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation
                        )
                    )
                    returnValue.result = makeVectorUInt8(copying: derived)
                    break
                }
                returnValue.errorCode = .InvalidArgument
            case .p521(let signing):
                let scalar = try P521.KeyAgreement.PrivateKey(
                    rawRepresentation: signing.rawRepresentation
                )
                if case .p521(let publicKey) = internalPub {
                    let derived = try scalar.sharedSecretFromKeyAgreement(
                        with: try P521.KeyAgreement.PublicKey(
                            rawRepresentation: publicKey.rawRepresentation
                        )
                    )
                    returnValue.result = makeVectorUInt8(copying: derived)
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

@_expose(Cxx)
final class EdKey {
    @_expose(Cxx)
    static func generatePrivateKey(algo: PAL.Crypto.EdSigningAlgorithm) -> PAL.Crypto.VectorUInt8 {
        switch algo {
        case .ED25519:
            makeVectorUInt8(copying: Curve25519.Signing.PrivateKey().rawRepresentation)
        case .ED448:
            makeVectorUInt8(copying: Data(count: 0))
        @unknown default:
            fatalError()
        }
    }

    @_expose(Cxx)
    static func generatePrivateKeyKeyAgreement(algo: PAL.Crypto.EdKeyAgreementAlgorithm) -> PAL.Crypto.VectorUInt8 {
        switch algo {
        case .X25519:
            makeVectorUInt8(copying: Curve25519.KeyAgreement.PrivateKey().rawRepresentation)
        case .X448:
            makeVectorUInt8(copying: Data(count: 0))
        @unknown default:
            fatalError()
        }
    }

    @_expose(Cxx)
    static func privateToPublic(
        algo: PAL.Crypto.EdSigningAlgorithm,
        privateKey: WTF.BorrowedBytes
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            if privateKey.size() != 32 {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .ED25519:
                returnValue.result = try makeVectorUInt8(
                    copying: Curve25519.Signing.PrivateKey(span: privateKey).publicKey.rawRepresentation
                )
                if returnValue.result.size() != 32 {
                    throw LocalErrors.invalidArgument
                }
                returnValue.errorCode = .Success
            case .ED448:
                returnValue.errorCode = .UnsupportedAlgorithm
            @unknown default:
                fatalError()
            }
        } catch {
            returnValue.errorCode = .FailedToImport
        }
        return returnValue
    }

    @_expose(Cxx)
    static func privateToPublicKeyAgreement(
        algo: PAL.Crypto.EdKeyAgreementAlgorithm,
        privateKey: WTF.BorrowedBytes
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            if privateKey.size() != 32 {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .X25519:
                returnValue.result = try makeVectorUInt8(
                    copying: Curve25519.KeyAgreement.PrivateKey(span: privateKey).publicKey.rawRepresentation
                )
                if returnValue.result.size() != 32 {
                    throw LocalErrors.invalidArgument
                }
                returnValue.errorCode = .Success
            case .X448:
                returnValue.errorCode = .UnsupportedAlgorithm
            @unknown default:
                fatalError()
            }
        } catch {
            returnValue.errorCode = .FailedToImport
        }
        return returnValue
    }

    @_expose(Cxx)
    static func validateKeyPair(
        algo: PAL.Crypto.EdSigningAlgorithm,
        privateKey: WTF.BorrowedBytes,
        publicKey: WTF.BorrowedBytes
    ) -> Bool {
        do {
            if privateKey.size() != 32 || publicKey.size() != 32 {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .ED25519:
                let derivedPublicKey = try Curve25519.Signing.PrivateKey(span: privateKey).publicKey.rawRepresentation
                let importedPublicKey = try Curve25519.Signing.PublicKey(span: publicKey).rawRepresentation
                return derivedPublicKey == importedPublicKey
            case .ED448:
                return false
            @unknown default:
                fatalError()
            }
        } catch {
            return false
        }
    }

    @_expose(Cxx)
    static func validateKeyPairKeyAgreement(
        algo: PAL.Crypto.EdKeyAgreementAlgorithm,
        privateKey: WTF.BorrowedBytes,
        publicKey: WTF.BorrowedBytes
    ) -> Bool {
        do {
            if privateKey.size() != 32 || publicKey.size() != 32 {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .X25519:
                let derivedPublicKey = try Curve25519.KeyAgreement.PrivateKey(span: privateKey).publicKey.rawRepresentation
                let importedPublicKey = try Curve25519.KeyAgreement.PublicKey(span: publicKey).rawRepresentation
                return derivedPublicKey == importedPublicKey
            case .X448:
                return false
            @unknown default:
                fatalError()
            }
        } catch {
            return false
        }
    }

    @_expose(Cxx)
    static func sign(
        algo: PAL.Crypto.EdSigningAlgorithm,
        privateKey: WTF.BorrowedBytes,
        data: WTF.BorrowedBytes
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            switch algo {
            case .ED25519:
                let privateKeyImported = try Curve25519.Signing.PrivateKey(span: privateKey)
                returnValue.result = try privateKeyImported.signature(span: data)
                returnValue.errorCode = .Success
            case .ED448:
                returnValue.errorCode = .UnsupportedAlgorithm
            @unknown default:
                fatalError()
            }
        } catch {
            returnValue.errorCode = .FailedToSign
        }
        return returnValue
    }

    @_expose(Cxx)
    static func verify(
        algo: PAL.Crypto.EdSigningAlgorithm,
        publicKey: WTF.BorrowedBytes,
        signature: WTF.BorrowedBytes,
        data: WTF.BorrowedBytes
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            switch algo {
            case .ED25519:
                let publicKeyImported = try Curve25519.Signing.PublicKey(span: publicKey)
                returnValue.errorCode =
                    publicKeyImported.isValidSignature(signature: signature, data: data)
                    ? .Success : .FailedToVerify
            case .ED448:
                returnValue.errorCode = .UnsupportedAlgorithm
            @unknown default:
                fatalError()
            }
        } catch {
            returnValue.errorCode = .FailedToSign
        }
        return returnValue
    }

    @_expose(Cxx)
    static func deriveBits(
        algo: PAL.Crypto.EdKeyAgreementAlgorithm,
        privateKey: WTF.BorrowedBytes,
        publicKey: WTF.BorrowedBytes
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            switch algo {
            case .X25519:
                let privateKeyImported = try Curve25519.KeyAgreement.PrivateKey(span: privateKey)
                returnValue.result = try privateKeyImported.sharedSecretFromKeyAgreement(pubSpan: publicKey)
                returnValue.errorCode = .Success
            case .X448:
                returnValue.errorCode = .UnsupportedAlgorithm
            @unknown default:
                fatalError()
            }
        } catch {
            returnValue.errorCode = .FailedToDerive
        }
        return returnValue
    }
}

@_expose(Cxx)
final class HMAC {
    @_expose(Cxx)
    static func sign(
        key: WTF.BorrowedBytes,
        data: WTF.BorrowedBytes,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> PAL.Crypto.VectorUInt8 {
        let key = SymmetricKey(data: key.asNonNullBytes)
        switch hashFunction {
        case .SHA_1:
            return makeVectorUInt8(copying: CryptoKit.HMAC<Insecure.SHA1>.authenticationCode(for: data.asNonNullBytes, using: key))
        case .SHA_256:
            return makeVectorUInt8(copying: CryptoKit.HMAC<SHA256>.authenticationCode(for: data.asNonNullBytes, using: key))
        case .SHA_384:
            return makeVectorUInt8(copying: CryptoKit.HMAC<SHA384>.authenticationCode(for: data.asNonNullBytes, using: key))
        case .SHA_512:
            return makeVectorUInt8(copying: CryptoKit.HMAC<SHA512>.authenticationCode(for: data.asNonNullBytes, using: key))
        case .DEPRECATED_SHA_224:
            fatalError("DEPRECATED_SHA_224 is not supported")
        @unknown default:
            fatalError("Unknown PAL.Crypto.CryptoDigestHashFunction enum case value: \(hashFunction.rawValue)")
        }
    }

    @_expose(Cxx)
    static func verify(
        mac: WTF.BorrowedBytes,
        key: WTF.BorrowedBytes,
        data: WTF.BorrowedBytes,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> Bool {
        let key = SymmetricKey(data: key.asNonNullBytes)
        switch hashFunction {
        case .SHA_1:
            return CryptoKit.HMAC<Insecure.SHA1>
                .isValidAuthenticationCode(mac.asNonNullBytes, authenticating: data.asNonNullBytes, using: key)
        case .SHA_256:
            return CryptoKit.HMAC<SHA256>.isValidAuthenticationCode(mac.asNonNullBytes, authenticating: data.asNonNullBytes, using: key)
        case .SHA_384:
            return CryptoKit.HMAC<SHA384>.isValidAuthenticationCode(mac.asNonNullBytes, authenticating: data.asNonNullBytes, using: key)
        case .SHA_512:
            return CryptoKit.HMAC<SHA512>.isValidAuthenticationCode(mac.asNonNullBytes, authenticating: data.asNonNullBytes, using: key)
        case .DEPRECATED_SHA_224:
            fatalError("DEPRECATED_SHA_224 is not supported")
        @unknown default:
            fatalError("Unknown PAL.CryptoDigestHashFunction enum case value: \(hashFunction.rawValue)")
        }
    }
}

// https://www.ietf.org/rfc/rfc5869.txt
private let hkdfInputSizeLimitSHA1 = 255 * Insecure.SHA1.byteCount * 8
private let hkdfInputSizeLimitSHA256 = 255 * SHA256.byteCount * 8
private let hkdfInputSizeLimitSHA384 = 255 * SHA384.byteCount * 8
private let hkdfInputSizeLimitSHA512 = 255 * SHA512.byteCount * 8

@_expose(Cxx)
final class HKDF {
    @_expose(Cxx)
    static func deriveBits(
        key: WTF.BorrowedBytes,
        salt: WTF.BorrowedBytes,
        info: WTF.BorrowedBytes,
        outputBitCount: Int,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        if outputBitCount <= 0 || outputBitCount % 8 != 0 {
            returnValue.errorCode = .InvalidArgument
            return returnValue
        } else {
            returnValue.errorCode = .Success
        }
        let key = SymmetricKey(data: key.asNonNullBytes)
        switch hashFunction {
        case .SHA_1:
            if outputBitCount > hkdfInputSizeLimitSHA1 {
                returnValue.errorCode = .InvalidArgument
                break
            }
            returnValue.result = makeVectorUInt8(
                copying:
                    CryptoKit.HKDF<Insecure.SHA1>
                    .deriveKey(
                        inputKeyMaterial: key,
                        salt: salt.asNonNullBytes,
                        info: info.asNonNullBytes,
                        outputByteCount: outputBitCount / 8
                    )
            )

        case .SHA_256:
            if outputBitCount > hkdfInputSizeLimitSHA256 {
                returnValue.errorCode = .InvalidArgument
                break
            }
            returnValue.result = makeVectorUInt8(
                copying:
                    CryptoKit.HKDF<SHA256>
                    .deriveKey(
                        inputKeyMaterial: key,
                        salt: salt.asNonNullBytes,
                        info: info.asNonNullBytes,
                        outputByteCount: outputBitCount / 8
                    )
            )

        case .SHA_384:
            if outputBitCount > hkdfInputSizeLimitSHA384 {
                returnValue.errorCode = .InvalidArgument
                break
            }
            returnValue.result = makeVectorUInt8(
                copying:
                    CryptoKit.HKDF<SHA384>
                    .deriveKey(
                        inputKeyMaterial: key,
                        salt: salt.asNonNullBytes,
                        info: info.asNonNullBytes,
                        outputByteCount: outputBitCount / 8
                    )
            )

        case .SHA_512:
            if outputBitCount > hkdfInputSizeLimitSHA512 {
                returnValue.errorCode = .InvalidArgument
                break
            }
            returnValue.result = makeVectorUInt8(
                copying:
                    CryptoKit.HKDF<SHA512>
                    .deriveKey(
                        inputKeyMaterial: key,
                        salt: salt.asNonNullBytes,
                        info: info.asNonNullBytes,
                        outputByteCount: outputBitCount / 8
                    )
            )

        case .DEPRECATED_SHA_224:
            fatalError("DEPRECATED_SHA_224 is not supported")

        @unknown default:
            fatalError("Unknown PAL.CryptoDigestHashFunction enum case value: \(hashFunction.rawValue)")
        }

        return returnValue
    }
}

// Thin wrappers adapting WTF.BorrowedBytes (which conforms to ContiguousBytes and
// DataProtocol) to the CryptoKit key/signature initializers and helpers. No
// `unsafe`: BorrowedBytes borrows the C++ bytes with no copy and crashes cleanly
// if the borrow has been revoked.

extension WTF.BorrowedBytes {
    // The key/signature decoders below all reject empty input up front, because
    // CryptoKit's representation initializers mishandle it. Returns self so a
    // decoder can guard and forward in a single expression.
    fileprivate func requireNonEmpty() throws -> WTF.BorrowedBytes {
        if isEmpty {
            throw LocalErrors.emptySpan
        }
        return self
    }
}

extension P256.Signing.ECDSASignature {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(rawRepresentation: span.requireNonEmpty())
    }
}

extension P384.Signing.ECDSASignature {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(rawRepresentation: span.requireNonEmpty())
    }
}

extension P521.Signing.ECDSASignature {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(rawRepresentation: span.requireNonEmpty())
    }
}

extension P256.Signing.PublicKey {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(x963Representation: span.requireNonEmpty())
    }

    init(spanCompressed: WTF.BorrowedBytes) throws {
        try self.init(compressedRepresentation: spanCompressed.requireNonEmpty())
    }
}

extension P384.Signing.PublicKey {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(x963Representation: span.requireNonEmpty())
    }

    init(spanCompressed: WTF.BorrowedBytes) throws {
        try self.init(compressedRepresentation: spanCompressed.requireNonEmpty())
    }
}

extension P521.Signing.PublicKey {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(x963Representation: span.requireNonEmpty())
    }

    init(spanCompressed: WTF.BorrowedBytes) throws {
        try self.init(compressedRepresentation: spanCompressed.requireNonEmpty())
    }
}

extension P256.Signing.PrivateKey {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(x963Representation: span.requireNonEmpty())
    }
}

extension P384.Signing.PrivateKey {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(x963Representation: span.requireNonEmpty())
    }
}

extension P521.Signing.PrivateKey {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(x963Representation: span.requireNonEmpty())
    }
}

extension Curve25519.Signing.PrivateKey {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(rawRepresentation: span.requireNonEmpty())
    }

    func signature(span: WTF.BorrowedBytes) throws -> PAL.Crypto.VectorUInt8 {
        makeVectorUInt8(copying: try self.signature(for: span.asNonNullBytes))
    }
}

extension Curve25519.Signing.PublicKey {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(rawRepresentation: span.requireNonEmpty())
    }

    func isValidSignature(signature: WTF.BorrowedBytes, data: WTF.BorrowedBytes) -> Bool {
        if signature.isEmpty || data.isEmpty {
            return false
        }
        return self.isValidSignature(signature, for: data)
    }
}

extension Curve25519.KeyAgreement.PrivateKey {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(rawRepresentation: span.requireNonEmpty())
    }

    func sharedSecretFromKeyAgreement(pubSpan: WTF.BorrowedBytes) throws -> PAL.Crypto.VectorUInt8 {
        let pub = try Curve25519.KeyAgreement.PublicKey(rawRepresentation: pubSpan.requireNonEmpty())
        return makeVectorUInt8(copying: try self.sharedSecretFromKeyAgreement(with: pub))
    }
}

extension Curve25519.KeyAgreement.PublicKey {
    init(span: WTF.BorrowedBytes) throws {
        try self.init(rawRepresentation: span.requireNonEmpty())
    }
}
