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

#import <WebKit/WKFoundation.h>

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JavaScriptCore.h>

@class _WKFrameHandle;
@class WKWebProcessPlugInHitTestResult;
@class WKWebProcessPlugInNodeHandle;
@class WKWebProcessPlugInRangeHandle;
@class WKWebProcessPlugInScriptWorld;

WK_CLASS_AVAILABLE(macos(10.10), ios(8.0))
@interface WKWebProcessPlugInFrame : NSObject

@property (nonatomic, readonly) NSURL *URL;
@property (nonatomic, readonly) NSArray *childFrames;
@property (nonatomic, readonly) BOOL containsAnyFormElements;

@property (nonatomic, readonly) _WKFrameHandle *handle;

// Returns URLs ordered by resolution in descending order.
// FIXME: These should be tagged nonnull.
@property (nonatomic, readonly) NSArray<NSURL *> *appleTouchIconURLs WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, readonly) NSArray<NSURL *> *faviconURLs WK_API_AVAILABLE(macos(10.12), ios(10.0));

- (JSContext *)jsContextForWorld:(WKWebProcessPlugInScriptWorld *)world;
- (WKWebProcessPlugInHitTestResult *)hitTest:(CGPoint)point;
- (JSValue *)jsNodeForNodeHandle:(WKWebProcessPlugInNodeHandle *)nodeHandle inWorld:(WKWebProcessPlugInScriptWorld *)world;
- (JSValue *)jsRangeForRangeHandle:(WKWebProcessPlugInRangeHandle *)rangeHandle inWorld:(WKWebProcessPlugInScriptWorld *)world WK_API_AVAILABLE(macos(10.12.3), ios(10.3));

@end
