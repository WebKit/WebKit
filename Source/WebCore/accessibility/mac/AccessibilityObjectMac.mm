/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

#import "AXObjectCache.h"
#import "AXRemoteFrame.h"
#import "AccessibilityLabel.h"
#import "AccessibilityList.h"
#import "ColorCocoa.h"
#import "CompositionHighlight.h"
#import "CompositionUnderline.h"
#import "Editor.h"
#import "ElementAncestorIteratorInlines.h"
#import "FrameSelection.h"
#import "HTMLFieldSetElement.h"
#import "HTMLInputElement.h"
#import "LocalFrame.h"
#import "LocalFrameView.h"
#import "LocalizedStrings.h"
#import "RenderObject.h"
#import "Settings.h"
#import "TextCheckerClient.h"
#import "TextCheckingHelper.h"
#import "TextDecorationPainter.h"
#import "TextIterator.h"
#import <wtf/cocoa/SpanCocoa.h>

#if PLATFORM(MAC)

#import "PlatformScreen.h"
#import "WebAccessibilityObjectWrapperMac.h"
#import "Widget.h"

#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>

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

void AccessibilityObject::overrideAttachmentParent(AccessibilityObject* parent)
{
    if (!isAttachment())
        return;
    
    id parentWrapper = nil;
    if (parent) {
        if (parent->isIgnored())
            parent = parent->parentObjectUnignored();
        parentWrapper = parent->wrapper();
    }

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[wrapper() attachmentView] accessibilitySetOverrideValue:parentWrapper forAttribute:NSAccessibilityParentAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END
}

FloatRect AccessibilityObject::primaryScreenRect() const
{
    return screenRectForPrimaryScreen();
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
    // LocalFrameView attachments are now handled by AccessibilityScrollView,
    // so if this is the attachment, it should be ignored.
    Widget* widget = nullptr;
    if (isAttachment() && (widget = widgetForAttachmentView()) && widget->isLocalFrameView())
        return true;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    id attachmentView = widget ? NSAccessibilityUnignoredDescendant(widget->platformWidget()) : nil;
    if (attachmentView)
        return [attachmentView accessibilityIsIgnored];
ALLOW_DEPRECATED_DECLARATIONS_END

    // Attachments are ignored by default (unless we determine that we should expose them).
    return true;
}

AccessibilityObjectInclusion AccessibilityObject::accessibilityPlatformIncludesObject() const
{
    if (isMenuListPopup() || isMenuListOption())
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
    auto* frame = this->frame();
    return frame && frame->settings().caretBrowsingEnabled();
}

void AccessibilityObject::setCaretBrowsingEnabled(bool on)
{
    auto* frame = this->frame();
    if (!frame)
        return;
    frame->settings().setCaretBrowsingEnabled(on);
}

String AccessibilityObject::rolePlatformString() const
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (isAttachment())
        return [[wrapper() attachmentView] accessibilityAttributeValue:NSAccessibilityRoleAttribute];

    if (isRemoteFrame())
        return [remoteFramePlatformElement().get() accessibilityAttributeValue:NSAccessibilityRoleAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END

    auto role = roleValue();

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

static bool isEmptyGroup(AccessibilityObject& object)
{
#if ENABLE(MODEL_ELEMENT)
    if (object.isModel())
        return false;
#endif

    if (object.isRemoteFrame())
        return false;

    return [object.rolePlatformString() isEqual:NSAccessibilityGroupRole]
        && object.unignoredChildren().isEmpty()
        && ![renderWidgetChildren(object) count];
}

NSArray *renderWidgetChildren(const AXCoreObject& object)
{
    if (!object.isWidget())
        return nil;

    id child = Accessibility::retrieveAutoreleasedValueFromMainThread<id>([object = Ref { object }] () -> RetainPtr<id> {
        auto* widget = object->widget();
        return widget ? widget->accessibilityObject() : nil;
    });

    if (child)
        return @[child];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [object.platformWidget() accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END
}

String AccessibilityObject::subrolePlatformString() const
{
    if (isEmptyGroup(*const_cast<AccessibilityObject*>(this)))
        return @"AXEmptyGroup";

    if (isSecureField())
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
    case AccessibilityRole::Feed:
    case AccessibilityRole::Footnote:
    case AccessibilityRole::Group:
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

    // Only return a subrole for explicitly defined (via ARIA) text groups.
    if (ariaRoleAttribute() == AccessibilityRole::TextGroup)
        return "AXApplicationGroup"_s;

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
    case AccessibilityRole::RubyInline:
        return "AXRubyInline"_s;
    case AccessibilityRole::RubyText:
        return "AXRubyText"_s;
    default:
        break;
    }

    if (isCode())
        return "AXCodeStyleGroup"_s;

    using namespace HTMLNames;
    auto tag = tagName();
    if (tag == kbdTag)
        return "AXKeyboardInputStyleGroup"_s;
    if (tag == preTag)
        return "AXPreformattedStyleGroup"_s;
    if (tag == sampTag)
        return "AXSampleStyleGroup"_s;
    if (tag == varTag)
        return "AXVariableStyleGroup"_s;
    if (tag == citeTag)
        return "AXCiteStyleGroup"_s;
    ASSERT_WITH_MESSAGE(!isStyleFormatGroup(), "Should've been able to compute a subrole for style format group object");

    return String();
}

String AccessibilityObject::rolePlatformDescription() const
{
    // Attachments have the AXImage role, but may have different subroles.
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (isAttachment())
        return [[wrapper() attachmentView] accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];

    if (isRemoteFrame())
        return [remoteFramePlatformElement().get() accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END

    NSString *axRole = rolePlatformString();

    if ([axRole isEqualToString:NSAccessibilityGroupRole]) {
        if (isOutput())
            return AXOutputText();

        String ariaLandmarkRoleDescription = this->ariaLandmarkRoleDescription();
        if (!ariaLandmarkRoleDescription.isEmpty())
            return ariaLandmarkRoleDescription;

        switch (roleValue()) {
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
            return { };
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
        if (RefPtr input = dynamicDowncast<HTMLInputElement>(node())) {
            if (input->isEmailField())
                return AXEmailFieldText();
            if (input->isTelephoneField())
                return AXTelephoneFieldText();
            if (input->isURLField())
                return AXURLFieldText();
            if (input->isNumberField())
                return AXNumberFieldText();

            // These input types are not enabled on mac yet, we check the type attribute for now.
            // FIXME: "date", "time", and "datetime-local" are enabled on macOS.
            auto& type = input->attributeWithoutSynchronization(HTMLNames::typeAttr);
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

    if (roleValue() == AccessibilityRole::HorizontalRule)
        return AXHorizontalRuleDescriptionText();

    // AppKit also returns AXTab for the role description for a tab item.
    if (isTabItem())
        return NSAccessibilityRoleDescription(@"AXTab", nil);

    if (isSummary())
        return AXSummaryText();

    return { };
}

// NSAttributedString support.

static void attributedStringSetStyle(NSMutableAttributedString *attrString, RenderObject* renderer, const NSRange& range)
{
    if (!renderer || !attributedStringContainsRange(attrString, range))
        return;

    const auto& style = renderer->style();

    // set basic font info
    attributedStringSetFont(attrString, style.fontCascade().primaryFont().getCTFont(), range);

    // Set basic colors.
    attributedStringSetColor(attrString, NSAccessibilityForegroundColorTextAttribute, cocoaColor(style.visitedDependentColor(CSSPropertyColor)).get(), range);
    attributedStringSetColor(attrString, NSAccessibilityBackgroundColorTextAttribute, cocoaColor(style.visitedDependentColor(CSSPropertyBackgroundColor)).get(), range);

    // Set super/sub scripting.
    auto alignment = style.verticalAlign();
    if (alignment == VerticalAlign::Sub)
        attributedStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, @(-1), range);
    else if (alignment == VerticalAlign::Super)
        attributedStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, @(1), range);

    // Set shadow.
    if (style.textShadow())
        attributedStringSetNumber(attrString, NSAccessibilityShadowTextAttribute, @YES, range);

    // Set underline and strikethrough.
    auto decor = style.textDecorationsInEffect();
    if (decor & TextDecorationLine::Underline || decor & TextDecorationLine::LineThrough) {
        // FIXME: Should the underline style be reported here?
        auto decorationStyles = TextDecorationPainter::stylesForRenderer(*renderer, decor);

        if (decor & TextDecorationLine::Underline) {
            attributedStringSetNumber(attrString, NSAccessibilityUnderlineTextAttribute, @YES, range);
            attributedStringSetColor(attrString, NSAccessibilityUnderlineColorTextAttribute, cocoaColor(decorationStyles.underline.color).get(), range);
        }

        if (decor & TextDecorationLine::LineThrough) {
            attributedStringSetNumber(attrString, NSAccessibilityStrikethroughTextAttribute, @YES, range);
            attributedStringSetColor(attrString, NSAccessibilityStrikethroughColorTextAttribute, cocoaColor(decorationStyles.linethrough.color).get(), range);
        }
    }

    // FIXME: This traversal should be replaced by a flag in AccessibilityObject::m_ancestorFlags (e.g., AXAncestorFlag::HasHTMLMarkAncestor)
    // Indicate background highlighting.
    for (Node* node = renderer->node(); node; node = node->parentNode()) {
        if (node->hasTagName(HTMLNames::markTag))
            attributedStringSetNumber(attrString, @"AXHighlight", @YES, range);
        if (auto* element = dynamicDowncast<Element>(*node)) {
            auto& roleValue = element->attributeWithoutSynchronization(HTMLNames::roleAttr);
            if (equalLettersIgnoringASCIICase(roleValue, "insertion"_s))
                attributedStringSetNumber(attrString, @"AXIsSuggestedInsertion", @YES, range);
            else if (equalLettersIgnoringASCIICase(roleValue, "deletion"_s))
                attributedStringSetNumber(attrString, @"AXIsSuggestedDeletion", @YES, range);
            else if (equalLettersIgnoringASCIICase(roleValue, "suggestion"_s))
                attributedStringSetNumber(attrString, @"AXIsSuggestion", @YES, range);
            else if (equalLettersIgnoringASCIICase(roleValue, "mark"_s))
                attributedStringSetNumber(attrString, @"AXHighlight", @YES, range);
        }
    }
}

static void attributedStringSetHeadingLevel(NSMutableAttributedString *attrString, RenderObject* renderer, const NSRange& range)
{
    if (!renderer || !attributedStringContainsRange(attrString, range))
        return;

    // Sometimes there are objects between the text and the heading.
    // In those cases the parent hierarchy should be queried to see if there is a heading ancestor.
    auto* cache = renderer->document().axObjectCache();
    RefPtr ancestor = cache ? cache->getOrCreate(renderer->parent()) : nullptr;
    for (; ancestor; ancestor = ancestor->parentObject()) {
        if (unsigned level = ancestor->headingLevel()) {
            [attrString addAttribute:@"AXHeadingLevel" value:@(level) range:range];
            return;
        }
    }
}

static void attributedStringSetBlockquoteLevel(NSMutableAttributedString *attrString, RenderObject* renderer, const NSRange& range)
{
    if (!renderer || !attributedStringContainsRange(attrString, range))
        return;

    auto* cache = renderer->document().axObjectCache();
    RefPtr object = cache ? cache->getOrCreate(*renderer) : nullptr;
    if (!object)
        return;

    unsigned level = object->blockquoteLevel();
    if (level)
        [attrString addAttribute:NSAccessibilityBlockQuoteLevelAttribute value:@(level) range:range];
}

static void attributedStringSetExpandedText(NSMutableAttributedString *attrString, RenderObject* renderer, const NSRange& range)
{
    if (!renderer || !attributedStringContainsRange(attrString, range))
        return;

    auto* cache = renderer->document().axObjectCache();
    RefPtr object = cache ? cache->getOrCreate(*renderer) : nullptr;
    if (object && object->supportsExpandedTextValue())
        [attrString addAttribute:NSAccessibilityExpandedTextValueAttribute value:object->expandedTextValue() range:range];
}

static void attributedStringSetElement(NSMutableAttributedString *attrString, NSString *attribute, AccessibilityObject* object, const NSRange& range)
{
    if (!attributedStringContainsRange(attrString, range))
        return;

    auto* renderObject = dynamicDowncast<AccessibilityRenderObject>(object);
    if (!renderObject)
        return;

    // Make a serializable AX object.
    auto* renderer = renderObject->renderer();
    if (!renderer)
        return;

    id wrapper = object->wrapper();
    if ([attribute isEqualToString:NSAccessibilityAttachmentTextAttribute] && object->isAttachment()) {
        if (id attachmentView = [wrapper attachmentView])
            wrapper = [wrapper attachmentView];
    }

    if (auto axElement = adoptCF(NSAccessibilityCreateAXUIElementRef(wrapper)))
        [attrString addAttribute:attribute value:(__bridge id)axElement.get() range:range];
}

static void attributedStringSetCompositionAttributes(NSMutableAttributedString *attrString, Node& node, const SimpleRange& textSimpleRange)
{
#if HAVE(INLINE_PREDICTIONS)
    auto& editor = node.document().editor();
    if (&node != editor.compositionNode())
        return;

    auto scope = makeRangeSelectingNodeContents(node);
    auto textRange = characterRange(scope, textSimpleRange);

    auto& annotations = editor.customCompositionAnnotations();
    if (auto it = annotations.find(NSTextCompletionAttributeName); it != annotations.end()) {
        for (auto& annotationRange : it->value) {
            auto intersectionRange = NSIntersectionRange(textRange, annotationRange);
            if (intersectionRange.length) {
                auto completionRange = NSMakeRange(intersectionRange.location - textRange.location, intersectionRange.length);
                attributedStringSetNumber(attrString, NSAccessibilityTextCompletionAttribute, @YES, completionRange);
            }
        }
    }
#else
    UNUSED_PARAM(attrString);
    UNUSED_PARAM(node);
    UNUSED_PARAM(textSimpleRange);
#endif
}

static bool shouldHaveAnySpellCheckAttribute(Node& node)
{
    // If this node is not inside editable content, do not run the spell checker on the text.
    auto* cache = node.document().axObjectCache();
    return cache && cache->rootAXEditableElement(&node);
}

void attributedStringSetNeedsSpellCheck(NSMutableAttributedString *attributedString, Node& node)
{
    if (!shouldHaveAnySpellCheckAttribute(node))
        return;

    // Inform the AT that we want it to spell-check for us by setting AXDidSpellCheck to @NO.
    attributedStringSetNumber(attributedString, AXDidSpellCheckAttribute, @NO, NSMakeRange(0, attributedString.length));
}

void attributedStringSetSpelling(NSMutableAttributedString *attrString, Node& node, StringView text, const NSRange& range)
{
    if (!shouldHaveAnySpellCheckAttribute(node))
        return;

    if (unifiedTextCheckerEnabled(node.document().frame())) {
        // Check the spelling directly since document->markersForNode() does not store the misspelled marking when the cursor is in a word.
        auto* checker = node.document().editor().textChecker();

        // checkTextOfParagraph is the only spelling/grammar checker implemented in WK1 and WK2
        Vector<TextCheckingResult> results;
        checkTextOfParagraph(*checker, text, TextCheckingType::Spelling, results, node.document().frame()->selection().selection());
        for (const auto& result : results) {
            attributedStringSetNumber(attrString, NSAccessibilityMisspelledTextAttribute, @YES, NSMakeRange(result.range.location + range.location, result.range.length));
            attributedStringSetNumber(attrString, NSAccessibilityMarkedMisspelledTextAttribute, @YES, NSMakeRange(result.range.location + range.location, result.range.length));
        }

        return;
    }

    for (unsigned current = 0; current < text.length(); ) {
        int misspellingLocation = -1;
        int misspellingLength = 0;
        node.document().editor().textChecker()->checkSpellingOfString(text.substring(current), &misspellingLocation, &misspellingLength);
        if (misspellingLocation < 0 || !misspellingLength)
            break;

        NSRange spellRange = NSMakeRange(range.location + current + misspellingLocation, misspellingLength);
        attributedStringSetNumber(attrString, NSAccessibilityMisspelledTextAttribute, @YES, spellRange);
        attributedStringSetNumber(attrString, NSAccessibilityMarkedMisspelledTextAttribute, @YES, spellRange);

        current += misspellingLocation + misspellingLength;
    }
}

RetainPtr<NSAttributedString> attributedStringCreate(Node& node, StringView text, const SimpleRange& textRange, AXCoreObject::SpellCheck spellCheck)
{
    if (!text.length())
        return nil;

    // Skip invisible text.
    CheckedPtr renderer = node.renderer();
    if (!renderer)
        return nil;

    auto result = adoptNS([[NSMutableAttributedString alloc] initWithString:text.createNSStringWithoutCopying().get()]);
    NSRange range = NSMakeRange(0, [result length]);

    // Set attributes.
    attributedStringSetStyle(result.get(), renderer.get(), range);
    attributedStringSetHeadingLevel(result.get(), renderer.get(), range);
    attributedStringSetBlockquoteLevel(result.get(), renderer.get(), range);
    attributedStringSetExpandedText(result.get(), renderer.get(), range);
    attributedStringSetElement(result.get(), NSAccessibilityLinkTextAttribute, AccessibilityObject::anchorElementForNode(node), range);
    attributedStringSetCompositionAttributes(result.get(), node, textRange);
    // Do spelling last because it tends to break up the range.
    if (spellCheck == AXCoreObject::SpellCheck::Yes) {
        if (AXObjectCache::shouldSpellCheck())
            attributedStringSetSpelling(result.get(), node, text, range);
        else
            attributedStringSetNeedsSpellCheck(result.get(), node);
    }

    return result;
}

std::span<const uint8_t> AXRemoteFrame::generateRemoteToken() const
{
    if (auto* parent = parentObject()) {
        // We use the parent's wrapper so that the remote frame acts as a pass through for the remote token bridge.
        NSData *data = [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:parent->wrapper()];
        return span(data);
    }

    return std::span<const uint8_t> { };
}

void AXRemoteFrame::initializePlatformElementWithRemoteToken(std::span<const uint8_t> token, int processIdentifier)
{
    m_processIdentifier = processIdentifier;
    if ([wrapper() respondsToSelector:@selector(accessibilitySetPresenterProcessIdentifier:)])
        [(id)wrapper() accessibilitySetPresenterProcessIdentifier:processIdentifier];
    m_remoteFramePlatformElement = adoptNS([[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:toNSData(BufferSource { token }).get()]);

    if (auto* cache = axObjectCache())
        cache->onRemoteFrameInitialized(*this);
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
        { AccessibilityRole::DateTime, @"AXDateTimeArea" },
        { AccessibilityRole::Splitter, NSAccessibilitySplitterRole },
        { AccessibilityRole::Code, NSAccessibilityGroupRole },
        { AccessibilityRole::ColorWell, NSAccessibilityColorWellRole },
        { AccessibilityRole::Link, NSAccessibilityLinkRole },
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
    };
    PlatformRoleMap roleMap;
    for (auto& role : roles)
        roleMap.add(static_cast<unsigned>(role.value), role.string);
    return roleMap;
}

} // namespace Accessibility

} // WebCore

#endif // PLATFORM(MAC)
