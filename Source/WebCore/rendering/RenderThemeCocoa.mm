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

#include "config.h"
#include "RenderThemeCocoa.h"

#if ENABLE(APPLE_PAY)

#include "PassKitSPI.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "SoftLinking.h"
#include "TranslateTransformOperation.h"

#if PLATFORM(MAC)
SOFT_LINK_PRIVATE_FRAMEWORK(PassKit);
#else
SOFT_LINK_FRAMEWORK(PassKit);
#endif

SOFT_LINK_MAY_FAIL(PassKit, PKDrawApplePayButton, void, (CGContextRef context, CGRect drawRect, CGFloat scale, PKPaymentButtonType type, PKPaymentButtonStyle style, NSString *languageCode), (context, drawRect, scale, type, style, languageCode));

namespace WebCore {

static const auto applePayButtonMinimumWidth = 140;
static const auto applePayButtonPlainMinimumWidth = 100;
static const auto applePayButtonMinimumHeight = 30;

void RenderThemeCocoa::adjustApplePayButtonStyle(StyleResolver&, RenderStyle& style, const Element*) const
{
    if (style.applePayButtonType() == ApplePayButtonType::Plain)
        style.setMinWidth(Length(applePayButtonPlainMinimumWidth, Fixed));
    else
        style.setMinWidth(Length(applePayButtonMinimumWidth, Fixed));
    style.setMinHeight(Length(applePayButtonMinimumHeight, Fixed));
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
    case ApplePayButtonType::Other:
        // FIXME: Use a named constant here.
        return (PKPaymentButtonType)4;
    }
}

bool RenderThemeCocoa::paintApplePayButton(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& paintRect)
{
    if (!canLoadPKDrawApplePayButton())
        return false;

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    paintInfo.context().setShouldSmoothFonts(true);
    paintInfo.context().scale(FloatSize(1, -1));

    CGContextSetTextMatrix(paintInfo.context().platformContext(), CGAffineTransformIdentity);
    PKDrawApplePayButton(paintInfo.context().platformContext(), CGRectMake(paintRect.x(), -paintRect.maxY(), paintRect.width(), paintRect.height()), 1.0, toPKPaymentButtonType(renderer.style().applePayButtonType()), toPKPaymentButtonStyle(renderer.style().applePayButtonStyle()), renderer.style().locale());

    return false;
}

}

#endif
