/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#if ENABLE(ACCESSIBILITY) && PLATFORM(IOS_FAMILY)

#import "AccessibilityRenderObject.h"
#import "EventNames.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "LocalFrameView.h"
#import "RenderObject.h"
#import "WAKView.h"
#import "WebAccessibilityObjectWrapperIOS.h"
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenBlockquoteLevel, NSString *);
#define AccessibilityTokenBlockquoteLevel getUIAccessibilityTokenBlockquoteLevel()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenUnderline, NSString *);
#define AccessibilityTokenUnderline getUIAccessibilityTokenUnderline()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityTokenLanguage, NSString *);
#define AccessibilityTokenLanguage getUIAccessibilityTokenLanguage()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityInlineTextCompletion, NSString *);
#define AccessibilityInlineTextCompletion getUIAccessibilityInlineTextCompletion()
SOFT_LINK_CONSTANT(AXRuntime, UIAccessibilityAcceptedInlineTextCompletion, NSString *);
#define AccessibilityAcceptedInlineTextCompletion getUIAccessibilityAcceptedInlineTextCompletion()

namespace WebCore {
    
void AccessibilityObject::detachPlatformWrapper(AccessibilityDetachmentType)
{
    [wrapper() detach];
}

void AccessibilityObject::detachFromParent()
{
}

FloatRect AccessibilityObject::convertRectToPlatformSpace(const FloatRect& rect, AccessibilityConversionSpace space) const
{
    auto* frameView = documentFrameView();
    WAKView *documentView = frameView ? frameView->documentView() : nullptr;
    if (documentView) {
        CGPoint point = CGPointMake(rect.x(), rect.y());
        CGSize size = CGSizeMake(rect.size().width(), rect.size().height());
        CGRect cgRect = CGRectMake(point.x, point.y, size.width, size.height);

        cgRect = [documentView convertRect:cgRect toView:nil];

        // we need the web document view to give us our final screen coordinates
        // because that can take account of the scroller
        id webDocument = [wrapper() _accessibilityWebDocumentView];
        if (webDocument)
            cgRect = [webDocument convertRect:cgRect toView:nil];
        return cgRect;
    }

    return convertFrameToSpace(rect, space);
}

// On iOS, we don't have to return the value in the title. We can return the actual title, given the API.
bool AccessibilityObject::fileUploadButtonReturnsValueInTitle() const
{
    return false;
}

void AccessibilityObject::overrideAttachmentParent(AccessibilityObject*)
{
}
    
// In iPhone only code for now. It's debateable whether this is desired on all platforms.
int AccessibilityObject::accessibilitySecureFieldLength()
{
    if (!isSecureField())
        return 0;
    RenderObject* renderObject = downcast<AccessibilityRenderObject>(*this).renderer();
    
    if (!renderObject || !is<HTMLInputElement>(renderObject->node()))
        return false;
    
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*renderObject->node());
    return inputElement.value().length();
}

bool AccessibilityObject::accessibilityIgnoreAttachment() const
{
    return [[wrapper() attachmentView] accessibilityIsIgnored];
}
    
AccessibilityObjectInclusion AccessibilityObject::accessibilityPlatformIncludesObject() const
{
    if (roleValue() == AccessibilityRole::Unknown)
        return AccessibilityObjectInclusion::IgnoreObject;
    return AccessibilityObjectInclusion::DefaultBehavior;
}

bool AccessibilityObject::hasTouchEventListener() const
{
    // Check whether this->node or any of its ancestors has any of the touch-related event listeners.
    auto& eventNames = WebCore::eventNames();
    // If the node is in a shadowRoot, going up the node parent tree will stop and
    // not check the entire chain of ancestors. Thus, use the parentInComposedTree instead.
    for (auto* node = this->node(); node; node = node->parentInComposedTree()) {
        if (node->containsMatchingEventListener([&](const AtomString& name, auto&) {
            return eventNames.typeInfoForEvent(name).isInCategory(EventCategory::TouchRelated);
        }))
            return true;
    }
    return false;
}

bool AccessibilityObject::isInputTypePopupButton() const
{
    if (is<HTMLInputElement>(node()))
        return roleValue() == AccessibilityRole::PopUpButton;
    return false;
}

void AccessibilityObject::setLastPresentedTextPrediction(Node& previousCompositionNode, CompositionState state, const String& text, size_t location, bool handlingAcceptedCandidate)
{
#if HAVE(INLINE_PREDICTIONS)
    if (handlingAcceptedCandidate)
        m_lastPresentedTextPrediction = { text, location };

    if (state == CompositionState::Ended && !lastPresentedTextPrediction().text.isEmpty()) {
        String previousCompositionNodeText = previousCompositionNode.isTextNode() ? dynamicDowncast<Text>(previousCompositionNode)->wholeText() : String();
        size_t wordStart = 0;

        // Find the location of the complete word being predicted by iterating backwards through the text to find whitespace.
        if (previousCompositionNodeText.length()) {
            for (size_t position = previousCompositionNodeText.length() - 1; position > 0; position--) {
                if (isASCIIWhitespace(previousCompositionNodeText[position])) {
                    wordStart = position + 1;
                    break;
                }
            }
        }
        if (wordStart)
            previousCompositionNodeText = previousCompositionNodeText.substring(wordStart);

        m_lastPresentedTextPredictionComplete = { previousCompositionNodeText + m_lastPresentedTextPrediction.text, wordStart };

        // Reset last presented prediction since a candidate was accepted.
        m_lastPresentedTextPrediction.reset();
    } else if (state == CompositionState::InProgress || state == CompositionState::Started)
        m_lastPresentedTextPredictionComplete.reset();
#else
    UNUSED_PARAM(previousCompositionNode);
    UNUSED_PARAM(state);
    UNUSED_PARAM(text);
    UNUSED_PARAM(location);
    UNUSED_PARAM(handlingAcceptedCandidate);
#endif // HAVE (INLINE_PREDICTIONS)
}

// NSAttributedString support.

static void attributeStringSetLanguage(NSMutableAttributedString *attrString, RenderObject* renderer, const NSRange& range)
{
    if (!renderer)
        return;

    RefPtr object = renderer->document().axObjectCache()->getOrCreate(renderer);
    NSString *language = object->language();
    if (language.length)
        [attrString addAttribute:AccessibilityTokenLanguage value:language range:range];
    else
        [attrString removeAttribute:AccessibilityTokenLanguage range:range];
}

static unsigned blockquoteLevel(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    unsigned result = 0;
    for (Node* node = renderer->node(); node; node = node->parentNode()) {
        if (node->hasTagName(HTMLNames::blockquoteTag))
            ++result;
    }

    return result;
}

static void attributeStringSetBlockquoteLevel(NSMutableAttributedString *attrString, RenderObject* renderer, const NSRange& range)
{
    unsigned quoteLevel = blockquoteLevel(renderer);

    if (quoteLevel)
        [attrString addAttribute:AccessibilityTokenBlockquoteLevel value:@(quoteLevel) range:range];
    else
        [attrString removeAttribute:AccessibilityTokenBlockquoteLevel range:range];
}

static void attributeStringSetStyle(NSMutableAttributedString *attrString, RenderObject* renderer, const NSRange& range)
{
    if (!renderer)
        return;

    auto& style = renderer->style();

    // Set basic font info.
    attributedStringSetFont(attrString, style.fontCascade().primaryFont().getCTFont(), range);

    auto decor = style.textDecorationsInEffect();
    if (decor & TextDecorationLine::Underline)
        attributedStringSetNumber(attrString, AccessibilityTokenUnderline, @YES, range);

    // Add code context if this node is within a <code> block.
    RefPtr object = renderer->document().axObjectCache()->getOrCreate(renderer);
    auto matchFunc = [] (const auto& axObject) {
        return axObject.isCode();
    };

    if (const auto* parent = Accessibility::findAncestor<AccessibilityObject>(*object, true, WTFMove(matchFunc)))
        [attrString addAttribute:UIAccessibilityTextAttributeContext value:UIAccessibilityTextualContextSourceCode range:range];
}

static void attributedStringSetCompositionAttributes(NSMutableAttributedString *attributedString, RenderObject* renderer)
{
#if HAVE(INLINE_PREDICTIONS)
    if (!renderer)
        return;

    RefPtr object = renderer->document().axObjectCache()->getOrCreate(renderer);

    if (!object)
        return;

    auto& lastPresentedCompleteWord = object->lastPresentedTextPredictionComplete();
    unsigned lastPresentedCompleteWordLength = lastPresentedCompleteWord.text.length();
    unsigned lastPresentedCompleteWordPosition = lastPresentedCompleteWord.location;

    if (!lastPresentedCompleteWord.text.isEmpty() && lastPresentedCompleteWordPosition + lastPresentedCompleteWordLength <= [attributedString length]) {
        NSRange completeWordRange = NSMakeRange(lastPresentedCompleteWordPosition, lastPresentedCompleteWordLength);
        if ([[attributedString.string substringWithRange:completeWordRange] isEqualToString:lastPresentedCompleteWord.text])
            [attributedString addAttribute:AccessibilityAcceptedInlineTextCompletion value:lastPresentedCompleteWord.text range:completeWordRange];
    }

    auto& lastPresentedTextPrediction = object->lastPresentedTextPrediction();
    unsigned lastPresentedLength = lastPresentedTextPrediction.text.length();
    unsigned lastPresentedPosition = lastPresentedTextPrediction.location;

    if (!lastPresentedTextPrediction.text.isEmpty() && lastPresentedPosition + lastPresentedLength <= [attributedString length]) {
        NSRange presentedRange = NSMakeRange(lastPresentedPosition, lastPresentedLength);
        if (![[attributedString.string substringWithRange:presentedRange] isEqualToString:lastPresentedTextPrediction.text])
            return;

        [attributedString addAttribute:AccessibilityInlineTextCompletion value:[attributedString.string substringWithRange:presentedRange] range:presentedRange];
    }
#else
    UNUSED_PARAM(attributedString);
    UNUSED_PARAM(renderer);
#endif // HAVE(INLINE_PREDICTIONS)
}

RetainPtr<NSAttributedString> attributedStringCreate(Node* node, StringView text, const SimpleRange&, AXCoreObject::SpellCheck)
{
    // Skip invisible text.
    auto* renderer = node->renderer();
    if (!renderer)
        return nil;

    auto result = adoptNS([[NSMutableAttributedString alloc] initWithString:text.createNSStringWithoutCopying().get()]);
    NSRange range = NSMakeRange(0, [result length]);

    // Set attributes.
    attributeStringSetStyle(result.get(), renderer, range);
    attributeStringSetBlockquoteLevel(result.get(), renderer, range);
    attributeStringSetLanguage(result.get(), renderer, range);
    attributedStringSetCompositionAttributes(result.get(), renderer);

    return result;
}

} // WebCore

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(IOS_FAMILY)
