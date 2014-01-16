/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef CryptoAlgorithmRSA_OAEP_h
#define CryptoAlgorithmRSA_OAEP_h

#include "CryptoAlgorithm.h"

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

class CryptoAlgorithmRsaOaepParams;
class CryptoKeyRSA;

class CryptoAlgorithmRSA_OAEP final : public CryptoAlgorithm {
public:
    static const char* const s_name;
    static const CryptoAlgorithmIdentifier s_identifier = CryptoAlgorithmIdentifier::RSA_OAEP;

    static std::unique_ptr<CryptoAlgorithm> create();

    virtual CryptoAlgorithmIdentifier identifier() const override;

    virtual void encrypt(const CryptoAlgorithmParameters&, const CryptoKey&, const CryptoOperationData&, VectorCallback, VoidCallback failureCallback, ExceptionCode&) override;
    virtual void decrypt(const CryptoAlgorithmParameters&, const CryptoKey&, const CryptoOperationData&, VectorCallback, VoidCallback failureCallback, ExceptionCode&) override;
    virtual void generateKey(const CryptoAlgorithmParameters&, bool extractable, CryptoKeyUsage, KeyOrKeyPairCallback, VoidCallback failureCallback, ExceptionCode&) override;
    virtual void importKey(const CryptoAlgorithmParameters&, const CryptoKeyData&, bool extractable, CryptoKeyUsage, KeyCallback, VoidCallback failureCallback, ExceptionCode&) override;

private:
    CryptoAlgorithmRSA_OAEP();
    virtual ~CryptoAlgorithmRSA_OAEP();

    bool keyAlgorithmMatches(const CryptoAlgorithmRsaOaepParams& algorithmParameters, const CryptoKey&) const;
    void platformEncrypt(const CryptoAlgorithmRsaOaepParams&, const CryptoKeyRSA&, const CryptoOperationData&, VectorCallback, VoidCallback failureCallback, ExceptionCode&);
    void platformDecrypt(const CryptoAlgorithmRsaOaepParams&, const CryptoKeyRSA&, const CryptoOperationData&, VectorCallback, VoidCallback failureCallback, ExceptionCode&);
};

}

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CryptoAlgorithmRSA_OAEP_h
