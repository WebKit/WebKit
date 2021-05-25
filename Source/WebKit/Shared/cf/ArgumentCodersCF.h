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

#pragma once

#include "ArgumentCoder.h"
#include <Security/SecCertificate.h>
#include <Security/SecTrust.h>
#include <wtf/RetainPtr.h>

#if HAVE(SEC_KEYCHAIN)
#include <Security/SecKeychainItem.h>
#endif

typedef struct CGColorSpace* CGColorSpaceRef;

namespace IPC {

class Encoder;
class Decoder;

// NOTE: These coders are structured such that they expose encoder functions for both CFType and RetainPtr<CFType>
// but only a decoder only for RetainPtr<CFType>.

template<typename T>
struct CFRetainPtrArgumentCoder {
    template<typename Encoder> static void encode(Encoder& encoder, const RetainPtr<T>& retainPtr)
    {
        ArgumentCoder<T>::encode(encoder, retainPtr.get());
    }
};

// CFTypeRef
template<> struct ArgumentCoder<CFTypeRef> {
    template<typename Encoder> static void encode(Encoder&, CFTypeRef);
};

template<> struct ArgumentCoder<RetainPtr<CFTypeRef>> : CFRetainPtrArgumentCoder<CFTypeRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFTypeRef>&);
};

// CFArrayRef
template<> struct ArgumentCoder<CFArrayRef> {
    template<typename Encoder> static void encode(Encoder&, CFArrayRef);
};

template<> struct ArgumentCoder<RetainPtr<CFArrayRef>> : CFRetainPtrArgumentCoder<CFArrayRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFArrayRef>&);
};

// CFBooleanRef
template<> struct ArgumentCoder<CFBooleanRef> {
    template<typename Encoder> static void encode(Encoder&, CFBooleanRef);
};

template<> struct ArgumentCoder<RetainPtr<CFBooleanRef>> : CFRetainPtrArgumentCoder<CFBooleanRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFBooleanRef>&);
};

// CFDataRef
template<> struct ArgumentCoder<CFDataRef> {
    template<typename Encoder> static void encode(Encoder&, CFDataRef);
};

template<> struct ArgumentCoder<RetainPtr<CFDataRef>> : CFRetainPtrArgumentCoder<CFDataRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFDataRef>&);
};

// CFDateRef
template<> struct ArgumentCoder<CFDateRef> {
    template<typename Encoder> static void encode(Encoder&, CFDateRef);
};

template<> struct ArgumentCoder<RetainPtr<CFDateRef>> : CFRetainPtrArgumentCoder<CFDateRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFDateRef>&);
};

// CFDictionaryRef
template<> struct ArgumentCoder<CFDictionaryRef> {
    template<typename Encoder> static void encode(Encoder&, CFDictionaryRef);
};

template<> struct ArgumentCoder<RetainPtr<CFDictionaryRef>> : CFRetainPtrArgumentCoder<CFDictionaryRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFDictionaryRef>&);
};

// CFNumberRef
template<> struct ArgumentCoder<CFNumberRef> {
    template<typename Encoder> static void encode(Encoder&, CFNumberRef);
};

template<> struct ArgumentCoder<RetainPtr<CFNumberRef>> : CFRetainPtrArgumentCoder<CFNumberRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFNumberRef>&);
};

// CFStringRef
template<> struct ArgumentCoder<CFStringRef> {
    template<typename Encoder> static void encode(Encoder&, CFStringRef);
};

template<> struct ArgumentCoder<RetainPtr<CFStringRef>> : CFRetainPtrArgumentCoder<CFStringRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFStringRef>&);
};

// CFURLRef
template<> struct ArgumentCoder<CFURLRef> {
    template<typename Encoder> static void encode(Encoder&, CFURLRef);
};

template<> struct ArgumentCoder<RetainPtr<CFURLRef>> : CFRetainPtrArgumentCoder<CFURLRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CFURLRef>&);
};

// CGColorSpaceRef
template<> struct ArgumentCoder<CGColorSpaceRef> {
    template<typename Encoder> static void encode(Encoder&, CGColorSpaceRef);
};

template<> struct ArgumentCoder<RetainPtr<CGColorSpaceRef>> : CFRetainPtrArgumentCoder<CGColorSpaceRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<CGColorSpaceRef>&);
};

// SecCertificateRef
template<> struct ArgumentCoder<SecCertificateRef> {
    template<typename Encoder> static void encode(Encoder&, SecCertificateRef);
};

template<> struct ArgumentCoder<RetainPtr<SecCertificateRef>> : CFRetainPtrArgumentCoder<SecCertificateRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<SecCertificateRef>&);
};

#if HAVE(SEC_KEYCHAIN)
// SecKeychainItemRef
template<> struct ArgumentCoder<SecKeychainItemRef> {
    template<typename Encoder> static void encode(Encoder&, SecKeychainItemRef);
};

template<> struct ArgumentCoder<RetainPtr<SecKeychainItemRef>> : CFRetainPtrArgumentCoder<SecKeychainItemRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<SecKeychainItemRef>&);
};
#endif

#if HAVE(SEC_ACCESS_CONTROL)
// SecAccessControlRef
template<> struct ArgumentCoder<SecAccessControlRef> {
    template<typename Encoder> static void encode(Encoder&, SecAccessControlRef);
};

template<> struct ArgumentCoder<RetainPtr<SecAccessControlRef>> : CFRetainPtrArgumentCoder<SecAccessControlRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<SecAccessControlRef>&);
};
#endif

#if HAVE(SEC_TRUST_SERIALIZATION)
// SecTrustRef
template<> struct ArgumentCoder<SecTrustRef> {
    template<typename Encoder> static void encode(Encoder&, SecTrustRef);
};

template<> struct ArgumentCoder<RetainPtr<SecTrustRef>> : CFRetainPtrArgumentCoder<SecTrustRef> {
    static WARN_UNUSED_RETURN bool decode(Decoder&, RetainPtr<SecTrustRef>&);
};
#endif

CFTypeRef tokenNullptrTypeRef();

} // namespace IPC
