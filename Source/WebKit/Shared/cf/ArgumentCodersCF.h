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

#include <Security/SecCertificate.h>
#include <Security/SecTrust.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/RetainPtr.h>

#if HAVE(SEC_KEYCHAIN)
#include <Security/SecKeychainItem.h>
#endif

typedef struct CGColorSpace* CGColorSpaceRef;

namespace IPC {

class Encoder;
class Decoder;

template<typename T>
struct CFRetainPtrArgumentCoder {
    template<typename Encoder> static void encode(Encoder& encoder, const RetainPtr<T>& retainPtr)
    {
        ArgumentCoder<T>::encode(encoder, retainPtr.get());
    }
};

template<> struct ArgumentCoder<CFTypeRef> {
    template<typename Encoder> static void encode(Encoder&, CFTypeRef);
};
template<> struct ArgumentCoder<RetainPtr<CFTypeRef>> : CFRetainPtrArgumentCoder<CFTypeRef> {
    static std::optional<RetainPtr<CFTypeRef>> decode(Decoder&);
};

template<> struct ArgumentCoder<CFArrayRef> {
    template<typename Encoder> static void encode(Encoder&, CFArrayRef);
};
template<> struct ArgumentCoder<RetainPtr<CFArrayRef>> : CFRetainPtrArgumentCoder<CFArrayRef> {
    static std::optional<RetainPtr<CFArrayRef>> decode(Decoder&);
};

template<> struct ArgumentCoder<CFCharacterSetRef> {
    template<typename Encoder> static void encode(Encoder&, CFCharacterSetRef);
};
template<> struct ArgumentCoder<RetainPtr<CFCharacterSetRef>> : CFRetainPtrArgumentCoder<CFCharacterSetRef> {
    static std::optional<RetainPtr<CFCharacterSetRef>> decode(Decoder&);
};

template<> struct ArgumentCoder<CFDictionaryRef> {
    template<typename Encoder> static void encode(Encoder&, CFDictionaryRef);
};
template<> struct ArgumentCoder<RetainPtr<CFDictionaryRef>> : CFRetainPtrArgumentCoder<CFDictionaryRef> {
    static std::optional<RetainPtr<CFDictionaryRef>> decode(Decoder&);
};

template<> struct ArgumentCoder<CGColorSpaceRef> {
    template<typename Encoder> static void encode(Encoder&, CGColorSpaceRef);
};
template<> struct ArgumentCoder<RetainPtr<CGColorSpaceRef>> : CFRetainPtrArgumentCoder<CGColorSpaceRef> {
    static std::optional<RetainPtr<CGColorSpaceRef>> decode(Decoder&);
};

template<> struct ArgumentCoder<SecCertificateRef> {
    template<typename Encoder> static void encode(Encoder&, SecCertificateRef);
};
template<> struct ArgumentCoder<RetainPtr<SecCertificateRef>> : CFRetainPtrArgumentCoder<SecCertificateRef> {
    static std::optional<RetainPtr<SecCertificateRef>> decode(Decoder&);
};

#if HAVE(SEC_KEYCHAIN)
template<> struct ArgumentCoder<SecKeychainItemRef> {
    template<typename Encoder> static void encode(Encoder&, SecKeychainItemRef);
};
template<> struct ArgumentCoder<RetainPtr<SecKeychainItemRef>> : CFRetainPtrArgumentCoder<SecKeychainItemRef> {
    static std::optional<RetainPtr<SecKeychainItemRef>> decode(Decoder&);
};
#endif

#if HAVE(SEC_ACCESS_CONTROL)
template<> struct ArgumentCoder<SecAccessControlRef> {
    template<typename Encoder> static void encode(Encoder&, SecAccessControlRef);
};
template<> struct ArgumentCoder<RetainPtr<SecAccessControlRef>> : CFRetainPtrArgumentCoder<SecAccessControlRef> {
    static std::optional<RetainPtr<SecAccessControlRef>> decode(Decoder&);
};
#endif

template<> struct ArgumentCoder<SecTrustRef> {
    template<typename Encoder> static void encode(Encoder&, SecTrustRef);
};
template<> struct ArgumentCoder<RetainPtr<SecTrustRef>> : CFRetainPtrArgumentCoder<SecTrustRef> {
    template<typename Decoder> static std::optional<RetainPtr<SecTrustRef>> decode(Decoder&);
};

} // namespace IPC
