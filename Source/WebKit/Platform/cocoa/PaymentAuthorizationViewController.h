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

#pragma once

#if USE(PASSKIT)

#include "PaymentAuthorizationPresenter.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS PKPaymentAuthorizationViewController;
OBJC_CLASS PKPaymentRequest;
OBJC_CLASS WKPaymentAuthorizationDelegate;
OBJC_CLASS WKPaymentAuthorizationViewControllerDelegate;

namespace WebKit {

class WebPaymentCoordinatorProxy;

class PaymentAuthorizationViewController final : public PaymentAuthorizationPresenter {
public:
    PaymentAuthorizationViewController(PaymentAuthorizationPresenter::Client&, PKPaymentRequest *, PKPaymentAuthorizationViewController * = nil);

private:
    // PaymentAuthorizationPresenter
    WKPaymentAuthorizationDelegate *platformDelegate() final;
    void dismiss() final;
#if PLATFORM(IOS_FAMILY)
    void present(UIViewController *, CompletionHandler<void(bool)>&&) final;
#endif

    RetainPtr<PKPaymentAuthorizationViewController> m_viewController;
    RetainPtr<WKPaymentAuthorizationViewControllerDelegate> m_delegate;
};

} // namespace WebKit

#endif // USE(PASSKIT)
