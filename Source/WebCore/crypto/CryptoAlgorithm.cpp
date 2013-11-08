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

#include "config.h"
#include "CryptoAlgorithm.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "ExceptionCode.h"

namespace WebCore {

CryptoAlgorithm::CryptoAlgorithm()
{
}

CryptoAlgorithm::~CryptoAlgorithm()
{
}

void CryptoAlgorithm::encrypt(const CryptoAlgorithmParameters&, const CryptoKey&, const Vector<CryptoOperationData>&, std::unique_ptr<PromiseWrapper>, ExceptionCode& ec)
{
    ec = NOT_SUPPORTED_ERR;
}

void CryptoAlgorithm::decrypt(const CryptoAlgorithmParameters&, const CryptoKey&, const Vector<CryptoOperationData>&, std::unique_ptr<PromiseWrapper>, ExceptionCode& ec)
{
    ec = NOT_SUPPORTED_ERR;
}

void CryptoAlgorithm::sign(const CryptoAlgorithmParameters&, const CryptoKey&, const Vector<CryptoOperationData>&, std::unique_ptr<PromiseWrapper>, ExceptionCode& ec)
{
    ec = NOT_SUPPORTED_ERR;
}

void CryptoAlgorithm::verify(const CryptoAlgorithmParameters&, const CryptoKey&, const CryptoOperationData&, const Vector<CryptoOperationData>&, std::unique_ptr<PromiseWrapper>, ExceptionCode& ec)
{
    ec = NOT_SUPPORTED_ERR;
}

void CryptoAlgorithm::digest(const CryptoAlgorithmParameters&, const Vector<CryptoOperationData>&, std::unique_ptr<PromiseWrapper>, ExceptionCode& ec)
{
    ec = NOT_SUPPORTED_ERR;
}

void CryptoAlgorithm::generateKey(const CryptoAlgorithmParameters&, bool, CryptoKeyUsage, std::unique_ptr<PromiseWrapper>, ExceptionCode& ec)
{
    ec = NOT_SUPPORTED_ERR;
}

void CryptoAlgorithm::deriveKey(const CryptoAlgorithmParameters&, const CryptoKey&, CryptoAlgorithm*, bool, CryptoKeyUsage, std::unique_ptr<PromiseWrapper>, ExceptionCode& ec)
{
    ec = NOT_SUPPORTED_ERR;
}

void CryptoAlgorithm::deriveBits(const CryptoAlgorithmParameters&, const CryptoKey&, unsigned long, std::unique_ptr<PromiseWrapper>, ExceptionCode& ec)
{
    ec = NOT_SUPPORTED_ERR;
}

void CryptoAlgorithm::importKey(const CryptoAlgorithmParameters&, const CryptoKeyData&, bool, CryptoKeyUsage, std::unique_ptr<PromiseWrapper>, ExceptionCode& ec)
{
    ec = NOT_SUPPORTED_ERR;
}

}

#endif
