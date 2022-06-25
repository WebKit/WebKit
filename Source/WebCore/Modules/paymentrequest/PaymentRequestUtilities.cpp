/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "PaymentRequestUtilities.h"

#if ENABLE(PAYMENT_REQUEST)

namespace WebCore {

// Implements the "valid decimal monetary value" validity checker
// https://www.w3.org/TR/payment-request/#dfn-valid-decimal-monetary-value
bool isValidDecimalMonetaryValue(StringView value)
{
    enum class State {
        Start,
        Sign,
        Digit,
        Dot,
        DotDigit,
    };

    auto state = State::Start;
    for (auto character : value.codeUnits()) {
        switch (state) {
        case State::Start:
            if (character == '-') {
                state = State::Sign;
                break;
            }

            if (isASCIIDigit(character)) {
                state = State::Digit;
                break;
            }

            return false;

        case State::Sign:
            if (isASCIIDigit(character)) {
                state = State::Digit;
                break;
            }

            return false;

        case State::Digit:
            if (character == '.') {
                state = State::Dot;
                break;
            }

            if (isASCIIDigit(character)) {
                state = State::Digit;
                break;
            }

            return false;

        case State::Dot:
            if (isASCIIDigit(character)) {
                state = State::DotDigit;
                break;
            }

            return false;

        case State::DotDigit:
            if (isASCIIDigit(character)) {
                state = State::DotDigit;
                break;
            }

            return false;
        }
    }

    return state == State::Digit || state == State::DotDigit;
}

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
