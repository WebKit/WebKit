/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#import "PaymentSummaryItems.h"

#if ENABLE(APPLE_PAY)

#import <pal/cocoa/PassKitSoftLink.h>

namespace WebCore {

static NSDecimalNumber *toDecimalNumber(const String& amount)
{
    if (!amount)
        return [NSDecimalNumber zero];
    return [NSDecimalNumber decimalNumberWithString:amount locale:@{ NSLocaleDecimalSeparator : @"." }];
}

static PKPaymentSummaryItemType toPKPaymentSummaryItemType(ApplePaySessionPaymentRequest::LineItem::Type type)
{
    switch (type) {
    case ApplePaySessionPaymentRequest::LineItem::Type::Final:
        return PKPaymentSummaryItemTypeFinal;
    case ApplePaySessionPaymentRequest::LineItem::Type::Pending:
        return PKPaymentSummaryItemTypePending;
    }
}

static PKPaymentSummaryItem *toPKPaymentSummaryItem(const ApplePaySessionPaymentRequest::LineItem& lineItem)
{
    return [PAL::getPKPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
}

NSArray *platformSummaryItems(const ApplePaySessionPaymentRequest::TotalAndLineItems& totalAndLineItems)
{
    return platformSummaryItems(totalAndLineItems.total, totalAndLineItems.lineItems);
}

NSArray *platformSummaryItems(const ApplePaySessionPaymentRequest::LineItem& total, const Vector<ApplePaySessionPaymentRequest::LineItem>& lineItems)
{
    NSMutableArray *paymentSummaryItems = [NSMutableArray arrayWithCapacity:lineItems.size() + 1];
    for (auto& lineItem : lineItems) {
        if (PKPaymentSummaryItem *summaryItem = toPKPaymentSummaryItem(lineItem))
            [paymentSummaryItems addObject:summaryItem];
    }

    if (PKPaymentSummaryItem *totalItem = toPKPaymentSummaryItem(total))
        [paymentSummaryItems addObject:totalItem];

    return adoptNS([paymentSummaryItems copy]).autorelease();
}

} // namespace WebbCore

#endif // ENABLE(APPLE_PAY)
