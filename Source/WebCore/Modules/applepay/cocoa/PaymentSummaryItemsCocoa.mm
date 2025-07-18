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

NSDecimalNumber *toDecimalNumber(const String& amount)
{
    if (!amount)
        return [NSDecimalNumber zero];
    return [NSDecimalNumber decimalNumberWithString:amount.createNSString().get() locale:@{ NSLocaleDecimalSeparator : @"." }];
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
    RetainPtr<PKRecurringPaymentSummaryItem> summaryItem = [PAL::getPKRecurringPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label.createNSString().get() amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
    if (!lineItem.recurringPaymentStartDate.isNaN())
        summaryItem.get().startDate = toDate(lineItem.recurringPaymentStartDate);
    summaryItem.get().intervalUnit = toCalendarUnit(lineItem.recurringPaymentIntervalUnit);
    summaryItem.get().intervalCount = lineItem.recurringPaymentIntervalCount;
    if (!lineItem.recurringPaymentEndDate.isNaN())
        summaryItem.get().endDate = toDate(lineItem.recurringPaymentEndDate);
    return summaryItem.get();
}

#endif // HAVE(PASSKIT_RECURRING_SUMMARY_ITEM)

#if HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)

PKDeferredPaymentSummaryItem *platformDeferredSummaryItem(const ApplePayLineItem& lineItem)
{
    ASSERT(lineItem.paymentTiming == ApplePayPaymentTiming::Deferred);
    RetainPtr<PKDeferredPaymentSummaryItem> summaryItem = [PAL::getPKDeferredPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label.createNSString().get() amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
    if (!lineItem.deferredPaymentDate.isNaN())
        summaryItem.get().deferredDate = toDate(lineItem.deferredPaymentDate);
    return summaryItem.get();
}

#endif // HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)

#if HAVE(PASSKIT_AUTOMATIC_RELOAD_SUMMARY_ITEM)

PKAutomaticReloadPaymentSummaryItem *platformAutomaticReloadSummaryItem(const ApplePayLineItem& lineItem)
{
    ASSERT(lineItem.paymentTiming == ApplePayPaymentTiming::AutomaticReload);
    RetainPtr<PKAutomaticReloadPaymentSummaryItem> summaryItem = [PAL::getPKAutomaticReloadPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label.createNSString().get() amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
    summaryItem.get().thresholdAmount = toDecimalNumber(lineItem.automaticReloadPaymentThresholdAmount);
    return summaryItem.get();
}

#endif // HAVE(PASSKIT_AUTOMATIC_RELOAD_SUMMARY_ITEM)

#if HAVE(PASSKIT_DISBURSEMENTS)

PKDisbursementSummaryItem *platformDisbursementSummaryItem(const ApplePayLineItem& lineItem)
{
    ASSERT(lineItem.disbursementLineItemType == ApplePayLineItem::DisbursementLineItemType::Disbursement);
    return [PAL::getPKDisbursementSummaryItemClass() summaryItemWithLabel:lineItem.label.createNSString().get() amount:toDecimalNumber(lineItem.amount)];
}

PKInstantFundsOutFeeSummaryItem *platformInstantFundsOutFeeSummaryItem(const ApplePayLineItem& lineItem)
{
    ASSERT(lineItem.disbursementLineItemType == ApplePayLineItem::DisbursementLineItemType::InstantFundsOutFee);
    return [PAL::getPKInstantFundsOutFeeSummaryItemClass() summaryItemWithLabel:lineItem.label.createNSString().get() amount:toDecimalNumber(lineItem.amount)];
}

#endif // HAVE(PASSKIT_DISBURSEMENTS)

PKPaymentSummaryItem *platformSummaryItem(const ApplePayLineItem& lineItem)
{
#if HAVE(PASSKIT_DISBURSEMENTS)
    if (lineItem.disbursementLineItemType.has_value()) {
        switch (lineItem.disbursementLineItemType.value()) {
        case ApplePayLineItem::DisbursementLineItemType::Disbursement:
            return platformDisbursementSummaryItem(lineItem);
        case ApplePayLineItem::DisbursementLineItemType::InstantFundsOutFee:
            return platformInstantFundsOutFeeSummaryItem(lineItem);
        }
    }
#endif // HAVE(PASSKIT_DISBURSEMENTS)

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

    return [PAL::getPKPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label.createNSString().get() amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
}

#if HAVE(PASSKIT_DISBURSEMENTS)
// Disbursement Requests have a unique quirk: the total doesn't actually matter, we need to disregard any totals (this is a separate method to avoid confusion rather than making the total in `platformSummaryItems` optional
NSArray *platformDisbursementSummaryItems(const Vector<ApplePayLineItem>& lineItems)
{
    NSMutableArray *paymentSummaryItems = [NSMutableArray arrayWithCapacity:lineItems.size()];
    for (auto& lineItem : lineItems) {
        if (RetainPtr summaryItem = platformSummaryItem(lineItem))
            [paymentSummaryItems addObject:summaryItem.get()];
    }
    return adoptNS([paymentSummaryItems copy]).autorelease();
}
#endif // HAVE(PASSKIT_DISBURSEMENTS)

NSArray *platformSummaryItems(const ApplePayLineItem& total, const Vector<ApplePayLineItem>& lineItems)
{
    NSMutableArray *paymentSummaryItems = [NSMutableArray arrayWithCapacity:lineItems.size() + 1];
    for (auto& lineItem : lineItems) {
        if (RetainPtr summaryItem = platformSummaryItem(lineItem))
            [paymentSummaryItems addObject:summaryItem.get()];
    }

    if (RetainPtr totalItem = platformSummaryItem(total))
        [paymentSummaryItems addObject:totalItem.get()];

    return adoptNS([paymentSummaryItems copy]).autorelease();
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
