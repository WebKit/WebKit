/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#if USE(APPLE_INTERNAL_SDK)

#include <Security/SecAccessControlPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecTask.h>
#include <Security/SecTrustPriv.h>

#if PLATFORM(MAC)
#include <Security/keyTemplates.h>
#endif

#else

#include <Security/SecBase.h>

typedef uint32_t SecSignatureHashAlgorithm;
enum {
    kSecSignatureHashAlgorithmUnknown = 0,
    kSecSignatureHashAlgorithmMD2 = 1,
    kSecSignatureHashAlgorithmMD4 = 2,
    kSecSignatureHashAlgorithmMD5 = 3,
    kSecSignatureHashAlgorithmSHA1 = 4,
    kSecSignatureHashAlgorithmSHA224 = 5,
    kSecSignatureHashAlgorithmSHA256 = 6,
    kSecSignatureHashAlgorithmSHA384 = 7,
    kSecSignatureHashAlgorithmSHA512 = 8
};

WTF_EXTERN_C_BEGIN

#if PLATFORM(MAC)
OSStatus SecTrustedApplicationCreateFromPath(const char* path, SecTrustedApplicationRef*);
#endif

SecSignatureHashAlgorithm SecCertificateGetSignatureHashAlgorithm(SecCertificateRef);
extern const CFStringRef kSecAttrNoLegacy;

WTF_EXTERN_C_END

#endif // USE(APPLE_INTERNAL_SDK)

typedef struct __SecTask *SecTaskRef;
typedef struct __SecTrust *SecTrustRef;

WTF_EXTERN_C_BEGIN

SecTaskRef SecTaskCreateWithAuditToken(CFAllocatorRef, audit_token_t);
SecTaskRef SecTaskCreateFromSelf(CFAllocatorRef);
CFTypeRef SecTaskCopyValueForEntitlement(SecTaskRef, CFStringRef entitlement, CFErrorRef*);
SecIdentityRef SecIdentityCreate(CFAllocatorRef, SecCertificateRef, SecKeyRef);
OSStatus SecKeyFindWithPersistentRef(CFDataRef persistentRef, SecKeyRef* lookedUpData);
SecAccessControlRef SecAccessControlCreateFromData(CFAllocatorRef, CFDataRef, CFErrorRef*);
CFDataRef SecAccessControlCopyData(SecAccessControlRef);

#if PLATFORM(MAC)
#include <Security/SecAsn1Types.h>
CFStringRef SecTaskCopySigningIdentifier(SecTaskRef, CFErrorRef *);
extern const SecAsn1Template kSecAsn1AlgorithmIDTemplate[];
extern const SecAsn1Template kSecAsn1SubjectPublicKeyInfoTemplate[];
uint32_t SecTaskGetCodeSignStatus(SecTaskRef);
#endif

#if HAVE(SEC_TRUST_SERIALIZATION)
CF_RETURNS_RETAINED CFDataRef SecTrustSerialize(SecTrustRef, CFErrorRef *);
CF_RETURNS_RETAINED SecTrustRef SecTrustDeserialize(CFDataRef serializedTrust, CFErrorRef *);
#endif

CF_RETURNS_RETAINED CFDictionaryRef SecTrustCopyInfo(SecTrustRef);

extern const CFStringRef kSecTrustInfoExtendedValidationKey;
extern const CFStringRef kSecTrustInfoCompanyNameKey;
extern const CFStringRef kSecTrustInfoRevocationKey;

WTF_EXTERN_C_END
