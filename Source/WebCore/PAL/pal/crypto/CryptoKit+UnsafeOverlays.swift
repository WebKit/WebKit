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

import CryptoKit
import Foundation
import pal.Core.crypto.CryptoTypes

// FIXME: (rdar://164560176) resolve the many 'unsafe' statements here

private enum UnsafeErrors: Error {
    case invalidLength
    case emptySpan
}

extension Data {
    fileprivate static func temporaryDataFromSpan(spanNoCopy: PAL.Crypto.SpanConstUInt8) -> Data {
        guard unsafe spanNoCopy.empty() else {
            return unsafe Data(
                bytesNoCopy: UnsafeMutablePointer(mutating: spanNoCopy.__dataUnsafe()),
                count: spanNoCopy.size(),
                deallocator: .none
            )
        }

        // CryptoKit does not support a null pointer with zero length. We instead need to pass an empty Data. This class provides that.
        return Data()
    }
}

extension CryptoKit.HashFunction {
    mutating func update(data: PAL.Crypto.SpanConstUInt8) {
        if unsafe data.empty() {
            self.update(data: Data())
        } else {
            unsafe self.update(
                bufferPointer: UnsafeRawBufferPointer(
                    start: data.__dataUnsafe(),
                    count: data.size()
                )
            )
        }
    }
}

extension AES.GCM {
    static func seal(
        _ message: PAL.Crypto.SpanConstUInt8,
        key: PAL.Crypto.SpanConstUInt8,
        iv: PAL.Crypto.SpanConstUInt8,
        ad: PAL.Crypto.SpanConstUInt8
    ) throws -> AES.GCM.SealedBox {
        guard unsafe ad.size() > 0 else {
            return try unsafe AES.GCM.seal(
                Data.temporaryDataFromSpan(spanNoCopy: message),
                using: SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: key)),
                nonce: AES.GCM.Nonce(data: Data.temporaryDataFromSpan(spanNoCopy: iv))
            )
        }
        return try unsafe AES.GCM.seal(
            Data.temporaryDataFromSpan(spanNoCopy: message),
            using: SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: key)),
            nonce: AES.GCM.Nonce(data: Data.temporaryDataFromSpan(spanNoCopy: iv)),
            authenticating: Data.temporaryDataFromSpan(spanNoCopy: ad)
        )
    }
}

extension AES.KeyWrap {
    static func unwrap(_ wrapped: PAL.Crypto.SpanConstUInt8, using: PAL.Crypto.SpanConstUInt8) throws -> SymmetricKey {
        try unsafe AES.KeyWrap.unwrap(
            Data.temporaryDataFromSpan(spanNoCopy: wrapped),
            using: SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: using))
        )
    }

    static func wrap(_ keyToWrap: PAL.Crypto.SpanConstUInt8, using: PAL.Crypto.SpanConstUInt8) throws -> PAL.Crypto.VectorUInt8 {
        try unsafe AES.KeyWrap
            .wrap(
                SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: keyToWrap)),
                using: SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: using))
            )
            .copyToVectorUInt8()
    }
}

extension P256.Signing.ECDSASignature {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(rawRepresentation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }
}

extension P384.Signing.ECDSASignature {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(rawRepresentation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }
}

extension P521.Signing.ECDSASignature {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(rawRepresentation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }
}

extension P256.Signing.PublicKey {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(x963Representation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }

    init(spanCompressed: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe spanCompressed.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(
            compressedRepresentation: Data.temporaryDataFromSpan(spanNoCopy: spanCompressed)
        )
    }
}

extension P384.Signing.PublicKey {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(x963Representation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }

    init(spanCompressed: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe spanCompressed.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(
            compressedRepresentation: Data.temporaryDataFromSpan(spanNoCopy: spanCompressed)
        )
    }
}

extension P521.Signing.PublicKey {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(x963Representation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }

    init(spanCompressed: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe spanCompressed.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(
            compressedRepresentation: Data.temporaryDataFromSpan(spanNoCopy: spanCompressed)
        )
    }
}

extension P256.Signing.PrivateKey {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(x963Representation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }
}

extension P384.Signing.PrivateKey {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(x963Representation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }
}

extension P521.Signing.PrivateKey {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(x963Representation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }
}

extension Curve25519.Signing.PrivateKey {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(rawRepresentation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }

    func signature(span: PAL.Crypto.SpanConstUInt8) throws -> PAL.Crypto.VectorUInt8 {
        if unsafe span.empty() {
            return try self.signature(for: Data()).copyToVectorUInt8()
        }
        return try unsafe self.signature(for: Data.temporaryDataFromSpan(spanNoCopy: span))
            .copyToVectorUInt8()
    }
}

extension Curve25519.Signing.PublicKey {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(rawRepresentation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }

    func isValidSignature(signature: PAL.Crypto.SpanConstUInt8, data: PAL.Crypto.SpanConstUInt8) -> Bool {
        if unsafe (signature.empty() || data.empty()) {
            return false
        }

        return unsafe self.isValidSignature(
            Data.temporaryDataFromSpan(spanNoCopy: signature),
            for: Data.temporaryDataFromSpan(spanNoCopy: data)
        )
    }
}

extension Curve25519.KeyAgreement.PrivateKey {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(rawRepresentation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }

    func sharedSecretFromKeyAgreement(pubSpan: PAL.Crypto.SpanConstUInt8) throws -> PAL.Crypto.VectorUInt8 {
        if unsafe pubSpan.empty() {
            throw UnsafeErrors.emptySpan
        }
        let pub = try unsafe Curve25519.KeyAgreement.PublicKey(
            rawRepresentation: Data.temporaryDataFromSpan(spanNoCopy: pubSpan)
        )
        return try self.sharedSecretFromKeyAgreement(with: pub).copyToVectorUInt8()
    }
}

extension Curve25519.KeyAgreement.PublicKey {
    init(span: PAL.Crypto.SpanConstUInt8) throws {
        if unsafe span.empty() {
            throw UnsafeErrors.emptySpan
        }
        try unsafe self.init(rawRepresentation: Data.temporaryDataFromSpan(spanNoCopy: span))
    }
}

extension CryptoKit.HMAC {
    static func authenticationCode(data: PAL.Crypto.SpanConstUInt8, key: PAL.Crypto.SpanConstUInt8) -> PAL.Crypto.VectorUInt8 {
        unsafe self.authenticationCode(
            for: Data.temporaryDataFromSpan(spanNoCopy: data),
            using: SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: key))
        )
        .copyToVectorUInt8()
    }

    static func isValidAuthenticationCode(
        mac: PAL.Crypto.SpanConstUInt8,
        data: PAL.Crypto.SpanConstUInt8,
        key: PAL.Crypto.SpanConstUInt8
    ) -> Bool {
        unsafe Self.isValidAuthenticationCode(
            Data.temporaryDataFromSpan(spanNoCopy: mac),
            authenticating: Data.temporaryDataFromSpan(spanNoCopy: data),
            using: SymmetricKey(data: Data.temporaryDataFromSpan(spanNoCopy: key))
        )
    }
}

extension CryptoKit.HKDF {
    static func deriveKey(
        inputKeyMaterial: PAL.Crypto.SpanConstUInt8,
        salt: PAL.Crypto.SpanConstUInt8,
        info: PAL.Crypto.SpanConstUInt8,
        outputByteCount: Int
    ) -> PAL.Crypto.VectorUInt8 {
        unsafe Self.deriveKey(
            inputKeyMaterial: SymmetricKey(
                data: Data.temporaryDataFromSpan(spanNoCopy: inputKeyMaterial)
            ),
            salt: Data.temporaryDataFromSpan(spanNoCopy: salt),
            info: Data.temporaryDataFromSpan(spanNoCopy: info),
            outputByteCount: outputByteCount
        )
        .copyToVectorUInt8()
    }
}
