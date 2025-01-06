/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "SerializedCryptoKeyWrap.h"

#import "CommonCryptoUtilities.h"
#import "LocalizedStrings.h"
#import "WrappedCryptoKey.h"
#import <CommonCrypto/CommonSymmetricKeywrap.h>
#import <crt_externs.h>
#import <wtf/CryptographicUtilities.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/cf/VectorCF.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/Base64.h>
#import <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY)
#define USE_KEYCHAIN_ACCESS_CONTROL_LISTS 0
#else
#define USE_KEYCHAIN_ACCESS_CONTROL_LISTS 1
#endif

#if USE(KEYCHAIN_ACCESS_CONTROL_LISTS)
#import <wtf/cf/TypeCastsCF.h>
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
WTF_DECLARE_CF_TYPE_TRAIT(SecACL);
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

namespace WebCore {

const NSUInteger currentSerializationVersion = 1;

const NSString* versionKey = @"version";
const NSString* wrappedKEKKey = @"wrappedKEK";
const NSString* encryptedKeyKey = @"encryptedKey";
const NSString* tagKey = @"tag";

constexpr size_t masterKeySizeInBytes = 16;
constexpr size_t kekSizeInBytes = 16;
constexpr size_t expectedTagLengthAES = 16;

// https://datatracker.ietf.org/doc/html/rfc3394#section-2.2.1
constexpr size_t wrappedKekSize = kekSizeInBytes + 8;

static NSString* masterKeyAccountNameForCurrentApplication()
{
#if PLATFORM(IOS_FAMILY)
    NSString *bundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
#else
    NSString *bundleIdentifier = [[NSRunningApplication currentApplication] bundleIdentifier];
#endif
    if (!bundleIdentifier)
        bundleIdentifier = [NSString stringWithCString:*_NSGetProgname() encoding:NSASCIIStringEncoding];
    return [NSString stringWithFormat:@"com.apple.WebKit.WebCrypto.master+%@", bundleIdentifier];
}

static std::optional<Vector<uint8_t>> createAndStoreMasterKey()
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));

    Vector<uint8_t> masterKeyData(masterKeySizeInBytes);
    auto rc = CCRandomGenerateBytes(masterKeyData.data(), masterKeyData.size());
    RELEASE_ASSERT(rc == kCCSuccess);

#if PLATFORM(IOS_FAMILY)
    NSBundle *mainBundle = [NSBundle mainBundle];
    NSString *applicationName = [mainBundle objectForInfoDictionaryKey:@"CFBundleDisplayName"];
    if (!applicationName)
        applicationName = [mainBundle objectForInfoDictionaryKey:(NSString *)kCFBundleNameKey];
    if (!applicationName)
        applicationName = [mainBundle bundleIdentifier];
    NSString *localizedItemName = webCryptoMasterKeyKeychainLabel(applicationName);
#else
    NSString *localizedItemName = webCryptoMasterKeyKeychainLabel([[NSRunningApplication currentApplication] localizedName]);
#endif

    OSStatus status;

#if USE(KEYCHAIN_ACCESS_CONTROL_LISTS)
    SecAccessRef accessRef;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    status = SecAccessCreate((__bridge CFStringRef)localizedItemName, nullptr, &accessRef);
ALLOW_DEPRECATED_DECLARATIONS_END
    if (status) {
        WTFLogAlways("Cannot create a security access object for storing WebCrypto master key, error %d", (int)status);
        return std::nullopt;
    }
    RetainPtr<SecAccessRef> access = adoptCF(accessRef);

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<CFArrayRef> acls = adoptCF(SecAccessCopyMatchingACLList(accessRef, kSecACLAuthorizationExportClear));
ALLOW_DEPRECATED_DECLARATIONS_END
    SecACLRef acl = checked_cf_cast<SecACLRef>(CFArrayGetValueAtIndex(acls.get(), 0));

    SecTrustedApplicationRef trustedAppRef;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    status = SecTrustedApplicationCreateFromPath(0, &trustedAppRef);
ALLOW_DEPRECATED_DECLARATIONS_END
    if (status) {
        WTFLogAlways("Cannot create a trusted application object for storing WebCrypto master key, error %d", (int)status);
        return std::nullopt;
    }
    RetainPtr<SecTrustedApplicationRef> trustedApp = adoptCF(trustedAppRef);

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    status = SecACLSetContents(acl, (__bridge CFArrayRef)@[ (__bridge id)trustedApp.get() ], (__bridge CFStringRef)localizedItemName, kSecKeychainPromptRequirePassphase);
ALLOW_DEPRECATED_DECLARATIONS_END
    if (status) {
        WTFLogAlways("Cannot set ACL for WebCrypto master key, error %d", (int)status);
        return std::nullopt;
    }
#endif

    auto base64EncodedMasterKeyData = base64EncodeToVector(masterKeyData);

    // Cannot use kSecClassKey because of <rdar://problem/16068207>.
    NSDictionary *attributes = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrSynchronizable : @NO,
#if USE(KEYCHAIN_ACCESS_CONTROL_LISTS)
        (id)kSecAttrAccess : (__bridge id)access.get(),
#endif
        (id)kSecAttrComment : webCryptoMasterKeyKeychainComment(),
        (id)kSecAttrLabel : localizedItemName,
        (id)kSecAttrAccount : masterKeyAccountNameForCurrentApplication(),
        (id)kSecValueData : toNSData(base64EncodedMasterKeyData).autorelease(),
    };

    status = SecItemAdd((CFDictionaryRef)attributes, nullptr);
    if (status) {
        WTFLogAlways("Cannot store WebCrypto master key, error %d", (int)status);
        return std::nullopt;
    }

    return masterKeyData;
}

static std::optional<Vector<uint8_t>> findMasterKey()
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));

    NSDictionary *query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : masterKeyAccountNameForCurrentApplication(),
        (id)kSecReturnData : @YES,
    };

    CFDataRef keyDataRef;
    OSStatus status = SecItemCopyMatching((CFDictionaryRef)query, (CFTypeRef*)&keyDataRef);
    if (status) {
        if (status != errSecItemNotFound && status != errSecUserCanceled)
            WTFLogAlways("Could not find WebCrypto master key in Keychain, error %d", (int)status);
        return std::nullopt;
    }
    RetainPtr<CFDataRef> keyData = adoptCF(keyDataRef);
    return base64Decode(span(keyData.get()));
}

std::optional<Vector<uint8_t>> defaultWebCryptoMasterKey()
{
    if (auto masterKey = findMasterKey()) {
        RELEASE_ASSERT(masterKey->size() == masterKeySizeInBytes);
        return masterKey;
    }

    if (auto masterKey = createAndStoreMasterKey()) {
        RELEASE_ASSERT(masterKey->size() == masterKeySizeInBytes);
        return masterKey;
    }

    return std::nullopt;
}

bool deleteDefaultWebCryptoMasterKey()
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));

    NSDictionary *query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : masterKeyAccountNameForCurrentApplication(),
    };

    OSStatus status = SecItemDelete((CFDictionaryRef)query);
    if (status) {
        if (status != errSecItemNotFound && status != errSecUserCanceled)
            WTFLogAlways("Could not delete WebCrypto master key in Keychain, error %d", (int)status);
        return false;
    }
    return true;
}

bool wrapSerializedCryptoKey(const Vector<uint8_t>& masterKey, const Vector<uint8_t>& key, Vector<uint8_t>& result)
{
    if (masterKey.isEmpty())
        return false;
    Vector<uint8_t> kek(kekSizeInBytes);
    auto rc = CCRandomGenerateBytes(kek.data(), kek.size());
    RELEASE_ASSERT(rc == kCCSuccess);

    Vector<uint8_t> wrappedKEK(CCSymmetricWrappedSize(kCCWRAPAES, kek.size()));

    size_t wrappedKEKSize = wrappedKEK.size();
    CCCryptorStatus status = CCSymmetricKeyWrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, masterKey.data(), masterKey.size(), kek.data(), kek.size(), wrappedKEK.data(), &wrappedKEKSize);
    if (status != kCCSuccess)
        return false;

    wrappedKEK.shrink(wrappedKEKSize);

    Vector<uint8_t> encryptedKey(key.size());
    size_t tagLength = expectedTagLengthAES;
    uint8_t tag[expectedTagLengthAES] = { 0 };

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    status = CCCryptorGCM(kCCEncrypt, kCCAlgorithmAES128, kek.data(), kek.size(),
        nullptr, 0, // iv
        nullptr, 0, // auth data
        key.data(), key.size(),
        encryptedKey.data(),
        tag, &tagLength);
ALLOW_DEPRECATED_DECLARATIONS_END

    if (status != kCCSuccess)
        return false;
    RELEASE_ASSERT(tagLength == expectedTagLengthAES);

    auto dictionary = @{
        versionKey: [NSNumber numberWithUnsignedInteger:currentSerializationVersion],
        wrappedKEKKey: toNSData(wrappedKEK).autorelease(),
        encryptedKeyKey: toNSData(encryptedKey).autorelease(),
        tagKey: [NSData dataWithBytes:tag length:tagLength]
    };

    NSData* serialization = [NSPropertyListSerialization dataWithPropertyList:dictionary format:NSPropertyListBinaryFormat_v1_0 options:0 error:nullptr];
    if (!serialization)
        return false;

    result = makeVector(serialization);
    return true;
}

template<size_t size>
static std::optional<std::array<uint8_t, size>> createArrayFromData(NSData * data)
{
    if (size != data.length)
        return std::nullopt;
    std::array<uint8_t, size> rv { };
    [data getBytes:rv.data() length:size];
    return rv;
}

std::optional<struct WrappedCryptoKey> readSerializedCryptoKey(const Vector<uint8_t>& wrappedKey)
{
    NSDictionary* dictionary = [NSPropertyListSerialization propertyListWithData:[NSData dataWithBytesNoCopy:(void*)wrappedKey.data() length:wrappedKey.size() freeWhenDone:NO] options:0 format:nullptr error:nullptr];
    if (!dictionary)
        return std::nullopt;

    id versionObject = [dictionary objectForKey:versionKey];
    if (![versionObject isKindOfClass:[NSNumber class]])
        return std::nullopt;
    if ([versionObject unsignedIntegerValue] > currentSerializationVersion)
        return std::nullopt;

    id wrappedKEKObject = [dictionary objectForKey:wrappedKEKKey];
    if (![wrappedKEKObject isKindOfClass:[NSData class]])
        return std::nullopt;
    auto wrappedKEK = createArrayFromData<wrappedKekSize>(wrappedKEKObject);
    if (!wrappedKEK)
        return std::nullopt;

    id encryptedKeyObject = [dictionary objectForKey:encryptedKeyKey];
    if (![encryptedKeyObject isKindOfClass:[NSData class]])
        return std::nullopt;
    auto encryptedKey = Vector<uint8_t>(span(encryptedKeyObject));

    id tagObject = [dictionary objectForKey:tagKey];
    if (![tagObject isKindOfClass:[NSData class]])
        return std::nullopt;
    auto tag = createArrayFromData<expectedTagLengthAES>(tagObject);
    if (!tag)
        return std::nullopt;
    struct WrappedCryptoKey k { *wrappedKEK, encryptedKey, *tag };
    return k;
}

std::optional<Vector<uint8_t>> unwrapCryptoKey(const Vector<uint8_t>& masterKey, const struct WrappedCryptoKey& wrappedKey)
{
    if (masterKey.isEmpty())
        return std::nullopt;
    auto wrappedKEK = wrappedKey.wrappedKEK;
    auto encryptedKey = wrappedKey.encryptedKey.span();
    auto tag = wrappedKey.tag;

    Vector<uint8_t> kek(CCSymmetricUnwrappedSize(kCCWRAPAES, wrappedKEK.size()));
    size_t kekSize = kek.size();
    CCCryptorStatus status = CCSymmetricKeyUnwrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, masterKey.data(), masterKey.size(), wrappedKEK.data(), wrappedKEK.size(), kek.data(), &kekSize);
    if (status != kCCSuccess)
        return std::nullopt;
    kek.shrink(kekSize);

    size_t tagLength = expectedTagLengthAES;
    std::array<uint8_t, expectedTagLengthAES> actualTag { };

    Vector<uint8_t> key(encryptedKey.size());
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    status = CCCryptorGCM(kCCDecrypt, kCCAlgorithmAES128, kek.data(), kek.size(),
        nullptr, 0, // iv
        nullptr, 0, // auth data
        encryptedKey.data(), encryptedKey.size(),
        key.data(),
        actualTag.data(), &tagLength);
ALLOW_DEPRECATED_DECLARATIONS_END

    if (status != kCCSuccess)
        return std::nullopt;
    RELEASE_ASSERT(tagLength == expectedTagLengthAES);

    if (constantTimeMemcmp(tag, actualTag))
        return std::nullopt;

    return key;
}

} // namespace WebCore
