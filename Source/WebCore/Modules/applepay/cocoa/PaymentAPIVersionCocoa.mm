/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "PaymentAPIVersion.h"

#if ENABLE(APPLE_PAY)

#import <pal/cocoa/PassKitSoftLink.h>

namespace WebCore {

unsigned PaymentAPIVersion::current()
{
    static unsigned current = [] {
#if ENABLE(APPLE_PAY_FEATURES)
        // This version number should not be changed anymore, features can now be found in method.data.features.
        return 15;
#elif ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_LINE_ITEM) || ENABLE(APPLE_PAY_RECURRING_PAYMENTS) || ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS) || ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS) || ENABLE(APPLE_PAY_PAYMENT_ORDER_DETAILS)
        return 14;
#elif ENABLE(APPLE_PAY_SELECTED_SHIPPING_METHOD) || ENABLE(APPLE_PAY_AMS_UI)
        return 13;
#elif ENABLE(APPLE_PAY_COUPON_CODE) || ENABLE(APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE) || ENABLE(APPLE_PAY_RECURRING_LINE_ITEM) || ENABLE(APPLE_PAY_DEFERRED_LINE_ITEM) || ENABLE(APPLE_PAY_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
        return 12;
#elif ENABLE(APPLE_PAY_SESSION_V11)
        return 11;
#elif HAVE(PASSKIT_NEW_BUTTON_TYPES)
        return 10;
#elif HAVE(PASSKIT_INSTALLMENTS)
        if (PAL::getPKPaymentInstallmentConfigurationClassSingleton()) {
            if (PAL::getPKPaymentInstallmentItemClassSingleton())
                return 9;
            return 8;
        }
#endif
        return 7;
    }();
    return current;
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
