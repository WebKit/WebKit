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

#include "config.h"
#include "CodeSigning.h"

#if PLATFORM(MAC)

#include <Security/Security.h>
#include <wtf/RetainPtr.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static RetainPtr<SecCodeRef> secCodeForCurrentProcess()
{
    SecCodeRef code = nullptr;
    RELEASE_ASSERT(!SecCodeCopySelf(kSecCSDefaultFlags, &code));
    return adoptCF(code);
}

static RetainPtr<SecCodeRef> secCodeForProcess(pid_t pid)
{
    RetainPtr<CFNumberRef> pidCFNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid));
    const void* keys[] = { kSecGuestAttributePid };
    const void* values[] = { pidCFNumber.get() };
    RetainPtr<CFDictionaryRef> attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    SecCodeRef code = nullptr;
    OSStatus errorCode = noErr;
    // FIXME: We should RELEASE_ASSERT() that SecCodeCopyGuestWithAttributes() returns without error. See <rdar://problem/25706517>.
    if ((errorCode = SecCodeCopyGuestWithAttributes(nullptr, attributes.get(), kSecCSDefaultFlags, &code))) {
        WTFLogAlways("SecCodeCopyGuestWithAttributes() failed with error: %ld\n", static_cast<long>(errorCode));
        return nullptr;
    }
    return adoptCF(code);
}

static RetainPtr<CFDictionaryRef> secCodeSigningInformation(SecCodeRef code)
{
    CFDictionaryRef signingInfo = nullptr;
    RELEASE_ASSERT(!SecCodeCopySigningInformation(code, kSecCSDefaultFlags, &signingInfo));
    return adoptCF(signingInfo);
}

static RetainPtr<SecRequirementRef> appleSignedOrMacAppStoreSignedOrAppleDeveloperSignedRequirement()
{
    CFStringRef requirement = CFSTR("(anchor apple) or (anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.9]) or (anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] and certificate leaf[field.1.2.840.113635.100.6.1.13])");
    SecRequirementRef signingRequirement = nullptr;
    RELEASE_ASSERT(!SecRequirementCreateWithString(requirement, kSecCSDefaultFlags, &signingRequirement));
    return adoptCF(signingRequirement);
}

static String secCodeSigningIdentifier(SecCodeRef code)
{
    RetainPtr<SecRequirementRef> signingRequirement = appleSignedOrMacAppStoreSignedOrAppleDeveloperSignedRequirement();
    OSStatus errorCode = SecCodeCheckValidity(code, kSecCSDefaultFlags, signingRequirement.get());
    if (errorCode == errSecCSUnsigned || errorCode == errSecCSReqFailed)
        return String(); // Unsigned or signed by a third-party
    RELEASE_ASSERT_WITH_MESSAGE(!errorCode, "SecCodeCheckValidity() failed with error: %ld", static_cast<long>(errorCode));
    String codeSigningIdentifier;
    RetainPtr<CFDictionaryRef> signingInfo = secCodeSigningInformation(code);
    if (CFDictionaryRef plist = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(signingInfo.get(), kSecCodeInfoPList)))
        codeSigningIdentifier = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(plist, kCFBundleIdentifierKey));
    else
        codeSigningIdentifier = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(signingInfo.get(), kSecCodeInfoIdentifier));
    RELEASE_ASSERT(!codeSigningIdentifier.isEmpty());
    return codeSigningIdentifier;
}

String codeSigningIdentifier()
{
    return secCodeSigningIdentifier(secCodeForCurrentProcess().get());
}

String codeSigningIdentifierForProcess(pid_t pid)
{
    auto code = secCodeForProcess(pid);
    if (!code)
        return String();
    return secCodeSigningIdentifier(code.get());
}
    
} // namespace WebKit

#endif // PLATFORM(MAC)
