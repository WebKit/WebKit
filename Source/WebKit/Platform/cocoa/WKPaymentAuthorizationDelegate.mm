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
#import "WKPaymentAuthorizationDelegate.h"

#if USE(PASSKIT)

#import <WebCore/Payment.h>
#import <WebCore/PaymentMethod.h>

@implementation WKPaymentAuthorizationDelegate {
    RetainPtr<NSArray<PKPaymentSummaryItem *>> _summaryItems;
    RetainPtr<NSArray<PKShippingMethod *>> _shippingMethods;
    WeakPtr<WebKit::PaymentAuthorizationPresenter> _presenter;
    WebKit::DidAuthorizePaymentCompletion _didAuthorizePaymentCompletion;
    WebKit::DidRequestMerchantSessionCompletion _didRequestMerchantSessionCompletion;
    WebKit::DidSelectPaymentMethodCompletion _didSelectPaymentMethodCompletion;
    WebKit::DidSelectShippingContactCompletion _didSelectShippingContactCompletion;
    WebKit::DidSelectShippingMethodCompletion _didSelectShippingMethodCompletion;
}

- (NSArray<PKPaymentSummaryItem *> *)summaryItems
{
    return _summaryItems.get();
}

- (NSArray<PKShippingMethod *> *)shippingMethods
{
    return _shippingMethods.get();
}

- (void)completeMerchantValidation:(PKPaymentMerchantSession *)session error:(NSError *)error
{
    std::exchange(_didRequestMerchantSessionCompletion, nil)(session, error);
}

- (void)completePaymentMethodSelection:(NSArray<PKPaymentSummaryItem *> *)summaryItems
{
    _summaryItems = summaryItems;
#if HAVE(PASSKIT_GRANULAR_ERRORS)
    auto update = adoptNS([PAL::allocPKPaymentRequestPaymentMethodUpdateInstance() initWithPaymentSummaryItems:_summaryItems.get()]);
    std::exchange(_didSelectPaymentMethodCompletion, nil)(update.get());
#else
    std::exchange(_didSelectPaymentMethodCompletion, nil)(_summaryItems);
#endif
}

- (void)completePaymentSession:(PKPaymentAuthorizationStatus)status errors:(NSArray<NSError *> *)errors didReachFinalState:(BOOL)didReachFinalState
{
    _didReachFinalState = didReachFinalState;
#if HAVE(PASSKIT_GRANULAR_ERRORS)
    auto result = adoptNS([PAL::allocPKPaymentAuthorizationResultInstance() initWithStatus:status errors:errors]);
    std::exchange(_didAuthorizePaymentCompletion, nil)(result.get());
#else
    ASSERT(!errors.count);
    std::exchange(_didAuthorizePaymentCompletion, nil)(status);
#endif
}

- (void)completeShippingContactSelection:(PKPaymentAuthorizationStatus)status summaryItems:(NSArray<PKPaymentSummaryItem *> *)summaryItems shippingMethods:(NSArray<PKShippingMethod *> *)shippingMethods errors:(NSArray<NSError *> *)errors
{
    _summaryItems = summaryItems;
    _shippingMethods = shippingMethods;
#if HAVE(PASSKIT_GRANULAR_ERRORS)
    ASSERT(status == PKPaymentAuthorizationStatusSuccess);
    auto update = adoptNS([PAL::allocPKPaymentRequestShippingContactUpdateInstance() initWithErrors:errors paymentSummaryItems:_summaryItems.get() shippingMethods:_shippingMethods.get()]);
    std::exchange(_didSelectShippingContactCompletion, nil)(update.get());
#else
    ASSERT(!errors.count);
    std::exchange(_didSelectShippingContactCompletion, nil)(status, _shippingMethods, _summaryItems);
#endif
}

- (void)completeShippingMethodSelection:(NSArray<PKPaymentSummaryItem *> *)summaryItems
{
    _summaryItems = summaryItems;
#if HAVE(PASSKIT_GRANULAR_ERRORS)
    auto update = adoptNS([PAL::allocPKPaymentRequestShippingMethodUpdateInstance() initWithPaymentSummaryItems:_summaryItems.get()]);
    std::exchange(_didSelectShippingMethodCompletion, nil)(update.get());
#else
    std::exchange(_didSelectShippingMethodCompletion, nil)(PKPaymentAuthorizationStatusSuccess, _summaryItems);
#endif
}

- (void)invalidate
{
    if (_didAuthorizePaymentCompletion)
        [self completePaymentSession:PKPaymentAuthorizationStatusFailure errors:@[] didReachFinalState:YES];
}

@end

@implementation WKPaymentAuthorizationDelegate (Protected)

- (instancetype)_initWithRequest:(PKPaymentRequest *)request presenter:(WebKit::PaymentAuthorizationPresenter&)presenter
{
    if (!(self = [super init]))
        return nil;

    _presenter = makeWeakPtr(presenter);
    _request = request;
    _shippingMethods = request.shippingMethods;
    _summaryItems = request.paymentSummaryItems;
    return self;
}

- (void)_didAuthorizePayment:(PKPayment *)payment completion:(WebKit::DidAuthorizePaymentCompletion::BlockType)completion
{
    ASSERT(!_didAuthorizePaymentCompletion);
    _didAuthorizePaymentCompletion = completion;

    auto presenter = _presenter.get();
    if (!presenter)
        return [self completePaymentSession:PKPaymentAuthorizationStatusFailure errors:@[ ] didReachFinalState:YES];

    presenter->client().presenterDidAuthorizePayment(*presenter, WebCore::Payment(payment));
}

- (void)_didFinish
{
    if (auto presenter = _presenter.get())
        presenter->client().presenterDidFinish(*presenter, _didReachFinalState);
}

- (void)_didRequestMerchantSession:(WebKit::DidRequestMerchantSessionCompletion::BlockType)completion
{
    ASSERT(!_didRequestMerchantSessionCompletion);
    _didRequestMerchantSessionCompletion = completion;

    [self _getPaymentServicesMerchantURL:^(NSURL *merchantURL, NSError *error) {
        if (error)
            LOG_ERROR("PKCanMakePaymentsWithMerchantIdentifierAndDomain error %@", error);

        dispatch_async(dispatch_get_main_queue(), ^{
            ASSERT(_didRequestMerchantSessionCompletion);

            auto presenter = _presenter.get();
            if (!presenter) {
                _didRequestMerchantSessionCompletion(nil, nil);
                return;
            }

            presenter->client().presenterWillValidateMerchant(*presenter, merchantURL);
        });
    }];
}

- (void)_didSelectPaymentMethod:(PKPaymentMethod *)paymentMethod completion:(WebKit::DidSelectPaymentMethodCompletion::BlockType)completion
{
    ASSERT(!_didSelectPaymentMethodCompletion);
    _didSelectPaymentMethodCompletion = completion;

    auto presenter = _presenter.get();
    if (!presenter)
        return [self completePaymentMethodSelection:@[ ]];

    presenter->client().presenterDidSelectPaymentMethod(*presenter, WebCore::PaymentMethod(paymentMethod));
}

- (void)_didSelectShippingContact:(PKContact *)contact completion:(WebKit::DidSelectShippingContactCompletion::BlockType)completion
{
    ASSERT(!_didSelectShippingContactCompletion);
    _didSelectShippingContactCompletion = completion;

    auto presenter = _presenter.get();
    if (!presenter)
        return [self completeShippingContactSelection:PKPaymentAuthorizationStatusFailure summaryItems:@[ ] shippingMethods:@[ ] errors:@[ ]];

    presenter->client().presenterDidSelectShippingContact(*presenter, WebCore::PaymentContact(contact));
}

static WebCore::ApplePaySessionPaymentRequest::ShippingMethod toShippingMethod(PKShippingMethod *shippingMethod)
{
    ASSERT(shippingMethod);

    WebCore::ApplePaySessionPaymentRequest::ShippingMethod result;
    result.amount = shippingMethod.amount.stringValue;
    result.detail = shippingMethod.detail;
    result.identifier = shippingMethod.identifier;
    result.label = shippingMethod.label;

    return result;
}

- (void)_didSelectShippingMethod:(PKShippingMethod *)shippingMethod completion:(WebKit::DidSelectShippingMethodCompletion::BlockType)completion
{
    ASSERT(!_didSelectShippingMethodCompletion);
    _didSelectShippingMethodCompletion = completion;

    auto presenter = _presenter.get();
    if (!presenter)
        return [self completeShippingMethodSelection:@[ ]];

    presenter->client().presenterDidSelectShippingMethod(*presenter, toShippingMethod(shippingMethod));
}

- (void) NO_RETURN_DUE_TO_ASSERT _getPaymentServicesMerchantURL:(void(^)(NSURL *, NSError *))completion
{
    ASSERT_NOT_REACHED();
    completion(nil, nil);
}

- (void)_willFinishWithError:(NSError *)error
{
}

@end

#endif // USE(PASSKIT)
