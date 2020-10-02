/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#import "AccessibilityLabel.h"
#import "AccessibilityList.h"
#import "ElementAncestorIterator.h"
#import "HTMLFieldSetElement.h"
#import "HTMLInputElement.h"
#import "LocalizedStrings.h"
#import "RenderObject.h"
#import "Settings.h"

#if ENABLE(ACCESSIBILITY) && PLATFORM(MAC)

#import "WebAccessibilityObjectWrapperMac.h"
#import "Widget.h"

namespace WebCore {

void AccessibilityObject::detachPlatformWrapper(AccessibilityDetachmentType)
{
    [wrapper() detach];
}

void AccessibilityObject::detachFromParent()
{
    if (isAttachment())
        overrideAttachmentParent(nullptr);
}

void AccessibilityObject::overrideAttachmentParent(AXCoreObject* parent)
{
    if (!isAttachment())
        return;
    
    id parentWrapper = nil;
    if (parent) {
        if (parent->accessibilityIsIgnored())
            parent = parent->parentObjectUnignored();
        parentWrapper = parent->wrapper();
    }

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[wrapper() attachmentView] accessibilitySetOverrideValue:parentWrapper forAttribute:NSAccessibilityParentAttribute];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

// On iOS, we don't have to return the value in the title. We can return the actual title, given the API.
bool AccessibilityObject::fileUploadButtonReturnsValueInTitle() const
{
    return true;
}

bool AccessibilityObject::accessibilityIgnoreAttachment() const
{
    // FrameView attachments are now handled by AccessibilityScrollView, 
    // so if this is the attachment, it should be ignored.
    Widget* widget = nullptr;
    if (isAttachment() && (widget = widgetForAttachmentView()) && widget->isFrameView())
        return true;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if ([wrapper() attachmentView])
        return [[wrapper() attachmentView] accessibilityIsIgnored];
    ALLOW_DEPRECATED_DECLARATIONS_END

    // Attachments are ignored by default (unless we determine that we should expose them).
    return true;
}

AccessibilityObjectInclusion AccessibilityObject::accessibilityPlatformIncludesObject() const
{
    if (isMenuListPopup() || isMenuListOption())
        return AccessibilityObjectInclusion::IgnoreObject;

    if (roleValue() == AccessibilityRole::Caption && ariaRoleAttribute() == AccessibilityRole::Unknown)
        return AccessibilityObjectInclusion::IgnoreObject;
    
    if (roleValue() == AccessibilityRole::Mark)
        return AccessibilityObjectInclusion::IncludeObject;

    // Never expose an unknown object on the Mac. Clients of the AX API will not know what to do with it.
    // Special case is when the unknown object is actually an attachment.
    if (roleValue() == AccessibilityRole::Unknown && !isAttachment())
        return AccessibilityObjectInclusion::IgnoreObject;
    
    if (roleValue() == AccessibilityRole::Inline && !isStyleFormatGroup())
        return AccessibilityObjectInclusion::IgnoreObject;

    if (RenderObject* renderer = this->renderer()) {
        // The legend element is ignored if it lives inside of a fieldset element that uses it to generate alternative text.
        if (renderer->isLegend()) {
            Element* element = this->element();
            if (element && ancestorsOfType<HTMLFieldSetElement>(*element).first())
                return AccessibilityObjectInclusion::IgnoreObject;
        }
    }
    
    return AccessibilityObjectInclusion::DefaultBehavior;
}
    
bool AccessibilityObject::caretBrowsingEnabled() const
{
    Frame* frame = this->frame();
    return frame && frame->settings().caretBrowsingEnabled();
}

void AccessibilityObject::setCaretBrowsingEnabled(bool on)
{
    Frame* frame = this->frame();
    if (!frame)
        return;
    frame->settings().setCaretBrowsingEnabled(on);
}

String AccessibilityObject::rolePlatformString() const
{
    AccessibilityRole role = roleValue();

    // If it is a label with just static text or an anonymous math operator, remap role to StaticText.
    // The mfenced element creates anonymous RenderMathMLOperators with no RenderText
    // descendants. These anonymous renderers are the only accessible objects
    // containing the operator.
    if ((role == AccessibilityRole::Label && is<AccessibilityLabel>(*this) && downcast<AccessibilityLabel>(*this).containsOnlyStaticText())
        || isAnonymousMathOperator())
        role = AccessibilityRole::StaticText;
    else if (role == AccessibilityRole::Canvas && canvasHasFallbackContent())
        role = AccessibilityRole::Group;

    return Accessibility::roleToPlatformString(role);
}

String AccessibilityObject::rolePlatformDescription() const
{
    AccessibilityRole role = roleValue();
    NSString *axRole = rolePlatformString();

    if ([axRole isEqualToString:NSAccessibilityGroupRole]) {
        if (isOutput())
            return AXOutputText();

        String ariaLandmarkRoleDescription = this->ariaLandmarkRoleDescription();
        if (!ariaLandmarkRoleDescription.isEmpty())
            return ariaLandmarkRoleDescription;

        switch (role) {
        case AccessibilityRole::Audio:
            return localizedMediaControlElementString("AudioElement");
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
        case AccessibilityRole::Footer:
            return AXFooterRoleDescriptionText();
        case AccessibilityRole::Mark:
            return AXMarkText();
        case AccessibilityRole::Video:
            return localizedMediaControlElementString("VideoElement");
        case AccessibilityRole::GraphicsDocument:
            return AXARIAContentGroupText(@"ARIADocument");
        default:
            return String();
        }
    }

    if ([axRole isEqualToString:@"AXWebArea"])
        return AXWebAreaText();

    if ([axRole isEqualToString:@"AXLink"])
        return AXLinkText();

    if ([axRole isEqualToString:@"AXListMarker"])
        return AXListMarkerText();

    if ([axRole isEqualToString:@"AXImageMap"])
        return AXImageMapText();

    if ([axRole isEqualToString:@"AXHeading"])
        return AXHeadingText();

    if ([axRole isEqualToString:NSAccessibilityTextFieldRole]) {
        auto* node = this->node();
        if (is<HTMLInputElement>(node)) {
            auto& input = downcast<HTMLInputElement>(*node);
            if (input.isEmailField())
                return AXEmailFieldText();
            if (input.isTelephoneField())
                return AXTelephoneFieldText();
            if (input.isURLField())
                return AXURLFieldText();
            if (input.isNumberField())
                return AXNumberFieldText();

            // These input types are not enabled on mac yet, we check the type attribute for now.
            auto& type = input.attributeWithoutSynchronization(HTMLNames::typeAttr);
            if (equalLettersIgnoringASCIICase(type, "date"))
                return AXDateFieldText();
            if (equalLettersIgnoringASCIICase(type, "time"))
                return AXTimeFieldText();
            if (equalLettersIgnoringASCIICase(type, "week"))
                return AXWeekFieldText();
            if (equalLettersIgnoringASCIICase(type, "month"))
                return AXMonthFieldText();
            if (equalLettersIgnoringASCIICase(type, "datetime-local"))
                return AXDateTimeFieldText();
        }
    }

    if (isFileUploadButton())
        return AXFileUploadButtonText();

    // Only returning for DL (not UL or OL) because description changed with HTML5 from 'definition list' to
    // superset 'description list' and does not return the same values in AX API on some OS versions.
    if (isDescriptionList())
        return AXDescriptionListText();

    if (role == AccessibilityRole::HorizontalRule)
        return AXHorizontalRuleDescriptionText();

    // AppKit also returns AXTab for the role description for a tab item.
    if (isTabItem())
        return NSAccessibilityRoleDescription(@"AXTab", nil);

    if (isSummary())
        return AXSummaryText();

    return String();
}

namespace Accessibility {

PlatformRoleMap createPlatformRoleMap()
{
    struct RoleEntry {
        AccessibilityRole value;
        NSString *string;
    };
    static const RoleEntry roles[] = {
        { AccessibilityRole::Unknown, NSAccessibilityUnknownRole },
        { AccessibilityRole::Button, NSAccessibilityButtonRole },
        { AccessibilityRole::RadioButton, NSAccessibilityRadioButtonRole },
        { AccessibilityRole::CheckBox, NSAccessibilityCheckBoxRole },
        { AccessibilityRole::Slider, NSAccessibilitySliderRole },
        { AccessibilityRole::TabGroup, NSAccessibilityTabGroupRole },
        { AccessibilityRole::TextField, NSAccessibilityTextFieldRole },
        { AccessibilityRole::StaticText, NSAccessibilityStaticTextRole },
        { AccessibilityRole::TextArea, NSAccessibilityTextAreaRole },
        { AccessibilityRole::ScrollArea, NSAccessibilityScrollAreaRole },
        { AccessibilityRole::PopUpButton, NSAccessibilityPopUpButtonRole },
        { AccessibilityRole::MenuButton, NSAccessibilityMenuButtonRole },
        { AccessibilityRole::Table, NSAccessibilityTableRole },
        { AccessibilityRole::Application, NSAccessibilityApplicationRole },
        { AccessibilityRole::Group, NSAccessibilityGroupRole },
        { AccessibilityRole::TextGroup, NSAccessibilityGroupRole },
        { AccessibilityRole::RadioGroup, NSAccessibilityRadioGroupRole },
        { AccessibilityRole::List, NSAccessibilityListRole },
        { AccessibilityRole::Directory, NSAccessibilityListRole },
        { AccessibilityRole::ScrollBar, NSAccessibilityScrollBarRole },
        { AccessibilityRole::ValueIndicator, NSAccessibilityValueIndicatorRole },
        { AccessibilityRole::Image, NSAccessibilityImageRole },
        { AccessibilityRole::MenuBar, NSAccessibilityMenuBarRole },
        { AccessibilityRole::Menu, NSAccessibilityMenuRole },
        { AccessibilityRole::MenuItem, NSAccessibilityMenuItemRole },
        { AccessibilityRole::MenuItemCheckbox, NSAccessibilityMenuItemRole },
        { AccessibilityRole::MenuItemRadio, NSAccessibilityMenuItemRole },
        { AccessibilityRole::Column, NSAccessibilityColumnRole },
        { AccessibilityRole::Row, NSAccessibilityRowRole },
        { AccessibilityRole::Toolbar, NSAccessibilityToolbarRole },
        { AccessibilityRole::BusyIndicator, NSAccessibilityBusyIndicatorRole },
        { AccessibilityRole::ProgressIndicator, NSAccessibilityProgressIndicatorRole },
        { AccessibilityRole::Meter, NSAccessibilityLevelIndicatorRole },
        { AccessibilityRole::Window, NSAccessibilityWindowRole },
        { AccessibilityRole::Drawer, NSAccessibilityDrawerRole },
        { AccessibilityRole::SystemWide, NSAccessibilitySystemWideRole },
        { AccessibilityRole::Outline, NSAccessibilityOutlineRole },
        { AccessibilityRole::Incrementor, NSAccessibilityIncrementorRole },
        { AccessibilityRole::Browser, NSAccessibilityBrowserRole },
        { AccessibilityRole::ComboBox, NSAccessibilityComboBoxRole },
        { AccessibilityRole::SplitGroup, NSAccessibilitySplitGroupRole },
        { AccessibilityRole::Splitter, NSAccessibilitySplitterRole },
        { AccessibilityRole::ColorWell, NSAccessibilityColorWellRole },
        { AccessibilityRole::GrowArea, NSAccessibilityGrowAreaRole },
        { AccessibilityRole::Sheet, NSAccessibilitySheetRole },
        { AccessibilityRole::HelpTag, NSAccessibilityHelpTagRole },
        { AccessibilityRole::Matte, NSAccessibilityMatteRole },
        { AccessibilityRole::Ruler, NSAccessibilityRulerRole },
        { AccessibilityRole::RulerMarker, NSAccessibilityRulerMarkerRole },
        { AccessibilityRole::Link, NSAccessibilityLinkRole },
        { AccessibilityRole::DisclosureTriangle, NSAccessibilityDisclosureTriangleRole },
        { AccessibilityRole::Grid, NSAccessibilityTableRole },
        { AccessibilityRole::TreeGrid, NSAccessibilityTableRole },
        { AccessibilityRole::WebCoreLink, NSAccessibilityLinkRole },
        { AccessibilityRole::ImageMapLink, NSAccessibilityLinkRole },
        { AccessibilityRole::ImageMap, @"AXImageMap" },
        { AccessibilityRole::ListMarker, @"AXListMarker" },
        { AccessibilityRole::WebArea, @"AXWebArea" },
        { AccessibilityRole::Heading, @"AXHeading" },
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
        { AccessibilityRole::ApplicationAlert, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationAlertDialog, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationDialog, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationGroup, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationTextGroup, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationLog, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationMarquee, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationStatus, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationTimer, NSAccessibilityGroupRole },
        { AccessibilityRole::Document, NSAccessibilityGroupRole },
        { AccessibilityRole::DocumentArticle, NSAccessibilityGroupRole },
        { AccessibilityRole::DocumentMath, NSAccessibilityGroupRole },
        { AccessibilityRole::DocumentNote, NSAccessibilityGroupRole },
        { AccessibilityRole::UserInterfaceTooltip, NSAccessibilityGroupRole },
        { AccessibilityRole::Tab, NSAccessibilityRadioButtonRole },
        { AccessibilityRole::TabList, NSAccessibilityTabGroupRole },
        { AccessibilityRole::TabPanel, NSAccessibilityGroupRole },
        { AccessibilityRole::Tree, NSAccessibilityOutlineRole },
        { AccessibilityRole::TreeItem, NSAccessibilityRowRole },
        { AccessibilityRole::ListItem, NSAccessibilityGroupRole },
        { AccessibilityRole::Paragraph, NSAccessibilityGroupRole },
        { AccessibilityRole::Label, NSAccessibilityGroupRole },
        { AccessibilityRole::Div, NSAccessibilityGroupRole },
        { AccessibilityRole::Form, NSAccessibilityGroupRole },
        { AccessibilityRole::SpinButton, NSAccessibilityIncrementorRole },
        { AccessibilityRole::SpinButtonPart, @"AXIncrementorArrow" },
        { AccessibilityRole::Footer, NSAccessibilityGroupRole },
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
        { AccessibilityRole::RubyBase, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyBlock, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyInline, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyRun, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyText, NSAccessibilityGroupRole },
        { AccessibilityRole::Details, NSAccessibilityGroupRole },
        { AccessibilityRole::Summary, NSAccessibilityButtonRole },
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
        { AccessibilityRole::Subscript, NSAccessibilityGroupRole },
        { AccessibilityRole::Superscript, NSAccessibilityGroupRole },
    };
    PlatformRoleMap roleMap;
    for (auto& role : roles)
        roleMap.add(static_cast<unsigned>(role.value), role.string);
    return roleMap;
}

} // namespace Accessibility

} // WebCore

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(MAC)
