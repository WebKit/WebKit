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


#if ENABLE(VIDEO)

#import "DOMHTMLVideoElement.h"

#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLVideoElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLVideoElement

- (unsigned)width
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getUnsignedIntegralAttribute(WebCore::HTMLNames::widthAttr);
}

- (void)setWidth:(unsigned)newWidth
{
    WebCore::JSMainThreadNullState state;
    IMPL->setUnsignedIntegralAttribute(WebCore::HTMLNames::widthAttr, newWidth);
}

- (unsigned)height
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getUnsignedIntegralAttribute(WebCore::HTMLNames::heightAttr);
}

- (void)setHeight:(unsigned)newHeight
{
    WebCore::JSMainThreadNullState state;
    IMPL->setUnsignedIntegralAttribute(WebCore::HTMLNames::heightAttr, newHeight);
}

- (unsigned)videoWidth
{
    WebCore::JSMainThreadNullState state;
    return IMPL->videoWidth();
}

- (unsigned)videoHeight
{
    WebCore::JSMainThreadNullState state;
    return IMPL->videoHeight();
}

- (NSString *)poster
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getURLAttribute(WebCore::HTMLNames::posterAttr);
}

- (void)setPoster:(NSString *)newPoster
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::posterAttr, newPoster);
}

- (BOOL)playsInline
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::playsinlineAttr);
}

- (void)setPlaysInline:(BOOL)newPlaysInline
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::playsinlineAttr, newPlaysInline);
}

- (BOOL)webkitSupportsFullscreen
{
    WebCore::JSMainThreadNullState state;
    return IMPL->webkitSupportsFullscreen();
}

- (BOOL)webkitDisplayingFullscreen
{
    WebCore::JSMainThreadNullState state;
    return IMPL->webkitDisplayingFullscreen();
}

- (void)webkitEnterFullscreen
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->webkitEnterFullscreen());
}

- (void)webkitExitFullscreen
{
    WebCore::JSMainThreadNullState state;
    IMPL->webkitExitFullscreen();
}

- (void)webkitEnterFullScreen
{
    [self webkitEnterFullscreen];
}

- (void)webkitExitFullScreen
{
    [self webkitExitFullscreen];
}

@end

#endif // ENABLE(VIDEO)
