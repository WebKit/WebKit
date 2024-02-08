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

template<> struct ArgumentCoder<CFCharacterSetRef> {
    template<typename Encoder> static void encode(Encoder&, CFCharacterSetRef);
};
template<> struct ArgumentCoder<RetainPtr<CFCharacterSetRef>> : CFRetainPtrArgumentCoder<CFCharacterSetRef> {
    static std::optional<RetainPtr<CFCharacterSetRef>> decode(Decoder&);
};

} // namespace IPC
