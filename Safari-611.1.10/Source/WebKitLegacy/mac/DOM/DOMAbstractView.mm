/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "DOMAbstractViewInternal.h"

#import "DOMDocumentInternal.h"
#import "DOMInternal.h"
#import <WebCore/DOMWindow.h>
#import <WebCore/Document.h>
#import "ExceptionHandlers.h"
#import <WebCore/Frame.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/WindowProxy.h>

#define IMPL reinterpret_cast<WebCore::Frame*>(_internal)

@implementation DOMAbstractView

- (void)dealloc
{
    WebCoreThreadViolationCheckRoundOne();
    [super dealloc];
}

- (DOMDocument *)document
{
    if (!_internal)
        return nil;
    return kit(IMPL->document());
}

@end

@implementation DOMAbstractView (WebKitLegacyInternal)

- (void)_disconnectFrame
{
    ASSERT(_internal);
    removeDOMWrapper(_internal);
    _internal = 0;
}

@end

WebCore::DOMWindow* core(DOMAbstractView *wrapper)
{
    if (!wrapper)
        return 0;
    if (!wrapper->_internal)
        return 0;
    return reinterpret_cast<WebCore::Frame*>(wrapper->_internal)->document()->domWindow();
}

DOMAbstractView *kit(WebCore::DOMWindow* value)
{
    WebCoreThreadViolationCheckRoundOne();

    if (!value)
        return nil;
    WebCore::Frame* frame = value->frame();
    if (!frame)
        return nil;
    if (DOMAbstractView *wrapper = getDOMWrapper(frame))
        return [[wrapper retain] autorelease];
    DOMAbstractView *wrapper = [[DOMAbstractView alloc] _init];
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(frame);
    addDOMWrapper(wrapper, frame);
    return [wrapper autorelease];
}

DOMAbstractView *kit(WebCore::AbstractDOMWindow* value)
{
    if (!is<WebCore::DOMWindow>(value))
        return nil;

    return kit(downcast<WebCore::DOMWindow>(value));
}

DOMAbstractView *kit(WebCore::WindowProxy* windowProxy)
{
    if (!windowProxy)
        return nil;

    return kit(windowProxy->window());
}

WebCore::WindowProxy* toWindowProxy(DOMAbstractView *view)
{
    auto* window = core(view);
    if (!window || !window->frame())
        return nil;
    return &window->frame()->windowProxy();
}

#undef IMPL
