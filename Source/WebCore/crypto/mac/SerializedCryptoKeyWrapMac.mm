/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_CRYPTO)

#import "CommonCryptoUtilities.h"
#import "LocalizedStrings.h"
#import <CommonCrypto/CommonSymmetricKeywrap.h>
#import <crt_externs.h>
#import <wtf/CryptographicUtilities.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
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
WTF_DECLARE_CF_TYPE_TRAIT(SecACL);
#endif

namespace WebCore {

const NSUInteger currentSerializationVersion = 1;

const NSString* versionKey = @"version";
const NSString* wrappedKEKKey = @"wrappedKEK";
const NSString* encryptedKeyKey = @"encryptedKey";
const NSString* tagKey = @"tag";

const size_t masterKeySizeInBytes = 16;

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
    status = SecAccessCreate((__bridge CFStringRef)localizedItemName, nullptr, &accessRef);
    if (status) {
        WTFLogAlways("Cannot create a security access object for storing WebCrypto master key, error %d", (int)status);
        return std::nullopt;
    }
    RetainPtr<SecAccessRef> access = adoptCF(accessRef);

    RetainPtr<CFArrayRef> acls = adoptCF(SecAccessCopyMatchingACLList(accessRef, kSecACLAuthorizationExportClear));
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

    status = SecACLSetContents(acl, (__bridge CFArrayRef)@[ (__bridge id)trustedApp.get() ], (__bridge CFStringRef)localizedItemName, kSecKeychainPromptRequirePassphase);
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
        (id)kSecValueData : [NSData dataWithBytes:base64EncodedMasterKeyData.data() length:base64EncodedMasterKeyData.size()],
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
    return base64Decode(CFDataGetBytePtr(keyData.get()), CFDataGetLength(keyData.get()));
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
    Vector<uint8_t> kek(16);
    auto rc = CCRandomGenerateBytes(kek.data(), kek.size());
    RELEASE_ASSERT(rc == kCCSuccess);

    Vector<uint8_t> wrappedKEK(CCSymmetricWrappedSize(kCCWRAPAES, kek.size()));

    size_t wrappedKEKSize = wrappedKEK.size();
    CCCryptorStatus status = CCSymmetricKeyWrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, masterKey.data(), masterKey.size(), kek.data(), kek.size(), wrappedKEK.data(), &wrappedKEKSize);
    if (status != kCCSuccess)
        return false;

    wrappedKEK.shrink(wrappedKEKSize);

    Vector<uint8_t> encryptedKey(key.size());
    constexpr size_t maxTagLength = 16;
    size_t tagLength = maxTagLength;
    uint8_t tag[maxTagLength];

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

    constexpr size_t maxTagLength = 16;
    size_t tagLength = maxTagLength;
    uint8_t actualTag[maxTagLength];

    key.resize(encryptedKey.size());
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    status = CCCryptorGCM(kCCDecrypt, kCCAlgorithmAES128, kek.data(), kek.size(),
        nullptr, 0, // iv
        nullptr, 0, // auth data
        encryptedKey.data(), encryptedKey.size(),
        key.data(),
        actualTag, &tagLength);
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (status != kCCSuccess)
        return false;
    RELEASE_ASSERT(tagLength == 16);

    if (constantTimeMemcmp(tag.data(), actualTag, tagLength))
        return false;

    return true;
}

}

#endif // ENABLE(WEB_CRYPTO)
