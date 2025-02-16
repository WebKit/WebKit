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

#import "AXObjectCache.h"
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

#if PLATFORM(MAC)
#import "WebAccessibilityObjectWrapperMac.h"
#endif // PLATFORM(MAC)

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
        [fontAttributes setValue:@YES forKey:NSAccessibilityFontBoldAttribute];
    if (traits & kCTFontTraitItalic)
        [fontAttributes setValue:@YES forKey:NSAccessibilityFontItalicAttribute];

    [attributedString addAttribute:NSAccessibilityFontTextAttribute value:fontAttributes.get() range:range];
#endif // PLATFORM(MAC)
}

#if PLATFORM(MAC)
void attributedStringSetColor(NSMutableAttributedString *string, NSString *attribute, NSColor *color, const NSRange& range)
{
    if (!attributedStringContainsRange(string, range))
        return;

    if (color) {
        // Use the CGColor instead of the passed NSColor because that's what the AX system framework expects. Using the NSColor causes that the AX client gets nil instead of a valid NSAttributedString.
        [string addAttribute:attribute value:(__bridge id)color.CGColor range:range];
    }
}

void attributedStringSetExpandedText(NSMutableAttributedString *string, const AXCoreObject& object, const NSRange& range)
{
    if (!attributedStringContainsRange(string, range))
        return;

    if (object.supportsExpandedTextValue())
        [string addAttribute:NSAccessibilityExpandedTextValueAttribute value:object.expandedTextValue() range:range];
}

void attributedStringSetBlockquoteLevel(NSMutableAttributedString *string, const AXCoreObject& object, const NSRange& range)
{
    if (!attributedStringContainsRange(string, range))
        return;

    if (unsigned level = object.blockquoteLevel())
        [string addAttribute:NSAccessibilityBlockQuoteLevelAttribute value:@(level) range:range];
}

void attributedStringSetNeedsSpellCheck(NSMutableAttributedString *string, const AXCoreObject& object)
{
    // If this object is not inside editable content, it's not eligible for spell-checking.
    if (!object.editableAncestor())
        return;

    // Inform the AT that we want it to spell-check for us by setting AXDidSpellCheck to @NO.
    attributedStringSetNumber(string, NSAccessibilityDidSpellCheckAttribute, @NO, NSMakeRange(0, string.length));
}

void attributedStringSetElement(NSMutableAttributedString *string, NSString *attribute, const AXCoreObject& object, const NSRange& range)
{
    if (!attributedStringContainsRange(string, range))
        return;

    id wrapper = object.wrapper();
    if ([attribute isEqualToString:NSAccessibilityAttachmentTextAttribute] && object.isAttachment()) {
        if (id attachmentView = [wrapper attachmentView])
            wrapper = attachmentView;
    }

    if (RetainPtr axElement = adoptCF(NSAccessibilityCreateAXUIElementRef(wrapper)))
        [string addAttribute:attribute value:(__bridge id)axElement.get() range:range];
}

static void attributedStringSetStyle(NSMutableAttributedString *attributedString, AttributedStringStyle&& style, const NSRange& range)
{
    attributedStringSetFont(attributedString, style.font.get(), range);

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
}

// FIXME: This function should eventually be adapted to also work for PLATFORM(IOS), or be moved to a currently non-existent AXCoreObjectMac file.
RetainPtr<NSMutableAttributedString> AXCoreObject::createAttributedString(StringView text, SpellCheck spellCheck) const
{
    if (text.isEmpty())
        return nil;

    auto string = adoptNS([[NSMutableAttributedString alloc] initWithString:text.createNSStringWithoutCopying().get()]);
    NSRange range = NSMakeRange(0, [string length]);
    attributedStringSetStyle(string.get(), stylesForAttributedString(), range);

    // Set attributes determined by `this`, or an ancestor of `this`.
    bool didSetHeadingLevel = false;
    for (RefPtr ancestor = this; ancestor; ancestor = ancestor->parentObject()) {
        if (ancestor->hasMarkTag())
            attributedStringSetNumber(string.get(), NSAccessibilityHighlightAttribute, @YES, range);

        switch (ancestor->roleValue()) {
        case AccessibilityRole::Insertion:
            attributedStringSetNumber(string.get(), NSAccessibilityIsSuggestedInsertionAttribute, @YES, range);
            break;
        case AccessibilityRole::Deletion:
            attributedStringSetNumber(string.get(), NSAccessibilityIsSuggestedDeletionAttribute, @YES, range);
            break;
        case AccessibilityRole::Suggestion:
            attributedStringSetNumber(string.get(), NSAccessibilityIsSuggestionAttribute, @YES, range);
            break;
        case AccessibilityRole::Mark:
            attributedStringSetNumber(string.get(), NSAccessibilityHighlightAttribute, @YES, range);
            break;
        default:
            break;
        }

        if (ancestor->isLink())
            attributedStringSetElement(string.get(), NSAccessibilityLinkTextAttribute, *ancestor, range);

        if (!didSetHeadingLevel) {
            if (unsigned level = ancestor->headingLevel()) {
                didSetHeadingLevel = true;
                [string.get() addAttribute:NSAccessibilityHeadingLevelAttribute value:@(level) range:range];
            }
        }
    }

    // FIXME: Computing block-quote level is an ancestry walk, so ideally we would just combine that with the
    // ancestry walk we do above, but we currently cache this as its own property (AXProperty::BlockquoteLevel)
    // so just use that for now.
    attributedStringSetBlockquoteLevel(string.get(), *this, range);
    attributedStringSetExpandedText(string.get(), *this, range);

    // FIXME: We need to implement this, but there are several issues with it:
    //  1. It requires a Node and SimpleRange, which an AXIsolatedObject will never have.
    //  2. The implementation of this function requires accessing some Editor state, which AXIsolatedObjects cannot do.
    //  3. The current implementation doesn't work well from a user experience perspective, which may require tweaking
    //     ATs and also how WebKit represents this information. So we should probably do that work in tandem with fixing this.
    // attributedStringSetCompositionAttributes(string.get(), node, textRange);

    if (spellCheck == AXCoreObject::SpellCheck::Yes) {
        RefPtr node = this->node();
        if (AXObjectCache::shouldSpellCheck() && node) {
            // FIXME: This eagerly resolves misspellings, and since it requires a node, we will
            // never do this if `this` is an AXIsolatedObject`. We might need to figure out how
            // to spellcheck off the main-thread.
            attributedStringSetSpelling(string.get(), *node, text, range);
        } else
            attributedStringSetNeedsSpellCheck(string.get(), *this);
    }

    return string;
}
#endif // PLATFORM(MAC)

} // namespace WebCore

#endif // PLATFORM(COCOA)
