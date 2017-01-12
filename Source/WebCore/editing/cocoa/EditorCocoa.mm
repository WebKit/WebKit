/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
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
#import "Editor.h"

#import "ArchiveResource.h"
#import "CSSValueList.h"
#import "CSSValuePool.h"
#import "DocumentFragment.h"
#import "EditingStyle.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "HTMLSpanElement.h"
#import "NSAttributedStringSPI.h"
#import "RenderElement.h"
#import "RenderStyle.h"
#import "SoftLinking.h"
#import "Text.h"
#import "htmlediting.h"

#if PLATFORM(IOS)
SOFT_LINK_PRIVATE_FRAMEWORK(WebKitLegacy)
#endif

#if PLATFORM(MAC)
SOFT_LINK_FRAMEWORK_IN_UMBRELLA(WebKit, WebKitLegacy)
#endif

SOFT_LINK(WebKitLegacy, _WebCreateFragment, void, (WebCore::Document& document, NSAttributedString *string, WebCore::FragmentAndResources& result), (document, string, result))

namespace WebCore {

// FIXME: This figures out the current style by inserting a <span>!
const RenderStyle* Editor::styleForSelectionStart(Frame* frame, Node*& nodeToRemove)
{
    nodeToRemove = nullptr;
    
    if (frame->selection().isNone())
        return nullptr;

    Position position = adjustedSelectionStartForStyleComputation(frame->selection().selection());
    if (!position.isCandidate() || position.isNull())
        return nullptr;

    RefPtr<EditingStyle> typingStyle = frame->selection().typingStyle();
    if (!typingStyle || !typingStyle->style())
        return &position.deprecatedNode()->renderer()->style();

    auto styleElement = HTMLSpanElement::create(*frame->document());

    String styleText = typingStyle->style()->asText() + " display: inline";
    styleElement->setAttribute(HTMLNames::styleAttr, styleText);

    styleElement->appendChild(frame->document()->createEditingTextNode(emptyString()));

    auto positionNode = position.deprecatedNode();
    if (!positionNode || !positionNode->parentNode() || positionNode->parentNode()->appendChild(styleElement).hasException())
        return nullptr;

    nodeToRemove = styleElement.ptr();

    frame->document()->updateStyleIfNeeded();
    return styleElement->renderer() ? &styleElement->renderer()->style() : nullptr;
}

void Editor::getTextDecorationAttributesRespectingTypingStyle(const RenderStyle& style, NSMutableDictionary* result) const
{
    RefPtr<EditingStyle> typingStyle = m_frame.selection().typingStyle();
    if (typingStyle && typingStyle->style()) {
        RefPtr<CSSValue> value = typingStyle->style()->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
        if (value && value->isValueList()) {
            CSSValueList& valueList = downcast<CSSValueList>(*value);
            if (valueList.hasValue(CSSValuePool::singleton().createIdentifierValue(CSSValueLineThrough).ptr()))
                [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];
            if (valueList.hasValue(CSSValuePool::singleton().createIdentifierValue(CSSValueUnderline).ptr()))
                [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
        }
    } else {
        int decoration = style.textDecorationsInEffect();
        if (decoration & TextDecorationLineThrough)
            [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];
        if (decoration & TextDecorationUnderline)
            [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
    }
}

FragmentAndResources Editor::createFragment(NSAttributedString *string)
{
    // FIXME: The algorithm to convert an attributed string into HTML should be implemented here in WebCore.
    // For now, though, we call into WebKitLegacy, which in turn calls into AppKit/TextKit.
    FragmentAndResources result;
    _WebCreateFragment(*m_frame.document(), string, result);
    return result;
}

}
