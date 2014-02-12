/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "SerializedCryptoKeyWrap.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CommonCryptoUtilities.h"
#include <CommonCrypto/CommonSymmetricKeywrap.h>
#include <wtf/text/CString.h>
#include <wtf/CryptographicUtilities.h>

namespace WebCore {

const NSUInteger currentSerializationVersion = 1;

const NSString* versionKey = @"version";
const NSString* wrappedKEKKey = @"wrappedKEK";
const NSString* encryptedKeyKey = @"encryptedKey";
const NSString* tagKey = @"tag";

inline Vector<uint8_t> vectorFromNSData(NSData* data)
{
    Vector<uint8_t> result;
    result.append((const uint8_t*)[data bytes], [data length]);
    return result;
}

bool wrapSerializedCryptoKey(const Vector<uint8_t>& masterKey, const Vector<uint8_t>& key, Vector<uint8_t>& result)
{
    Vector<uint8_t> kek(16);
    CCRandomCopyBytes(kCCRandomDefault, kek.data(), kek.size());

    Vector<uint8_t> wrappedKEK(CCSymmetricWrappedSize(kCCWRAPAES, kek.size()));

    size_t wrappedKEKSize = wrappedKEK.size();
    CCCryptorStatus status = CCSymmetricKeyWrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, masterKey.data(), masterKey.size(), kek.data(), kek.size(), wrappedKEK.data(), &wrappedKEKSize);
    if (status != kCCSuccess)
        return false;

    wrappedKEK.shrink(wrappedKEKSize);

    Vector<uint8_t> encryptedKey(key.size());
    size_t tagLength = 16;
    uint8_t tag[tagLength];

    status = CCCryptorGCM(kCCEncrypt, kCCAlgorithmAES128, kek.data(), kek.size(),
        nullptr, 0, // iv
        nullptr, 0, // auth data
        key.data(), key.size(),
        encryptedKey.data(),
        tag, &tagLength);

    if (status != kCCSuccess)
        return false;
    RELEASE_ASSERT(tagLength == 16);

    auto dictionary = @{
        versionKey: [NSNumber numberWithUnsignedInteger:currentSerializationVersion],
        wrappedKEKKey: [NSData dataWithBytes:wrappedKEK.data() length:wrappedKEK.size()],
        encryptedKeyKey: [NSData dataWithBytes:encryptedKey.data() length:encryptedKey.size()],
        tagKey: [NSData dataWithBytes:tag length:tagLength]
    };

    NSData* serialization = [NSPropertyListSerialization dataWithPropertyList:dictionary format:NSPropertyListBinaryFormat_v1_0 options:0 error:nullptr];
    if (!serialization)
        return false;

    result = vectorFromNSData(serialization);
    return true;
}

bool unwrapSerializedCryptoKey(const Vector<uint8_t>& masterKey, const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key)
{
    NSDictionary* dictionary = [NSPropertyListSerialization propertyListWithData:[NSData dataWithBytesNoCopy:(void*)wrappedKey.data() length:wrappedKey.size() freeWhenDone:NO] options:0 format:nullptr error:nullptr];
    if (!dictionary)
        return false;

    id versionObject = [dictionary objectForKey:versionKey];
    if (![versionObject isKindOfClass:[NSNumber class]])
        return false;
    if ([versionObject unsignedIntegerValue] > currentSerializationVersion)
        return false;

    id wrappedKEKObject = [dictionary objectForKey:wrappedKEKKey];
    if (![wrappedKEKObject isKindOfClass:[NSData class]])
        return false;
    Vector<uint8_t> wrappedKEK = vectorFromNSData(wrappedKEKObject);

    id encryptedKeyObject = [dictionary objectForKey:encryptedKeyKey];
    if (![encryptedKeyObject isKindOfClass:[NSData class]])
        return false;
    Vector<uint8_t> encryptedKey = vectorFromNSData(encryptedKeyObject);

    id tagObject = [dictionary objectForKey:tagKey];
    if (![tagObject isKindOfClass:[NSData class]])
        return false;
    Vector<uint8_t> tag = vectorFromNSData(tagObject);
    if (tag.size() != 16)
        return false;

    Vector<uint8_t> kek(CCSymmetricUnwrappedSize(kCCWRAPAES, wrappedKEK.size()));
    size_t kekSize = kek.size();
    CCCryptorStatus status = CCSymmetricKeyUnwrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, masterKey.data(), masterKey.size(), wrappedKEK.data(), wrappedKEK.size(), kek.data(), &kekSize);
    if (status != kCCSuccess)
        return false;
    kek.shrink(kekSize);

    size_t tagLength = 16;
    uint8_t actualTag[tagLength];

    key.resize(encryptedKey.size());
    status = CCCryptorGCM(kCCDecrypt, kCCAlgorithmAES128, kek.data(), kek.size(),
        nullptr, 0, // iv
        nullptr, 0, // auth data
        encryptedKey.data(), encryptedKey.size(),
        key.data(),
        actualTag, &tagLength);

    if (status != kCCSuccess)
        return false;
    RELEASE_ASSERT(tagLength == 16);

    if (constantTimeMemcmp(tag.data(), actualTag, tagLength))
        return false;

    return true;
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
