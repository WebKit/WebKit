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

#import "DOMHTMLScriptElementInternal.h"

#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/ElementInlines.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLScriptElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL protect(downcast<WebCore::HTMLScriptElement>(reinterpret_cast<WebCore::Node*>(_internal)))

@implementation DOMHTMLScriptElement

- (NSString *)text
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->text()).autorelease();
}

- (void)setText:(NSString *)newText
{
    WebCore::JSMainThreadNullState state;
    IMPL->setText(StringImpl::create(newText));
}

- (NSString *)htmlFor
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::forAttr)).autorelease();
}

- (void)setHtmlFor:(NSString *)newHtmlFor
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::forAttr, AtomStringImpl::add((__bridge CFStringRef)newHtmlFor).get());
}

- (NSString *)event
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::eventAttr)).autorelease();
}

- (void)setEvent:(NSString *)newEvent
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::eventAttr, AtomStringImpl::add((__bridge CFStringRef)newEvent).get());
}

- (NSString *)charset
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::charsetAttr)).autorelease();
}

- (void)setCharset:(NSString *)newCharset
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::charsetAttr, AtomStringImpl::add((__bridge CFStringRef)newCharset).get());
}

- (BOOL)async
{
    WebCore::JSMainThreadNullState state;
    return IMPL->async();
}

- (void)setAsync:(BOOL)newAsync
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAsync(newAsync);
}

- (BOOL)defer
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::deferAttr);
}

- (void)setDefer:(BOOL)newDefer
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::deferAttr, newDefer);
}

- (NSString *)src
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getURLAttribute(WebCore::HTMLNames::srcAttr).string()).autorelease();
}

- (void)setSrc:(NSString *)newSrc
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::srcAttr, AtomStringImpl::add((__bridge CFStringRef)newSrc).get());
}

- (NSString *)type
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::typeAttr)).autorelease();
}

- (void)setType:(NSString *)newType
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::typeAttr, AtomStringImpl::add((__bridge CFStringRef)newType).get());
}

- (NSString *)crossOrigin
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->crossOrigin()).autorelease();
}

- (void)setCrossOrigin:(NSString *)newCrossOrigin
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::crossoriginAttr, AtomStringImpl::add((__bridge CFStringRef)newCrossOrigin).get());
}

- (NSString *)nonce
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::nonceAttr)).autorelease();
}

- (void)setNonce:(NSString *)newNonce
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::nonceAttr, AtomStringImpl::add((__bridge CFStringRef)newNonce).get());
}

@end

SUPPRESS_NODELETE DOMHTMLScriptElement *kit(WebCore::HTMLScriptElement* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMHTMLScriptElement*>(kit(static_cast<WebCore::Node*>(value)));
}

#undef IMPL
