/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKWebProcessPlugInFrameInternal.h"

#if WK_API_ENABLED

#import "WKFrameHandleInternal.h"
#import "WKNSArray.h"
#import "WKNSURLExtras.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import "WKWebProcessPlugInHitTestResultInternal.h"
#import "WKWebProcessPlugInNodeHandleInternal.h"
#import "WKWebProcessPlugInScriptWorldInternal.h"
#import "WebProcess.h"
#import <JavaScriptCore/JSValue.h>
#import <WebCore/IntPoint.h>

using namespace WebKit;

@implementation WKWebProcessPlugInFrame {
    API::ObjectStorage<WebFrame> _frame;
}

+ (instancetype)lookUpFrameFromHandle:(WKFrameHandle *)handle
{
    WebFrame* webFrame = WebProcess::shared().webFrame(handle._frameID);
    if (!webFrame)
        return nil;

    return wrapper(*webFrame);
}

- (void)dealloc
{
    _frame->~WebFrame();
    [super dealloc];
}

- (JSContext *)jsContextForWorld:(WKWebProcessPlugInScriptWorld *)world
{
    return [JSContext contextWithJSGlobalContextRef:_frame->jsContextForWorld(&[world _scriptWorld])];
}

- (WKWebProcessPlugInHitTestResult *)hitTest:(CGPoint)point
{
    RefPtr<InjectedBundleHitTestResult> hitTestResult = _frame->hitTest(WebCore::IntPoint(point));
    return [wrapper(*hitTestResult.release().leakRef()) autorelease];
}

- (JSValue *)jsNodeForNodeHandle:(WKWebProcessPlugInNodeHandle *)nodeHandle inWorld:(WKWebProcessPlugInScriptWorld *)world
{
    JSValueRef valueRef = _frame->jsWrapperForWorld(&[nodeHandle _nodeHandle], &[world _scriptWorld]);
    return [JSValue valueWithJSValueRef:valueRef inContext:[self jsContextForWorld:world]];
}

- (WKWebProcessPlugInBrowserContextController *)_browserContextController
{
    return wrapper(*_frame->page());
}

- (NSURL *)URL
{
    return [NSURL _web_URLWithWTFString:_frame->url()];
}

- (NSArray *)childFrames
{
    return [wrapper(*_frame->childFrames().leakRef()) autorelease];
}

- (BOOL)containsAnyFormElements
{
    return !!_frame->containsAnyFormElements();
}

- (WKFrameHandle *)handle
{
    return [wrapper(*API::FrameHandle::create(_frame->frameID()).leakRef()) autorelease];
}

- (BOOL)_hasCustomContentProvider
{
    if (!_frame->isMainFrame())
        return false;

    return _frame->page()->mainFrameHasCustomContentProvider();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_frame;
}

@end

#endif // WK_API_ENABLED
