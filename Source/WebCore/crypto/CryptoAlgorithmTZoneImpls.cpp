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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CryptoAlgorithmAesCbcCfbParams.h"
#include "CryptoAlgorithmAesCtrParams.h"
#include "CryptoAlgorithmAesGcmParams.h"
#include "CryptoAlgorithmAesKeyParams.h"
#include "CryptoAlgorithmEcKeyParams.h"
#include "CryptoAlgorithmEcdhKeyDeriveParams.h"
#include "CryptoAlgorithmEcdsaParams.h"
#include "CryptoAlgorithmHkdfParams.h"
#include "CryptoAlgorithmHmacKeyParams.h"
#include "CryptoAlgorithmParameters.h"
#include "CryptoAlgorithmPbkdf2Params.h"
#include "CryptoAlgorithmRsaHashedImportParams.h"
#include "CryptoAlgorithmRsaKeyGenParams.h"
#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoAlgorithmRsaPssParams.h"
#include "CryptoAlgorithmX25519Params.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmAesCbcCfbParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmAesCtrParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmAesGcmParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmAesKeyParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmEcKeyParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmEcdhKeyDeriveParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmEcdsaParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmHkdfParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmHmacKeyParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmPbkdf2Params);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmRsaHashedImportParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmRsaKeyGenParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmRsaOaepParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmRsaPssParams);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmX25519Params);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CryptoAlgorithmParameters);

} // namespace WebCore
