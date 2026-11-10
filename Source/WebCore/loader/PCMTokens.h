/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>
#include <wtf/Seconds.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS RSABSSATokenReady;
OBJC_CLASS RSABSSATokenWaitingActivation;
OBJC_CLASS RSABSSATokenBlinder;
#endif

namespace WebCore::PCM {

struct UnlinkableToken {
#if PLATFORM(COCOA)
    RetainPtr<RSABSSATokenBlinder> blinder;
    RetainPtr<RSABSSATokenWaitingActivation> waitingToken;
    RetainPtr<RSABSSATokenReady> readyToken;
#endif
    RefPtr<StringImpl> valueBase64URL;
    
    UnlinkableToken isolatedCopy() const &;
    UnlinkableToken isolatedCopy() &&;
};

struct SourceUnlinkableToken : UnlinkableToken {
    SourceUnlinkableToken isolatedCopy() const &;
    SourceUnlinkableToken isolatedCopy() &&;
};

struct DestinationUnlinkableToken : UnlinkableToken {
    DestinationUnlinkableToken isolatedCopy() const &;
    DestinationUnlinkableToken isolatedCopy() &&;
};

struct SecretToken {
    RefPtr<StringImpl> tokenBase64URL;
    RefPtr<StringImpl> signatureBase64URL;
    RefPtr<StringImpl> keyIDBase64URL;
    SecretToken isolatedCopy() const & { return { WTF::isolatedCopy(tokenBase64URL), WTF::isolatedCopy(signatureBase64URL), WTF::isolatedCopy(keyIDBase64URL) }; }
    SecretToken isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(tokenBase64URL)), WTF::isolatedCopy(WTF::move(signatureBase64URL)), WTF::isolatedCopy(WTF::move(keyIDBase64URL)) }; }
    NODELETE bool isValid() const;
};

struct SourceSecretToken : SecretToken {
    SourceSecretToken isolatedCopy() const & { return { SecretToken::isolatedCopy() }; }
    SourceSecretToken isolatedCopy() && { return { SecretToken::isolatedCopy() }; }
};

struct DestinationSecretToken : SecretToken {
    DestinationSecretToken isolatedCopy() const & { return { SecretToken::isolatedCopy() }; }
    DestinationSecretToken isolatedCopy() && { return { SecretToken::isolatedCopy() }; }
};

}
