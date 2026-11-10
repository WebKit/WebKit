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

#import "DOMHTMLObjectElement.h"

#import "DOMDocumentInternal.h"
#import "DOMHTMLFormElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMPrivate.h"
#import "ExceptionHandlers.h"
#import <WebCore/ContainerNodeInlines.h>
#import <WebCore/Document.h>
#import <WebCore/ElementInlines.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLObjectElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/RenderElement.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL protect(downcast<WebCore::HTMLObjectElement>(reinterpret_cast<WebCore::Node*>(_internal)))

@implementation DOMHTMLObjectElement

- (DOMHTMLFormElement *)form
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->form()));
}

- (NSString *)code
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::codeAttr)).autorelease();
}

- (void)setCode:(NSString *)newCode
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::codeAttr, AtomStringImpl::add((__bridge CFStringRef)newCode).get());
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

- (NSString *)archive
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::archiveAttr)).autorelease();
}

- (void)setArchive:(NSString *)newArchive
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::archiveAttr, AtomStringImpl::add((__bridge CFStringRef)newArchive).get());
}

- (NSString *)border
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::borderAttr)).autorelease();
}

- (void)setBorder:(NSString *)newBorder
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::borderAttr, AtomStringImpl::add((__bridge CFStringRef)newBorder).get());
}

- (NSString *)codeBase
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::codebaseAttr)).autorelease();
}

- (void)setCodeBase:(NSString *)newCodeBase
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::codebaseAttr, AtomStringImpl::add((__bridge CFStringRef)newCodeBase).get());
}

- (NSString *)codeType
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::codetypeAttr)).autorelease();
}

- (void)setCodeType:(NSString *)newCodeType
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::codetypeAttr, AtomStringImpl::add((__bridge CFStringRef)newCodeType).get());
}

- (NSString *)data
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getURLAttribute(WebCore::HTMLNames::dataAttr).string()).autorelease();
}

- (void)setData:(NSString *)newData
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::dataAttr, AtomStringImpl::add((__bridge CFStringRef)newData).get());
}

- (BOOL)declare
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::declareAttr);
}

- (void)setDeclare:(BOOL)newDeclare
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::declareAttr, newDeclare);
}

- (NSString *)height
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::heightAttr)).autorelease();
}

- (void)setHeight:(NSString *)newHeight
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::heightAttr, AtomStringImpl::add((__bridge CFStringRef)newHeight).get());
}

- (int)hspace
{
    WebCore::JSMainThreadNullState state;
    return IMPL->integralAttribute(WebCore::HTMLNames::hspaceAttr);
}

- (void)setHspace:(int)newHspace
{
    WebCore::JSMainThreadNullState state;
    IMPL->setIntegralAttribute(WebCore::HTMLNames::hspaceAttr, newHspace);
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

- (NSString *)standby
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::standbyAttr)).autorelease();
}

- (void)setStandby:(NSString *)newStandby
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::standbyAttr, AtomStringImpl::add((__bridge CFStringRef)newStandby).get());
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

- (NSString *)useMap
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::usemapAttr)).autorelease();
}

- (void)setUseMap:(NSString *)newUseMap
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::usemapAttr, AtomStringImpl::add((__bridge CFStringRef)newUseMap).get());
}

- (int)vspace
{
    WebCore::JSMainThreadNullState state;
    return IMPL->integralAttribute(WebCore::HTMLNames::vspaceAttr);
}

- (void)setVspace:(int)newVspace
{
    WebCore::JSMainThreadNullState state;
    IMPL->setIntegralAttribute(WebCore::HTMLNames::vspaceAttr, newVspace);
}

- (NSString *)width
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::widthAttr)).autorelease();
}

- (void)setWidth:(NSString *)newWidth
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::widthAttr, AtomStringImpl::add((__bridge CFStringRef)newWidth).get());
}

- (DOMDocument *)contentDocument
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->contentDocument()));
}

- (NSURL *)absoluteImageURL
{
    WebCore::JSMainThreadNullState state;
    CheckedPtr renderer = IMPL->renderer();
    if (!renderer || !renderer->isImage())
        return nil;
    return [self _getURLAttribute:@"data"];
}

@end

#undef IMPL
