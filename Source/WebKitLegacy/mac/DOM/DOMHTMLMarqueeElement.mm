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

#import "DOMHTMLMarqueeElement.h"

#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/ElementInlines.h>
#import <WebCore/HTMLMarqueeElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL protect(downcast<WebCore::HTMLMarqueeElement>(reinterpret_cast<WebCore::Node*>(_internal)))

@implementation DOMHTMLMarqueeElement

- (NSString *)behavior
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::behaviorAttr)).autorelease();
}

- (void)setBehavior:(NSString *)newBehavior
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::behaviorAttr, AtomStringImpl::add((__bridge CFStringRef)newBehavior).get());
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

- (NSString *)direction
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::directionAttr)).autorelease();
}

- (void)setDirection:(NSString *)newDirection
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::directionAttr, AtomStringImpl::add((__bridge CFStringRef)newDirection).get());
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

- (unsigned)hspace
{
    WebCore::JSMainThreadNullState state;
    return IMPL->unsignedIntegralAttribute(WebCore::HTMLNames::hspaceAttr);
}

- (void)setHspace:(unsigned)newHspace
{
    WebCore::JSMainThreadNullState state;
    IMPL->setUnsignedIntegralAttribute(WebCore::HTMLNames::hspaceAttr, newHspace);
}

- (int)loop
{
    WebCore::JSMainThreadNullState state;
    return IMPL->loop();
}

- (void)setLoop:(int)newLoop
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setLoop(newLoop));
}

- (unsigned)scrollAmount
{
    WebCore::JSMainThreadNullState state;
    return IMPL->scrollAmount();
}

- (void)setScrollAmount:(unsigned)newScrollAmount
{
    WebCore::JSMainThreadNullState state;
    IMPL->setScrollAmount(newScrollAmount);
}

- (unsigned)scrollDelay
{
    WebCore::JSMainThreadNullState state;
    return IMPL->scrollDelay();
}

- (void)setScrollDelay:(unsigned)newScrollDelay
{
    WebCore::JSMainThreadNullState state;
    IMPL->setScrollDelay(newScrollDelay);
}

- (BOOL)trueSpeed
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::truespeedAttr);
}

- (void)setTrueSpeed:(BOOL)newTrueSpeed
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::truespeedAttr, newTrueSpeed);
}

- (unsigned)vspace
{
    WebCore::JSMainThreadNullState state;
    return IMPL->unsignedIntegralAttribute(WebCore::HTMLNames::vspaceAttr);
}

- (void)setVspace:(unsigned)newVspace
{
    WebCore::JSMainThreadNullState state;
    IMPL->setUnsignedIntegralAttribute(WebCore::HTMLNames::vspaceAttr, newVspace);
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

- (void)start
{
    WebCore::JSMainThreadNullState state;
    IMPL->start();
}

- (void)stop
{
    WebCore::JSMainThreadNullState state;
    IMPL->stop();
}

@end

#undef IMPL
