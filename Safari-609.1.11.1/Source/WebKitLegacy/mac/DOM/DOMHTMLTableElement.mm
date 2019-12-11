/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#import "DOMHTMLTableElement.h"

#import "DOMHTMLCollectionInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMHTMLTableCaptionElementInternal.h"
#import "DOMHTMLTableSectionElementInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLCollection.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLTableCaptionElement.h>
#import <WebCore/HTMLTableElement.h>
#import <WebCore/HTMLTableSectionElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLTableElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLTableElement

- (DOMHTMLTableCaptionElement *)caption
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->caption()));
}

- (void)setCaption:(DOMHTMLTableCaptionElement *)newCaption
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setCaption(core(newCaption)));
}

- (DOMHTMLTableSectionElement *)tHead
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->tHead()));
}

- (void)setTHead:(DOMHTMLTableSectionElement *)newTHead
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setTHead(core(newTHead)));
}

- (DOMHTMLTableSectionElement *)tFoot
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->tFoot()));
}

- (void)setTFoot:(DOMHTMLTableSectionElement *)newTFoot
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setTFoot(core(newTFoot)));
}

- (DOMHTMLCollection *)rows
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->rows()));
}

- (DOMHTMLCollection *)tBodies
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->tBodies()));
}

- (NSString *)align
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::alignAttr);
}

- (void)setAlign:(NSString *)newAlign
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, newAlign);
}

- (NSString *)bgColor
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::bgcolorAttr);
}

- (void)setBgColor:(NSString *)newBgColor
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::bgcolorAttr, newBgColor);
}

- (NSString *)border
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::borderAttr);
}

- (void)setBorder:(NSString *)newBorder
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::borderAttr, newBorder);
}

- (NSString *)cellPadding
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::cellpaddingAttr);
}

- (void)setCellPadding:(NSString *)newCellPadding
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::cellpaddingAttr, newCellPadding);
}

- (NSString *)cellSpacing
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::cellspacingAttr);
}

- (void)setCellSpacing:(NSString *)newCellSpacing
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::cellspacingAttr, newCellSpacing);
}

- (NSString *)frameBorders
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::frameAttr);
}

- (void)setFrameBorders:(NSString *)newFrameBorders
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::frameAttr, newFrameBorders);
}

- (NSString *)rules
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::rulesAttr);
}

- (void)setRules:(NSString *)newRules
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::rulesAttr, newRules);
}

- (NSString *)summary
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::summaryAttr);
}

- (void)setSummary:(NSString *)newSummary
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::summaryAttr, newSummary);
}

- (NSString *)width
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::widthAttr);
}

- (void)setWidth:(NSString *)newWidth
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::widthAttr, newWidth);
}

- (DOMHTMLElement *)createTHead
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createTHead()));
}

- (void)deleteTHead
{
    WebCore::JSMainThreadNullState state;
    IMPL->deleteTHead();
}

- (DOMHTMLElement *)createTFoot
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createTFoot()));
}

- (void)deleteTFoot
{
    WebCore::JSMainThreadNullState state;
    IMPL->deleteTFoot();
}

- (DOMHTMLElement *)createTBody
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createTBody()));
}

- (DOMHTMLElement *)createCaption
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createCaption()));
}

- (void)deleteCaption
{
    WebCore::JSMainThreadNullState state;
    IMPL->deleteCaption();
}

- (DOMHTMLElement *)insertRow:(int)index
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->insertRow(index)).ptr());
}

- (void)deleteRow:(int)index
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->deleteRow(index));
}

@end

#undef IMPL
