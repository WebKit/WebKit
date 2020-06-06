/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#import "DOMDocumentFragmentInternal.h"
#import "DOMExtensions.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMHTMLDocumentInternal.h"
#import "DOMHTMLInputElementInternal.h"
#import "DOMHTMLSelectElementInternal.h"
#import "DOMHTMLTextAreaElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMPrivate.h"
#import <WebCore/DocumentFragment.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLCollection.h>
#import <WebCore/HTMLDocument.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLParserIdioms.h>
#import <WebCore/HTMLSelectElement.h>
#import <WebCore/HTMLTextAreaElement.h>
#import <WebCore/Range.h>
#import <WebCore/RenderTextControl.h>
#import <WebCore/Settings.h>
#import <WebCore/markup.h>

#if PLATFORM(IOS_FAMILY)
#import "DOMHTMLElementInternal.h"
#import <WebCore/Autocapitalize.h>
#import <WebCore/HTMLTextFormControlElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WebCoreThreadMessage.h>
#endif

// FIXME: We should move all these into the various specific element source files.
// These were originally here because they were hand written and the rest generated,
// but that is no longer true.

#if PLATFORM(IOS_FAMILY)

@implementation DOMHTMLElement (DOMHTMLElementExtensions)

- (int)scrollXOffset
{
    auto* renderer = core(self)->renderer();
    if (!renderer)
        return 0;

    if (!is<WebCore::RenderBlockFlow>(*renderer))
        renderer = renderer->containingBlock();

    if (!is<WebCore::RenderBox>(*renderer) || !renderer->hasOverflowClip())
        return 0;

    return downcast<WebCore::RenderBox>(*renderer).layer()->scrollOffset().x();
}

- (int)scrollYOffset
{
    auto* renderer = core(self)->renderer();
    if (!renderer)
        return 0;

    if (!is<WebCore::RenderBlockFlow>(*renderer))
        renderer = renderer->containingBlock();
    if (!is<WebCore::RenderBox>(*renderer) || !renderer->hasOverflowClip())
        return 0;

    return downcast<WebCore::RenderBox>(*renderer).layer()->scrollOffset().y();
}

- (void)setScrollXOffset:(int)x scrollYOffset:(int)y
{
    [self setScrollXOffset:x scrollYOffset:y adjustForIOSCaret:NO];
}

- (void)setScrollXOffset:(int)x scrollYOffset:(int)y adjustForIOSCaret:(BOOL)adjustForIOSCaret
{
    auto* renderer = core(self)->renderer();
    if (!renderer)
        return;

    if (!is<WebCore::RenderBlockFlow>(*renderer))
        renderer = renderer->containingBlock();
    if (!renderer->hasOverflowClip() || !is<WebCore::RenderBox>(*renderer))
        return;

    auto* layer = downcast<WebCore::RenderBox>(*renderer).layer();
    if (adjustForIOSCaret)
        layer->setAdjustForIOSCaretWhenScrolling(true);
    layer->scrollToOffset(WebCore::ScrollOffset(x, y), WebCore::ScrollType::Programmatic, WebCore::ScrollClamping::Unclamped);
    if (adjustForIOSCaret)
        layer->setAdjustForIOSCaretWhenScrolling(false);
}

- (void)absolutePosition:(int *)x :(int *)y :(int *)w :(int *)h
{
    auto* renderer = core(self)->renderBox();
    if (renderer) {
        if (w)
            *w = renderer->width();
        if (h)
            *h = renderer->width();
        if (x && y) {
            WebCore::FloatPoint floatPoint(*x, *y);
            renderer->localToAbsolute(floatPoint);
            WebCore::IntPoint point = roundedIntPoint(floatPoint);
            *x = point.x();
            *y = point.y();
        }
    }
}

@end

#endif // PLATFORM(IOS_FAMILY)

//------------------------------------------------------------------------------------------
// DOMHTMLDocument

@implementation DOMHTMLDocument (DOMHTMLDocumentExtensions)

- (DOMDocumentFragment *)createDocumentFragmentWithMarkupString:(NSString *)markupString baseURL:(NSURL *)baseURL
{
    return kit(createFragmentFromMarkup(*core(self), markupString, [baseURL absoluteString]).ptr());
}

- (DOMDocumentFragment *)createDocumentFragmentWithText:(NSString *)text
{
    // FIXME: Since this is not a contextual fragment, it won't handle whitespace properly.
    return kit(createFragmentFromText(core(self)->createRange(), text).ptr());
}

@end

@implementation DOMHTMLDocument (WebPrivate)

- (DOMDocumentFragment *)_createDocumentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString
{
    NSURL *baseURL = core(self)->completeURL(WebCore::stripLeadingAndTrailingHTMLSpaces(baseURLString));
    return [self createDocumentFragmentWithMarkupString:markupString baseURL:baseURL];
}

- (DOMDocumentFragment *)_createDocumentFragmentWithText:(NSString *)text
{
    return [self createDocumentFragmentWithText:text];
}

@end

@implementation DOMHTMLInputElement (FormAutoFillTransition)

- (BOOL)_isTextField
{
    return core(self)->isTextField();
}

@end

@implementation DOMHTMLSelectElement (FormAutoFillTransition)

- (void)_activateItemAtIndex:(int)index
{
    // Use the setSelectedIndexByUser function so a change event will be fired. <rdar://problem/6760590>
    if (WebCore::HTMLSelectElement* select = core(self))
        select->optionSelectedByUser(index, true);
}

- (void)_activateItemAtIndex:(int)index allowMultipleSelection:(BOOL)allowMultipleSelection
{
    // Use the setSelectedIndexByUser function so a change event will be fired. <rdar://problem/6760590>
    // If this is a <select multiple> the allowMultipleSelection flag will allow setting multiple
    // selections without clearing the other selections.
    if (WebCore::HTMLSelectElement* select = core(self))
        select->optionSelectedByUser(index, true, allowMultipleSelection);
}

@end

#if PLATFORM(IOS_FAMILY)

@implementation DOMHTMLInputElement (FormPromptAdditions)

- (BOOL)_isEdited
{
    return core(self)->lastChangeWasUserEdit();
}

@end

@implementation DOMHTMLTextAreaElement (FormPromptAdditions)

- (BOOL)_isEdited
{
    return core(self)->lastChangeWasUserEdit();
}

@end

static WebAutocapitalizeType webAutocapitalizeType(WebCore::AutocapitalizeType type)
{
    switch (type) {
    case WebCore::AutocapitalizeType::Default:
        return WebAutocapitalizeTypeDefault;
    case WebCore::AutocapitalizeType::None:
        return WebAutocapitalizeTypeNone;
    case WebCore::AutocapitalizeType::Words:
        return WebAutocapitalizeTypeWords;
    case WebCore::AutocapitalizeType::Sentences:
        return WebAutocapitalizeTypeSentences;
    case WebCore::AutocapitalizeType::AllCharacters:
        return WebAutocapitalizeTypeAllCharacters;
    }
}

@implementation DOMHTMLInputElement (AutocapitalizeAdditions)

- (WebAutocapitalizeType)_autocapitalizeType
{
    WebCore::HTMLInputElement* inputElement = core(self);
    return webAutocapitalizeType(inputElement->autocapitalizeType());
}

@end

@implementation DOMHTMLTextAreaElement (AutocapitalizeAdditions)

- (WebAutocapitalizeType)_autocapitalizeType
{
    WebCore::HTMLTextAreaElement* textareaElement = core(self);
    return webAutocapitalizeType(textareaElement->autocapitalizeType());
}

@end

@implementation DOMHTMLInputElement (WebInputChangeEventAdditions)

- (void)setValueWithChangeEvent:(NSString *)newValue
{
    WebCore::JSMainThreadNullState state;
    core(self)->setValue(newValue, WebCore::DispatchInputAndChangeEvent);
}

- (void)setValueAsNumberWithChangeEvent:(double)newValueAsNumber
{
    WebCore::JSMainThreadNullState state;
    core(self)->setValueAsNumber(newValueAsNumber, WebCore::DispatchInputAndChangeEvent);
}

@end

#endif

Class kitClass(WebCore::HTMLCollection* collection)
{
    if (collection->type() == WebCore::SelectOptions)
        return [DOMHTMLOptionsCollection class];
    return [DOMHTMLCollection class];
}
