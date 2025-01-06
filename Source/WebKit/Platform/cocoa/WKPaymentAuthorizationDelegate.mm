/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#import "WKPaymentAuthorizationDelegate.h"

#if USE(PASSKIT) && ENABLE(APPLE_PAY)

#import "PaymentAuthorizationPresenter.h"
#import <WebCore/ApplePayShippingMethod.h>
#import <WebCore/Payment.h>
#import <WebCore/PaymentMethod.h>
#import <WebCore/PaymentSessionError.h>
#import <wtf/RunLoop.h>
#import <wtf/URL.h>

#import <pal/cocoa/PassKitSoftLink.h>

@implementation WKPaymentAuthorizationDelegate {
    RetainPtr<NSArray<PKPaymentSummaryItem *>> _summaryItems;
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
    RetainPtr<PKShippingMethods> _availableShippingMethods;
#else
    RetainPtr<NSArray<PKShippingMethod *>> _shippingMethods;
#endif
    RetainPtr<NSError> _sessionError;
    WebKit::DidAuthorizePaymentCompletion _didAuthorizePaymentCompletion;
    WebKit::DidRequestMerchantSessionCompletion _didRequestMerchantSessionCompletion;
    WebKit::DidSelectPaymentMethodCompletion _didSelectPaymentMethodCompletion;
    WebKit::DidSelectShippingContactCompletion _didSelectShippingContactCompletion;
    WebKit::DidSelectShippingMethodCompletion _didSelectShippingMethodCompletion;
#if HAVE(PASSKIT_COUPON_CODE)
    WebKit::DidChangeCouponCodeCompletion _didChangeCouponCodeCompletion;
#endif
}

- (void)completeMerchantValidation:(PKPaymentMerchantSession *)session error:(NSError *)error
{
    std::exchange(_didRequestMerchantSessionCompletion, nil)(session, error);
}

- (void)completePaymentMethodSelection:(PKPaymentRequestPaymentMethodUpdate *)paymentMethodUpdate
{
    RetainPtr update = paymentMethodUpdate;
    if (update) {
        _summaryItems = adoptNS([[update paymentSummaryItems] copy]);
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
        _availableShippingMethods = adoptNS([[update availableShippingMethods] copy]);
#elif HAVE(PASSKIT_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_SUMMARY_ITEMS)
        _shippingMethods = adoptNS([[update shippingMethods] copy]);
#endif
    } else {
        update = adoptNS([PAL::allocPKPaymentRequestPaymentMethodUpdateInstance() initWithPaymentSummaryItems:_summaryItems.get()]);
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
        [update setAvailableShippingMethods:_availableShippingMethods.get()];
#elif HAVE(PASSKIT_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_SUMMARY_ITEMS)
        [update setShippingMethods:_shippingMethods.get()];
#endif
    }

    std::exchange(_didSelectPaymentMethodCompletion, nil)(update.get());
}

- (void)completePaymentSession:(PKPaymentAuthorizationStatus)status errors:(NSArray<NSError *> *)errors
{
    auto result = adoptNS([PAL::allocPKPaymentAuthorizationResultInstance() initWithStatus:status errors:errors]);
    std::exchange(_didAuthorizePaymentCompletion, nil)(result.get());
}

#if HAVE(PASSKIT_PAYMENT_ORDER_DETAILS)

- (void)completePaymentSession:(PKPaymentAuthorizationStatus)status errors:(NSArray<NSError *> *)errors orderDetails:(PKPaymentOrderDetails *)orderDetails
{
    auto result = adoptNS([PAL::allocPKPaymentAuthorizationResultInstance() initWithStatus:status errors:errors]);
    [result setOrderDetails:orderDetails];
    std::exchange(_didAuthorizePaymentCompletion, nil)(result.get());
}

#endif // HAVE(PASSKIT_PAYMENT_ORDER_DETAILS)

- (void)completeShippingContactSelection:(PKPaymentRequestShippingContactUpdate *)shippingContactUpdate
{
    RetainPtr update = shippingContactUpdate;
    if (update) {
        _summaryItems = adoptNS([[update paymentSummaryItems] copy]);
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
        _availableShippingMethods = adoptNS([[update availableShippingMethods] copy]);
#else
        _shippingMethods = adoptNS([[update shippingMethods] copy]);
#endif
    } else {
        update = adoptNS([PAL::allocPKPaymentRequestShippingContactUpdateInstance() initWithPaymentSummaryItems:_summaryItems.get()]);
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
        [update setAvailableShippingMethods:_availableShippingMethods.get()];
#else
        [update setShippingMethods:_shippingMethods.get()];
#endif
    }

    std::exchange(_didSelectShippingContactCompletion, nil)(update.get());
}

- (void)completeShippingMethodSelection:(PKPaymentRequestShippingMethodUpdate *)shippingMethodUpdate
{
    RetainPtr update = shippingMethodUpdate;
    if (update) {
        _summaryItems = adoptNS([[update paymentSummaryItems] copy]);
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
        _availableShippingMethods = adoptNS([[update availableShippingMethods] copy]);
#elif HAVE(PASSKIT_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_SUMMARY_ITEMS)
        _shippingMethods = adoptNS([[update shippingMethods] copy]);
#endif
    } else {
        update = adoptNS([PAL::allocPKPaymentRequestShippingMethodUpdateInstance() initWithPaymentSummaryItems:_summaryItems.get()]);
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
        [update setAvailableShippingMethods:_availableShippingMethods.get()];
#elif HAVE(PASSKIT_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_SUMMARY_ITEMS)
        [update setShippingMethods:_shippingMethods.get()];
#endif
    }

    std::exchange(_didSelectShippingMethodCompletion, nil)(update.get());
}

#if HAVE(PASSKIT_COUPON_CODE)

- (void)completeCouponCodeChange:(PKPaymentRequestCouponCodeUpdate *)couponCodeUpdate
{
    RetainPtr update = couponCodeUpdate;
    if (update) {
        _summaryItems = adoptNS([[update paymentSummaryItems] copy]);
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
        _availableShippingMethods = adoptNS([[update availableShippingMethods] copy]);
#else
        _shippingMethods = adoptNS([[update shippingMethods] copy]);
#endif
    } else {
        update = adoptNS([PAL::allocPKPaymentRequestCouponCodeUpdateInstance() initWithPaymentSummaryItems:_summaryItems.get()]);
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
        [update setAvailableShippingMethods:_availableShippingMethods.get()];
#else
        [update setShippingMethods:_shippingMethods.get()];
#endif
    }

    std::exchange(_didChangeCouponCodeCompletion, nil)(update.get());
}

#endif // HAVE(PASSKIT_COUPON_CODE)

- (void)invalidate
{
    if (_didAuthorizePaymentCompletion)
        [self completePaymentSession:PKPaymentAuthorizationStatusFailure errors:@[ ]];
}

@end

@implementation WKPaymentAuthorizationDelegate (Protected)

- (instancetype)_initWithRequest:(PKPaymentRequest *)request presenter:(WebKit::PaymentAuthorizationPresenter&)presenter
{
    if (!(self = [super init]))
        return nil;

    _presenter = presenter;
    _request = request;
#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
    _availableShippingMethods = request.availableShippingMethods;
#else
    _shippingMethods = request.shippingMethods;
#endif
    _summaryItems = request.paymentSummaryItems;
    return self;
}

- (void)_didAuthorizePayment:(PKPayment *)payment completion:(WebKit::DidAuthorizePaymentCompletion::BlockType)completion
{
    ASSERT(!_didAuthorizePaymentCompletion);
    _didAuthorizePaymentCompletion = completion;

    auto presenter = _presenter.get();
    if (!presenter)
        return [self completePaymentSession:PKPaymentAuthorizationStatusFailure errors:@[ ]];

    presenter->checkedClient()->presenterDidAuthorizePayment(*presenter, WebCore::Payment(payment));
}

- (void)_didFinish
{
    if (auto presenter = _presenter.get())
        presenter->checkedClient()->presenterDidFinish(*presenter, { std::exchange(_sessionError, nil) });
}

- (void)_didRequestMerchantSession:(WebKit::DidRequestMerchantSessionCompletion::BlockType)completion
{
    ASSERT(!_didRequestMerchantSessionCompletion);
    _didRequestMerchantSessionCompletion = completion;

    [self _getPaymentServicesMerchantURL:^(NSURL *merchantURL, NSError *error) {
        if (error)
            LOG_ERROR("PKCanMakePaymentsWithMerchantIdentifierAndDomain error %@", error);

        RunLoop::main().dispatch([self, protectedSelf = retainPtr(self), merchantURL = retainPtr(merchantURL)] {
            ASSERT(_didRequestMerchantSessionCompletion);

            auto presenter = _presenter.get();
            if (!presenter) {
                _didRequestMerchantSessionCompletion(nil, nil);
                return;
            }

            presenter->checkedClient()->presenterWillValidateMerchant(*presenter, merchantURL.get());
        });
    }];
}

- (void)_didSelectPaymentMethod:(PKPaymentMethod *)paymentMethod completion:(WebKit::DidSelectPaymentMethodCompletion::BlockType)completion
{
    ASSERT(!_didSelectPaymentMethodCompletion);
    _didSelectPaymentMethodCompletion = completion;

    RefPtr presenter = _presenter.get();
    if (!presenter)
        return [self completePaymentMethodSelection:nil];

    presenter->checkedClient()->presenterDidSelectPaymentMethod(*presenter, WebCore::PaymentMethod(paymentMethod));
}

- (void)_didSelectShippingContact:(PKContact *)contact completion:(WebKit::DidSelectShippingContactCompletion::BlockType)completion
{
    ASSERT(!_didSelectShippingContactCompletion);
    _didSelectShippingContactCompletion = completion;

    RefPtr presenter = _presenter.get();
    if (!presenter)
        return [self completeShippingContactSelection:nil];

    presenter->checkedClient()->presenterDidSelectShippingContact(*presenter, WebCore::PaymentContact(contact));
}

#if HAVE(PASSKIT_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)

static WebCore::ApplePayDateComponents toDateComponents(NSDateComponents *dateComponents)
{
    ASSERT(dateComponents);

    WebCore::ApplePayDateComponents result;
    result.years = dateComponents.year;
    result.months = dateComponents.month;
    result.days = dateComponents.day;
    result.hours = dateComponents.hour;
    return result;
}

static WebCore::ApplePayDateComponentsRange toDateComponentsRange(PKDateComponentsRange *dateComponentsRange)
{
    ASSERT(dateComponentsRange);

    WebCore::ApplePayDateComponentsRange result;
    result.startDateComponents = toDateComponents(dateComponentsRange.startDateComponents);
    result.endDateComponents = toDateComponents(dateComponentsRange.endDateComponents);
    return result;
}

#endif // HAVE(PASSKIT_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)

static WebCore::ApplePayShippingMethod toShippingMethod(PKShippingMethod *shippingMethod, bool selected)
{
    ASSERT(shippingMethod);

    WebCore::ApplePayShippingMethod result;
    result.amount = shippingMethod.amount.stringValue;
    result.detail = shippingMethod.detail;
    result.identifier = shippingMethod.identifier;
    result.label = shippingMethod.label;
#if HAVE(PASSKIT_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
    if (shippingMethod.dateComponentsRange)
        result.dateComponentsRange = toDateComponentsRange(shippingMethod.dateComponentsRange);
#endif
#if ENABLE(APPLE_PAY_SELECTED_SHIPPING_METHOD)
    result.selected = selected;
#else
    UNUSED_PARAM(selected);
#endif
    return result;
}

- (void)_didSelectShippingMethod:(PKShippingMethod *)shippingMethod completion:(WebKit::DidSelectShippingMethodCompletion::BlockType)completion
{
    ASSERT(!_didSelectShippingMethodCompletion);
    _didSelectShippingMethodCompletion = completion;

    RefPtr presenter = _presenter.get();
    if (!presenter)
        return [self completeShippingMethodSelection:nil];

    presenter->checkedClient()->presenterDidSelectShippingMethod(*presenter, toShippingMethod(shippingMethod, true));
}

#if HAVE(PASSKIT_COUPON_CODE)

- (void)_didChangeCouponCode:(NSString *)couponCode completion:(void (^)(PKPaymentRequestCouponCodeUpdate *update))completion
{
    ASSERT(!_didChangeCouponCodeCompletion);
    _didChangeCouponCodeCompletion = completion;

    RefPtr presenter = _presenter.get();
    if (!presenter)
        return [self completeCouponCodeChange:nil];

    presenter->checkedClient()->presenterDidChangeCouponCode(*presenter, couponCode);
}

#endif // HAVE(PASSKIT_COUPON_CODE)

- (void) NO_RETURN_DUE_TO_ASSERT _getPaymentServicesMerchantURL:(void(^)(NSURL *, NSError *))completion
{
    ASSERT_NOT_REACHED();
    completion(nil, nil);
}

- (void)_willFinishWithError:(NSError *)error
{
    if (![error.domain isEqualToString:PKPassKitErrorDomain])
        return;

    _sessionError = error;
}

@end

#endif // USE(PASSKIT) && ENABLE(APPLE_PAY)
