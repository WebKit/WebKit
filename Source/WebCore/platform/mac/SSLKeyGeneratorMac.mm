/*
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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
#import "SSLKeyGenerator.h"

#if PLATFORM(MAC)

#import "LocalizedStrings.h"
#import <Security/SecAsn1Coder.h>
#import <Security/SecAsn1Templates.h>
#import <Security/SecEncodeTransform.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/Scope.h>
#import <wtf/URL.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/Base64.h>

WTF_DECLARE_CF_TYPE_TRAIT(SecACL);

namespace WebCore {

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

struct PublicKeyAndChallenge {
    CSSM_X509_SUBJECT_PUBLIC_KEY_INFO subjectPublicKeyInfo;
    CSSM_DATA challenge;
};

struct SignedPublicKeyAndChallenge {
    PublicKeyAndChallenge publicKeyAndChallenge;
    CSSM_X509_ALGORITHM_IDENTIFIER algorithmIdentifier;
    CSSM_DATA signature;
};

static const SecAsn1Template publicKeyAndChallengeTemplate[] {
    { SEC_ASN1_SEQUENCE, 0, nullptr, sizeof(PublicKeyAndChallenge) },
    { SEC_ASN1_INLINE, offsetof(PublicKeyAndChallenge, subjectPublicKeyInfo), kSecAsn1SubjectPublicKeyInfoTemplate, 0},
    { SEC_ASN1_INLINE, offsetof(PublicKeyAndChallenge, challenge), kSecAsn1IA5StringTemplate, 0 },
    { 0, 0, 0, 0}
};

static const SecAsn1Template signedPublicKeyAndChallengeTemplate[] {
    { SEC_ASN1_SEQUENCE, 0, nullptr, sizeof(SignedPublicKeyAndChallenge) },
    { SEC_ASN1_INLINE, offsetof(SignedPublicKeyAndChallenge, publicKeyAndChallenge), publicKeyAndChallengeTemplate, 0 },
    { SEC_ASN1_INLINE, offsetof(SignedPublicKeyAndChallenge, algorithmIdentifier), kSecAsn1AlgorithmIDTemplate, 0 },
    { SEC_ASN1_BIT_STRING, offsetof(SignedPublicKeyAndChallenge, signature), 0, 0 },
    { 0, 0, 0, 0 }
};

static bool getSubjectPublicKey(CSSM_CSP_HANDLE cspHandle, SecKeyRef publicKey, CSSM_KEY_PTR subjectPublicKey)
{
    const CSSM_KEY* cssmPublicKey;
    if (SecKeyGetCSSMKey(publicKey, &cssmPublicKey) != noErr)
        return false;

    CSSM_ACCESS_CREDENTIALS credentials { };
    CSSM_CC_HANDLE ccHandle;
    if (CSSM_CSP_CreateSymmetricContext(cspHandle, CSSM_ALGID_NONE, CSSM_ALGMODE_NONE, &credentials, nullptr, nullptr, CSSM_PADDING_NONE, 0, &ccHandle) != noErr)
        return false;

    auto deleteContext = makeScopeExit([&] {
        CSSM_DeleteContext(ccHandle);
    });

    CSSM_CONTEXT_ATTRIBUTE publicKeyFormatAttribute;
    publicKeyFormatAttribute.AttributeType = CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT;
    publicKeyFormatAttribute.AttributeLength = sizeof(uint32);
    publicKeyFormatAttribute.Attribute.Data = (CSSM_DATA_PTR)CSSM_KEYBLOB_RAW_FORMAT_X509;

    if (CSSM_UpdateContextAttributes(ccHandle, 1, &publicKeyFormatAttribute) != noErr)
        return false;

    if (CSSM_WrapKey(ccHandle, &credentials, cssmPublicKey, nullptr, subjectPublicKey) != noErr)
        return false;

    return true;
}

static bool signPublicKeyAndChallenge(CSSM_CSP_HANDLE cspHandle, const CSSM_DATA* plainText, SecKeyRef privateKey, CSSM_ALGORITHMS algorithmID, CSSM_DATA& signature)
{
    ASSERT(!signature.Data);
    ASSERT(!signature.Length);

    const CSSM_KEY* cssmPrivateKey;
    if (SecKeyGetCSSMKey(privateKey, &cssmPrivateKey) != noErr)
        return false;

    const CSSM_ACCESS_CREDENTIALS* credentials;
    if (SecKeyGetCredentials(privateKey, CSSM_ACL_AUTHORIZATION_SIGN, kSecCredentialTypeDefault, &credentials) != noErr)
        return false;

    CSSM_CC_HANDLE ccHandle;
    if (CSSM_CSP_CreateSignatureContext(cspHandle, algorithmID, credentials, cssmPrivateKey, &ccHandle) != noErr)
        return false;
    auto deleteContext = makeScopeExit([&] {
        CSSM_DeleteContext(ccHandle);
    });

    if (CSSM_SignData(ccHandle, plainText, 1, CSSM_ALGID_NONE, &signature) != noErr)
        return false;

    return true;
}

static String signedPublicKeyAndChallengeString(unsigned keySize, const CString& challenge, const String& keyDescription)
{
    ASSERT(keySize >= 2048);

    SignedPublicKeyAndChallenge signedPublicKeyAndChallenge { };

    SecAccessRef accessRef { nullptr };
    if (SecAccessCreate(keyDescription.createCFString().get(), nullptr, &accessRef) != noErr)
        return String();
    RetainPtr<SecAccessRef> access = adoptCF(accessRef);

    CFArrayRef aclsRef { nullptr };
    if (SecAccessCopySelectedACLList(access.get(), CSSM_ACL_AUTHORIZATION_DECRYPT, &aclsRef) != noErr)
        return String();
    RetainPtr<CFArrayRef> acls = adoptCF(aclsRef);

    SecACLRef acl = checked_cf_cast<SecACLRef>(CFArrayGetValueAtIndex(acls.get(), 0));

    // Passing nullptr to SecTrustedApplicationCreateFromPath tells that function to assume the application bundle.
    SecTrustedApplicationRef trustedAppRef { nullptr };
    if (SecTrustedApplicationCreateFromPath(nullptr, &trustedAppRef) != noErr)
        return String();
    RetainPtr<SecTrustedApplicationRef> trustedApp = adoptCF(trustedAppRef);

    const CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR defaultSelector = {
        CSSM_ACL_KEYCHAIN_PROMPT_CURRENT_VERSION, 0
    };
    if (SecACLSetSimpleContents(acl, (__bridge CFArrayRef)@[ (__bridge id)trustedApp.get() ], keyDescription.createCFString().get(), &defaultSelector) != noErr)
        return String();

    SecKeyRef publicKeyRef { nullptr };
    SecKeyRef privateKeyRef { nullptr };
    if (SecKeyCreatePair(nullptr, CSSM_ALGID_RSA, keySize, 0, CSSM_KEYUSE_ANY, CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_REF, CSSM_KEYUSE_ANY, CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE, access.get(), &publicKeyRef, &privateKeyRef) != noErr)
        return String();
    RetainPtr<SecKeyRef> publicKey = adoptCF(publicKeyRef);
    RetainPtr<SecKeyRef> privateKey = adoptCF(privateKeyRef);

    CSSM_CSP_HANDLE cspHandle;
    if (SecKeyGetCSPHandle(privateKey.get(), &cspHandle) != noErr)
        return String();
    
    CSSM_KEY subjectPublicKey { };
    if (!getSubjectPublicKey(cspHandle, publicKey.get(), &subjectPublicKey))
        return String();
    auto freeSubjectPublicKey = makeScopeExit([&] {
        CSSM_FreeKey(cspHandle, nullptr, &subjectPublicKey, CSSM_FALSE);
    });

    SecAsn1CoderRef coder = nullptr;
    if (SecAsn1CoderCreate(&coder) != noErr)
        return String();
    auto releaseCoder = makeScopeExit([&] {
        SecAsn1CoderRelease(coder);
    });

    if (SecAsn1DecodeData(coder, &subjectPublicKey.KeyData, kSecAsn1SubjectPublicKeyInfoTemplate, &signedPublicKeyAndChallenge.publicKeyAndChallenge.subjectPublicKeyInfo) != noErr)
        return String();

    ASSERT(challenge.data());

    // Length needs to account for the null terminator.
    signedPublicKeyAndChallenge.publicKeyAndChallenge.challenge.Length = challenge.length() + 1;
    signedPublicKeyAndChallenge.publicKeyAndChallenge.challenge.Data = reinterpret_cast<uint8_t*>(const_cast<char*>(challenge.data()));

    CSSM_DATA encodedPublicKeyAndChallenge { 0, nullptr };
    if (SecAsn1EncodeItem(coder, &signedPublicKeyAndChallenge.publicKeyAndChallenge, publicKeyAndChallengeTemplate, &encodedPublicKeyAndChallenge) != noErr)
        return String();

    CSSM_DATA signature { };
    if (!signPublicKeyAndChallenge(cspHandle, &encodedPublicKeyAndChallenge, privateKey.get(), CSSM_ALGID_MD5WithRSA, signature))
        return String();
    auto freeSignatureData = makeScopeExit([&] {
        CSSM_API_MEMORY_FUNCS memoryFunctions;
        if (CSSM_GetAPIMemoryFunctions(cspHandle, &memoryFunctions) != noErr)
            return;

        memoryFunctions.free_func(signature.Data, memoryFunctions.AllocRef);
    });

    uint8 encodeNull[2] { SEC_ASN1_NULL, 0 };
    signedPublicKeyAndChallenge.algorithmIdentifier.algorithm = CSSMOID_MD5WithRSA;
    signedPublicKeyAndChallenge.algorithmIdentifier.parameters.Data = (uint8 *)encodeNull;
    signedPublicKeyAndChallenge.algorithmIdentifier.parameters.Length = 2;
    signedPublicKeyAndChallenge.signature = signature;

    // We want the signature length to be in bits.
    signedPublicKeyAndChallenge.signature.Length *= 8;

    CSSM_DATA encodedSignedPublicKeyAndChallenge { 0, nullptr };
    if (SecAsn1EncodeItem(coder, &signedPublicKeyAndChallenge, signedPublicKeyAndChallengeTemplate, &encodedSignedPublicKeyAndChallenge) != noErr)
        return String();

    return base64Encode(encodedSignedPublicKeyAndChallenge.Data, encodedSignedPublicKeyAndChallenge.Length);
}

ALLOW_DEPRECATED_DECLARATIONS_END

void getSupportedKeySizes(Vector<String>& supportedKeySizes)
{
    ASSERT(supportedKeySizes.isEmpty());
    supportedKeySizes.append(keygenMenuItem2048());
}

String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String& challengeString, const URL& url)
{
    // This switch statement must always be synced with the UI strings returned by getSupportedKeySizes.
    UInt32 keySize;
    switch (keySizeIndex) {
    case 0:
        keySize = 2048;
        break;
    default:
        ASSERT_NOT_REACHED();
        return String();
    }

    auto challenge = challengeString.isAllASCII() ? challengeString.ascii() : "";

    return signedPublicKeyAndChallengeString(keySize, challenge, keygenKeychainItemName(url.host().toString()));
}

}

#endif // PLATFORM(MAC)
