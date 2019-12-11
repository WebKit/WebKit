/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "PaymentSessionError.h"

#if ENABLE(APPLE_PAY)

#import "ApplePaySessionError.h"
#import <pal/cocoa/PassKitSoftLink.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/PaymentSessionErrorCocoaAdditions.mm>
#else
namespace WebCore {
static Optional<ApplePaySessionError> additionalError(NSError *) { return WTF::nullopt; }
}
#endif

namespace WebCore {

PaymentSessionError::PaymentSessionError(RetainPtr<NSError>&& error)
    : m_platformError { WTFMove(error) }
{
}

ApplePaySessionError PaymentSessionError::sessionError() const
{
    ASSERT(!m_platformError || [[m_platformError domain] isEqualToString:PAL::get_PassKit_PKPassKitErrorDomain()]);

    if (auto error = additionalError(m_platformError.get()))
        return *error;

    return unknownError();
}

NSError *PaymentSessionError::platformError() const
{
    return m_platformError.get();
}

ApplePaySessionError PaymentSessionError::unknownError() const
{
    return { "unknown"_s, { } };
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
