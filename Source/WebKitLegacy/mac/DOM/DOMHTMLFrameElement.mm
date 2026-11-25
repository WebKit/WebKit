/*
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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

#import "DOMHTMLFrameElement.h"

#import "DOMAbstractViewInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/Document.h>
#import <WebCore/ElementInlines.h>
#import <WebCore/HTMLFrameElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/LocalDOMWindow.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/WindowProxy.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL protect(downcast<WebCore::HTMLFrameElement>(reinterpret_cast<WebCore::Node*>(_internal)))

@implementation DOMHTMLFrameElement

- (NSString *)frameBorder
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::frameborderAttr)).autorelease();
}

- (void)setFrameBorder:(NSString *)newFrameBorder
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::frameborderAttr, AtomStringImpl::add((__bridge CFStringRef)newFrameBorder).get());
}

- (NSString *)longDesc
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::longdescAttr)).autorelease();
}

- (void)setLongDesc:(NSString *)newLongDesc
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::longdescAttr, AtomStringImpl::add((__bridge CFStringRef)newLongDesc).get());
}

- (NSString *)marginHeight
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::marginheightAttr)).autorelease();
}

- (void)setMarginHeight:(NSString *)newMarginHeight
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::marginheightAttr, AtomStringImpl::add((__bridge CFStringRef)newMarginHeight).get());
}

- (NSString *)marginWidth
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::marginwidthAttr)).autorelease();
}

- (void)setMarginWidth:(NSString *)newMarginWidth
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::marginwidthAttr, AtomStringImpl::add((__bridge CFStringRef)newMarginWidth).get());
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

- (BOOL)noResize
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::noresizeAttr);
}

- (void)setNoResize:(BOOL)newNoResize
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::noresizeAttr, newNoResize);
}

- (NSString *)scrolling
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::scrollingAttr)).autorelease();
}

- (void)setScrolling:(NSString *)newScrolling
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::scrollingAttr, AtomStringImpl::add((__bridge CFStringRef)newScrolling).get());
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

- (DOMDocument *)contentDocument
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->contentDocument()));
}

- (DOMAbstractView *)contentWindow
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->contentWindow()));
}

- (NSString *)location
{
    return nullptr;
}

- (void)setLocation:(NSString *)newLocation
{
    return;
}

@end

#undef IMPL
