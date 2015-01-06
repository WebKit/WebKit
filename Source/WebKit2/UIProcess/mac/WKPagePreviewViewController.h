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

#ifndef WKPagePreviewViewController_h
#define WKPagePreviewViewController_h

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

#import <wtf/RetainPtr.h>

@class NSString;
@class NSTextField;
@class NSURL;
@class NSView;
@class WKPagePreviewViewController;

@protocol WKPagePreviewViewControllerDelegate <NSObject>
- (NSView *)pagePreviewViewController:(WKPagePreviewViewController *)pagePreviewViewController viewForPreviewingURL:(NSURL *)url initialFrameSize:(NSSize)initialFrameSize;
- (NSString *)pagePreviewViewController:(WKPagePreviewViewController *)pagePreviewViewController titleForPreviewOfURL:(NSURL *)url;
- (void)pagePreviewViewControllerWasClicked:(WKPagePreviewViewController *)pagePreviewViewController;
@end

@interface WKPagePreviewViewController : NSViewController {
@public
    NSSize _mainViewSize;
    RetainPtr<NSURL> _url;
    RetainPtr<NSView> _previewView;
    RetainPtr<NSTextField> _titleTextField;
    RetainPtr<NSString> _previewTitle;
    RetainPtr<NSProgressIndicator> _spinner;
    BOOL _loading;
    id <WKPagePreviewViewControllerDelegate> _delegate;
    CGFloat _popoverToViewScale;
}

@property (nonatomic, copy) NSString *previewTitle;
@property (nonatomic, getter=isLoading) BOOL loading;

- (instancetype)initWithPageURL:(NSURL *)URL mainViewSize:(NSSize)size popoverToViewScale:(CGFloat)scale;
- (void)replacePreviewWithImage:(NSImage *)image atSize:(NSSize)size;

+ (NSSize)previewPadding;

@end

#endif // PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

#endif // WKPagePreviewViewController_h
