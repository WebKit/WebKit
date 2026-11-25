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

#import "DOMHTMLLinkElementInternal.h"

#import "DOMNodeInternal.h"
#import "DOMPrivate.h"
#import "DOMStyleSheetInternal.h"
#import "DOMTokenListInternal.h"
#import <WebCore/DOMTokenList.h>
#import "ExceptionHandlers.h"
#import <WebCore/ElementInlines.h>
#import <WebCore/HTMLLinkElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/StyleSheet.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL protect(downcast<WebCore::HTMLLinkElement>(reinterpret_cast<WebCore::Node*>(_internal)))

@implementation DOMHTMLLinkElement

- (BOOL)disabled
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr);
}

- (void)setDisabled:(BOOL)newDisabled
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::disabledAttr, newDisabled);
}

- (NSString *)charset
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::charsetAttr)).autorelease();
}

- (void)setCharset:(NSString *)newCharset
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::charsetAttr, AtomStringImpl::add((__bridge CFStringRef)newCharset).get());
}

- (NSString *)href
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getURLAttribute(WebCore::HTMLNames::hrefAttr).string()).autorelease();
}

- (void)setHref:(NSString *)newHref
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::hrefAttr, AtomStringImpl::add((__bridge CFStringRef)newHref).get());
}

- (NSString *)hreflang
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::hreflangAttr)).autorelease();
}

- (void)setHreflang:(NSString *)newHreflang
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::hreflangAttr, AtomStringImpl::add((__bridge CFStringRef)newHreflang).get());
}

- (NSString *)media
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::mediaAttr)).autorelease();
}

- (void)setMedia:(NSString *)newMedia
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::mediaAttr, AtomStringImpl::add((__bridge CFStringRef)newMedia).get());
}

- (NSString *)rel
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::relAttr)).autorelease();
}

- (void)setRel:(NSString *)newRel
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::relAttr, AtomStringImpl::add((__bridge CFStringRef)newRel).get());
}

- (NSString *)rev
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::revAttr)).autorelease();
}

- (void)setRev:(NSString *)newRev
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::revAttr, AtomStringImpl::add((__bridge CFStringRef)newRev).get());
}

- (NSString *)target
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::targetAttr)).autorelease();
}

- (void)setTarget:(NSString *)newTarget
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::targetAttr, AtomStringImpl::add((__bridge CFStringRef)newTarget).get());
}

- (NSString *)type
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::typeAttr)).autorelease();
}

- (void)setType:(NSString *)newType
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::typeAttr, AtomStringImpl::add((__bridge CFStringRef)newType).get());
}

- (NSString *)as
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::asAttr)).autorelease();
}

- (void)setAs:(NSString *)newAs
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::asAttr, AtomStringImpl::add((__bridge CFStringRef)newAs).get());
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

- (DOMStyleSheet *)sheet
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->sheet()));
}

- (NSURL *)absoluteLinkURL
{
    WebCore::JSMainThreadNullState state;
    return [self _getURLAttribute:@"href"];
}

- (DOMTokenList *)relList
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->relList()));
}

@end

WebCore::HTMLLinkElement* core(DOMHTMLLinkElement *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::HTMLLinkElement*>(wrapper->_internal) : 0;
}

#undef IMPL
