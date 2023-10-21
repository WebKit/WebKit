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
#import "AccessibilityObject.h"

#if ENABLE(ACCESSIBILITY) && PLATFORM(COCOA)

#import "AXObjectCache.h"
#import "TextIterator.h"
#import "WebAccessibilityObjectWrapperBase.h"
#import <wtf/cocoa/TypeCastsCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(AXRuntime);

SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenFontName, NSString *);
#define AccessibilityTokenFontName getUIAccessibilityTokenFontName()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenFontFamily, NSString *);
#define AccessibilityTokenFontFamily getUIAccessibilityTokenFontFamily()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenFontSize, NSString *);
#define AccessibilityTokenFontSize getUIAccessibilityTokenFontSize()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenBold, NSString *);
#define AccessibilityTokenBold getUIAccessibilityTokenBold()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenItalic, NSString *);
#define AccessibilityTokenItalic getUIAccessibilityTokenItalic()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenAttachment, NSString *);
#define AccessibilityTokenAttachment getUIAccessibilityTokenAttachment()

#endif // PLATFORM(IOS_FAMILY)

namespace WebCore {

String AccessibilityObject::speechHintAttributeValue() const
{
    auto speak = speakAsProperty();
    NSMutableArray<NSString *> *hints = [NSMutableArray array];
    [hints addObject:(speak & SpeakAs::SpellOut) ? @"spell-out" : @"normal"];
    if (speak & SpeakAs::Digits)
        [hints addObject:@"digits"];
    if (speak & SpeakAs::LiteralPunctuation)
        [hints addObject:@"literal-punctuation"];
    if (speak & SpeakAs::NoPunctuation)
        [hints addObject:@"no-punctuation"];
    return [hints componentsJoinedByString:@" "];
}

static bool isVisibleText(AccessibilityTextSource textSource)
{
    switch (textSource) {
    case AccessibilityTextSource::Visible:
    case AccessibilityTextSource::Children:
    case AccessibilityTextSource::LabelByElement:
        return true;
    case AccessibilityTextSource::Alternative:
    case AccessibilityTextSource::Summary:
    case AccessibilityTextSource::Help:
    case AccessibilityTextSource::TitleTag:
    case AccessibilityTextSource::Placeholder:
    case AccessibilityTextSource::Title:
    case AccessibilityTextSource::Subtitle:
    case AccessibilityTextSource::Action:
        return false;
    }
}

static bool isDescriptiveText(AccessibilityTextSource textSource)
{
    switch (textSource) {
    case AccessibilityTextSource::Alternative:
    case AccessibilityTextSource::Visible:
    case AccessibilityTextSource::Children:
    case AccessibilityTextSource::LabelByElement:
        return true;
    case AccessibilityTextSource::Summary:
    case AccessibilityTextSource::Help:
    case AccessibilityTextSource::TitleTag:
    case AccessibilityTextSource::Placeholder:
    case AccessibilityTextSource::Title:
    case AccessibilityTextSource::Subtitle:
    case AccessibilityTextSource::Action:
        return false;
    }
}

FloatPoint AccessibilityObject::screenRelativePosition() const
{
    auto rect = snappedIntRect(elementRect());
    // The Cocoa accessibility API wants the lower-left corner.
    return convertRectToPlatformSpace(FloatRect(FloatPoint(rect.x(), rect.maxY()), FloatSize()), AccessibilityConversionSpace::Screen).location();
}

String AccessibilityObject::descriptionAttributeValue() const
{
    if (!shouldComputeDescriptionAttributeValue())
        return { };

    Vector<AccessibilityText> textOrder;
    accessibilityText(textOrder);
    
    // Determine if any visible text is available, which influences our usage of title tag.
    bool visibleTextAvailable = false;
    for (const auto& text : textOrder) {
        if (isVisibleText(text.textSource) && !text.text.isEmpty()) {
            visibleTextAvailable = true;
            break;
        }
    }
    
    NSMutableString *returnText = [NSMutableString string];
    for (const auto& text : textOrder) {
        if (text.textSource == AccessibilityTextSource::Alternative) {
            [returnText appendString:text.text];
            break;
        }
        
        switch (text.textSource) {
        // These are sub-components of one element (Attachment) that are re-combined in OSX and iOS.
        case AccessibilityTextSource::Title:
        case AccessibilityTextSource::Subtitle:
        case AccessibilityTextSource::Action: {
            if (!text.text.length())
                break;
            if ([returnText length])
                [returnText appendString:@", "];
            [returnText appendString:text.text];
            break;
        }
        default:
            break;
        }

        if (text.textSource == AccessibilityTextSource::TitleTag && !visibleTextAvailable) {
            [returnText appendString:text.text];
            break;
        }
    }

    return returnText;
}

String AccessibilityObject::titleAttributeValue() const
{
    // Meter elements should communicate their content via AXValueDescription.
    if (!shouldComputeTitleAttributeValue() || isMeter())
        return { };

    // Summary element should use its text node as AXTitle.
    if (isSummary())
        return textUnderElement();

    // A file upload button presents a challenge because it has button text and a value, but the
    // API doesn't support this paradigm.
    // The compromise is to return the button type in the role description and the value of the file path in the title
    if (isFileUploadButton() && fileUploadButtonReturnsValueInTitle())
        return stringValue();

    Vector<AccessibilityText> textOrder;
    accessibilityText(textOrder);

    for (const auto& text : textOrder) {
        // If we have alternative text, then we should not expose a title.
        if (text.textSource == AccessibilityTextSource::Alternative)
            break;

        // Once we encounter visible text, or the text from our children that should be used foremost.
        if (text.textSource == AccessibilityTextSource::Visible || text.textSource == AccessibilityTextSource::Children)
            return text.text;

        // If there's an element that labels this object and it's not exposed, then we should use
        // that text as our title.
        if (text.textSource == AccessibilityTextSource::LabelByElement)
            return text.text;
    }

    return { };
}

String AccessibilityObject::helpTextAttributeValue() const
{
    Vector<AccessibilityText> textOrder;
    accessibilityText(textOrder);

    // Determine if any descriptive text is available, which influences our usage of title tag.
    bool descriptiveTextAvailable = false;
    for (const auto& text : textOrder) {
        if (isDescriptiveText(text.textSource) && !text.text.isEmpty()) {
            descriptiveTextAvailable = true;
            break;
        }
    }

    for (const auto& text : textOrder) {
        if (text.textSource == AccessibilityTextSource::Help || text.textSource == AccessibilityTextSource::Summary)
            return text.text;

        // If an element does NOT have other descriptive text the title tag should be used as its descriptive text.
        // But, if those ARE available, then the title tag should be used for help text instead.
        if (text.textSource == AccessibilityTextSource::TitleTag && descriptiveTextAvailable)
            return text.text;
    }

    return { };
}

AXTextMarkerRange AccessibilityObject::textMarkerRangeForNSRange(const NSRange& range) const
{
    if (range.location == NSNotFound)
        return { };

    if (!isTextControl())
        return { visiblePositionForIndex(range.location), visiblePositionForIndex(range.location + range.length) };

    if (range.location + range.length > text().length())
        return { };

    if (auto* cache = axObjectCache()) {
        auto start = cache->characterOffsetForIndex(range.location, this);
        auto end = cache->characterOffsetForIndex(range.location + range.length, this);
        return cache->rangeForUnorderedCharacterOffsets(start, end);
    }
    return { };
}

// NSAttributedString support.

#ifndef NSAttachmentCharacter
#define NSAttachmentCharacter 0xfffc
#endif

static void addObjectWrapperToArray(const AccessibilityObject& object, NSMutableArray *array)
{
    auto* wrapper = object.wrapper();
    if (!wrapper)
        return;

    // Don't add the same object twice.
    if ([array containsObject:wrapper])
        return;

#if PLATFORM(IOS_FAMILY)
    // Explicitly set that this is a new element, in case other logic tries to override.
    [wrapper setValue:@YES forKey:@"isAccessibilityElement"];
#endif

    [array addObject:wrapper];
}

// When modifying attributed strings, the range can come from a source which may provide faulty information (e.g. the spell checker).
// To protect against such cases, the range should be validated before adding or removing attributes.
bool attributedStringContainsRange(NSAttributedString *attributedString, const NSRange& range)
{
    return NSMaxRange(range) <= attributedString.length;
}

void attributedStringSetNumber(NSMutableAttributedString *attrString, NSString *attribute, NSNumber *number, const NSRange& range)
{
    if (!attributedStringContainsRange(attrString, range))
        return;

    if (number)
        [attrString addAttribute:attribute value:number range:range];
}

void attributedStringSetFont(NSMutableAttributedString *attributedString, CTFontRef font, const NSRange& range)
{
    if (!attributedStringContainsRange(attributedString, range) || !font)
        return;

    auto fontAttributes = adoptNS([[NSMutableDictionary alloc] init]);
    auto familyName = adoptCF(CTFontCopyFamilyName(font));
    NSNumber *size = [NSNumber numberWithFloat:CTFontGetSize(font)];

#if PLATFORM(IOS_FAMILY)
    auto fullName = adoptCF(CTFontCopyFullName(font));
    if (fullName)
        [fontAttributes setValue:bridge_cast(fullName.get()) forKey:AccessibilityTokenFontName];
    if (familyName)
        [fontAttributes setValue:bridge_cast(familyName.get()) forKey:AccessibilityTokenFontFamily];
    if ([size boolValue])
        [fontAttributes setValue:size forKey:AccessibilityTokenFontSize];
    auto traits = CTFontGetSymbolicTraits(font);
    if (traits & kCTFontTraitBold)
        [fontAttributes setValue:@YES forKey:AccessibilityTokenBold];
    if (traits & kCTFontTraitItalic)
        [fontAttributes setValue:@YES forKey:AccessibilityTokenItalic];

    [attributedString addAttributes:fontAttributes.get() range:range];
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
    [fontAttributes setValue:size forKey:NSAccessibilityFontSizeKey];

    if (familyName)
        [fontAttributes setValue:bridge_cast(familyName.get()) forKey:NSAccessibilityFontFamilyKey];
    auto postScriptName = adoptCF(CTFontCopyPostScriptName(font));
    if (postScriptName)
        [fontAttributes setValue:bridge_cast(postScriptName.get()) forKey:NSAccessibilityFontNameKey];
    auto traits = CTFontGetSymbolicTraits(font);
    if (traits & kCTFontTraitBold)
        [fontAttributes setValue:@YES forKey:@"AXFontBold"];
    if (traits & kCTFontTraitItalic)
        [fontAttributes setValue:@YES forKey:@"AXFontItalic"];

    [attributedString addAttribute:NSAccessibilityFontTextAttribute value:fontAttributes.get() range:range];
#endif
}

static void attributedStringAppendWrapper(NSMutableAttributedString *attrString, WebAccessibilityObjectWrapper *wrapper)
{
    const auto attachmentCharacter = static_cast<UniChar>(NSAttachmentCharacter);
    [attrString appendAttributedString:adoptNS([[NSMutableAttributedString alloc] initWithString:[NSString stringWithCharacters:&attachmentCharacter length:1]
#if PLATFORM(MAC)
        attributes:@{ NSAccessibilityAttachmentTextAttribute : (__bridge id)adoptCF(NSAccessibilityCreateAXUIElementRef(wrapper)).get() }
#else
        attributes:@{ AccessibilityTokenAttachment : wrapper }
#endif
    ]).get()];
}

RetainPtr<NSArray> AccessibilityObject::contentForRange(const SimpleRange& range, SpellCheck spellCheck) const
{
    auto result = adoptNS([[NSMutableArray alloc] init]);

    // Iterate over the range to build the AX attributed strings.
    for (TextIterator it(range); !it.atEnd(); it.advance()) {
        Node& node = it.range().start.container;

        // Non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX).
        if (it.text().length()) {
            auto listMarkerText = listMarkerTextForNodeAndPosition(&node, makeContainerOffsetPosition(it.range().start));
            if (!listMarkerText.isEmpty()) {
                if (auto attrString = attributedStringCreate(&node, listMarkerText, it.range(), SpellCheck::No))
                    [result addObject:attrString.get()];
            }

            if (auto attrString = attributedStringCreate(&node, it.text(), it.range(), spellCheck))
                [result addObject:attrString.get()];
        } else {
            if (Node* replacedNode = it.node()) {
                auto* object = axObjectCache()->getOrCreate(replacedNode->renderer());
                if (object)
                    addObjectWrapperToArray(*object, result.get());
            }
        }
    }

    return result;
}

RetainPtr<NSAttributedString> AccessibilityObject::attributedStringForTextMarkerRange(AXTextMarkerRange&& textMarkerRange, SpellCheck spellCheck) const
{
#if PLATFORM(MAC)
    auto range = rangeForTextMarkerRange(axObjectCache(), textMarkerRange);
#else
    auto range = textMarkerRange.simpleRange();
#endif
    return range ? attributedStringForRange(*range, spellCheck) : nil;
}

RetainPtr<NSAttributedString> AccessibilityObject::attributedStringForRange(const SimpleRange& range, SpellCheck spellCheck) const
{
    auto result = adoptNS([[NSMutableAttributedString alloc] init]);

    auto contents = contentForRange(range, spellCheck);
    for (id content in contents.get()) {
        auto item = retainPtr(content);
        if ([item isKindOfClass:[WebAccessibilityObjectWrapper class]]) {
            attributedStringAppendWrapper(result.get(), item.get());
            continue;
        }

        if (![item isKindOfClass:[NSAttributedString class]])
            continue;

        [result appendAttributedString:item.get()];
    }

    return result;
}

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(COCOA)
