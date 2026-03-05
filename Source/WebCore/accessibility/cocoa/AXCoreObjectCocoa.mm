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

#import "AXLoggerBase.h"
#import "AXObjectCache.h"
#import "AXTreeStoreInlines.h"
#import "AccessibilityObjectInlines.h"
#import "ColorCocoa.h"
#import "RenderObjectInlines.h"
#import "WebAccessibilityObjectWrapperBase.h"
#import "Widget.h"

#if PLATFORM(IOS_FAMILY)
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(AXRuntime);

SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenFontName, NSString *);
#define AccessibilityTokenFontName getUIAccessibilityTokenFontNameSingleton()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenFontFamily, NSString *);
#define AccessibilityTokenFontFamily getUIAccessibilityTokenFontFamilySingleton()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenFontSize, NSString *);
#define AccessibilityTokenFontSize getUIAccessibilityTokenFontSizeSingleton()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenBold, NSString *);
#define AccessibilityTokenBold getUIAccessibilityTokenBoldSingleton()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenItalic, NSString *);
#define AccessibilityTokenItalic getUIAccessibilityTokenItalicSingleton()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenAttachment, NSString *);
#define AccessibilityTokenAttachment getUIAccessibilityTokenAttachmentSingleton()

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
#import "WebAccessibilityObjectWrapperMac.h"
#endif // PLATFORM(MAC)

#if PLATFORM(COCOA)

namespace WebCore {

RetainPtr<id> AXCoreObject::platformElement() const
{
    if (isRemoteFrame()) [[unlikely]]
        return remoteFramePlatformElement();
    return wrapper();
}

String AXCoreObject::speechHint() const
{
    auto speakAs = this->speakAs();

    StringBuilder builder;
    builder.append(speakAs.contains(Style::SpeakAsValue::SpellOut) ? "spell-out"_s : "normal"_s);
    if (speakAs.contains(Style::SpeakAsValue::Digits))
        builder.append(" digits"_s);
    if (speakAs.contains(Style::SpeakAsValue::LiteralPunctuation))
        builder.append(" literal-punctuation"_s);
    if (speakAs.contains(Style::SpeakAsValue::NoPunctuation))
        builder.append(" no-punctuation"_s);

    return builder.toString();
}

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
        [string addAttribute:NSAccessibilityExpandedTextValueAttribute value:object.expandedTextValue().createNSString().get() range:range];
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

    if (isReplacedElement()) {
        if (id wrapper = this->wrapper()) {
#if PLATFORM(MAC)
            [string.get() addAttribute:NSAccessibilityAttachmentTextAttribute value:(__bridge id)adoptCF(NSAccessibilityCreateAXUIElementRef(wrapper)).get() range:range];
#else
            [string.get() addAttribute:AccessibilityTokenAttachment value:wrapper range:range];
#endif // PLATFORM(MAC)
        }
        return string;
    }

    attributedStringSetStyle(string.get(), stylesForAttributedString(), range);

    unsigned blockquoteLevel = 0;
    // Set attributes determined by `this`, or an ancestor of `this`.
    bool didSetHeadingLevel = false;
    for (RefPtr ancestor = this; ancestor; ancestor = ancestor->parentObject()) {
        if (ancestor->hasMarkTag())
            attributedStringSetNumber(string.get(), NSAccessibilityHighlightAttribute, @YES, range);

        switch (ancestor->role()) {
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

        if (ancestor->role() == AccessibilityRole::Blockquote)
            ++blockquoteLevel;

        if (ancestor->isExposableTable()) {
            if (id wrapper = ancestor->wrapper())
                [string.get() addAttribute:NSAccessibilityTableAttribute value:(__bridge id)adoptCF(NSAccessibilityCreateAXUIElementRef(wrapper)).get() range:range];
        }
    }
    if (blockquoteLevel)
        [string.get() addAttribute:NSAccessibilityBlockQuoteLevelAttribute value:@(blockquoteLevel) range:range];

    attributedStringSetExpandedText(string.get(), *this, range);

    // FIXME: We need to implement this, but there are several issues with it:
    //  1. It requires a Node and SimpleRange, which an AXIsolatedObject will never have.
    //  2. The implementation of this function requires accessing some Editor state, which AXIsolatedObjects cannot do.
    //  3. The current implementation doesn't work well from a user experience perspective, which may require tweaking
    //     ATs and also how WebKit represents this information. So we should probably do that work in tandem with fixing this.
    // attributedStringSetCompositionAttributes(string.get(), node, textRange);

    if (spellCheck == AXCoreObject::SpellCheck::Yes) {
        if (RefPtr node = AXObjectCache::shouldSpellCheck() ? this->node() : nullptr) {
            // FIXME: This eagerly resolves misspellings, and since it requires a node, we will
            // never do this if `this` is an AXIsolatedObject`. We might need to figure out how
            // to spellcheck off the main-thread.
            attributedStringSetSpelling(string.get(), *node, text, range);
        } else
            attributedStringSetNeedsSpellCheck(string.get(), *this);
    }
    return string;
}

NSArray *renderWidgetChildren(const AXCoreObject& object)
{
    if (!object.isWidget()) [[likely]]
        return nil;

    id child = Accessibility::retrieveAutoreleasedValueFromMainThread<id>([object = Ref { object }] () -> RetainPtr<id> {
        RefPtr widget = object->widget();
        return widget ? widget->accessibilityObject() : nil;
    });

    if (child)
        return @[child];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [object.platformWidget() accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END
}

String AXCoreObject::rolePlatformDescription()
{
    // Attachments have the AXImage role, but may have different subroles.
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (isAttachment())
        return [[wrapper() attachmentView] accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];

    if (isRemoteFrame())
        return [remoteFramePlatformElement().get() accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END

    RetainPtr axRole = rolePlatformString().createNSString();

    if ([axRole isEqualToString:NSAccessibilityGroupRole]) {
        if (isOutput())
            return AXOutputText();

        String ariaLandmarkRoleDescription = this->ariaLandmarkRoleDescription();
        if (!ariaLandmarkRoleDescription.isEmpty())
            return ariaLandmarkRoleDescription;

        switch (role()) {
        case AccessibilityRole::Audio:
            return localizedMediaControlElementString("AudioElement"_s);
        case AccessibilityRole::Definition:
            return AXDefinitionText();
        case AccessibilityRole::DescriptionListTerm:
        case AccessibilityRole::Term:
            return AXDescriptionListTermText();
        case AccessibilityRole::DescriptionListDetail:
            return AXDescriptionListDetailText();
        case AccessibilityRole::Details:
            return AXDetailsText();
        case AccessibilityRole::Feed:
            return AXFeedText();
        case AccessibilityRole::SectionFooter:
            return AXFooterRoleDescriptionText();
        case AccessibilityRole::SectionHeader:
            return AXHeaderRoleDescriptionText();
        case AccessibilityRole::Mark:
            return AXMarkText();
        case AccessibilityRole::Video:
            return localizedMediaControlElementString("VideoElement"_s);
        case AccessibilityRole::GraphicsDocument:
            return AXARIAContentGroupText("ARIADocument"_s);
        default:
            return { };
        }
    }

    if ([axRole isEqualToString:NSAccessibilityWebAreaRole])
        return AXWebAreaText();

    if ([axRole isEqualToString:NSAccessibilityLinkRole])
        return AXLinkText();

    if ([axRole isEqualToString:NSAccessibilityListMarkerRole])
        return AXListMarkerText();

    if ([axRole isEqualToString:NSAccessibilityImageMapRole])
        return AXImageMapText();

    if ([axRole isEqualToString:NSAccessibilityHeadingRole])
        return AXHeadingText();

    if ([axRole isEqualToString:NSAccessibilityTextFieldRole]) {
        if (std::optional type = inputType()) {
            switch (*type) {
            case InputType::Type::Email:
                return AXEmailFieldText();
            case InputType::Type::Telephone:
                return AXTelephoneFieldText();
            case InputType::Type::URL:
                return AXURLFieldText();
            case InputType::Type::Number:
                return AXNumberFieldText();
            case InputType::Type::Date:
                return AXDateFieldText();
            case InputType::Type::Time:
                return AXTimeFieldText();
            case InputType::Type::Week:
                return AXWeekFieldText();
            case InputType::Type::Month:
                return AXMonthFieldText();
            case InputType::Type::DateTimeLocal:
                return AXDateTimeFieldText();
            default:
                break;
            }
        }
    }

    if (isFileUploadButton())
        return AXFileUploadButtonText();

    // Only returning for DL (not UL or OL) because description changed with HTML5 from 'definition list' to
    // superset 'description list' and does not return the same values in AX API on some OS versions.
    if (isDescriptionList())
        return AXDescriptionListText();

    if (role() == AccessibilityRole::HorizontalRule)
        return AXHorizontalRuleDescriptionText();

    // AppKit also returns AXTab for the role description for a tab item.
    if (isTabItem())
        return NSAccessibilityRoleDescription(@"AXTab", nil);

    if (isSummary())
        return AXSummaryText();

    return { };
}

String AXCoreObject::rolePlatformString()
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (isAttachment())
        return [[wrapper() attachmentView] accessibilityAttributeValue:NSAccessibilityRoleAttribute];

    if (isRemoteFrame())
        return [remoteFramePlatformElement().get() accessibilityAttributeValue:NSAccessibilityRoleAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END

    auto role = this->role();
    if (role == AccessibilityRole::Label) {
        // Labels that only contain static text should just be mapped to static text.
        if (containsOnlyStaticText())
            role = AccessibilityRole::StaticText;
    } else if (isAnonymousMathOperator()) {
        // The mfenced element creates anonymous RenderMathMLOperators with no RenderText
        // descendants. These anonymous renderers are the only accessible objects
        // containing the operator.
        role = AccessibilityRole::StaticText;
    } else if (role == AccessibilityRole::Canvas && hasUnignoredChild() && !containsOnlyStaticText()) {
        // If this is a canvas with fallback content (one or more non-text thing), re-map to group.
        role = AccessibilityRole::Group;
    } else {
        switch (listBoxInterpretation()) {
        case ListBoxInterpretation::ActuallyListBox:
            role = AccessibilityRole::ListBox;
            break;
        case ListBoxInterpretation::ActuallyStaticList:
            role = AccessibilityRole::List;
            break;
        case ListBoxInterpretation::InvalidListBox:
            role = AccessibilityRole::Group;
            break;
        case ListBoxInterpretation::NotListBox:
            break;
        }
    }

    return Accessibility::roleToPlatformString(role);
}

bool AXCoreObject::isEmptyGroup()
{
#if ENABLE(MODEL_ELEMENT)
    if (isModel()) [[unlikely]]
        return false;
#endif

    if (isRemoteFrame()) [[unlikely]]
        return false;

    return [rolePlatformString().createNSString() isEqual:NSAccessibilityGroupRole]
        && !hasUnignoredChild()
        && ![renderWidgetChildren(*this) count];
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::crossFrameSortedDescendants(size_t limit, PreSortedObjectType type) const
{
    AX_ASSERT(type == PreSortedObjectType::LiveRegion || type == PreSortedObjectType::WebArea);
    auto sortedObjects = type == PreSortedObjectType::LiveRegion ? allSortedLiveRegions() : allSortedNonRootWebAreas();
    AXCoreObject::AccessibilityChildrenVector results;
    for (const Ref<AXCoreObject>& object : sortedObjects) {
        if (crossFrameIsAncestorOfObject(object)) {
            results.append(object);
            if (results.size() >= limit)
                break;
        }
    }
    return results;
}

namespace Accessibility {

PlatformRoleMap createPlatformRoleMap()
{
    struct RoleEntry {
        AccessibilityRole value;
        RetainPtr<NSString> string;
    };
    static const NeverDestroyed roles = std::to_array<RoleEntry>({
        { AccessibilityRole::Unknown, NSAccessibilityUnknownRole },
        { AccessibilityRole::Button, NSAccessibilityButtonRole },
        { AccessibilityRole::RadioButton, NSAccessibilityRadioButtonRole },
        { AccessibilityRole::Checkbox, NSAccessibilityCheckBoxRole },
        { AccessibilityRole::Slider, NSAccessibilitySliderRole },
        { AccessibilityRole::TabGroup, NSAccessibilityTabGroupRole },
        { AccessibilityRole::TextField, NSAccessibilityTextFieldRole },
        { AccessibilityRole::StaticText, NSAccessibilityStaticTextRole },
        { AccessibilityRole::TextArea, NSAccessibilityTextAreaRole },
        { AccessibilityRole::ScrollArea, NSAccessibilityScrollAreaRole },
        { AccessibilityRole::PopUpButton, NSAccessibilityPopUpButtonRole },
        { AccessibilityRole::Table, NSAccessibilityTableRole },
        { AccessibilityRole::Application, NSAccessibilityApplicationRole },
        { AccessibilityRole::Group, NSAccessibilityGroupRole },
        { AccessibilityRole::TextGroup, NSAccessibilityGroupRole },
        { AccessibilityRole::RadioGroup, NSAccessibilityRadioGroupRole },
        { AccessibilityRole::List, NSAccessibilityListRole },
        { AccessibilityRole::Directory, NSAccessibilityListRole },
        { AccessibilityRole::ScrollBar, NSAccessibilityScrollBarRole },
        { AccessibilityRole::Image, NSAccessibilityImageRole },
        { AccessibilityRole::MenuBar, NSAccessibilityMenuBarRole },
        { AccessibilityRole::Menu, NSAccessibilityMenuRole },
        { AccessibilityRole::MenuItem, NSAccessibilityMenuItemRole },
        { AccessibilityRole::MenuItemCheckbox, NSAccessibilityMenuItemRole },
        { AccessibilityRole::MenuItemRadio, NSAccessibilityMenuItemRole },
        { AccessibilityRole::Column, NSAccessibilityColumnRole },
        { AccessibilityRole::Row, NSAccessibilityRowRole },
        { AccessibilityRole::Toolbar, NSAccessibilityToolbarRole },
        { AccessibilityRole::ProgressIndicator, NSAccessibilityProgressIndicatorRole },
        { AccessibilityRole::Meter, NSAccessibilityLevelIndicatorRole },
        { AccessibilityRole::ComboBox, NSAccessibilityComboBoxRole },
        { AccessibilityRole::DateTime, NSAccessibilityDateTimeAreaRole },
        { AccessibilityRole::Splitter, NSAccessibilitySplitterRole },
        { AccessibilityRole::Code, NSAccessibilityGroupRole },
        { AccessibilityRole::ColorWell, NSAccessibilityColorWellRole },
        { AccessibilityRole::Link, NSAccessibilityLinkRole },
        { AccessibilityRole::Grid, NSAccessibilityTableRole },
        { AccessibilityRole::TreeGrid, NSAccessibilityTableRole },
        { AccessibilityRole::ImageMap, NSAccessibilityImageMapRole },
        { AccessibilityRole::ListMarker, NSAccessibilityListMarkerRole },
        { AccessibilityRole::WebArea, NSAccessibilityWebAreaRole },
        { AccessibilityRole::Heading, NSAccessibilityHeadingRole },
        { AccessibilityRole::ListBox, NSAccessibilityListRole },
        { AccessibilityRole::ListBoxOption, NSAccessibilityStaticTextRole },
        { AccessibilityRole::Cell, NSAccessibilityCellRole },
        { AccessibilityRole::GridCell, NSAccessibilityCellRole },
        { AccessibilityRole::TableHeaderContainer, NSAccessibilityGroupRole },
        { AccessibilityRole::ColumnHeader, NSAccessibilityCellRole },
        { AccessibilityRole::RowHeader, NSAccessibilityCellRole },
        { AccessibilityRole::Definition, NSAccessibilityGroupRole },
        { AccessibilityRole::DescriptionListDetail, NSAccessibilityGroupRole },
        { AccessibilityRole::DescriptionListTerm, NSAccessibilityGroupRole },
        { AccessibilityRole::Term, NSAccessibilityGroupRole },
        { AccessibilityRole::DescriptionList, NSAccessibilityListRole },
        { AccessibilityRole::SliderThumb, NSAccessibilityValueIndicatorRole },
        { AccessibilityRole::WebApplication, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkBanner, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkComplementary, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkDocRegion, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkContentInfo, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkMain, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkNavigation, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkRegion, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkSearch, NSAccessibilityGroupRole },
        { AccessibilityRole::SectionFooter, NSAccessibilityGroupRole },
        { AccessibilityRole::SectionHeader, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationAlert, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationAlertDialog, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationDialog, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationLog, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationMarquee, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationStatus, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationTimer, NSAccessibilityGroupRole },
        { AccessibilityRole::Document, NSAccessibilityGroupRole },
        { AccessibilityRole::DocumentArticle, NSAccessibilityGroupRole },
        { AccessibilityRole::DocumentMath, NSAccessibilityGroupRole },
        { AccessibilityRole::DocumentNote, NSAccessibilityGroupRole },
        { AccessibilityRole::Emphasis, NSAccessibilityGroupRole },
        { AccessibilityRole::UserInterfaceTooltip, NSAccessibilityGroupRole },
        { AccessibilityRole::Tab, NSAccessibilityRadioButtonRole },
        { AccessibilityRole::TabList, NSAccessibilityTabGroupRole },
        { AccessibilityRole::TabPanel, NSAccessibilityGroupRole },
        { AccessibilityRole::Tree, NSAccessibilityOutlineRole },
        { AccessibilityRole::TreeItem, NSAccessibilityRowRole },
        { AccessibilityRole::ListItem, NSAccessibilityGroupRole },
        { AccessibilityRole::Paragraph, NSAccessibilityGroupRole },
        { AccessibilityRole::Label, NSAccessibilityGroupRole },
        { AccessibilityRole::Form, NSAccessibilityGroupRole },
        { AccessibilityRole::Generic, NSAccessibilityGroupRole },
        { AccessibilityRole::SpinButton, NSAccessibilityIncrementorRole },
        { AccessibilityRole::SpinButtonPart, NSAccessibilityIncrementorArrowRole },
        { AccessibilityRole::ToggleButton, NSAccessibilityCheckBoxRole },
        { AccessibilityRole::Canvas, NSAccessibilityImageRole },
        { AccessibilityRole::SVGRoot, NSAccessibilityGroupRole },
        { AccessibilityRole::Legend, NSAccessibilityGroupRole },
        { AccessibilityRole::MathElement, NSAccessibilityGroupRole },
        { AccessibilityRole::Audio, NSAccessibilityGroupRole },
        { AccessibilityRole::Video, NSAccessibilityGroupRole },
        { AccessibilityRole::HorizontalRule, NSAccessibilitySplitterRole },
        { AccessibilityRole::Blockquote, NSAccessibilityGroupRole },
        { AccessibilityRole::Switch, NSAccessibilityCheckBoxRole },
        { AccessibilityRole::SearchField, NSAccessibilityTextFieldRole },
        { AccessibilityRole::Pre, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyInline, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyText, NSAccessibilityGroupRole },
        { AccessibilityRole::Details, NSAccessibilityGroupRole },
        { AccessibilityRole::Summary, NSAccessibilityDisclosureTriangleRole },
        { AccessibilityRole::SVGTextPath, NSAccessibilityGroupRole },
        { AccessibilityRole::SVGText, NSAccessibilityGroupRole },
        { AccessibilityRole::SVGTSpan, NSAccessibilityGroupRole },
        { AccessibilityRole::Inline, NSAccessibilityGroupRole },
        { AccessibilityRole::Mark, NSAccessibilityGroupRole },
        { AccessibilityRole::Time, NSAccessibilityGroupRole },
        { AccessibilityRole::Feed, NSAccessibilityGroupRole },
        { AccessibilityRole::Figure, NSAccessibilityGroupRole },
        { AccessibilityRole::Footnote, NSAccessibilityGroupRole },
        { AccessibilityRole::GraphicsDocument, NSAccessibilityGroupRole },
        { AccessibilityRole::GraphicsObject, NSAccessibilityGroupRole },
        { AccessibilityRole::GraphicsSymbol, NSAccessibilityImageRole },
        { AccessibilityRole::Caption, NSAccessibilityGroupRole },
        { AccessibilityRole::Deletion, NSAccessibilityGroupRole },
        { AccessibilityRole::Insertion, NSAccessibilityGroupRole },
        { AccessibilityRole::Strong, NSAccessibilityGroupRole },
        { AccessibilityRole::Subscript, NSAccessibilityGroupRole },
        { AccessibilityRole::Superscript, NSAccessibilityGroupRole },
        { AccessibilityRole::Model, NSAccessibilityGroupRole },
        { AccessibilityRole::Suggestion, NSAccessibilityGroupRole },
        { AccessibilityRole::RemoteFrame, NSAccessibilityGroupRole },
        { AccessibilityRole::LocalFrame, NSAccessibilityGroupRole },
        { AccessibilityRole::FrameHost, NSAccessibilityGroupRole },
    });
    PlatformRoleMap roleMap;
    for (auto& role : roles.get())
        roleMap.add(static_cast<unsigned>(role.value), role.string.get());
    return roleMap;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
std::optional<AXTextMarkerRange> markerRangeFrom(NSRange range, const AXCoreObject& object)
{
    std::optional stopAtID = object.idOfNextSiblingIncludingIgnoredOrParent();
    auto markerToLocation = AXTextMarker { object, 0 }.nextMarkerFromOffset(range.location, ForceSingleOffsetMovement::Yes, stopAtID);
    if (!markerToLocation.isValid())
        return std::nullopt;

    auto markerToRangeEnd = markerToLocation.nextMarkerFromOffset(range.length, ForceSingleOffsetMovement::Yes, stopAtID);
    if (!markerToRangeEnd.isValid())
        return std::nullopt;
    return std::optional(AXTextMarkerRange { WTF::move(markerToLocation), WTF::move(markerToRangeEnd) });
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

} // namespace Accessibility

#endif // PLATFORM(MAC)

} // namespace WebCore

#endif // PLATFORM(COCOA)
