/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "AXCoreObject.h"

#import "ColorCocoa.h"
#import "WebAccessibilityObjectWrapperBase.h"

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

#if PLATFORM(COCOA)

namespace WebCore {

// When modifying attributed strings, the range can come from a source which may provide faulty information (e.g. the spell checker).
// To protect against such cases, the range should be validated before adding or removing attributes.
bool attributedStringContainsRange(NSAttributedString *attributedString, const NSRange& range)
{
    return NSMaxRange(range) <= attributedString.length;
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
#endif // PLATFORM(MAC)
}

#if PLATFORM(MAC)
void attributedStringSetColor(NSMutableAttributedString *attrString, NSString *attribute, NSColor *color, const NSRange& range)
{
    if (color) {
        // Use the CGColor instead of the passed NSColor because that's what the AX system framework expects. Using the NSColor causes that the AX client gets nil instead of a valid NSAttributedString.
        [attrString addAttribute:attribute value:(__bridge id)color.CGColor range:range];
    }
}
#endif // PLATFORM(MAC)

static void attributedStringSetStyle(NSMutableAttributedString *attributedString, AttributedStringStyle&& style, const NSRange& range)
{
    attributedStringSetFont(attributedString, style.font.get(), range);

#if PLATFORM(MAC)
    attributedStringSetColor(attributedString, NSAccessibilityForegroundColorTextAttribute, cocoaColor(style.textColor).get(), range);
    attributedStringSetColor(attributedString, NSAccessibilityBackgroundColorTextAttribute, cocoaColor(style.backgroundColor).get(), range);

    // Set subscript / superscript.
    if (style.isSubscript)
        attributedStringSetNumber(attributedString, NSAccessibilitySuperscriptTextAttribute, @(-1), range);
    else if (style.isSuperscript)
        attributedStringSetNumber(attributedString, NSAccessibilitySuperscriptTextAttribute, @(1), range);

    // Set text shadow.
    if (style.hasTextShadow)
        attributedStringSetNumber(attributedString, NSAccessibilityShadowTextAttribute, @YES, range);

    // Set underline and strikethrough.
    if (style.hasUnderline()) {
        attributedStringSetNumber(attributedString, NSAccessibilityUnderlineTextAttribute, @YES, range);
        attributedStringSetColor(attributedString, NSAccessibilityUnderlineColorTextAttribute, cocoaColor(style.underlineColor()).get(), range);
    }

    if (style.hasLinethrough()) {
        attributedStringSetNumber(attributedString, NSAccessibilityStrikethroughTextAttribute, @YES, range);
        attributedStringSetColor(attributedString, NSAccessibilityStrikethroughColorTextAttribute, cocoaColor(style.linethroughColor()).get(), range);
    }
#endif // PLATFORM(MAC)

    // FIXME: Implement this.
//    // Indicate background highlighting.
//    for (Node* node = renderer->node(); node; node = node->parentNode()) {
//        if (node->hasTagName(HTMLNames::markTag))
//            attributedStringSetNumber(attributedString, @"AXHighlight", @YES, range);
//        if (auto* element = dynamicDowncast<Element>(*node)) {
//            auto& roleValue = element->attributeWithoutSynchronization(HTMLNames::roleAttr);
//            if (equalLettersIgnoringASCIICase(roleValue, "insertion"_s))
//                attributedStringSetNumber(attributedString, @"AXIsSuggestedInsertion", @YES, range);
//            else if (equalLettersIgnoringASCIICase(roleValue, "deletion"_s))
//                attributedStringSetNumber(attributedString, @"AXIsSuggestedDeletion", @YES, range);
//            else if (equalLettersIgnoringASCIICase(roleValue, "suggestion"_s))
//                attributedStringSetNumber(attributedString, @"AXIsSuggestion", @YES, range);
//            else if (equalLettersIgnoringASCIICase(roleValue, "mark"_s))
//                attributedStringSetNumber(attributedString, @"AXHighlight", @YES, range);
//        }
//    }
}

// This is intended to replace the static attributedStringCreate() methods once we have ported all functionality.
RetainPtr<NSAttributedString> AXCoreObject::createAttributedString(String&& text) const
{
    if (text.isEmpty())
        return nil;

    auto result = adoptNS([[NSMutableAttributedString alloc] initWithString:WTFMove(text)]);
    NSRange range = NSMakeRange(0, [result length]);
    attributedStringSetStyle(result.get(), stylesForAttributedString(), range);

    // FIXME: Implement the rest of these.
//    attributedStringSetHeadingLevel(result.get(), renderer.get(), range);
//    attributedStringSetBlockquoteLevel(result.get(), renderer.get(), range);
//    attributedStringSetExpandedText(result.get(), renderer.get(), range);
//    attributedStringSetElement(result.get(), NSAccessibilityLinkTextAttribute, AccessibilityObject::anchorElementForNode(node), range);
//    attributedStringSetCompositionAttributes(result.get(), node, textRange);
//    // Do spelling last because it tends to break up the range.
//    if (spellCheck == AXCoreObject::SpellCheck::Yes) {
//        if (AXObjectCache::shouldSpellCheck())
//            attributedStringSetSpelling(result.get(), node, text, range);
//        else
//            attributedStringSetNeedsSpellCheck(result.get(), node);
//    }
//
    return result;
}

} // namespace WebCore

#endif // PLATFORM(COCOA)
