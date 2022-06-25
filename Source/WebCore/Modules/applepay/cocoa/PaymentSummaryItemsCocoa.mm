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

#import "ApplePayLineItem.h"
#import <pal/cocoa/PassKitSoftLink.h>

namespace WebCore {

static NSDecimalNumber *toDecimalNumber(const String& amount)
{
    if (!amount)
        return [NSDecimalNumber zero];
    return [NSDecimalNumber decimalNumberWithString:amount locale:@{ NSLocaleDecimalSeparator : @"." }];
}

static PKPaymentSummaryItemType toPKPaymentSummaryItemType(ApplePayLineItem::Type type)
{
    switch (type) {
    case ApplePayLineItem::Type::Final:
        return PKPaymentSummaryItemTypeFinal;
    case ApplePayLineItem::Type::Pending:
        return PKPaymentSummaryItemTypePending;
    }
}

} // namespace WebCore

namespace WebCore {

#if HAVE(PASSKIT_RECURRING_SUMMARY_ITEM) || HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)

static NSDate *toDate(WallTime date)
{
    return [NSDate dateWithTimeIntervalSince1970:date.secondsSinceEpoch().value()];
}

#endif // HAVE(PASSKIT_RECURRING_SUMMARY_ITEM) || HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)

#if HAVE(PASSKIT_RECURRING_SUMMARY_ITEM)

static NSCalendarUnit toCalendarUnit(ApplePayRecurringPaymentDateUnit unit)
{
    switch (unit) {
    case ApplePayRecurringPaymentDateUnit::Year:
        return NSCalendarUnitYear;

    case ApplePayRecurringPaymentDateUnit::Month:
        return NSCalendarUnitMonth;

    case ApplePayRecurringPaymentDateUnit::Day:
        return NSCalendarUnitDay;

    case ApplePayRecurringPaymentDateUnit::Hour:
        return NSCalendarUnitHour;

    case ApplePayRecurringPaymentDateUnit::Minute:
        return NSCalendarUnitMinute;
    }
}

PKRecurringPaymentSummaryItem *platformRecurringSummaryItem(const ApplePayLineItem& lineItem)
{
    ASSERT(lineItem.paymentTiming == ApplePayPaymentTiming::Recurring);
    PKRecurringPaymentSummaryItem *summaryItem = [PAL::getPKRecurringPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
    if (!std::isnan(lineItem.recurringPaymentStartDate))
        summaryItem.startDate = toDate(lineItem.recurringPaymentStartDate);
    summaryItem.intervalUnit = toCalendarUnit(lineItem.recurringPaymentIntervalUnit);
    summaryItem.intervalCount = lineItem.recurringPaymentIntervalCount;
    if (!std::isnan(lineItem.recurringPaymentEndDate))
        summaryItem.endDate = toDate(lineItem.recurringPaymentEndDate);
    return summaryItem;
}

#endif // HAVE(PASSKIT_RECURRING_SUMMARY_ITEM)

#if HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)

PKDeferredPaymentSummaryItem *platformDeferredSummaryItem(const ApplePayLineItem& lineItem)
{
    ASSERT(lineItem.paymentTiming == ApplePayPaymentTiming::Deferred);
    PKDeferredPaymentSummaryItem *summaryItem = [PAL::getPKDeferredPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
    if (!std::isnan(lineItem.deferredPaymentDate))
        summaryItem.deferredDate = toDate(lineItem.deferredPaymentDate);
    return summaryItem;
}

#endif // HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)

#if HAVE(PASSKIT_AUTOMATIC_RELOAD_SUMMARY_ITEM)

PKAutomaticReloadPaymentSummaryItem *platformAutomaticReloadSummaryItem(const ApplePayLineItem& lineItem)
{
    ASSERT(lineItem.paymentTiming == ApplePayPaymentTiming::AutomaticReload);
    PKAutomaticReloadPaymentSummaryItem *summaryItem = [PAL::getPKAutomaticReloadPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
    summaryItem.thresholdAmount = toDecimalNumber(lineItem.automaticReloadPaymentThresholdAmount);
    return summaryItem;
}

#endif // HAVE(PASSKIT_AUTOMATIC_RELOAD_SUMMARY_ITEM)

PKPaymentSummaryItem *platformSummaryItem(const ApplePayLineItem& lineItem)
{
    switch (lineItem.paymentTiming) {
    case ApplePayPaymentTiming::Immediate:
        break;

#if HAVE(PASSKIT_RECURRING_SUMMARY_ITEM)
    case ApplePayPaymentTiming::Recurring:
        return platformRecurringSummaryItem(lineItem);
#endif

#if HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)
    case ApplePayPaymentTiming::Deferred:
        return platformDeferredSummaryItem(lineItem);
#endif

#if HAVE(PASSKIT_AUTOMATIC_RELOAD_SUMMARY_ITEM)
    case ApplePayPaymentTiming::AutomaticReload:
        return platformAutomaticReloadSummaryItem(lineItem);
#endif
    }

    return [PAL::getPKPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
}

NSArray *platformSummaryItems(const ApplePayLineItem& total, const Vector<ApplePayLineItem>& lineItems)
{
    NSMutableArray *paymentSummaryItems = [NSMutableArray arrayWithCapacity:lineItems.size() + 1];
    for (auto& lineItem : lineItems) {
        if (PKPaymentSummaryItem *summaryItem = platformSummaryItem(lineItem))
            [paymentSummaryItems addObject:summaryItem];
    }

    if (PKPaymentSummaryItem *totalItem = platformSummaryItem(total))
        [paymentSummaryItems addObject:totalItem];

    return adoptNS([paymentSummaryItems copy]).autorelease();
}

} // namespace WebbCore

#endif // ENABLE(APPLE_PAY)
