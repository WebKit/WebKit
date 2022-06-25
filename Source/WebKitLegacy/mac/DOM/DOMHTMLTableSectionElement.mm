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

#import "DOMHTMLTableSectionElementInternal.h"

#import "DOMHTMLCollectionInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLCollection.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLTableSectionElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLTableSectionElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLTableSectionElement

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

- (NSString *)ch
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::charAttr);
}

- (void)setCh:(NSString *)newCh
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::charAttr, newCh);
}

- (NSString *)chOff
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::charoffAttr);
}

- (void)setChOff:(NSString *)newChOff
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::charoffAttr, newChOff);
}

- (NSString *)vAlign
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::valignAttr);
}

- (void)setVAlign:(NSString *)newVAlign
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::valignAttr, newVAlign);
}

- (DOMHTMLCollection *)rows
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->rows()));
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

WebCore::HTMLTableSectionElement* core(DOMHTMLTableSectionElement *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::HTMLTableSectionElement*>(wrapper->_internal) : 0;
}

DOMHTMLTableSectionElement *kit(WebCore::HTMLTableSectionElement* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMHTMLTableSectionElement*>(kit(static_cast<WebCore::Node*>(value)));
}

#undef IMPL
