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

#import "DOMHTMLFrameElement.h"

#import "DOMAbstractViewInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMNodeInternal.h"
#import <WebCore/DOMWindow.h>
#import <WebCore/Document.h>
#import "ExceptionHandlers.h"
#import <WebCore/HTMLFrameElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLFrameElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLFrameElement

- (NSString *)frameBorder
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::frameborderAttr);
}

- (void)setFrameBorder:(NSString *)newFrameBorder
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::frameborderAttr, newFrameBorder);
}

- (NSString *)longDesc
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::longdescAttr);
}

- (void)setLongDesc:(NSString *)newLongDesc
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::longdescAttr, newLongDesc);
}

- (NSString *)marginHeight
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::marginheightAttr);
}

- (void)setMarginHeight:(NSString *)newMarginHeight
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::marginheightAttr, newMarginHeight);
}

- (NSString *)marginWidth
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::marginwidthAttr);
}

- (void)setMarginWidth:(NSString *)newMarginWidth
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::marginwidthAttr, newMarginWidth);
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
    return IMPL->getAttribute(WebCore::HTMLNames::scrollingAttr);
}

- (void)setScrolling:(NSString *)newScrolling
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::scrollingAttr, newScrolling);
}

- (NSString *)src
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getURLAttribute(WebCore::HTMLNames::srcAttr);
}

- (void)setSrc:(NSString *)newSrc
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::srcAttr, newSrc);
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
    WebCore::JSMainThreadNullState state;
    return IMPL->location();
}

- (void)setLocation:(NSString *)newLocation
{
    WebCore::JSMainThreadNullState state;
    IMPL->setLocation(newLocation);
}

- (int)width
{
    WebCore::JSMainThreadNullState state;
    return IMPL->width();
}

- (int)height
{
    WebCore::JSMainThreadNullState state;
    return IMPL->height();
}

@end
