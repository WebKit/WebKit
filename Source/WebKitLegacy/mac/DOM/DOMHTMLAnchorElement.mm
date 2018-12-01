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
#import <WebCore/HTMLAnchorElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLAnchorElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLAnchorElement

- (NSString *)charset
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::charsetAttr);
}

- (void)setCharset:(NSString *)newCharset
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::charsetAttr, newCharset);
}

- (NSString *)coords
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::coordsAttr);
}

- (void)setCoords:(NSString *)newCoords
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::coordsAttr, newCoords);
}

#if ENABLE(DOWNLOAD_ATTRIBUTE)
- (NSString *)download
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::downloadAttr);
}

- (void)setDownload:(NSString *)newDownload
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::downloadAttr, newDownload);
}
#endif

- (NSString *)hreflang
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::hreflangAttr);
}

- (void)setHreflang:(NSString *)newHreflang
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::hreflangAttr, newHreflang);
}

- (NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getNameAttribute();
}

- (void)setName:(NSString *)newName
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, newName);
}

- (NSString *)ping
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::pingAttr);
}

- (void)setPing:(NSString *)newPing
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::pingAttr, newPing);
}

- (NSString *)rel
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::relAttr);
}

- (void)setRel:(NSString *)newRel
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::relAttr, newRel);
}

- (NSString *)rev
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::revAttr);
}

- (void)setRev:(NSString *)newRev
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::revAttr, newRev);
}

- (NSString *)shape
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::shapeAttr);
}

- (void)setShape:(NSString *)newShape
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::shapeAttr, newShape);
}

- (NSString *)target
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::targetAttr);
}

- (void)setTarget:(NSString *)newTarget
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::targetAttr, newTarget);
}

- (NSString *)type
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::typeAttr);
}

- (void)setType:(NSString *)newType
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::typeAttr, newType);
}

- (NSString *)accessKey
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::accesskeyAttr);
}

- (void)setAccessKey:(NSString *)newAccessKey
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::accesskeyAttr, newAccessKey);
}

- (NSString *)text
{
    WebCore::JSMainThreadNullState state;
    return IMPL->text();
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
    return IMPL->getURLAttribute(WebCore::HTMLNames::hrefAttr);
}

- (void)setHref:(NSString *)newHref
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::hrefAttr, newHref);
}

- (NSString *)origin
{
    WebCore::JSMainThreadNullState state;
    return IMPL->origin();
}

- (NSString *)protocol
{
    WebCore::JSMainThreadNullState state;
    return IMPL->protocol();
}

- (NSString *)host
{
    WebCore::JSMainThreadNullState state;
    return IMPL->host();
}

- (NSString *)hostname
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hostname();
}

- (NSString *)port
{
    WebCore::JSMainThreadNullState state;
    return IMPL->port();
}

- (NSString *)pathname
{
    WebCore::JSMainThreadNullState state;
    return IMPL->pathname();
}

- (NSString *)search
{
    WebCore::JSMainThreadNullState state;
    return IMPL->search();
}

- (NSString *)hashName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hash();
}

@end
