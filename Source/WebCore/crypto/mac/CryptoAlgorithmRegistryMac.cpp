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
#include "CryptoAlgorithmRegistry.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmAES_CBC.h"
#include "CryptoAlgorithmHMAC.h"
#include "CryptoAlgorithmRSASSA_PKCS1_v1_5.h"
#include "CryptoAlgorithmSHA1.h"
#include "CryptoAlgorithmSHA224.h"
#include "CryptoAlgorithmSHA256.h"
#include "CryptoAlgorithmSHA384.h"
#include "CryptoAlgorithmSHA512.h"

namespace WebCore {

void CryptoAlgorithmRegistry::platformRegisterAlgorithms()
{
    registerAlgorithm(CryptoAlgorithmAES_CBC::s_name, CryptoAlgorithmAES_CBC::s_identifier, CryptoAlgorithmAES_CBC::create);
    registerAlgorithm(CryptoAlgorithmHMAC::s_name, CryptoAlgorithmHMAC::s_identifier, CryptoAlgorithmHMAC::create);
    registerAlgorithm(CryptoAlgorithmRSASSA_PKCS1_v1_5::s_name, CryptoAlgorithmRSASSA_PKCS1_v1_5::s_identifier, CryptoAlgorithmRSASSA_PKCS1_v1_5::create);
    registerAlgorithm(CryptoAlgorithmSHA1::s_name, CryptoAlgorithmSHA1::s_identifier, CryptoAlgorithmSHA1::create);
    registerAlgorithm(CryptoAlgorithmSHA224::s_name, CryptoAlgorithmSHA224::s_identifier, CryptoAlgorithmSHA224::create);
    registerAlgorithm(CryptoAlgorithmSHA256::s_name, CryptoAlgorithmSHA256::s_identifier, CryptoAlgorithmSHA256::create);
    registerAlgorithm(CryptoAlgorithmSHA384::s_name, CryptoAlgorithmSHA384::s_identifier, CryptoAlgorithmSHA384::create);
    registerAlgorithm(CryptoAlgorithmSHA512::s_name, CryptoAlgorithmSHA512::s_identifier, CryptoAlgorithmSHA512::create);
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
