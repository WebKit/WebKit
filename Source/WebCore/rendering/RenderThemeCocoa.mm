/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "RenderThemeCocoa.h"

#import "GraphicsContextCG.h"
#import "HTMLInputElement.h"
#import "RenderText.h"

#if ENABLE(APPLE_PAY)

#import <pal/cocoa/PassKitSoftLink.h>

#endif // ENABLE(APPLE_PAY)

#if ENABLE(VIDEO)
#import "LocalizedStrings.h"
#import <wtf/BlockObjCExceptions.h>
#endif

namespace WebCore {

bool RenderThemeCocoa::shouldHaveCapsLockIndicator(const HTMLInputElement& element) const
{
    return element.isPasswordField();
}

#if ENABLE(APPLE_PAY)

static const auto applePayButtonMinimumWidth = 140;
static const auto applePayButtonPlainMinimumWidth = 100;
static const auto applePayButtonMinimumHeight = 30;

void RenderThemeCocoa::adjustApplePayButtonStyle(RenderStyle& style, const Element*) const
{
    if (style.applePayButtonType() == ApplePayButtonType::Plain)
        style.setMinWidth(Length(applePayButtonPlainMinimumWidth, Fixed));
    else
        style.setMinWidth(Length(applePayButtonMinimumWidth, Fixed));
    style.setMinHeight(Length(applePayButtonMinimumHeight, Fixed));

    if (!style.hasExplicitlySetBorderRadius()) {
        auto cornerRadius = PAL::get_PassKit_PKApplePayButtonDefaultCornerRadius();
        style.setBorderRadius({ { cornerRadius, Fixed }, { cornerRadius, Fixed } });
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
    }
}

bool RenderThemeCocoa::paintApplePayButton(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& paintRect)
{
    GraphicsContextStateSaver stateSaver(paintInfo.context());

    paintInfo.context().setShouldSmoothFonts(true);
    paintInfo.context().scale(FloatSize(1, -1));

    auto& style = renderer.style();
    auto largestCornerRadius = std::max<CGFloat>({
        floatValueForLength(style.borderTopLeftRadius().height, paintRect.height()),
        floatValueForLength(style.borderTopLeftRadius().width, paintRect.width()),
        floatValueForLength(style.borderTopRightRadius().height, paintRect.height()),
        floatValueForLength(style.borderTopRightRadius().width, paintRect.width()),
        floatValueForLength(style.borderBottomLeftRadius().height, paintRect.height()),
        floatValueForLength(style.borderBottomLeftRadius().width, paintRect.width()),
        floatValueForLength(style.borderBottomRightRadius().height, paintRect.height()),
        floatValueForLength(style.borderBottomRightRadius().width, paintRect.width())
    });

    PKDrawApplePayButtonWithCornerRadius(paintInfo.context().platformContext(), CGRectMake(paintRect.x(), -paintRect.maxY(), paintRect.width(), paintRect.height()), 1.0, largestCornerRadius, toPKPaymentButtonType(style.applePayButtonType()), toPKPaymentButtonStyle(style.applePayButtonStyle()), style.locale());
    return false;
}

#endif // ENABLE(APPLE_PAY)

String RenderThemeCocoa::mediaControlsFormattedStringForDuration(const double durationInSeconds)
{
    if (!std::isfinite(durationInSeconds))
        return WEB_UI_STRING("indefinite time", "accessibility help text for an indefinite media controller time value");

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (!m_durationFormatter) {
        m_durationFormatter = adoptNS([NSDateComponentsFormatter new]);
        m_durationFormatter.get().unitsStyle = NSDateComponentsFormatterUnitsStyleFull;
        m_durationFormatter.get().allowedUnits = NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond;
        m_durationFormatter.get().formattingContext = NSFormattingContextStandalone;
        m_durationFormatter.get().maximumUnitCount = 2;
    }
    return [m_durationFormatter.get() stringFromTimeInterval:durationInSeconds];
    END_BLOCK_OBJC_EXCEPTIONS;
}

}
