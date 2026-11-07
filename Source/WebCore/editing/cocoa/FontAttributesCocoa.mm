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
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#endif

namespace WebCore {

static RetainPtr<NSString> cocoaTextListMarkerName(const Style::ListStyleType& styleType, bool ordered)
{
    // See https://developer.apple.com/documentation/appkit/nstextlist/init(markerformat:options:)
    // for the description of the expected format string for a marker format.

    return WTF::switchOn(styleType,
        [&](const Style::CounterStyle& counterStyle) -> RetainPtr<NSString> {
            // Each of these keywords intrinsically denotes if the NSTextList is considered ordered or not,
            // which may or may not be logically consistent with the semantics of an element's <ol> / <ul> tag,
            // but *is* consistent with the list style type, which takes precedent per the specification.
            //
            // Consequently, a <ul> element with a list-style-type of `decimal` for example will have the
            // following properties when converted to an NSTextList:
            //
            // - The marker format would be NSTextListMarkerDecimal aka `{decimal}`
            // - The marker would be `n` where `n` is the list element ordinal
            // - The list will be considered "ordered".

            if (counterStyle == CSSValueID::Disc)
                return NSTextListMarkerDisc;
            if (counterStyle == CSSValueID::Circle)
                return NSTextListMarkerCircle;
            if (counterStyle == CSSValueID::Square)
                return NSTextListMarkerSquare;
            if (counterStyle == CSSValueID::Decimal)
                return NSTextListMarkerDecimal;
            if (counterStyle == CSSValueID::Octal)
                return NSTextListMarkerOctal;
            if (counterStyle == CSSValueID::LowerRoman)
                return NSTextListMarkerLowercaseRoman;
            if (counterStyle == CSSValueID::UpperRoman)
                return NSTextListMarkerUppercaseRoman;
            if (counterStyle == CSSValueID::LowerAlpha)
                return NSTextListMarkerLowercaseAlpha;
            if (counterStyle == CSSValueID::UpperAlpha)
                return NSTextListMarkerUppercaseAlpha;
            if (counterStyle == CSSValueID::LowerLatin)
                return NSTextListMarkerLowercaseLatin;
            if (counterStyle == CSSValueID::UpperLatin)
                return NSTextListMarkerUppercaseLatin;
            if (counterStyle == CSSValueID::LowerHexadecimal)
                return NSTextListMarkerLowercaseHexadecimal;
            if (counterStyle == CSSValueID::UpperHexadecimal)
                return NSTextListMarkerUppercaseHexadecimal;

            // The remaining web-exposed list style types have no Cocoa equivalents.
            // Fall back to default styles for ordered and unordered lists.
            return ordered ? NSTextListMarkerDecimal : NSTextListMarkerDisc;
        },
        [&](const CSS::Keyword::None&) {
            return retainPtr(ordered ? NSTextListMarkerDecimal : NSTextListMarkerDisc);
        },
        [&](const Style::String& identifier) {
            // The marker for the marker format `{<identifier>}` is `n` where `n` is the item ordinal, and the list is considered ordered.
            // The marker for the marker format `<identifier>` is `<identifier>`, and the list is considered unordered.
            auto format = ordered ? makeString("{"_s, identifier.value, "}"_s) : identifier.value;
            return format.createNSString();
        },
        [&](const Style::ListStyleType::SymbolsFunction&) {
            // The anonymous counter style defined by `symbols()` has no Cocoa equivalent.
            // Fall back to default styles for ordered and unordered lists.
            return retainPtr(ordered ? NSTextListMarkerDecimal : NSTextListMarkerDisc);
        }
    );
}

RetainPtr<NSTextList> TextList::createTextList() const
{
#if PLATFORM(MAC)
    Class textListClass = NSTextList.class;
#else
    Class textListClass = PAL::getNSTextListClassSingleton();
#endif
    RetainPtr result = adoptNS([[textListClass alloc] initWithMarkerFormat:cocoaTextListMarkerName(styleType, ordered).get() options:0]);
    [result setStartingItemNumber:startingItemNumber];
    return result;
}

// Returns nil for an invalid header level.
static RetainPtr<NSPresentationIntent> createPresentationIntent(const BlockPresentationIntent& intent, NSInteger identity, NSPresentationIntent *parentIntent)
{
    using Kind = BlockPresentationIntent::Kind;

    switch (intent.kind) {
    case Kind::Paragraph:
        return [NSPresentationIntent paragraphIntentWithIdentity:identity nestedInsideIntent:parentIntent];
    case Kind::Header:
        if (intent.headerLevel < 1 || intent.headerLevel > 6)
            return nil;
        return [NSPresentationIntent headerIntentWithIdentity:identity level:intent.headerLevel nestedInsideIntent:parentIntent];
    case Kind::OrderedList:
        return [NSPresentationIntent orderedListIntentWithIdentity:identity nestedInsideIntent:parentIntent];
    case Kind::UnorderedList:
        return [NSPresentationIntent unorderedListIntentWithIdentity:identity nestedInsideIntent:parentIntent];
    case Kind::ListItem:
        return [NSPresentationIntent listItemIntentWithIdentity:identity ordinal:intent.ordinal nestedInsideIntent:parentIntent];
    case Kind::CodeBlock:
        return [NSPresentationIntent codeBlockIntentWithIdentity:identity languageHint:nsStringNilIfEmpty(intent.languageHint).get() nestedInsideIntent:parentIntent];
    case Kind::BlockQuote:
        return [NSPresentationIntent blockQuoteIntentWithIdentity:identity nestedInsideIntent:parentIntent];
    case Kind::ThematicBreak:
        return [NSPresentationIntent thematicBreakIntentWithIdentity:identity nestedInsideIntent:parentIntent];
    }
    ASSERT_NOT_REACHED();
    return nil;
}

static std::optional<BlockPresentationIntent> listIntentForListItem(NSPresentationIntent *listItemIntent)
{
    using Kind = BlockPresentationIntent::Kind;

    switch (listItemIntent.parentIntent.intentKind) {
    case NSPresentationIntentKindOrderedList:
        return BlockPresentationIntent { Kind::OrderedList };
    case NSPresentationIntentKindUnorderedList:
        return BlockPresentationIntent { Kind::UnorderedList };
    default:
        return std::nullopt;
    }
}

std::optional<BlockPresentationIntent> BlockPresentationIntent::fromPresentationIntent(NSPresentationIntent *intent)
{
    if (!intent)
        return std::nullopt;

    switch (intent.intentKind) {
    case NSPresentationIntentKindParagraph:
        // Markdown puts a list item's text in a paragraph.
        if (intent.parentIntent.intentKind == NSPresentationIntentKindListItem)
            return listIntentForListItem(intent.parentIntent);
        return BlockPresentationIntent { Kind::Paragraph };
    case NSPresentationIntentKindHeader:
        if (intent.headerLevel < 1 || intent.headerLevel > 6)
            return std::nullopt;
        return BlockPresentationIntent { Kind::Header, static_cast<uint8_t>(intent.headerLevel) };
    case NSPresentationIntentKindOrderedList:
        return BlockPresentationIntent { Kind::OrderedList };
    case NSPresentationIntentKindUnorderedList:
        return BlockPresentationIntent { Kind::UnorderedList };
    case NSPresentationIntentKindListItem:
        return listIntentForListItem(intent);
    case NSPresentationIntentKindCodeBlock:
        return BlockPresentationIntent { Kind::CodeBlock, 0, 0, intent.languageHint };
    case NSPresentationIntentKindBlockQuote:
        return BlockPresentationIntent { Kind::BlockQuote };
    case NSPresentationIntentKindThematicBreak:
        return BlockPresentationIntent { Kind::ThematicBreak };
    case NSPresentationIntentKindTable:
    case NSPresentationIntentKindTableHeaderRow:
    case NSPresentationIntentKindTableRow:
    case NSPresentationIntentKindTableCell:
        return std::nullopt;
    }
    return std::nullopt;
}

RetainPtr<NSDictionary> FontAttributes::createDictionary() const
{
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
    if (RetainPtr cocoaFont = font ? bridge_cast(font->ctFont()) : nil)
        attributes[NSFontAttributeName] = cocoaFont.get();

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
    Class paragraphStyleClass = PAL::getNSParagraphStyleClassSingleton();
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

    RetainPtr<NSPresentationIntent> presentationIntent;
    NSInteger identity = 0;
    for (auto& blockIntent : blockPresentationIntents) {
        if (RetainPtr intent = createPresentationIntent(blockIntent, ++identity, presentationIntent.get()))
            presentationIntent = WTF::move(intent);
    }
    if (presentationIntent)
        attributes[NSPresentationIntentAttributeName] = presentationIntent.get();

    return attributes;
}

} // namespace WebCore
