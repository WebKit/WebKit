/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "CryptoKeyEC.h"

#if ENABLE(WEB_CRYPTO)

#include "NotImplemented.h"

namespace WebCore {

size_t CryptoKeyEC::keySizeInBits() const
{
    notImplemented();
    return 0;
}

bool CryptoKeyEC::platformSupportedCurve(NamedCurve)
{
    notImplemented();
    return false;
}

Optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier, NamedCurve, bool extractable, CryptoKeyUsageBitmap)
{
    UNUSED_PARAM(extractable);
    notImplemented();
    return { };
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap)
{
    UNUSED_PARAM(keyData);
    UNUSED_PARAM(extractable);
    notImplemented();
    return nullptr;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, bool extractable, CryptoKeyUsageBitmap)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(extractable);
    notImplemented();
    return nullptr;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPrivate(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, Vector<uint8_t>&& d, bool extractable, CryptoKeyUsageBitmap)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(d);
    UNUSED_PARAM(extractable);
    notImplemented();
    return nullptr;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportSpki(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap)
{
    UNUSED_PARAM(keyData);
    UNUSED_PARAM(extractable);
    notImplemented();
    return nullptr;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportPkcs8(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap)
{
    UNUSED_PARAM(keyData);
    UNUSED_PARAM(extractable);
    notImplemented();
    return nullptr;
}

Vector<uint8_t> CryptoKeyEC::platformExportRaw() const
{
    notImplemented();
    return { };
}

bool CryptoKeyEC::platformAddFieldElements(JsonWebKey&) const
{
    notImplemented();
    return false;
}

Vector<uint8_t> CryptoKeyEC::platformExportSpki() const
{
    notImplemented();
    return { };
}

Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const
{
    notImplemented();
    return { };
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
