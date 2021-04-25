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
#import "RenderThemeCocoa.h"

#import "GraphicsContextCG.h"
#import "HTMLInputElement.h"
#import "RenderText.h"
#import "UserAgentScripts.h"
#import "UserAgentStyleSheets.h"
#import <algorithm>
#import <pal/spi/cf/CoreTextSPI.h>

#if ENABLE(VIDEO)
#import "LocalizedStrings.h"
#import <wtf/BlockObjCExceptions.h>
#endif

#if PLATFORM(MAC)
#import <AppKit/NSFont.h>
#else
#import <UIKit/UIFont.h>
#import <pal/ios/UIKitSoftLink.h>
#endif

#if ENABLE(APPLE_PAY)
#import <pal/cocoa/PassKitSoftLink.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/RenderThemeCocoaAdditions.cpp>
#endif

@interface WebCoreRenderThemeBundle : NSObject
@end

@implementation WebCoreRenderThemeBundle
@end

namespace WebCore {

RenderThemeCocoa& RenderThemeCocoa::singleton()
{
    return static_cast<RenderThemeCocoa&>(RenderTheme::singleton());
}

void RenderThemeCocoa::purgeCaches()
{
#if ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)
    m_mediaControlsLocalizedStringsScript.clearImplIfNotShared();
    m_mediaControlsScript.clearImplIfNotShared();
    m_mediaControlsAdditionalScript.clearImplIfNotShared();
    m_mediaControlsStyleSheet.clearImplIfNotShared();
#endif // ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)

    RenderTheme::purgeCaches();
}

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
        style.setMinWidth(Length(applePayButtonPlainMinimumWidth, LengthType::Fixed));
    else
        style.setMinWidth(Length(applePayButtonMinimumWidth, LengthType::Fixed));
    style.setMinHeight(Length(applePayButtonMinimumHeight, LengthType::Fixed));

    if (!style.hasExplicitlySetBorderRadius()) {
        auto cornerRadius = PAL::get_PassKit_PKApplePayButtonDefaultCornerRadius();
        style.setBorderRadius({ { cornerRadius, LengthType::Fixed }, { cornerRadius, LengthType::Fixed } });
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
#endif
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

    PKDrawApplePayButtonWithCornerRadius(paintInfo.context().platformContext(), CGRectMake(paintRect.x(), -paintRect.maxY(), paintRect.width(), paintRect.height()), 1.0, largestCornerRadius, toPKPaymentButtonType(style.applePayButtonType()), toPKPaymentButtonStyle(style.applePayButtonStyle()), style.computedLocale());
    return false;
}

#endif // ENABLE(APPLE_PAY)

#if ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)

String RenderThemeCocoa::mediaControlsStyleSheet()
{
    if (m_mediaControlsStyleSheet.isEmpty())
        m_mediaControlsStyleSheet = StringImpl::createWithoutCopying(ModernMediaControlsUserAgentStyleSheet, sizeof(ModernMediaControlsUserAgentStyleSheet));
    return m_mediaControlsStyleSheet;
}

Vector<String, 3> RenderThemeCocoa::mediaControlsScripts()
{
    // FIXME: Localized strings are not worth having a script. We should make it JSON data etc. instead.
    if (m_mediaControlsLocalizedStringsScript.isEmpty()) {
        NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
        m_mediaControlsLocalizedStringsScript = [NSString stringWithContentsOfFile:[bundle pathForResource:@"modern-media-controls-localized-strings" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil];
    }

    if (m_mediaControlsScript.isEmpty())
        m_mediaControlsScript = StringImpl::createWithoutCopying(ModernMediaControlsJavaScript, sizeof(ModernMediaControlsJavaScript));

#if defined(RenderThemeCocoaAdditions_mediaControlsAdditionalScript)
    if (m_mediaControlsAdditionalScript.isEmpty())
        m_mediaControlsAdditionalScript = String(RenderThemeCocoaAdditions_mediaControlsAdditionalScript);
#endif

    return {
        m_mediaControlsLocalizedStringsScript,
        m_mediaControlsScript,
        m_mediaControlsAdditionalScript,
    };
}

String RenderThemeCocoa::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
    NSString *directory = @"modern-media-controls/images";
    NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
    return [[NSData dataWithContentsOfFile:[bundle pathForResource:iconName ofType:iconType inDirectory:directory]] base64EncodedStringWithOptions:0];
}

String RenderThemeCocoa::mediaControlsFormattedStringForDuration(const double durationInSeconds)
{
    if (!std::isfinite(durationInSeconds))
        return WEB_UI_STRING("indefinite time", "accessibility help text for an indefinite media controller time value");

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (!m_durationFormatter) {
        m_durationFormatter = adoptNS([NSDateComponentsFormatter new]);
        m_durationFormatter.get().unitsStyle = NSDateComponentsFormatterUnitsStyleFull;
        m_durationFormatter.get().allowedUnits = NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond;
        m_durationFormatter.get().formattingContext = NSFormattingContextStandalone;
        m_durationFormatter.get().maximumUnitCount = 2;
    }
    return [m_durationFormatter.get() stringFromTimeInterval:durationInSeconds];
    END_BLOCK_OBJC_EXCEPTIONS
}

#endif // ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)

FontCascadeDescription& RenderThemeCocoa::cachedSystemFontDescription(CSSValueID valueID) const
{
    static auto fontDescriptions = makeNeverDestroyed<std::array<FontCascadeDescription, 17>>({ });

    ASSERT(std::all_of(std::begin(fontDescriptions.get()), std::end(fontDescriptions.get()), [](auto& description) {
        return !description.isAbsoluteSize();
    }));

    switch (valueID) {
    case CSSValueAppleSystemHeadline:
        return fontDescriptions.get()[0];
    case CSSValueAppleSystemBody:
        return fontDescriptions.get()[1];
    case CSSValueAppleSystemTitle0:
        return fontDescriptions.get()[2];
    case CSSValueAppleSystemTitle1:
        return fontDescriptions.get()[3];
    case CSSValueAppleSystemTitle2:
        return fontDescriptions.get()[4];
    case CSSValueAppleSystemTitle3:
        return fontDescriptions.get()[5];
    case CSSValueAppleSystemTitle4:
        return fontDescriptions.get()[6];
    case CSSValueAppleSystemSubheadline:
        return fontDescriptions.get()[7];
    case CSSValueAppleSystemFootnote:
        return fontDescriptions.get()[8];
    case CSSValueAppleSystemCaption1:
        return fontDescriptions.get()[9];
    case CSSValueAppleSystemCaption2:
        return fontDescriptions.get()[10];
    case CSSValueAppleSystemShortHeadline:
        return fontDescriptions.get()[11];
    case CSSValueAppleSystemShortBody:
        return fontDescriptions.get()[12];
    case CSSValueAppleSystemShortSubheadline:
        return fontDescriptions.get()[13];
    case CSSValueAppleSystemShortFootnote:
        return fontDescriptions.get()[14];
    case CSSValueAppleSystemShortCaption1:
        return fontDescriptions.get()[15];
    case CSSValueAppleSystemTallBody:
        return fontDescriptions.get()[16];
    default:
        return RenderTheme::cachedSystemFontDescription(valueID);
    }
}

static inline FontSelectionValue cssWeightOfSystemFont(CTFontRef font)
{
    auto traits = adoptCF(CTFontCopyTraits(font));
    CFNumberRef resultRef = (CFNumberRef)CFDictionaryGetValue(traits.get(), kCTFontWeightTrait);
    float result = 0;
    CFNumberGetValue(resultRef, kCFNumberFloatType, &result);
    // These numbers were experimentally gathered from weights of the system font.
    static constexpr float weightThresholds[] = { -0.6, -0.365, -0.115, 0.130, 0.235, 0.350, 0.5, 0.7 };
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(weightThresholds); ++i) {
        if (result < weightThresholds[i])
            return FontSelectionValue((static_cast<int>(i) + 1) * 100);
    }
    return FontSelectionValue(900);
}

void RenderThemeCocoa::updateCachedSystemFontDescription(CSSValueID valueID, FontCascadeDescription& fontDescription) const
{
    auto cocoaFontClass = [] {
#if PLATFORM(IOS_FAMILY)
        return PAL::getUIFontClass();
#else
        return NSFont.class;
#endif
    };
    // FIXME: Hook up locale strings.
    RetainPtr<CTFontDescriptorRef> fontDescriptor;
    CFStringRef textStyle = nullptr;
    AtomString style;
    switch (valueID) {
    case CSSValueAppleSystemHeadline:
        textStyle = kCTUIFontTextStyleHeadline;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemBody:
        textStyle = kCTUIFontTextStyleBody;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemTitle0:
        textStyle = kCTUIFontTextStyleTitle0;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemTitle1:
        textStyle = kCTUIFontTextStyleTitle1;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemTitle2:
        textStyle = kCTUIFontTextStyleTitle2;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemTitle3:
        textStyle = kCTUIFontTextStyleTitle3;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemTitle4:
        textStyle = kCTUIFontTextStyleTitle4;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemSubheadline:
        textStyle = kCTUIFontTextStyleSubhead;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemFootnote:
        textStyle = kCTUIFontTextStyleFootnote;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemCaption1:
        textStyle = kCTUIFontTextStyleCaption1;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemCaption2:
        textStyle = kCTUIFontTextStyleCaption2;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemShortHeadline:
        textStyle = kCTUIFontTextStyleShortHeadline;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemShortBody:
        textStyle = kCTUIFontTextStyleShortBody;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemShortSubheadline:
        textStyle = kCTUIFontTextStyleShortSubhead;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemShortFootnote:
        textStyle = kCTUIFontTextStyleShortFootnote;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemShortCaption1:
        textStyle = kCTUIFontTextStyleShortCaption1;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueAppleSystemTallBody:
        textStyle = kCTUIFontTextStyleTallBody;
        fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(textStyle, contentSizeCategory(), nullptr));
        break;
    case CSSValueSmallCaption: {
        style = AtomString("system-ui", AtomString::ConstructFromLiteral);
        auto font = [cocoaFontClass() systemFontOfSize:[cocoaFontClass() smallSystemFontSize]];
        fontDescriptor = static_cast<CTFontDescriptorRef>(font.fontDescriptor);
        break;
    }
    case CSSValueMenu:
        style = AtomString("-apple-menu", AtomString::ConstructFromLiteral);
        fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontMenuItem, [cocoaFontClass() systemFontSize], nullptr));
        break;
    case CSSValueStatusBar: {
        style = AtomString("-apple-status-bar", AtomString::ConstructFromLiteral);
        fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, [cocoaFontClass() labelFontSize], nullptr));
        break;
    }
    case CSSValueWebkitMiniControl: {
        style = AtomString("system-ui", AtomString::ConstructFromLiteral);
#if PLATFORM(IOS_FAMILY)
        fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontMiniSystem, 0, nullptr));
#else
        auto font = [cocoaFontClass() systemFontOfSize:[cocoaFontClass() systemFontSizeForControlSize:NSControlSizeMini]];
        fontDescriptor = static_cast<CTFontDescriptorRef>(font.fontDescriptor);
#endif
        break;
    }
    case CSSValueWebkitSmallControl: {
        style = AtomString("system-ui", AtomString::ConstructFromLiteral);
#if PLATFORM(IOS_FAMILY)
        fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSmallSystem, 0, nullptr));
#else
        auto font = [cocoaFontClass() systemFontOfSize:[cocoaFontClass() systemFontSizeForControlSize:NSControlSizeSmall]];
        fontDescriptor = static_cast<CTFontDescriptorRef>(font.fontDescriptor);
#endif
        break;
    }
    case CSSValueWebkitControl: {
        style = AtomString("system-ui", AtomString::ConstructFromLiteral);
#if PLATFORM(IOS_FAMILY)
        fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, 0, nullptr));
#else
        auto font = [cocoaFontClass() systemFontOfSize:[cocoaFontClass() systemFontSizeForControlSize:NSControlSizeRegular]];
        fontDescriptor = static_cast<CTFontDescriptorRef>(font.fontDescriptor);
#endif
        break;
    }
    default:
        style = AtomString("system-ui", AtomString::ConstructFromLiteral);
        fontDescriptor = adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, 0, nullptr));
    }

    if (style.isNull())
        style = textStyle;

    ASSERT(fontDescriptor);
    auto font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), 0, nullptr));
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setOneFamily(style);
    fontDescription.setSpecifiedSize(CTFontGetSize(font.get()));
    fontDescription.setWeight(cssWeightOfSystemFont(font.get()));
    fontDescription.setItalic(normalItalicValue());
}

}
