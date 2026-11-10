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

#import "DOMHTMLTableRowElement.h"

#import "DOMHTMLCollectionInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLCollection.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLTableRowElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL protect(downcast<WebCore::HTMLTableRowElement>(reinterpret_cast<WebCore::Node*>(_internal)))

@implementation DOMHTMLTableRowElement

- (int)rowIndex
{
    WebCore::JSMainThreadNullState state;
    return IMPL->rowIndex();
}

- (int)sectionRowIndex
{
    WebCore::JSMainThreadNullState state;
    return IMPL->sectionRowIndex();
}

- (DOMHTMLCollection *)cells
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->cells()));
}

- (NSString *)align
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::alignAttr)).autorelease();
}

- (void)setAlign:(NSString *)newAlign
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, AtomStringImpl::add((__bridge CFStringRef)newAlign).get());
}

- (NSString *)bgColor
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::bgcolorAttr)).autorelease();
}

- (void)setBgColor:(NSString *)newBgColor
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::bgcolorAttr, AtomStringImpl::add((__bridge CFStringRef)newBgColor).get());
}

- (NSString *)ch
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::charAttr)).autorelease();
}

- (void)setCh:(NSString *)newCh
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::charAttr, AtomStringImpl::add((__bridge CFStringRef)newCh).get());
}

- (NSString *)chOff
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::charoffAttr)).autorelease();
}

- (void)setChOff:(NSString *)newChOff
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::charoffAttr, AtomStringImpl::add((__bridge CFStringRef)newChOff).get());
}

- (NSString *)vAlign
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::valignAttr)).autorelease();
}

- (void)setVAlign:(NSString *)newVAlign
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::valignAttr, AtomStringImpl::add((__bridge CFStringRef)newVAlign).get());
}

- (DOMHTMLElement *)insertCell:(int)index
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->insertCell(index)).ptr());
}

- (void)deleteCell:(int)index
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->deleteCell(index));
}

@end

#undef IMPL
