/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef ArgumentCodersCF_h
#define ArgumentCodersCF_h

#include <Security/SecCertificate.h>
#include <Security/SecTrust.h>
#include <wtf/RetainPtr.h>

#if HAVE(SEC_KEYCHAIN)
#include <Security/SecKeychainItem.h>
#endif

namespace IPC {

class Encoder;
class Decoder;

// CFArrayRef
void encode(Encoder&, CFArrayRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFArrayRef>& result);

// CFBooleanRef
void encode(Encoder&, CFBooleanRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFBooleanRef>& result);

// CFDataRef
void encode(Encoder&, CFDataRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFDataRef>& result);

// CFDateRef
void encode(Encoder&, CFDateRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFDateRef>& result);

// CFDictionaryRef
void encode(Encoder&, CFDictionaryRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFDictionaryRef>& result);

// CFNumberRef
void encode(Encoder&, CFNumberRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFNumberRef>& result);

// CFStringRef
void encode(Encoder&, CFStringRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFStringRef>& result);

// CFTypeRef
void encode(Encoder&, CFTypeRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFTypeRef>& result);

// CFURLRef
void encode(Encoder&, CFURLRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFURLRef>& result);

// SecCertificateRef
void encode(Encoder&, SecCertificateRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<SecCertificateRef>& result);

// SecIdentityRef
void encode(Encoder&, SecIdentityRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<SecIdentityRef>& result);

#if HAVE(SEC_KEYCHAIN)
// SecKeychainItemRef
void encode(Encoder&, SecKeychainItemRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<SecKeychainItemRef>& result);
#endif

#if HAVE(SEC_ACCESS_CONTROL)
// SecAccessControlRef
void encode(Encoder&, SecAccessControlRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<SecAccessControlRef>& result);
#endif

#if HAVE(SEC_TRUST_SERIALIZATION)
// SecTrustRef
void encode(Encoder&, SecTrustRef);
WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<SecTrustRef>&);
#endif

#if PLATFORM(IOS_FAMILY)
void setAllowsDecodingSecKeyRef(bool);
#endif

CFTypeRef tokenNullptrTypeRef();

} // namespace IPC

#endif // ArgumentCodersCF_h
