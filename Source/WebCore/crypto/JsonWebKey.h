/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "CryptoKeyUsage.h"
#include "RsaOtherPrimesInfo.h"
#include <wtf/Vector.h>

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

struct JsonWebKey {
    String kty;
    std::optional<String> use;
    // FIXME: Consider merging key_ops and usages.
    std::optional<Vector<CryptoKeyUsage>> key_ops;
    CryptoKeyUsageBitmap usages;
    std::optional<String> alg;

    std::optional<bool> ext;

    std::optional<String> crv;
    std::optional<String> x;
    std::optional<String> y;
    std::optional<String> d;
    std::optional<String> n;
    std::optional<String> e;
    std::optional<String> p;
    std::optional<String> q;
    std::optional<String> dp;
    std::optional<String> dq;
    std::optional<String> qi;
    std::optional<Vector<RsaOtherPrimesInfo>> oth;
    std::optional<String> k;
};

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
