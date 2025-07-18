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

#if PLATFORM(IOS_FAMILY)

#import "AXRemoteFrame.h"
#import "AccessibilityRenderObject.h"
#import "EventNames.h"
#import "EventTargetInlines.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "LocalFrameView.h"
#import "RenderObject.h"
#import "WAKView.h"
#import "WebAccessibilityObjectWrapperIOS.h"
#import <pal/spi/ios/AXRuntimeSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/WTFString.h>

#if !PLATFORM(MACCATALYST)
SOFT_LINK_CLASS_OPTIONAL(AXRuntime, AXRemoteElement);
#endif

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

// In iPhone only code for now. It's debateable whether this is desired on all platforms.
unsigned AccessibilityObject::accessibilitySecureFieldLength()
{
    CheckedPtr renderer = this->renderer();
    // Only consider secure fields that are rendered (i.e. have a non-null renderer).
    if (!renderer || !isSecureField())
        return 0;

    auto* inputElement = dynamicDowncast<HTMLInputElement>(renderer->node());
    return inputElement ? inputElement->value()->length() : 0;
}

bool AccessibilityObject::accessibilityIgnoreAttachment() const
{
    return [[wrapper() attachmentView] accessibilityIsIgnored];
}
    
AccessibilityObjectInclusion AccessibilityObject::accessibilityPlatformIncludesObject() const
{
    if (role() == AccessibilityRole::Unknown)
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

void AccessibilityObject::setLastPresentedTextPrediction(Node& previousCompositionNode, CompositionState state, const String& text, size_t location, bool handlingAcceptedCandidate)
{
#if HAVE(INLINE_PREDICTIONS)
    if (handlingAcceptedCandidate)
        m_lastPresentedTextPrediction = { text, location };

    if (state == CompositionState::Ended && !lastPresentedTextPrediction().text.isEmpty()) {
        auto* nodeText = dynamicDowncast<Text>(previousCompositionNode);
        String previousCompositionNodeText = nodeText ? nodeText->data() : String();
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

        m_lastPresentedTextPredictionComplete = { makeString(previousCompositionNodeText, m_lastPresentedTextPrediction.text), wordStart };

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

#if !PLATFORM(MACCATALYST)

static RetainPtr<NSDictionary> unarchivedTokenForData(RetainPtr<NSData> tokenData)
{
    NSError *error = nil;
    return [NSKeyedUnarchiver unarchivedObjectOfClasses:[NSSet setWithObjects:[NSDictionary class], [NSNumber class], [NSString class], nil] fromData:tokenData.get() error:&error];
}

#endif

Vector<uint8_t> AXRemoteFrame::generateRemoteToken() const
{
    if (RetainPtr data = Accessibility::newAccessibilityRemoteToken([[NSUUID UUID] UUIDString]))
        return makeVector(data.get());
    return { };
}

void AXRemoteFrame::initializePlatformElementWithRemoteToken(std::span<const uint8_t> token, int processIdentifier)
{
#if !PLATFORM(MACCATALYST)
    m_processIdentifier = processIdentifier;

    RetainPtr nsToken = WTF::toNSData(token);
    NSDictionary *tokenDictionary = nsToken ? unarchivedTokenForData(nsToken).get() : nil;
    if (!tokenDictionary)
        return;

    NSString *uuid = [tokenDictionary objectForKey:@"ax-uuid"];
    RetainPtr remoteElement = adoptNS([allocAXRemoteElementInstance() initWithUUID:uuid andRemotePid:processIdentifier andContextId:0]);
    remoteElement.get().onClientSide = YES;
    RefPtr parent = parentObjectUnignored();
    remoteElement.get().accessibilityContainer = parent ?  parent->wrapper() : nil;

    m_remoteFramePlatformElement = WTFMove(remoteElement);

    if (CheckedPtr cache = axObjectCache())
        cache->onRemoteFrameInitialized(*this);
#else
    UNUSED_PARAM(token);
    UNUSED_PARAM(processIdentifier);
#endif // !PLATFORM(MACCATALYST)
}

// NSAttributedString support.

static void attributeStringSetLanguage(NSMutableAttributedString *attrString, RenderObject* renderer, const NSRange& range)
{
    if (!renderer)
        return;

    RefPtr object = renderer->document().axObjectCache()->getOrCreate(*renderer);
    RetainPtr language = object->languageIncludingAncestors().createNSString();
    if (language.get().length)
        [attrString addAttribute:AccessibilityTokenLanguage value:language.get() range:range];
    else
        [attrString removeAttribute:AccessibilityTokenLanguage range:range];
}

static unsigned blockquoteLevel(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    unsigned result = 0;
    for (Node* node = renderer->node(); node; node = node->parentNode()) {
        if (WebCore::elementName(*node) == ElementName::HTML_blockquote)
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
    attributedStringSetFont(attrString, style.fontCascade().primaryFont()->getCTFont(), range);

    auto decor = style.textDecorationLineInEffect();
    if (decor & TextDecorationLine::Underline)
        attributedStringSetNumber(attrString, AccessibilityTokenUnderline, @YES, range);

    // Add code context if this node is within a <code> block.
    RefPtr object = renderer->document().axObjectCache()->getOrCreate(*renderer);
    auto matchFunc = [] (const auto& axObject) {
        return axObject.isCode();
    };

    if (Accessibility::findAncestor<AccessibilityObject>(*object, true, WTFMove(matchFunc)))
        [attrString addAttribute:UIAccessibilityTextAttributeContext value:UIAccessibilityTextualContextSourceCode range:range];
}

static void attributedStringSetCompositionAttributes(NSMutableAttributedString *attributedString, RenderObject* renderer)
{
#if HAVE(INLINE_PREDICTIONS)
    if (!renderer)
        return;

    RefPtr object = renderer->document().axObjectCache()->getOrCreate(*renderer);

    if (!object)
        return;

    auto& lastPresentedCompleteWord = object->lastPresentedTextPredictionComplete();
    unsigned lastPresentedCompleteWordLength = lastPresentedCompleteWord.text.length();
    unsigned lastPresentedCompleteWordPosition = lastPresentedCompleteWord.location;

    if (!lastPresentedCompleteWord.text.isEmpty() && lastPresentedCompleteWordPosition + lastPresentedCompleteWordLength <= [attributedString length]) {
        NSRange completeWordRange = NSMakeRange(lastPresentedCompleteWordPosition, lastPresentedCompleteWordLength);
        RetainPtr lastPresentedCompleteWordText = lastPresentedCompleteWord.text.createNSString();
        if ([[attributedString.string substringWithRange:completeWordRange] isEqualToString:lastPresentedCompleteWordText.get()])
            [attributedString addAttribute:AccessibilityAcceptedInlineTextCompletion value:lastPresentedCompleteWordText.get() range:completeWordRange];
    }

    auto& lastPresentedTextPrediction = object->lastPresentedTextPrediction();
    unsigned lastPresentedLength = lastPresentedTextPrediction.text.length();
    unsigned lastPresentedPosition = lastPresentedTextPrediction.location;

    if (!lastPresentedTextPrediction.text.isEmpty() && lastPresentedPosition + lastPresentedLength <= [attributedString length]) {
        NSRange presentedRange = NSMakeRange(lastPresentedPosition, lastPresentedLength);
        if (![[attributedString.string substringWithRange:presentedRange] isEqualToString:lastPresentedTextPrediction.text.createNSString().get()])
            return;

        [attributedString addAttribute:AccessibilityInlineTextCompletion value:[attributedString.string substringWithRange:presentedRange] range:presentedRange];
    }
#else
    UNUSED_PARAM(attributedString);
    UNUSED_PARAM(renderer);
#endif // HAVE(INLINE_PREDICTIONS)
}

RetainPtr<NSAttributedString> attributedStringCreate(Node& node, StringView text, const SimpleRange&, AXCoreObject::SpellCheck)
{
    // Skip invisible text.
    CheckedPtr renderer = node.renderer();
    if (!renderer)
        return nil;

    auto result = adoptNS([[NSMutableAttributedString alloc] initWithString:text.createNSStringWithoutCopying().get()]);
    NSRange range = NSMakeRange(0, [result length]);

    // Set attributes.
    attributeStringSetStyle(result.get(), renderer.get(), range);
    attributeStringSetBlockquoteLevel(result.get(), renderer.get(), range);
    attributeStringSetLanguage(result.get(), renderer.get(), range);
    attributedStringSetCompositionAttributes(result.get(), renderer.get());

    return result;
}

namespace Accessibility {

RetainPtr<NSData> newAccessibilityRemoteToken(NSString *uuidString)
{
    if (!uuidString)
        return nil;
    return [NSKeyedArchiver archivedDataWithRootObject:@{ @"ax-pid" : @(getpid()), @"ax-uuid" : uuidString, @"ax-register" : @YES } requiringSecureCoding:YES error:nullptr];
}

}

} // WebCore

#endif // PLATFORM(IOS_FAMILY)
