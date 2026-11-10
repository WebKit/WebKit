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

#import "DOMHTMLAnchorElement.h"

#import "DOMNodeInternal.h"
#import "DOMPrivate.h"
#import "DOMTokenListInternal.h"
#import <WebCore/DOMTokenList.h>
#import "ExceptionHandlers.h"
#import <WebCore/ElementInlines.h>
#import <WebCore/HTMLAnchorElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL protect(downcast<WebCore::HTMLAnchorElement>(reinterpret_cast<WebCore::Node*>(_internal)))

@implementation DOMHTMLAnchorElement

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

- (NSString *)coords
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::coordsAttr)).autorelease();
}

- (void)setCoords:(NSString *)newCoords
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::coordsAttr, AtomStringImpl::add((__bridge CFStringRef)newCoords).get());
}

- (NSString *)download
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::downloadAttr)).autorelease();
}

- (void)setDownload:(NSString *)newDownload
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::downloadAttr, AtomStringImpl::add((__bridge CFStringRef)newDownload).get());
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

- (NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getNameAttribute()).autorelease();
}

- (void)setName:(NSString *)newName
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, AtomStringImpl::add((__bridge CFStringRef)newName).get());
}

- (NSString *)ping
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::pingAttr)).autorelease();
}

- (void)setPing:(NSString *)newPing
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::pingAttr, AtomStringImpl::add((__bridge CFStringRef)newPing).get());
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

- (NSString *)shape
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::shapeAttr)).autorelease();
}

- (void)setShape:(NSString *)newShape
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::shapeAttr, AtomStringImpl::add((__bridge CFStringRef)newShape).get());
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

- (NSString *)accessKey
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::accesskeyAttr)).autorelease();
}

- (void)setAccessKey:(NSString *)newAccessKey
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::accesskeyAttr, AtomStringImpl::add((__bridge CFStringRef)newAccessKey).get());
}

- (NSString *)text
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->text()).autorelease();
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

- (NSString *)origin
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->origin()).autorelease();
}

- (NSString *)protocol
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->protocol()).autorelease();
}

- (NSString *)host
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->host()).autorelease();
}

- (NSString *)hostname
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->hostname()).autorelease();
}

- (NSString *)port
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->port()).autorelease();
}

- (NSString *)pathname
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->pathname()).autorelease();
}

- (NSString *)search
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->search()).autorelease();
}

- (NSString *)hashName
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->hash()).autorelease();
}

@end

#undef IMPL
