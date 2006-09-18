/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "DOMHTML.h"

#import "DOMExtensions.h"
#import "DOMInternal.h"
#import "DOMPrivate.h"
#import "DocumentFragment.h"
#import "FoundationExtras.h"
#import "FrameView.h"
#import "HTMLDocument.h"
#import "HTMLInputElement.h"
#import "HTMLObjectElement.h"
#import "KURL.h"
#import "Range.h"
#import "RenderTextControl.h"
#import "csshelper.h"
#import "markup.h"

//------------------------------------------------------------------------------------------
// DOMHTMLDocument

@implementation DOMHTMLDocument (DOMHTMLDocumentExtensions)

- (DOMDocumentFragment *)createDocumentFragmentWithMarkupString:(NSString *)markupString baseURL:(NSURL *)baseURL
{
    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromMarkup([self _document], markupString, [baseURL absoluteString]).get()];
}

- (DOMDocumentFragment *)createDocumentFragmentWithText:(NSString *)text
{
    // FIXME: Since this is not a contextual fragment, it won't handle whitespace properly.
    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromText([self _document]->createRange().get(), text).get()];
}

@end

@implementation DOMHTMLDocument (WebPrivate)

- (DOMDocumentFragment *)_createDocumentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString
{
    NSURL *baseURL = WebCore::KURL([self _document]->completeURL(WebCore::parseURL(baseURLString)).deprecatedString()).getNSURL();
    return [self createDocumentFragmentWithMarkupString:markupString baseURL:baseURL];
}

- (DOMDocumentFragment *)_createDocumentFragmentWithText:(NSString *)text
{
    return [self createDocumentFragmentWithText:text];
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLInputElement

@implementation DOMHTMLInputElement (DOMHTMLInputElementExtensions)

- (NSURL *)absoluteImageURL
{
    if (![self _HTMLInputElement]->renderer() || ![self _HTMLInputElement]->renderer()->isImage())
        return nil;
    return [self _getURLAttribute:@"src"];
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLObjectElement

@implementation DOMHTMLObjectElement (DOMHTMLObjectElementExtensions)

- (NSURL *)absoluteImageURL
{
    if (![self _HTMLObjectElement]->renderer() || ![self _HTMLObjectElement]->renderer()->isImage())
        return nil;
    return [self _getURLAttribute:@"data"];
}

@end


#pragma mark DOM EXTENSIONS

// This #import is used only by viewForElement and should be deleted 
// when that function goes away.
#import "RenderWidget.h"

// This function is used only by the two FormAutoFillTransition categories, and will go away
// as soon as possible.
static NSView *viewForElement(DOMElement *element)
{
    WebCore::RenderObject *renderer = [element _element]->renderer();
    if (renderer && renderer->isWidget()) {
        WebCore::Widget *widget = static_cast<const WebCore::RenderWidget*>(renderer)->widget();
        if (widget) {
            widget->populate();
            return widget->getView();
        }
    }
    return nil;
}

@implementation DOMHTMLInputElement(FormAutoFillTransition)

- (BOOL)_isTextField
{
    // We could make this public API as-is, or we could change it into a method that returns whether
    // the element is a text field or a button or ... ?
    static NSArray *textInputTypes = nil;
#ifndef NDEBUG
    static NSArray *nonTextInputTypes = nil;
#endif
    
    NSString *fieldType = [self type];
    
    // No type at all is treated as text type
    if ([fieldType length] == 0)
        return YES;
    
    if (textInputTypes == nil)
        textInputTypes = [[NSSet alloc] initWithObjects:@"text", @"password", @"search", @"isindex", nil];
    
    BOOL isText = [textInputTypes containsObject:[fieldType lowercaseString]];
    
#ifndef NDEBUG
    if (nonTextInputTypes == nil)
        nonTextInputTypes = [[NSSet alloc] initWithObjects:@"checkbox", @"radio", @"submit", @"reset", @"file", @"hidden", @"image", @"button", @"range", nil];
    
    // Catch cases where a new input type has been added that's not in these lists.
    ASSERT(isText || [nonTextInputTypes containsObject:[fieldType lowercaseString]]);
#endif    
    
    return isText;
}

- (NSRect)_rectOnScreen
{
    // Returns bounding rect of text field, in screen coordinates.
    NSView* view = [self _HTMLInputElement]->document()->view()->getDocumentView();
    NSRect result = [self boundingBox];
    result = [view convertRect:result toView:nil];
    result.origin = [[view window] convertBaseToScreen:result.origin];
    return result;
}

- (void)_replaceCharactersInRange:(NSRange)targetRange withString:(NSString *)replacementString selectingFromIndex:(int)index
{
    WebCore::HTMLInputElement* inputElement = [self _HTMLInputElement];
    if (inputElement) {
        WebCore::String newValue = inputElement->value().replace(targetRange.location, targetRange.length, replacementString);
        inputElement->setValue(newValue);
        inputElement->setSelectionRange(index, newValue.length());
    }
}

- (NSRange)_selectedRange
{
    WebCore::HTMLInputElement* inputElement = [self _HTMLInputElement];
    if (inputElement) {
        int start = inputElement->selectionStart();
        int end = inputElement->selectionEnd();
        return NSMakeRange(start, end - start); 
    }
    return NSMakeRange(NSNotFound, 0);
}    

- (void)_setAutofilled:(BOOL)filled
{
    // This notifies the input element that the content has been autofilled
    // This allows WebKit to obey the -webkit-autofill pseudo style, which
    // changes the background color.
    WebCore::HTMLInputElement* inputElement = [self _HTMLInputElement];
    if (inputElement)
        inputElement->setAutofilled(filled);
}

@end

@implementation DOMHTMLSelectElement(FormAutoFillTransition)

- (void)_activateItemAtIndex:(int)index
{
    NSPopUpButton *popUp = (NSPopUpButton *)viewForElement(self);
    [popUp selectItemAtIndex:index];
    // Must do this to simulate same side effect as if user made the choice
    [NSApp sendAction:[popUp action] to:[popUp target] from:popUp];
}

@end
