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
#import "AccessibilityObjectInlines.h"

#import "AXObjectCacheInlines.h"
#import "AXRemoteFrame.h"
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
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(MAC)

#import "PlatformScreen.h"
#import "WebAccessibilityObjectWrapperMac.h"
#import "Widget.h"

#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>

namespace WebCore {

void AccessibilityObject::detachPlatformWrapper(AccessibilityDetachmentType)
{
    [protect(wrapper()) detach];
}

void AccessibilityObject::detachFromParent()
{
    overrideAttachmentParent(nullptr);
}

void AccessibilityObject::overrideAttachmentParent(AccessibilityObject* parent)
{
    if (!isAttachment()) [[likely]]
        return;

    RefPtr axParent = parent;
    id parentWrapper = nil;
    if (axParent && axParent->isIgnored())
        axParent = axParent->parentObjectUnignored();

    if (axParent)
        parentWrapper = axParent->wrapper();

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [protect([protect(wrapper()) attachmentView]) accessibilitySetOverrideValue:parentWrapper forAttribute:NSAccessibilityParentAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END
}

FloatRect AccessibilityObject::primaryScreenRect() const
{
    return screenRectForPrimaryScreen();
}

FloatRect AccessibilityObject::convertRectToPlatformSpace(const FloatRect& rect, AccessibilityConversionSpace space) const
{
    // WebKit1 code path... platformWidget() exists.
    RefPtr frameView = documentFrameView();
    if (frameView && frameView->platformWidget()) {
        CGPoint point = CGPointMake(rect.x(), rect.y());
        CGSize size = CGSizeMake(rect.size().width(), rect.size().height());
        CGRect cgRect = CGRectMake(point.x, point.y, size.width, size.height);

        NSRect nsRect = NSRectFromCGRect(cgRect);
        RetainPtr view = frameView->documentView();

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        nsRect = [[view window] convertRectToScreen:[view convertRect:nsRect toView:nil]];
        ALLOW_DEPRECATED_DECLARATIONS_END

        return NSRectToCGRect(nsRect);
    }

    return convertFrameToSpace(rect, space);
}

bool AccessibilityObject::accessibilityIgnoreAttachment() const
{
    // LocalFrameView attachments are now handled by AccessibilityScrollView,
    // so if this is the attachment, it should be ignored.
    Widget* widget = nullptr;
    if (isAttachment() && (widget = widgetForAttachmentView()) && widget->isLocalFrameView())
        return true;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<id> attachmentView = widget ? NSAccessibilityUnignoredDescendant(protect(widget->platformWidget())) : nil;
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

    if (role() == AccessibilityRole::Mark)
        return AccessibilityObjectInclusion::IncludeObject;

    // Never expose an unknown object on the Mac. Clients of the AX API will not know what to do with it.
    // Special case is when the unknown object is actually an attachment.
    if (role() == AccessibilityRole::Unknown && !isAttachment())
        return AccessibilityObjectInclusion::IgnoreObject;

    if (role() == AccessibilityRole::Inline && !isStyleFormatGroup())
        return AccessibilityObjectInclusion::IgnoreObject;

    if (RenderObject* renderer = this->renderer()) {
        // The legend element is ignored if it lives inside of a fieldset element that uses it to generate alternative text.
        if (renderer->isLegend()) {
            RefPtr element = this->element();
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

AXCoreObject::AccessibilityChildrenVector AccessibilityObject::allSortedLiveRegions() const
{
    CheckedPtr cache = axObjectCache();
    if (!cache)
        return { };
    return cache->sortedLiveRegions();
}

AXCoreObject::AccessibilityChildrenVector AccessibilityObject::allSortedNonRootWebAreas() const
{
    CheckedPtr cache = axObjectCache();
    if (!cache)
        return { };
    return cache->sortedNonRootWebAreas();
}

RefPtr<StringImpl> AccessibilityObject::subrolePlatformString() const
{
    if (isSecureField())
        return StringImpl::create(NSAccessibilitySecureTextFieldSubrole);
    if (isSearchField())
        return StringImpl::create(NSAccessibilitySearchFieldSubrole);

    if (isAttachment()) {
        RetainPtr<id> attachView = [protect(wrapper()) attachmentView];

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([[attachView accessibilityAttributeNames] containsObject:NSAccessibilitySubroleAttribute])
            return StringImpl::create(dynamic_objc_cast<NSString>([attachView accessibilityAttributeValue:NSAccessibilitySubroleAttribute]));
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

    if (isMeter())
        return "AXMeter"_si;

#if ENABLE(MODEL_ELEMENT)
    if (isModel())
        return "AXModel"_si;
#endif

    auto role = this->role();
    if (role == AccessibilityRole::HorizontalRule)
        return "AXContentSeparator"_si;
    if (role == AccessibilityRole::ToggleButton)
        return StringImpl::create(NSAccessibilityToggleSubrole);
    if (role == AccessibilityRole::SectionFooter)
        return "AXSectionFooter"_si;
    if (role == AccessibilityRole::SectionHeader)
        return "AXSectionHeader"_si;
    if (role == AccessibilityRole::SpinButtonPart) {
        if (isIncrementor())
            return StringImpl::create(NSAccessibilityIncrementArrowSubrole);
        return StringImpl::create(NSAccessibilityDecrementArrowSubrole);
    }

    if (isFileUploadButton())
        return "AXFileUploadButton"_si;

    if (isTreeItem())
        return StringImpl::create(NSAccessibilityOutlineRowSubrole);

    if (isFieldset())
        return "AXFieldset"_si;

    if (isList()) {
        if (isUnorderedList() || isOrderedList())
            return "AXContentList"_si;
        if (isDescriptionList())
            return "AXDescriptionList"_si;
    }

    if (listBoxInterpretation() == ListBoxInterpretation::ActuallyStaticList)
        return "AXContentList"_si;

    // ARIA content subroles.
    switch (role) {
    case AccessibilityRole::Form:
        return "AXLandmarkForm"_si;
    case AccessibilityRole::LandmarkBanner:
        return "AXLandmarkBanner"_si;
    case AccessibilityRole::LandmarkComplementary:
        return "AXLandmarkComplementary"_si;
    case AccessibilityRole::LandmarkContentInfo:
        return "AXLandmarkContentInfo"_si;
    case AccessibilityRole::LandmarkMain:
        return "AXLandmarkMain"_si;
    case AccessibilityRole::LandmarkNavigation:
        return "AXLandmarkNavigation"_si;
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkRegion:
        return "AXLandmarkRegion"_si;
    case AccessibilityRole::LandmarkSearch:
        return "AXLandmarkSearch"_si;
    case AccessibilityRole::SectionFooter:
        return "AXSectionFooter"_si;
    case AccessibilityRole::SectionHeader:
        return "AXSectionHeader"_si;
    case AccessibilityRole::ApplicationAlert:
        return "AXApplicationAlert"_si;
    case AccessibilityRole::ApplicationAlertDialog:
        return "AXApplicationAlertDialog"_si;
    case AccessibilityRole::ApplicationDialog:
        return "AXApplicationDialog"_si;
    case AccessibilityRole::Feed:
    case AccessibilityRole::Footnote:
    case AccessibilityRole::Group:
        return "AXApplicationGroup"_si;
    case AccessibilityRole::ApplicationLog:
        return "AXApplicationLog"_si;
    case AccessibilityRole::ApplicationMarquee:
        return "AXApplicationMarquee"_si;
    case AccessibilityRole::ApplicationStatus:
        return "AXApplicationStatus"_si;
    case AccessibilityRole::ApplicationTimer:
        return "AXApplicationTimer"_si;
    case AccessibilityRole::Document:
    case AccessibilityRole::GraphicsDocument:
        return "AXDocument"_si;
    case AccessibilityRole::DocumentArticle:
        return "AXDocumentArticle"_si;
    case AccessibilityRole::DocumentMath:
        return "AXDocumentMath"_si;
    case AccessibilityRole::DocumentNote:
        return "AXDocumentNote"_si;
    case AccessibilityRole::UserInterfaceTooltip:
        return "AXUserInterfaceTooltip"_si;
    case AccessibilityRole::TabPanel:
        return "AXTabPanel"_si;
    case AccessibilityRole::Definition:
        return "AXDefinition"_si;
    case AccessibilityRole::DescriptionListTerm:
    case AccessibilityRole::Term:
        return "AXTerm"_si;
    case AccessibilityRole::DescriptionListDetail:
        return "AXDescription"_si;
    case AccessibilityRole::WebApplication:
        return "AXWebApplication"_si;
    case AccessibilityRole::Suggestion:
        return "AXSuggestion"_si;
        // Default doesn't return anything, so roles defined below can be chosen.
    default:
        break;
    }

    // Only return a subrole for explicitly defined (via ARIA) text groups.
    if (ariaRoleAttribute() == AccessibilityRole::TextGroup)
        return "AXApplicationGroup"_si;

    if (role == AccessibilityRole::MathElement) {
        if (isMathFraction())
            return "AXMathFraction"_si;
        if (isMathFenced())
            return "AXMathFenced"_si;
        if (isMathSubscriptSuperscript())
            return "AXMathSubscriptSuperscript"_si;
        if (isMathRow())
            return "AXMathRow"_si;
        if (isMathUnderOver())
            return "AXMathUnderOver"_si;
        if (isMathSquareRoot())
            return "AXMathSquareRoot"_si;
        if (isMathRoot())
            return "AXMathRoot"_si;
        if (isMathText())
            return "AXMathText"_si;
        if (isMathNumber())
            return "AXMathNumber"_si;
        if (isMathIdentifier())
            return "AXMathIdentifier"_si;
        if (isMathTable())
            return "AXMathTable"_si;
        if (isMathTableRow())
            return "AXMathTableRow"_si;
        if (isMathTableCell())
            return "AXMathTableCell"_si;
        if (isMathFenceOperator())
            return "AXMathFenceOperator"_si;
        if (isMathSeparatorOperator())
            return "AXMathSeparatorOperator"_si;
        if (isMathOperator())
            return "AXMathOperator"_si;
        if (isMathMultiscript())
            return "AXMathMultiscript"_si;
    }

    if (role == AccessibilityRole::Video)
        return "AXVideo"_si;
    if (role == AccessibilityRole::Audio)
        return "AXAudio"_si;
    if (role == AccessibilityRole::Details)
        return "AXDetails"_si;
    if (role == AccessibilityRole::Summary)
        return "AXSummary"_si;
    if (role == AccessibilityRole::Time)
        return "AXTimeGroup"_si;

    if (isMediaTimeline())
        return StringImpl::create(NSAccessibilityTimelineSubrole);

    if (isSwitch())
        return StringImpl::create(NSAccessibilitySwitchSubrole);

    if (role == AccessibilityRole::Insertion)
        return "AXInsertStyleGroup"_si;
    if (role == AccessibilityRole::Deletion)
        return "AXDeleteStyleGroup"_si;
    if (role == AccessibilityRole::Superscript)
        return "AXSuperscriptStyleGroup"_si;
    if (role == AccessibilityRole::Subscript)
        return "AXSubscriptStyleGroup"_si;

    switch (role) {
    case AccessibilityRole::RubyInline:
        return "AXRubyInline"_si;
    case AccessibilityRole::RubyText:
        return "AXRubyText"_si;
    default:
        break;
    }

    if (isCode())
        return "AXCodeStyleGroup"_si;

    using namespace HTMLNames;
    auto elementName = this->elementName();
    if (elementName == ElementName::HTML_kbd)
        return "AXKeyboardInputStyleGroup"_si;
    if (elementName == ElementName::HTML_pre)
        return "AXPreformattedStyleGroup"_si;
    if (elementName == ElementName::HTML_samp)
        return "AXSampleStyleGroup"_si;
    if (elementName == ElementName::HTML_var)
        return "AXVariableStyleGroup"_si;
    if (elementName == ElementName::HTML_cite)
        return "AXCiteStyleGroup"_si;
    ASSERT_WITH_MESSAGE(!isStyleFormatGroup(), "Should've been able to compute a subrole for style format group object");

    return nullptr;
}

// NSAttributedString support.

static void attributedStringSetCompositionAttributes(NSMutableAttributedString *attrString, Node& node, const SimpleRange& textSimpleRange)
{
#if HAVE(INLINE_PREDICTIONS)
    Ref editor = protect(node.document())->editor();
    if (&node != editor->compositionNode())
        return;

    auto scope = makeRangeSelectingNodeContents(node);
    auto textRange = characterRange(scope, textSimpleRange);

    auto& annotations = editor->customCompositionAnnotations();
    if (auto it = annotations.find(StringImpl::create(NSTextCompletionAttributeName).get()); it != annotations.end()) {
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
    CheckedPtr cache = protect(node.document())->axObjectCache();
    return cache && cache->rootAXEditableElement(&node);
}

void attributedStringSetSpelling(NSMutableAttributedString *attrString, Node& node, StringView text, const NSRange& range)
{
    if (!shouldHaveAnySpellCheckAttribute(node))
        return;

    Ref document = node.document();
    RefPtr frame = document->frame();
    Ref editor = document->editor();
    if (unifiedTextCheckerEnabled(frame)) {
        // Check the spelling directly since document->markersForNode() does not store the misspelled marking when the cursor is in a word.
        auto* checker = editor->textChecker();

        // checkTextOfParagraph is the only spelling/grammar checker implemented in WK1 and WK2
        Vector<TextCheckingResult> results;
        checkTextOfParagraph(*checker, text, TextCheckingType::Spelling, results, frame->selection().selection());
        for (const auto& result : results) {
            attributedStringSetNumber(attrString, NSAccessibilityMisspelledTextAttribute, @YES, NSMakeRange(result.range.location + range.location, result.range.length));
            attributedStringSetNumber(attrString, NSAccessibilityMarkedMisspelledTextAttribute, @YES, NSMakeRange(result.range.location + range.location, result.range.length));
        }

        return;
    }

    for (unsigned current = 0; current < text.length(); ) {
        int misspellingLocation = -1;
        int misspellingLength = 0;
        editor->textChecker()->checkSpellingOfString(text.substring(current), &misspellingLocation, &misspellingLength);
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

    CheckedPtr cache = protect(renderer->document())->axObjectCache();
    RefPtr object = cache ? cache->getOrCreate(node) : nullptr;
    if (!object)
        return nil;

    RetainPtr string = object->createAttributedString(text, spellCheck);
    // Ideally this would happen in `createAttributedString`, but that doesn't work for `AXIsolatedObject`s at the moment.
    // See the FIXME comment in that function.
    attributedStringSetCompositionAttributes(string.get(), node, textRange);
    return string;
}

AccessibilityRemoteToken AXRemoteFrame::generateRemoteToken() const
{
    if (RefPtr parent = parentObject()) {
        // We use the parent's wrapper so that the remote frame acts as a pass through for the remote token bridge.
        return { makeVector([NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:protect(parent->wrapper())]) };
    }

    return { };
}

void AXRemoteFrame::initializePlatformElementWithRemoteToken(AccessibilityRemoteToken token, int processIdentifier)
{
    m_processIdentifier = processIdentifier;
    RetainPtr wrapper = this->wrapper();
    if ([wrapper respondsToSelector:@selector(accessibilitySetPresenterProcessIdentifier:)])
        [(id)wrapper.get() accessibilitySetPresenterProcessIdentifier:processIdentifier];
    m_remoteFramePlatformElement = adoptNS([[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:WTF::toNSData(token.bytes.span()).get()]);

    if (CheckedPtr cache = axObjectCache())
        cache->onRemoteFrameInitialized(*this);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
