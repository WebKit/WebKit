/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef WKPreviewPopoverAnimationController_h
#define WKPreviewPopoverAnimationController_h

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

#import <WebCore/NSImmediateActionGestureRecognizerSPI.h>
#import <wtf/Forward.h>
#import <wtf/RetainPtr.h>

namespace WebKit {
class WebPageProxy;
};

@class NSImmediateActionGestureRecognizer;
@class NSPopoverAnimationController;
@class NSString;
@class NSURL;
@class WKPagePreviewViewController;
@class WKView;

@interface WKPreviewPopoverAnimationController : NSObject <NSImmediateActionAnimationController> {
    bool _shouldShowPreviewWhenLoaded;
    bool _hasFinishedLoading;

    WKView *_wkView;
    RetainPtr<NSURL> _url;
    WebKit::WebPageProxy* _page;

    RetainPtr<NSPopover> _previewPopover;
    NSRect _popoverOriginRect;
    RetainPtr<WKPagePreviewViewController> _previewViewController;
    NSPoint _eventLocationInView;

    RetainPtr<NSPopoverAnimationController> _popoverAnimationController;

    NSImmediateActionGestureRecognizer *_recognizer;
    bool _didCompleteAnimation;
    RetainPtr<NSTimer> _previewWatchdogTimer;
}

- (instancetype)initWithURL:(NSURL *)url view:(WKView *)wkView page:(WebKit::WebPageProxy&)page originRect:(NSRect)originRect eventLocationInView:(NSPoint)eventLocationInView;
- (void)close;

- (void)setPreviewTitle:(NSString *)previewTitle;
- (void)setPreviewLoading:(BOOL)loading;
- (void)setPreviewOverrideImage:(NSImage *)image;

@end


#endif // PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

#endif // WKPreviewPopoverAnimationController_h
