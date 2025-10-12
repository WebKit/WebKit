/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ApplePayButtonCocoa.h"

#if ENABLE(APPLE_PAY)

#import "ApplePayButtonPart.h"
#import "FloatRoundedRect.h"
#import "GraphicsContextCG.h"
#import <wtf/TZoneMallocInlines.h>
#import <pal/cocoa/PassKitSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ApplePayButtonCocoa);

ApplePayButtonCocoa::ApplePayButtonCocoa(ApplePayButtonPart& owningPart)
    : PlatformControl(owningPart)
{
}

static PKPaymentButtonType toPKPaymentButtonType(ApplePayButtonType type)
{
    switch (type) {
    case ApplePayButtonType::Plain:
        return PKPaymentButtonTypePlain;
    case ApplePayButtonType::Buy:
        return PKPaymentButtonTypeBuy;
    case ApplePayButtonType::SetUp:
        return PKPaymentButtonTypeSetUp;
    case ApplePayButtonType::Donate:
        return PKPaymentButtonTypeDonate;
    case ApplePayButtonType::CheckOut:
        return PKPaymentButtonTypeCheckout;
    case ApplePayButtonType::Book:
        return PKPaymentButtonTypeBook;
    case ApplePayButtonType::Subscribe:
        return PKPaymentButtonTypeSubscribe;
#if HAVE(PASSKIT_NEW_BUTTON_TYPES)
    case ApplePayButtonType::Reload:
        return PKPaymentButtonTypeReload;
    case ApplePayButtonType::AddMoney:
        return PKPaymentButtonTypeAddMoney;
    case ApplePayButtonType::TopUp:
        return PKPaymentButtonTypeTopUp;
    case ApplePayButtonType::Order:
        return PKPaymentButtonTypeOrder;
    case ApplePayButtonType::Rent:
        return PKPaymentButtonTypeRent;
    case ApplePayButtonType::Support:
        return PKPaymentButtonTypeSupport;
    case ApplePayButtonType::Contribute:
        return PKPaymentButtonTypeContribute;
    case ApplePayButtonType::Tip:
        return PKPaymentButtonTypeTip;
#endif // HAVE(PASSKIT_NEW_BUTTON_TYPES)
    }
}

static PKPaymentButtonStyle toPKPaymentButtonStyle(ApplePayButtonStyle style)
{
    switch (style) {
    case ApplePayButtonStyle::White:
        return PKPaymentButtonStyleWhite;
    case ApplePayButtonStyle::WhiteOutline:
        return PKPaymentButtonStyleWhiteOutline;
    case ApplePayButtonStyle::Black:
        return PKPaymentButtonStyleBlack;
    }
}

void ApplePayButtonCocoa::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle&)
{
    auto largestCornerRadius = std::max({
        borderRect.radii().topLeft().maxDimension(),
        borderRect.radii().topRight().maxDimension(),
        borderRect.radii().bottomLeft().maxDimension(),
        borderRect.radii().bottomRight().maxDimension()
    });

    GraphicsContextStateSaver stateSaver(context);

    context.setShouldSmoothFonts(true);
    context.scale(FloatSize(1, -1));

    auto logicalRect = borderRect.rect();
    Ref applePayButtonPart = owningApplePayButtonPart();
    
    PKDrawApplePayButtonWithCornerRadius(
        RetainPtr { context.platformContext() }.get(),
        CGRectMake(logicalRect.x(), -logicalRect.maxY(), logicalRect.width(), logicalRect.height()),
        1.0,
        largestCornerRadius,
        toPKPaymentButtonType(applePayButtonPart->buttonType()),
        toPKPaymentButtonStyle(applePayButtonPart->buttonStyle()),
        applePayButtonPart->locale().createNSString().get()
    );
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
