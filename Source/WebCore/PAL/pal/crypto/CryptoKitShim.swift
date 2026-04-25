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

// FIXME: (rdar://164560176) resolve the many 'unsafe' statements here

// FIXME: No symbols in this file should be `public`. Remove when support for compilers < 6.2.3 is no longer needed.

private enum LocalErrors: Error {
    case invalidArgument
}

// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
public final class AesGcm {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func encrypt(
        key: PAL.Crypto.SpanConstUInt8,
        iv: PAL.Crypto.SpanConstUInt8,
        ad: PAL.Crypto.SpanConstUInt8,
        message: PAL.Crypto.SpanConstUInt8,
        desiredTagLengthInBytes: Int
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            if unsafe iv.size() == 0 {
                returnValue.errorCode = .InvalidArgument
                return returnValue
            }
            let sealedBox: AES.GCM.SealedBox = try unsafe AES.GCM.seal(message, key: key, iv: iv, ad: ad)
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
            returnValue.result = result.copyToVectorUInt8()
            return returnValue
        } catch {
            returnValue.errorCode = .EncryptionFailed
        }
        return returnValue
    }
}

// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
public final class AesKw {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func wrap(
        keyToWrap: PAL.Crypto.SpanConstUInt8,
        using: PAL.Crypto.SpanConstUInt8
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            let result = try unsafe AES.KeyWrap.wrap(keyToWrap, using: using)
            returnValue.errorCode = .Success
            returnValue.result = result
        } catch {
            returnValue.errorCode = .EncryptionFailed
        }
        return returnValue
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func unwrap(
        wrappedKey: PAL.Crypto.SpanConstUInt8,
        using: PAL.Crypto.SpanConstUInt8
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            let result = try unsafe AES.KeyWrap.unwrap(
                wrappedKey,
                using: using
            )
            returnValue.errorCode = .Success
            returnValue.result = result.copyToVectorUInt8()
        } catch {
            returnValue.errorCode = .EncryptionFailed
        }
        return returnValue
    }
}

// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
public final class Digest {
    private var ctx: any CryptoKit.HashFunction

    private init<T: CryptoKit.HashFunction>(_: T.Type) {
        ctx = T()
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func sha1Init() -> Digest {
        Self(Insecure.SHA1.self)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func sha256Init() -> Digest {
        Self(SHA256.self)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func sha384Init() -> Digest {
        Self(SHA384.self)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func sha512Init() -> Digest {
        Self(SHA512.self)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func update(_ data: PAL.Crypto.SpanConstUInt8) {
        unsafe ctx.update(data: data)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func finalize() -> PAL.Crypto.VectorUInt8 {
        ctx.finalize().copyToVectorUInt8()
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func sha1(_ data: PAL.Crypto.SpanConstUInt8) -> PAL.Crypto.VectorUInt8 {
        unsafe digest(data, t: Insecure.SHA1.self)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func sha256(_ data: PAL.Crypto.SpanConstUInt8) -> PAL.Crypto.VectorUInt8 {
        unsafe digest(data, t: SHA256.self)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func sha384(_ data: PAL.Crypto.SpanConstUInt8) -> PAL.Crypto.VectorUInt8 {
        unsafe digest(data, t: SHA384.self)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func sha512(_ data: PAL.Crypto.SpanConstUInt8) -> PAL.Crypto.VectorUInt8 {
        unsafe digest(data, t: SHA512.self)
    }

    fileprivate static func digest<T: CryptoKit.HashFunction>(_ data: PAL.Crypto.SpanConstUInt8, _: T.Type) -> T.Digest {
        var hasher = T()
        unsafe hasher.update(data: data)
        return hasher.finalize()
    }

    fileprivate static func digest<T: CryptoKit.HashFunction>(_ data: PAL.Crypto.SpanConstUInt8, t: T.Type) -> PAL.Crypto.VectorUInt8 {
        unsafe Self.digest(data, t).copyToVectorUInt8()
    }

    fileprivate static func digest(
        _ data: PAL.Crypto.SpanConstUInt8,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> any CryptoKit.Digest {
        switch hashFunction {
        case .SHA_256:
            return unsafe digest(data, SHA256.self)
        case .SHA_384:
            return unsafe digest(data, SHA384.self)
        case .SHA_512:
            return unsafe digest(data, SHA512.self)
        case .SHA_1:
            return unsafe digest(data, Insecure.SHA1.self)
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

// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
public struct ECKey {
    private let key: ECKeyInternal

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public init(curve: PAL.Crypto.ECNamedCurve) {
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func toPub() -> ECKey {
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func importX963Pub(data: PAL.Crypto.SpanConstUInt8, curve: PAL.Crypto.ECNamedCurve) -> ECKey? {
        do {
            return switch curve {
            case .P256:
                unsafe ECKey(internalKey: .publicKey(.p256(try P256.Signing.PublicKey(span: data))))
            case .P384:
                unsafe ECKey(internalKey: .publicKey(.p384(try P384.Signing.PublicKey(span: data))))
            case .P521:
                unsafe ECKey(internalKey: .publicKey(.p521(try P521.Signing.PublicKey(span: data))))
            @unknown default:
                fatalError()
            }
        } catch {
            return nil
        }
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func exportX963Pub() -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func importCompressedPub(data: PAL.Crypto.SpanConstUInt8, curve: PAL.Crypto.ECNamedCurve) -> ECKey? {
        do {
            return switch curve {
            case .P256:
                unsafe ECKey(publicKey: .p256(try P256.Signing.PublicKey(spanCompressed: data)))
            case .P384:
                unsafe ECKey(publicKey: .p384(try P384.Signing.PublicKey(spanCompressed: data)))
            case .P521:
                unsafe ECKey(publicKey: .p521(try P521.Signing.PublicKey(spanCompressed: data)))
            @unknown default:
                fatalError()
            }
        } catch {
            return nil
        }
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func importX963Private(data: PAL.Crypto.SpanConstUInt8, curve: PAL.Crypto.ECNamedCurve) -> ECKey? {
        do {
            return switch curve {
            case .P256:
                unsafe ECKey(privateKey: .p256(try P256.Signing.PrivateKey(span: data)))
            case .P384:
                unsafe ECKey(privateKey: .p384(try P384.Signing.PrivateKey(span: data)))
            case .P521:
                unsafe ECKey(privateKey: .p521(try P521.Signing.PrivateKey(span: data)))
            @unknown default:
                fatalError()
            }
        } catch {
            return nil
        }
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func exportX963Private() -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func sign(
        message: PAL.Crypto.SpanConstUInt8,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            switch try getInternalPrivate() {
            case .p256(let cryptoKey):
                returnValue.result =
                    try unsafe cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                    .rawRepresentation.copyToVectorUInt8()
            case .p384(let cryptoKey):
                returnValue.result =
                    try unsafe cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                    .rawRepresentation.copyToVectorUInt8()
            case .p521(let cryptoKey):
                returnValue.result =
                    try unsafe cryptoKey.signature(for: Digest.digest(message, hashFunction: hashFunction))
                    .rawRepresentation.copyToVectorUInt8()
            }
            returnValue.errorCode = .Success
        } catch {
            returnValue.errorCode = .FailedToSign
        }
        return returnValue
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func verify(
        message: PAL.Crypto.SpanConstUInt8,
        signature: PAL.Crypto.SpanConstUInt8,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            let internalPublic = try getInternalPublic()
            switch internalPublic {
            case .p256(let cryptoKey):
                returnValue.errorCode =
                    unsafe cryptoKey.isValidSignature(
                        try P256.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction)
                    )
                    ? .Success : .FailedToVerify
            case .p384(let cryptoKey):
                returnValue.errorCode =
                    unsafe cryptoKey.isValidSignature(
                        try P384.Signing.ECDSASignature(span: signature),
                        for: Digest.digest(message, hashFunction: hashFunction)
                    )
                    ? .Success : .FailedToVerify
            case .p521(let cryptoKey):
                returnValue.errorCode =
                    unsafe cryptoKey.isValidSignature(
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func deriveBits(publicKey: ECKey) -> PAL.Crypto.CryptoOperationReturnValue {
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
                    returnValue.result = derived.copyToVectorUInt8()
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
                    returnValue.result = derived.copyToVectorUInt8()
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

// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
public final class EdKey {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func generatePrivateKey(algo: PAL.Crypto.EdSigningAlgorithm) -> PAL.Crypto.VectorUInt8 {
        switch algo {
        case .ED25519:
            Curve25519.Signing.PrivateKey().rawRepresentation.copyToVectorUInt8()
        case .ED448:
            Data(count: 0).copyToVectorUInt8()
        @unknown default:
            fatalError()
        }
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func generatePrivateKeyKeyAgreement(algo: PAL.Crypto.EdKeyAgreementAlgorithm) -> PAL.Crypto.VectorUInt8 {
        switch algo {
        case .X25519:
            Curve25519.KeyAgreement.PrivateKey().rawRepresentation.copyToVectorUInt8()
        case .X448:
            Data(count: 0).copyToVectorUInt8()
        @unknown default:
            fatalError()
        }
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func privateToPublic(
        algo: PAL.Crypto.EdSigningAlgorithm,
        privateKey: PAL.Crypto.SpanConstUInt8
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            if unsafe privateKey.size() != 32 {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .ED25519:
                returnValue.result = try unsafe Curve25519.Signing.PrivateKey(span: privateKey).publicKey
                    .rawRepresentation.copyToVectorUInt8()
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func privateToPublicKeyAgreement(
        algo: PAL.Crypto.EdKeyAgreementAlgorithm,
        privateKey: PAL.Crypto.SpanConstUInt8
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            if unsafe privateKey.size() != 32 {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .X25519:
                returnValue.result = try unsafe Curve25519.KeyAgreement.PrivateKey(span: privateKey).publicKey
                    .rawRepresentation.copyToVectorUInt8()
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func validateKeyPair(
        algo: PAL.Crypto.EdSigningAlgorithm,
        privateKey: PAL.Crypto.SpanConstUInt8,
        publicKey: PAL.Crypto.SpanConstUInt8
    ) -> Bool {
        do {
            if unsafe (privateKey.size() != 32 || publicKey.size() != 32) {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .ED25519:
                let derivedPublicKey = try unsafe Curve25519.Signing.PrivateKey(span: privateKey).publicKey.rawRepresentation
                let importedPublicKey = try unsafe Curve25519.Signing.PublicKey(span: publicKey).rawRepresentation
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func validateKeyPairKeyAgreement(
        algo: PAL.Crypto.EdKeyAgreementAlgorithm,
        privateKey: PAL.Crypto.SpanConstUInt8,
        publicKey: PAL.Crypto.SpanConstUInt8
    ) -> Bool {
        do {
            if unsafe (privateKey.size() != 32 || publicKey.size() != 32) {
                throw LocalErrors.invalidArgument
            }
            switch algo {
            case .X25519:
                let derivedPublicKey = try unsafe Curve25519.KeyAgreement.PrivateKey(span: privateKey).publicKey.rawRepresentation
                let importedPublicKey = try unsafe Curve25519.KeyAgreement.PublicKey(span: publicKey).rawRepresentation
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func sign(
        algo: PAL.Crypto.EdSigningAlgorithm,
        privateKey: PAL.Crypto.SpanConstUInt8,
        data: PAL.Crypto.SpanConstUInt8
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            switch algo {
            case .ED25519:
                let privateKeyImported = try unsafe Curve25519.Signing.PrivateKey(span: privateKey)
                returnValue.result = try unsafe privateKeyImported.signature(span: data)
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func verify(
        algo: PAL.Crypto.EdSigningAlgorithm,
        publicKey: PAL.Crypto.SpanConstUInt8,
        signature: PAL.Crypto.SpanConstUInt8,
        data: PAL.Crypto.SpanConstUInt8
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            switch algo {
            case .ED25519:
                let publicKeyImported = try unsafe Curve25519.Signing.PublicKey(span: publicKey)
                returnValue.errorCode =
                    unsafe publicKeyImported.isValidSignature(signature: signature, data: data)
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

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func deriveBits(
        algo: PAL.Crypto.EdKeyAgreementAlgorithm,
        privateKey: PAL.Crypto.SpanConstUInt8,
        publicKey: PAL.Crypto.SpanConstUInt8
    ) -> PAL.Crypto.CryptoOperationReturnValue {
        var returnValue = PAL.Crypto.CryptoOperationReturnValue()
        do {
            switch algo {
            case .X25519:
                let privateKeyImported = try unsafe Curve25519.KeyAgreement.PrivateKey(span: privateKey)
                returnValue.result = try unsafe privateKeyImported.sharedSecretFromKeyAgreement(pubSpan: publicKey)
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

// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
public final class HMAC {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func sign(
        key: PAL.Crypto.SpanConstUInt8,
        data: PAL.Crypto.SpanConstUInt8,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> PAL.Crypto.VectorUInt8 {
        switch hashFunction {
        case .SHA_1:
            return unsafe CryptoKit.HMAC<Insecure.SHA1>.authenticationCode(data: data, key: key)
        case .SHA_256:
            return unsafe CryptoKit.HMAC<SHA256>.authenticationCode(data: data, key: key)
        case .SHA_384:
            return unsafe CryptoKit.HMAC<SHA384>.authenticationCode(data: data, key: key)
        case .SHA_512:
            return unsafe CryptoKit.HMAC<SHA512>.authenticationCode(data: data, key: key)
        case .DEPRECATED_SHA_224:
            fatalError("DEPRECATED_SHA_224 is not supported")
        @unknown default:
            fatalError("Unknown PAL.Crypto.CryptoDigestHashFunction enum case value: \(hashFunction.rawValue)")
        }
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func verify(
        mac: PAL.Crypto.SpanConstUInt8,
        key: PAL.Crypto.SpanConstUInt8,
        data: PAL.Crypto.SpanConstUInt8,
        hashFunction: PAL.Crypto.CryptoDigestHashFunction
    ) -> Bool {
        switch hashFunction {
        case .SHA_1:
            return unsafe CryptoKit.HMAC<Insecure.SHA1>
                .isValidAuthenticationCode(
                    mac: mac,
                    data: data,
                    key: key
                )
        case .SHA_256:
            return unsafe CryptoKit.HMAC<SHA256>.isValidAuthenticationCode(mac: mac, data: data, key: key)
        case .SHA_384:
            return unsafe CryptoKit.HMAC<SHA384>.isValidAuthenticationCode(mac: mac, data: data, key: key)
        case .SHA_512:
            return unsafe CryptoKit.HMAC<SHA512>.isValidAuthenticationCode(mac: mac, data: data, key: key)
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

// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
public final class HKDF {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public static func deriveBits(
        key: PAL.Crypto.SpanConstUInt8,
        salt: PAL.Crypto.SpanConstUInt8,
        info: PAL.Crypto.SpanConstUInt8,
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
        switch hashFunction {
        case .SHA_1:
            if outputBitCount > hkdfInputSizeLimitSHA1 {
                returnValue.errorCode = .InvalidArgument
                break
            }
            returnValue.result =
                unsafe CryptoKit.HKDF<Insecure.SHA1>
                .deriveKey(
                    inputKeyMaterial: key,
                    salt: salt,
                    info: info,
                    outputByteCount: outputBitCount / 8
                )

        case .SHA_256:
            if outputBitCount > hkdfInputSizeLimitSHA256 {
                returnValue.errorCode = .InvalidArgument
                break
            }
            returnValue.result =
                unsafe CryptoKit.HKDF<SHA256>
                .deriveKey(
                    inputKeyMaterial: key,
                    salt: salt,
                    info: info,
                    outputByteCount: outputBitCount / 8
                )

        case .SHA_384:
            if outputBitCount > hkdfInputSizeLimitSHA384 {
                returnValue.errorCode = .InvalidArgument
                break
            }
            returnValue.result =
                unsafe CryptoKit.HKDF<SHA384>
                .deriveKey(
                    inputKeyMaterial: key,
                    salt: salt,
                    info: info,
                    outputByteCount: outputBitCount / 8
                )

        case .SHA_512:
            if outputBitCount > hkdfInputSizeLimitSHA512 {
                returnValue.errorCode = .InvalidArgument
                break
            }
            returnValue.result =
                unsafe CryptoKit.HKDF<SHA512>
                .deriveKey(
                    inputKeyMaterial: key,
                    salt: salt,
                    info: info,
                    outputByteCount: outputBitCount / 8
                )

        case .DEPRECATED_SHA_224:
            fatalError("DEPRECATED_SHA_224 is not supported")

        @unknown default:
            fatalError("Unknown PAL.CryptoDigestHashFunction enum case value: \(hashFunction.rawValue)")
        }

        return returnValue
    }
}
