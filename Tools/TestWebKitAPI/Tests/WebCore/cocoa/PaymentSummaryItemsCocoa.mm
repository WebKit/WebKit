/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(APPLE_PAY)

#import "Helpers/Test.h"
#import <WebCore/ApplePayLineItem.h>
#import <WebCore/PaymentSummaryItems.h>
#import <pal/spi/cocoa/PassKitSPI.h>

namespace TestWebKitAPI {

static WebCore::ApplePayLineItem makeLineItem(WebCore::ApplePayLineItem::Type type)
{
    WebCore::ApplePayLineItem lineItem;
    lineItem.type = type;
    lineItem.label = "Shipping"_s;
    lineItem.amount = "4.99"_s;
    return lineItem;
}

// WebCore and PassKit order Pending and Final differently, so the mapping must not be a cast.
TEST(PaymentSummaryItems, LineItemType)
{
    using enum WebCore::ApplePayLineItem::Type;

    EXPECT_EQ([WebCore::platformSummaryItem(makeLineItem(Pending)) type], PKPaymentSummaryItemTypePending);
    EXPECT_EQ([WebCore::platformSummaryItem(makeLineItem(Final)) type], PKPaymentSummaryItemTypeFinal);
#if ENABLE(APPLE_PAY_ESTIMATED_LINE_ITEM)
    EXPECT_EQ([WebCore::platformSummaryItem(makeLineItem(Estimated)) type], PKPaymentSummaryItemTypeEstimated);
#endif
}

} // namespace TestWebKitAPI

#endif // ENABLE(APPLE_PAY)
