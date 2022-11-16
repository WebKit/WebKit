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
#import "FrameView.h"
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

FloatRect AccessibilityObject::convertRectToPlatformSpace(const FloatRect& rect, AccessibilityConversionSpace space) const
{
    // WebKit1 code path... platformWidget() exists.
    auto* frameView = documentFrameView();
    if (frameView && frameView->platformWidget()) {
        CGPoint point = CGPointMake(rect.x(), rect.y());
        CGSize size = CGSizeMake(rect.size().width(), rect.size().height());
        CGRect cgRect = CGRectMake(point.x, point.y, size.width, size.height);

        NSRect nsRect = NSRectFromCGRect(cgRect);
        NSView *view = frameView->documentView();
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        nsRect = [[view window] convertRectToScreen:[view convertRect:nsRect toView:nil]];
        ALLOW_DEPRECATED_DECLARATIONS_END
        return NSRectToCGRect(nsRect);
    }

    return convertFrameToSpace(rect, space);
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
    id attachmentView = widget ? NSAccessibilityUnignoredDescendant(widget->platformWidget()) : nil;
    if (attachmentView)
        return [attachmentView accessibilityIsIgnored];
    ALLOW_DEPRECATED_DECLARATIONS_END

    // Attachments are ignored by default (unless we determine that we should expose them).
    return true;
}

static bool shouldIgnoreGroup(const AccessibilityObject& axObject)
{
    if (!axObject.isGroup() && axObject.roleValue() != AccessibilityRole::Div)
        return false;

    // Never ignore a group with event listeners attached to it (e.g. onclick).
    if (axObject.node() && axObject.node()->hasEventListeners())
        return false;

    if (!is<AccessibilityNodeObject>(axObject))
        return false;
    auto& axNodeObject = downcast<AccessibilityNodeObject>(axObject);

    // Ignore groups whose accessibility text is the same as their child's static-text content.
    auto* first = axObject.firstChild();
    if (!first || first->roleValue() != AccessibilityRole::StaticText || first != axObject.lastChild())
        return false;

    Vector<AccessibilityText> axText;
    axNodeObject.alternativeText(axText);
    if (!axText.size())
        axNodeObject.helpText(axText);
    if (!axText.size())
        return false;

    auto childString = first->stringValue();
    // stringValue() can be null if the underlying document needs style recalculation.
    if (childString.isNull())
        return false;
    return childString == axText[0].text;
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
    
    if (shouldIgnoreGroup(*this))
        return AccessibilityObjectInclusion::IgnoreObject;

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

String AccessibilityObject::subrolePlatformString() const
{
    if (isPasswordField())
        return NSAccessibilitySecureTextFieldSubrole;
    if (isSearchField())
        return NSAccessibilitySearchFieldSubrole;

    if (isAttachment()) {
        NSView* attachView = [wrapper() attachmentView];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([[attachView accessibilityAttributeNames] containsObject:NSAccessibilitySubroleAttribute])
            return [attachView accessibilityAttributeValue:NSAccessibilitySubroleAttribute];
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

    if (isMeter())
        return "AXMeter"_s;

#if ENABLE(MODEL_ELEMENT)
    if (isModel())
        return "AXModel"_s;
#endif

    AccessibilityRole role = roleValue();
    if (role == AccessibilityRole::HorizontalRule)
        return "AXContentSeparator"_s;
    if (role == AccessibilityRole::ToggleButton)
        return NSAccessibilityToggleSubrole;
    if (role == AccessibilityRole::Footer)
        return "AXFooter"_s;
    if (role == AccessibilityRole::SpinButtonPart) {
        if (isIncrementor())
            return NSAccessibilityIncrementArrowSubrole;
        return NSAccessibilityDecrementArrowSubrole;
    }

    if (isFileUploadButton())
        return "AXFileUploadButton"_s;

    if (isTreeItem())
        return NSAccessibilityOutlineRowSubrole;

    if (isFieldset())
        return "AXFieldset"_s;

    if (isList()) {
        if (isUnorderedList() || isOrderedList())
            return "AXContentList"_s;
        if (isDescriptionList())
            return "AXDescriptionList"_s;
    }

    // ARIA content subroles.
    switch (role) {
    case AccessibilityRole::LandmarkBanner:
        return "AXLandmarkBanner"_s;
    case AccessibilityRole::LandmarkComplementary:
        return "AXLandmarkComplementary"_s;
    case AccessibilityRole::LandmarkContentInfo:
        return "AXLandmarkContentInfo"_s;
    case AccessibilityRole::LandmarkMain:
        return "AXLandmarkMain"_s;
    case AccessibilityRole::LandmarkNavigation:
        return "AXLandmarkNavigation"_s;
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkRegion:
        return "AXLandmarkRegion"_s;
    case AccessibilityRole::LandmarkSearch:
        return "AXLandmarkSearch"_s;
    case AccessibilityRole::ApplicationAlert:
        return "AXApplicationAlert"_s;
    case AccessibilityRole::ApplicationAlertDialog:
        return "AXApplicationAlertDialog"_s;
    case AccessibilityRole::ApplicationDialog:
        return "AXApplicationDialog"_s;
    case AccessibilityRole::ApplicationGroup:
    case AccessibilityRole::ApplicationTextGroup:
    case AccessibilityRole::Feed:
    case AccessibilityRole::Footnote:
        return "AXApplicationGroup"_s;
    case AccessibilityRole::ApplicationLog:
        return "AXApplicationLog"_s;
    case AccessibilityRole::ApplicationMarquee:
        return "AXApplicationMarquee"_s;
    case AccessibilityRole::ApplicationStatus:
        return "AXApplicationStatus"_s;
    case AccessibilityRole::ApplicationTimer:
        return "AXApplicationTimer"_s;
    case AccessibilityRole::Document:
    case AccessibilityRole::GraphicsDocument:
        return "AXDocument"_s;
    case AccessibilityRole::DocumentArticle:
        return "AXDocumentArticle"_s;
    case AccessibilityRole::DocumentMath:
        return "AXDocumentMath"_s;
    case AccessibilityRole::DocumentNote:
        return "AXDocumentNote"_s;
    case AccessibilityRole::UserInterfaceTooltip:
        return "AXUserInterfaceTooltip"_s;
    case AccessibilityRole::TabPanel:
        return "AXTabPanel"_s;
    case AccessibilityRole::Definition:
        return "AXDefinition"_s;
    case AccessibilityRole::DescriptionListTerm:
    case AccessibilityRole::Term:
        return "AXTerm"_s;
    case AccessibilityRole::DescriptionListDetail:
        return "AXDescription"_s;
    case AccessibilityRole::WebApplication:
        return "AXWebApplication"_s;
    case AccessibilityRole::Suggestion:
        return "AXSuggestion"_s;
        // Default doesn't return anything, so roles defined below can be chosen.
    default:
        break;
    }

    if (role == AccessibilityRole::MathElement) {
        if (isMathFraction())
            return "AXMathFraction"_s;
        if (isMathFenced())
            return "AXMathFenced"_s;
        if (isMathSubscriptSuperscript())
            return "AXMathSubscriptSuperscript"_s;
        if (isMathRow())
            return "AXMathRow"_s;
        if (isMathUnderOver())
            return "AXMathUnderOver"_s;
        if (isMathSquareRoot())
            return "AXMathSquareRoot"_s;
        if (isMathRoot())
            return "AXMathRoot"_s;
        if (isMathText())
            return "AXMathText"_s;
        if (isMathNumber())
            return "AXMathNumber"_s;
        if (isMathIdentifier())
            return "AXMathIdentifier"_s;
        if (isMathTable())
            return "AXMathTable"_s;
        if (isMathTableRow())
            return "AXMathTableRow"_s;
        if (isMathTableCell())
            return "AXMathTableCell"_s;
        if (isMathFenceOperator())
            return "AXMathFenceOperator"_s;
        if (isMathSeparatorOperator())
            return "AXMathSeparatorOperator"_s;
        if (isMathOperator())
            return "AXMathOperator"_s;
        if (isMathMultiscript())
            return "AXMathMultiscript"_s;
    }

    if (role == AccessibilityRole::Video)
        return "AXVideo"_s;
    if (role == AccessibilityRole::Audio)
        return "AXAudio"_s;
    if (role == AccessibilityRole::Details)
        return "AXDetails"_s;
    if (role == AccessibilityRole::Summary)
        return "AXSummary"_s;
    if (role == AccessibilityRole::Time)
        return "AXTimeGroup"_s;

    if (isMediaTimeline())
        return NSAccessibilityTimelineSubrole;

    if (isSwitch())
        return NSAccessibilitySwitchSubrole;

    if (role == AccessibilityRole::Insertion)
        return "AXInsertStyleGroup"_s;
    if (role == AccessibilityRole::Deletion)
        return "AXDeleteStyleGroup"_s;
    if (role == AccessibilityRole::Superscript)
        return "AXSuperscriptStyleGroup"_s;
    if (role == AccessibilityRole::Subscript)
        return "AXSubscriptStyleGroup"_s;

    switch (role) {
    case AccessibilityRole::RubyBase:
        return "AXRubyBase"_s;
    case AccessibilityRole::RubyBlock:
        return "AXRubyBlock"_s;
    case AccessibilityRole::RubyInline:
        return "AXRubyInline"_s;
    case AccessibilityRole::RubyRun:
        return "AXRubyRun"_s;
    case AccessibilityRole::RubyText:
        return "AXRubyText"_s;
    default:
        break;
    }

    if (isStyleFormatGroup()) {
        using namespace HTMLNames;
        auto tag = tagName();
        if (tag == kbdTag)
            return "AXKeyboardInputStyleGroup"_s;
        if (tag == codeTag)
            return "AXCodeStyleGroup"_s;
        if (tag == preTag)
            return "AXPreformattedStyleGroup"_s;
        if (tag == sampTag)
            return "AXSampleStyleGroup"_s;
        if (tag == varTag)
            return "AXVariableStyleGroup"_s;
        if (tag == citeTag)
            return "AXCiteStyleGroup"_s;
        ASSERT_NOT_REACHED();
        return String();
    }

    return String();
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
        case AccessibilityRole::Footer:
            return AXFooterRoleDescriptionText();
        case AccessibilityRole::Mark:
            return AXMarkText();
        case AccessibilityRole::Video:
            return localizedMediaControlElementString("VideoElement"_s);
        case AccessibilityRole::GraphicsDocument:
            return AXARIAContentGroupText("ARIADocument"_s);
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
            if (equalLettersIgnoringASCIICase(type, "date"_s))
                return AXDateFieldText();
            if (equalLettersIgnoringASCIICase(type, "time"_s))
                return AXTimeFieldText();
            if (equalLettersIgnoringASCIICase(type, "week"_s))
                return AXWeekFieldText();
            if (equalLettersIgnoringASCIICase(type, "month"_s))
                return AXMonthFieldText();
            if (equalLettersIgnoringASCIICase(type, "datetime-local"_s))
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

AXTextMarkerRangeRef AccessibilityObject::textMarkerRangeForNSRange(const NSRange& range) const
{
    return textMarkerRangeFromVisiblePositions(axObjectCache(),
        visiblePositionForIndex(range.location),
        visiblePositionForIndex(range.location + range.length));
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
        { AccessibilityRole::Model, NSAccessibilityGroupRole },
        { AccessibilityRole::Suggestion, NSAccessibilityGroupRole },
    };
    PlatformRoleMap roleMap;
    for (auto& role : roles)
        roleMap.add(static_cast<unsigned>(role.value), role.string);
    return roleMap;
}

} // namespace Accessibility

} // WebCore

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(MAC)
