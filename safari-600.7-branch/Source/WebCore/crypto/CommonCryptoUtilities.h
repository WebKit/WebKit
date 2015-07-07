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

#ifndef CommonCryptoUtilities_h
#define CommonCryptoUtilities_h

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmIdentifier.h"
#include <CommonCrypto/CommonCryptor.h>
#include <wtf/Vector.h>

#if defined(__has_include)
#if __has_include(<CommonCrypto/CommonRSACryptor.h>)
#include <CommonCrypto/CommonRSACryptor.h>
#endif
#endif

#if defined(__has_include) && (PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090)
#if __has_include(<CommonCrypto/CommonRandomSPI.h>)
#include <CommonCrypto/CommonRandomSPI.h>
#endif
#endif

#ifndef _CC_RSACRYPTOR_H_
enum {
    kCCDigestNone = 0,
    kCCDigestSHA1 = 8,
    kCCDigestSHA224 = 9,
    kCCDigestSHA256 = 10,
    kCCDigestSHA384 = 11,
    kCCDigestSHA512 = 12,
};
typedef uint32_t CCDigestAlgorithm;

enum {
    ccRSAKeyPublic = 0,
    ccRSAKeyPrivate = 1
};
typedef uint32_t CCRSAKeyType;

enum {
    ccPKCS1Padding = 1001,
    ccOAEPPadding = 1002
};
typedef uint32_t CCAsymmetricPadding;

enum {
    kCCNotVerified = -4306
};
#endif

typedef struct _CCBigNumRef *CCBigNumRef;

typedef struct __CCRandom *CCRandomRef;
extern const CCRandomRef kCCRandomDefault;
extern "C" int CCRandomCopyBytes(CCRandomRef rnd, void *bytes, size_t count);

typedef struct _CCRSACryptor *CCRSACryptorRef;
extern "C" CCCryptorStatus CCRSACryptorEncrypt(CCRSACryptorRef publicKey, CCAsymmetricPadding padding, const void *plainText, size_t plainTextLen, void *cipherText, size_t *cipherTextLen, const void *tagData, size_t tagDataLen, CCDigestAlgorithm digestType);
extern "C" CCCryptorStatus CCRSACryptorDecrypt(CCRSACryptorRef privateKey, CCAsymmetricPadding padding, const void *cipherText, size_t cipherTextLen, void *plainText, size_t *plainTextLen, const void *tagData, size_t tagDataLen, CCDigestAlgorithm digestType);
extern "C" CCCryptorStatus CCRSACryptorSign(CCRSACryptorRef privateKey, CCAsymmetricPadding padding, const void *hashToSign, size_t hashSignLen, CCDigestAlgorithm digestType, size_t saltLen, void *signedData, size_t *signedDataLen);
extern "C" CCCryptorStatus CCRSACryptorVerify(CCRSACryptorRef publicKey, CCAsymmetricPadding padding, const void *hash, size_t hashLen, CCDigestAlgorithm digestType, size_t saltLen, const void *signedData, size_t signedDataLen);
extern "C" CCCryptorStatus CCRSACryptorCreateFromData(CCRSAKeyType keyType, uint8_t *modulus, size_t modulusLength, uint8_t *exponent, size_t exponentLength, uint8_t *p, size_t pLength, uint8_t *q, size_t qLength, CCRSACryptorRef *ref);
extern "C" CCCryptorStatus CCRSACryptorGeneratePair(size_t keysize, uint32_t e, CCRSACryptorRef *publicKey, CCRSACryptorRef *privateKey);
extern "C" CCRSACryptorRef CCRSACryptorGetPublicKeyFromPrivateKey(CCRSACryptorRef privkey);
extern "C" void CCRSACryptorRelease(CCRSACryptorRef key);
extern "C" CCCryptorStatus CCRSAGetKeyComponents(CCRSACryptorRef rsaKey, uint8_t *modulus, size_t *modulusLength, uint8_t *exponent, size_t *exponentLength, uint8_t *p, size_t *pLength, uint8_t *q, size_t *qLength);
extern "C" CCRSAKeyType CCRSAGetKeyType(CCRSACryptorRef key);
extern "C" CCCryptorStatus CCCryptorGCM(CCOperation op, CCAlgorithm alg, const void* key, size_t keyLength, const void* iv, size_t ivLen, const void* aData, size_t aDataLen, const void* dataIn, size_t dataInLength, void* dataOut, const void* tag, size_t* tagLength);

namespace WebCore {

class CCBigNum {
public:
    CCBigNum(const uint8_t*, size_t);
    ~CCBigNum();

    CCBigNum(const CCBigNum&);
    CCBigNum(CCBigNum&&);
    CCBigNum& operator=(const CCBigNum&);
    CCBigNum& operator=(CCBigNum&&);

    Vector<uint8_t> data() const;

    CCBigNum operator-(uint32_t) const;
    CCBigNum operator%(const CCBigNum&) const;
    CCBigNum inverse(const CCBigNum& modulus) const;

private:
    CCBigNum(CCBigNumRef);

    CCBigNumRef m_number;
};

bool getCommonCryptoDigestAlgorithm(CryptoAlgorithmIdentifier, CCDigestAlgorithm&);

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CommonCryptoUtilities_h
