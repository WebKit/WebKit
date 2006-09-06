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
#import "HTMLEmbedElement.h"
#import "HTMLFormElement.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "HTMLObjectElement.h"
#import "HTMLOptionsCollection.h"
#import "HTMLSelectElement.h"
#import "HTMLTableCaptionElement.h"
#import "HTMLTableCellElement.h"
#import "HTMLTableElement.h"
#import "HTMLTableSectionElement.h"
#import "KURL.h"
#import "NameNodeList.h"
#import "Range.h"
#import "RenderTextControl.h"
#import "csshelper.h"
#import "markup.h"

using namespace WebCore;
using namespace HTMLNames;


//------------------------------------------------------------------------------------------
// DOMHTMLCollection

@implementation DOMHTMLCollection (WebCoreInternal)

- (id)_initWithCollection:(HTMLCollection *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = DOM_cast<DOMObjectInternal*>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMHTMLCollection *)_HTMLCollectionWith:(HTMLCollection *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithCollection:impl] autorelease];
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLOptionsCollection

@implementation DOMHTMLOptionsCollection (WebCoreInternal)

- (id)_initWithOptionsCollection:(HTMLOptionsCollection *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = DOM_cast<DOMObjectInternal*>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMHTMLOptionsCollection *)_HTMLOptionsCollectionWith:(HTMLOptionsCollection *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithOptionsCollection:impl] autorelease];
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLElement

@implementation DOMHTMLElement (WebCoreInternal)

+ (DOMHTMLElement *)_HTMLElementWith:(HTMLElement *)impl
{
    return static_cast<DOMHTMLElement*>([DOMNode _nodeWith:impl]);
}

- (HTMLElement *)_HTMLElement
{
    return static_cast<HTMLElement*>(DOM_cast<Node*>(_internal));
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLDocument

@implementation DOMHTMLDocument (WebPrivate)

- (DOMDocumentFragment *)_createDocumentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString
{
    NSURL *baseURL = KURL([self _document]->completeURL(parseURL(baseURLString)).deprecatedString()).getNSURL();
    return [self createDocumentFragmentWithMarkupString:markupString baseURL:baseURL];
}

- (DOMDocumentFragment *)_createDocumentFragmentWithText:(NSString *)text
{
    return [self createDocumentFragmentWithText:text];
}

@end

@implementation DOMHTMLDocument (WebCoreInternal)

+ (DOMHTMLDocument *)_HTMLDocumentWith:(WebCore::HTMLDocument *)impl;
{
    return static_cast<DOMHTMLDocument *>([DOMNode _nodeWith:impl]);
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLFormElement

@implementation DOMHTMLFormElement (WebCoreInternal)

+ (DOMHTMLFormElement *)_HTMLFormElementWith:(HTMLFormElement *)impl
{
    return static_cast<DOMHTMLFormElement*>([DOMNode _nodeWith:impl]);
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

@implementation DOMHTMLInputElement (WebCoreInternal)

- (WebCore::HTMLInputElement *)_HTMLInputElement
{
    return static_cast<WebCore::HTMLInputElement *>(DOM_cast<WebCore::Node *>(_internal));
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLImageElement

@implementation DOMHTMLImageElement (WebCoreInternal)

- (WebCore::HTMLImageElement *)_HTMLImageElement
{
    return static_cast<WebCore::HTMLImageElement*>(DOM_cast<WebCore::Node*>(_internal));
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

@implementation DOMHTMLObjectElement (WebCoreInternal)

- (WebCore::HTMLObjectElement *)_HTMLObjectElement
{
    return static_cast<WebCore::HTMLObjectElement*>(DOM_cast<WebCore::Node*>(_internal));
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLTableCaptionElement

@implementation DOMHTMLTableCaptionElement (WebCoreInternal)

+ (DOMHTMLTableCaptionElement *)_HTMLTableCaptionElementWith:(HTMLTableCaptionElement *)impl
{
    return static_cast<DOMHTMLTableCaptionElement*>([DOMNode _nodeWith:impl]);
}

- (HTMLTableCaptionElement *)_HTMLTableCaptionElement
{
    return static_cast<HTMLTableCaptionElement*>(DOM_cast<Node*>(_internal));
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLTableSectionElement

@implementation DOMHTMLTableSectionElement (WebCoreInternal)

+ (DOMHTMLTableSectionElement *)_HTMLTableSectionElementWith:(HTMLTableSectionElement *)impl
{
    return static_cast<DOMHTMLTableSectionElement*>([DOMNode _nodeWith:impl]);
}

- (HTMLTableSectionElement *)_HTMLTableSectionElement
{
    return static_cast<HTMLTableSectionElement*>(DOM_cast<Node*>(_internal));
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLTableElement

@implementation DOMHTMLTableElement (WebCoreInternal)

+ (DOMHTMLTableElement *)_HTMLTableElementWith:(HTMLTableElement *)impl
{
    return static_cast<DOMHTMLTableElement*>([DOMNode _nodeWith:impl]);
}

- (HTMLTableElement *)_HTMLTableElement
{
    return static_cast<HTMLTableElement*>(DOM_cast<Node*>(_internal));
}

@end


//------------------------------------------------------------------------------------------
// DOMHTMLTableCellElement

@implementation DOMHTMLTableCellElement (WebCoreInternal)

+ (DOMHTMLTableCellElement *)_HTMLTableCellElementWith:(HTMLTableCellElement *)impl
{
    return static_cast<DOMHTMLTableCellElement*>([DOMNode _nodeWith:impl]);
}

- (HTMLTableCellElement *)_HTMLTableCellElement
{
    return static_cast<HTMLTableCellElement*>(DOM_cast<Node*>(_internal));
}

@end


#pragma mark DOM EXTENSIONS


//------------------------------------------------------------------------------------------
// DOMHTMLEmbedElement

@implementation DOMHTMLEmbedElement

- (HTMLEmbedElement *)_HTMLEmbedElement
{
    return static_cast<HTMLEmbedElement*>(DOM_cast<Node*>(_internal));
}

- (NSString *)align
{
    return [self _HTMLEmbedElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _HTMLEmbedElement]->setAttribute(alignAttr, align);
}

- (int)height
{
    return [self _HTMLEmbedElement]->getAttribute(heightAttr).toInt();
}

- (void)setHeight:(int)height
{
    [self _HTMLEmbedElement]->setAttribute(heightAttr, String::number(height));
}

- (NSString *)name
{
    return [self _HTMLEmbedElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _HTMLEmbedElement]->setAttribute(nameAttr, name);
}

- (NSString *)src
{
    return [self _HTMLEmbedElement]->getAttribute(srcAttr);
}

- (void)setSrc:(NSString *)src
{
    [self _HTMLEmbedElement]->setAttribute(srcAttr, src);
}

- (NSString *)type
{
    return [self _HTMLEmbedElement]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _HTMLEmbedElement]->setAttribute(typeAttr, type);
}

- (int)width
{
    return [self _HTMLEmbedElement]->getAttribute(widthAttr).toInt();
}

- (void)setWidth:(int)width
{
    [self _HTMLEmbedElement]->setAttribute(widthAttr, String::number(width));
}

@end

// These #imports and "usings" are used only by viewForElement and should be deleted 
// when that function goes away.
#import "RenderWidget.h"
using WebCore::RenderObject;
using WebCore::RenderWidget;

// This function is used only by the two FormAutoFillTransition categories, and will go away
// as soon as possible.
static NSView *viewForElement(DOMElement *element)
{
    RenderObject *renderer = [element _element]->renderer();
    if (renderer && renderer->isWidget()) {
        Widget *widget = static_cast<const RenderWidget*>(renderer)->widget();
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
    HTMLInputElement* inputElement = [self _HTMLInputElement];
    if (inputElement) {
        String newValue = inputElement->value().replace(targetRange.location, targetRange.length, replacementString);
        inputElement->setValue(newValue);
        inputElement->setSelectionRange(index, newValue.length());
    }
}

- (NSRange)_selectedRange
{
    HTMLInputElement* inputElement = [self _HTMLInputElement];
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
    HTMLInputElement* inputElement = [self _HTMLInputElement];
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
