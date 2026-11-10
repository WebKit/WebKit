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

#import "DOMHTMLIFrameElement.h"

#import "DOMAbstractViewInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/Document.h>
#import <WebCore/ElementInlines.h>
#import <WebCore/HTMLIFrameElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/LocalDOMWindow.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/WindowProxy.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL protect(downcast<WebCore::HTMLIFrameElement>(reinterpret_cast<WebCore::Node*>(_internal)))

@implementation DOMHTMLIFrameElement

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

- (NSString *)sandbox
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::sandboxAttr)).autorelease();
}

- (void)setSandbox:(NSString *)newSandbox
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::sandboxAttr, AtomStringImpl::add((__bridge CFStringRef)newSandbox).get());
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

- (NSString *)srcdoc
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::srcdocAttr)).autorelease();
}

- (void)setSrcdoc:(NSString *)newSrcdoc
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::srcdocAttr, AtomStringImpl::add((__bridge CFStringRef)newSrcdoc).get());
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

- (DOMAbstractView *)contentWindow
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->contentWindow()));
}

@end

#undef IMPL
