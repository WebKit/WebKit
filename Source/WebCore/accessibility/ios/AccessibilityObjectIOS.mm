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
#import <wtf/cocoa/TypeCastsCocoa.h>

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

void AccessibilityObject::overrideAttachmentParent(AXCoreObject*)
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
    auto touchEventNames = eventNames().touchRelatedEventNames();
    // If the node is in a shadowRoot, going up the node parent tree will stop and
    // not check the entire chain of ancestors. Thus, use the parentInComposedTree instead.
    for (auto* node = this->node(); node; node = node->parentInComposedTree()) {
        for (auto eventName : touchEventNames) {
            if (node->hasEventListeners(eventName))
                return true;
        }
    }
    return false;
}

bool AccessibilityObject::isInputTypePopupButton() const
{
    if (is<HTMLInputElement>(node()))
        return roleValue() == AccessibilityRole::PopUpButton;
    return false;
}

// NSAttributedString support.

static void attributeStringSetLanguage(NSMutableAttributedString *attrString, RenderObject* renderer, const NSRange& range)
{
    if (!renderer)
        return;

    RefPtr object = renderer->document().axObjectCache()->getOrCreate(renderer);
    NSString *language = object->language();
    if (language.length)
        [attrString addAttribute:UIAccessibilityTokenLanguage value:language range:range];
    else
        [attrString removeAttribute:UIAccessibilityTokenLanguage range:range];
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
        [attrString addAttribute:UIAccessibilityTokenBlockquoteLevel value:@(quoteLevel) range:range];
    else
        [attrString removeAttribute:UIAccessibilityTokenBlockquoteLevel range:range];
}

static void attributeStringSetHeadingLevel(NSMutableAttributedString *attrString, RenderObject* renderer, const NSRange& range)
{
    if (!renderer)
        return;

    RefPtr parent = renderer->document().axObjectCache()->getOrCreate(renderer->parent());
    if (!parent)
        return;

    unsigned parentHeadingLevel = parent->headingLevel();
    if (parentHeadingLevel)
        [attrString addAttribute:UIAccessibilityTokenHeadingLevel value:@(parentHeadingLevel) range:range];
    else
        [attrString removeAttribute:UIAccessibilityTokenHeadingLevel range:range];
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
        attributedStringSetNumber(attrString, UIAccessibilityTokenUnderline, @YES, range);

    // Add code context if this node is within a <code> block.
    RefPtr object = renderer->document().axObjectCache()->getOrCreate(renderer);
    auto matchFunc = [] (const auto& axObject) {
        return axObject.isCode();
    };

    if (const auto* parent = Accessibility::findAncestor<AccessibilityObject>(*object, true, WTFMove(matchFunc)))
        [attrString addAttribute:UIAccessibilityTextAttributeContext value:UIAccessibilityTextualContextSourceCode range:range];
}

RetainPtr<NSAttributedString> attributedStringCreate(Node* node, StringView text, AXCoreObject::SpellCheck)
{
    // Skip invisible text.
    auto* renderer = node->renderer();
    if (!renderer)
        return nil;

    auto result = adoptNS([[NSMutableAttributedString alloc] initWithString:text.createNSStringWithoutCopying().get()]);
    NSRange range = NSMakeRange(0, [result length]);

    // Set attributes.
    attributeStringSetStyle(result.get(), renderer, range);
    attributeStringSetHeadingLevel(result.get(), renderer, range);
    attributeStringSetBlockquoteLevel(result.get(), renderer, range);
    attributeStringSetLanguage(result.get(), renderer, range);

    return result;
}

} // WebCore

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(IOS_FAMILY)
