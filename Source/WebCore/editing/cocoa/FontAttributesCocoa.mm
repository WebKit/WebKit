/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "FontAttributes.h"

#import "CSSValueKeywords.h"
#import "ColorCocoa.h"
#import "FontCocoa.h"
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#endif

namespace WebCore {

static NSString *cocoaTextListMarkerName(ListStyleType styleType, bool ordered)
{
    if (styleType.type == ListStyleType::Type::Disc || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueDisc)))
        return NSTextListMarkerDisc;
    if (styleType.type == ListStyleType::Type::Circle || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueCircle)))
        return NSTextListMarkerCircle;
    if (styleType.type == ListStyleType::Type::Square || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueSquare)))
        return NSTextListMarkerSquare;
    if (styleType.type == ListStyleType::Type::Decimal || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueDecimal)))
        return NSTextListMarkerDecimal;
    if (styleType.type == ListStyleType::Type::Octal || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueOctal)))
        return NSTextListMarkerOctal;
    if (styleType.type == ListStyleType::Type::LowerRoman || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueLowerRoman)))
        return NSTextListMarkerLowercaseRoman;
    if (styleType.type == ListStyleType::Type::UpperRoman || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueUpperRoman)))
        return NSTextListMarkerUppercaseRoman;
    if (styleType.type == ListStyleType::Type::LowerAlpha || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueLowerAlpha)))
        return NSTextListMarkerLowercaseAlpha;
    if (styleType.type == ListStyleType::Type::UpperAlpha || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueUpperAlpha)))
        return NSTextListMarkerUppercaseAlpha;
    if (styleType.type == ListStyleType::Type::LowerLatin || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueLowerLatin)))
        return NSTextListMarkerLowercaseLatin;
    if (styleType.type == ListStyleType::Type::UpperLatin || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueUpperLatin)))
        return NSTextListMarkerUppercaseLatin;
    if (styleType.type == ListStyleType::Type::LowerHexadecimal || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueLowerHexadecimal)))
        return NSTextListMarkerLowercaseHexadecimal;
    if (styleType.type == ListStyleType::Type::UpperHexadecimal || (styleType.type == ListStyleType::Type::CounterStyle && styleType.identifier == nameLiteral(CSSValueUpperHexadecimal)))
        return NSTextListMarkerUppercaseHexadecimal;
    // The remaining web-exposed list style types have no Cocoa equivalents.
    // Fall back to default styles for ordered and unordered lists.
    return ordered ? NSTextListMarkerDecimal : NSTextListMarkerDisc;
}

RetainPtr<NSTextList> TextList::createTextList() const
{
#if PLATFORM(MAC)
    Class textListClass = NSTextList.class;
#else
    Class textListClass = PAL::getNSTextListClass();
#endif
    auto result = adoptNS([[textListClass alloc] initWithMarkerFormat:cocoaTextListMarkerName(styleType, ordered) options:0]);
    [result setStartingItemNumber:startingItemNumber];
    return result;
}

RetainPtr<NSDictionary> FontAttributes::createDictionary() const
{
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
    if (auto cocoaFont = font ? (__bridge CocoaFont *)font->getCTFont() : nil)
        attributes[NSFontAttributeName] = cocoaFont;

    if (foregroundColor.isValid())
        attributes[NSForegroundColorAttributeName] = cocoaColor(foregroundColor).get();

    if (backgroundColor.isValid())
        attributes[NSBackgroundColorAttributeName] = cocoaColor(backgroundColor).get();

    if (fontShadow.color.isValid() && (!fontShadow.offset.isZero() || fontShadow.blurRadius))
        attributes[NSShadowAttributeName] = fontShadow.createShadow().get();

    if (subscriptOrSuperscript == SubscriptOrSuperscript::Subscript)
        attributes[NSSuperscriptAttributeName] = @(-1);
    else if (subscriptOrSuperscript == SubscriptOrSuperscript::Superscript)
        attributes[NSSuperscriptAttributeName] = @1;

#if PLATFORM(MAC)
    Class paragraphStyleClass = NSParagraphStyle.class;
#else
    Class paragraphStyleClass = PAL::getNSParagraphStyleClass();
#endif
    auto style = adoptNS([[paragraphStyleClass defaultParagraphStyle] mutableCopy]);

    switch (horizontalAlignment) {
    case HorizontalAlignment::Left:
        [style setAlignment:NSTextAlignmentLeft];
        break;
    case HorizontalAlignment::Center:
        [style setAlignment:NSTextAlignmentCenter];
        break;
    case HorizontalAlignment::Right:
        [style setAlignment:NSTextAlignmentRight];
        break;
    case HorizontalAlignment::Justify:
        [style setAlignment:NSTextAlignmentJustified];
        break;
    case HorizontalAlignment::Natural:
        [style setAlignment:NSTextAlignmentNatural];
        break;
    }

    if (!textLists.isEmpty()) {
        [style setTextLists:createNSArray(textLists, [] (auto& textList) {
            return textList.createTextList();
        }).get()];
    }

    attributes[NSParagraphStyleAttributeName] = style.get();

    if (hasUnderline)
        attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);

    if (hasStrikeThrough)
        attributes[NSStrikethroughStyleAttributeName] = @(NSUnderlineStyleSingle);

    return attributes;
}

} // namespace WebCore
