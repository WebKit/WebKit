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

#import "ColorCocoa.h"
#import <pal/spi/cocoa/NSAttributedStringSPI.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#endif

namespace WebCore {

static NSString *cocoaTextListMarkerName(ListStyleType style, bool ordered)
{
    switch (style) {
    case ListStyleType::Disc:
        return NSTextListMarkerDisc;
    case ListStyleType::Circle:
        return NSTextListMarkerCircle;
    case ListStyleType::Square:
        return NSTextListMarkerSquare;
    case ListStyleType::Decimal:
        return NSTextListMarkerDecimal;
    case ListStyleType::Octal:
        return NSTextListMarkerOctal;
    case ListStyleType::LowerRoman:
        return NSTextListMarkerLowercaseRoman;
    case ListStyleType::UpperRoman:
        return NSTextListMarkerUppercaseRoman;
    case ListStyleType::LowerAlpha:
        return NSTextListMarkerLowercaseAlpha;
    case ListStyleType::UpperAlpha:
        return NSTextListMarkerUppercaseAlpha;
    case ListStyleType::LowerLatin:
        return NSTextListMarkerLowercaseLatin;
    case ListStyleType::UpperLatin:
        return NSTextListMarkerUppercaseLatin;
    case ListStyleType::LowerHexadecimal:
        return NSTextListMarkerLowercaseHexadecimal;
    case ListStyleType::UpperHexadecimal:
        return NSTextListMarkerUppercaseHexadecimal;
    default:
        // The remaining web-exposed list style types have no Cocoa equivalents.
        // Fall back to default styles for ordered and unordered lists.
        return ordered ? NSTextListMarkerDecimal : NSTextListMarkerDisc;
    }
}

RetainPtr<NSTextList> TextList::createTextList() const
{
#if PLATFORM(MAC)
    Class textListClass = NSTextList.class;
#else
    Class textListClass = PAL::get_UIKit_NSTextListClass();
#endif
    auto result = adoptNS([[textListClass alloc] initWithMarkerFormat:cocoaTextListMarkerName(style, ordered) options:0]);
    [result setStartingItemNumber:startingItemNumber];
    return result;
}

RetainPtr<NSDictionary> FontAttributes::createDictionary() const
{
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
    if (font)
        attributes[NSFontAttributeName] = font.get();

    if (foregroundColor.isValid())
        attributes[NSForegroundColorAttributeName] = platformColor(foregroundColor);

    if (backgroundColor.isValid())
        attributes[NSBackgroundColorAttributeName] = platformColor(backgroundColor);

    if (fontShadow.color.isValid() && (!fontShadow.offset.isZero() || fontShadow.blurRadius))
        attributes[NSShadowAttributeName] = fontShadow.createShadow().get();

    if (subscriptOrSuperscript == SubscriptOrSuperscript::Subscript)
        attributes[NSSuperscriptAttributeName] = @(-1);
    else if (subscriptOrSuperscript == SubscriptOrSuperscript::Superscript)
        attributes[NSSuperscriptAttributeName] = @1;

#if PLATFORM(MAC)
    Class paragraphStyleClass = NSParagraphStyle.class;
#else
    Class paragraphStyleClass = PAL::get_UIKit_NSParagraphStyleClass();
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
        auto textListArray = adoptNS([[NSMutableArray alloc] initWithCapacity:textLists.size()]);
        for (auto& textList : textLists)
            [textListArray addObject:textList.createTextList().get()];
        [style setTextLists:textListArray.get()];
    }

    attributes[NSParagraphStyleAttributeName] = style.get();

    if (hasUnderline)
        attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);

    if (hasStrikeThrough)
        attributes[NSStrikethroughStyleAttributeName] = @(NSUnderlineStyleSingle);

    return attributes;
}

} // namespace WebCore
