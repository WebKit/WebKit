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

#import "DOMHTMLAppletElement.h"

#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLAppletElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLAppletElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLAppletElement

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

- (NSString *)alt
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::altAttr);
}

- (void)setAlt:(NSString *)newAlt
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::altAttr, newAlt);
}

- (NSString *)archive
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::archiveAttr);
}

- (void)setArchive:(NSString *)newArchive
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::archiveAttr, newArchive);
}

- (NSString *)code
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::codeAttr);
}

- (void)setCode:(NSString *)newCode
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::codeAttr, newCode);
}

- (NSString *)codeBase
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::codebaseAttr);
}

- (void)setCodeBase:(NSString *)newCodeBase
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::codebaseAttr, newCodeBase);
}

- (NSString *)height
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::heightAttr);
}

- (void)setHeight:(NSString *)newHeight
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::heightAttr, newHeight);
}

- (int)hspace
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getIntegralAttribute(WebCore::HTMLNames::hspaceAttr);
}

- (void)setHspace:(int)newHspace
{
    WebCore::JSMainThreadNullState state;
    IMPL->setIntegralAttribute(WebCore::HTMLNames::hspaceAttr, newHspace);
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

- (NSString *)object
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::objectAttr);
}

- (void)setObject:(NSString *)newObject
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::objectAttr, newObject);
}

- (int)vspace
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getIntegralAttribute(WebCore::HTMLNames::vspaceAttr);
}

- (void)setVspace:(int)newVspace
{
    WebCore::JSMainThreadNullState state;
    IMPL->setIntegralAttribute(WebCore::HTMLNames::vspaceAttr, newVspace);
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

@end
