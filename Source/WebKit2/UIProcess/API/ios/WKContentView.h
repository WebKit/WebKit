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

#import "WKBase.h"
#import "WKBrowsingContextController.h"
#import "WKBrowsingContextGroup.h"
#import "WKProcessGroup.h"
#import <UIKit/UIKit.h>
#import <wtf/RetainPtr.h>

@class WKContentView;
@class WKWebViewConfiguration;

namespace WebKit {
class RemoteLayerTreeTransaction;
class WebContext;
struct WebPageConfiguration;
}

@protocol WKContentViewDelegate <NSObject>
@optional
- (void)contentViewDidCommitLoadForMainFrame:(WKContentView *)contentView;
- (void)contentView:(WKContentView *)contentView didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction;

// FIXME: This doesn't belong in a 'delegate'.
- (RetainPtr<CGImageRef>)takeViewSnapshotForContentView:(WKContentView *)contentView;
@end

WK_API_CLASS
@interface WKContentView : UIView

@property (nonatomic, readonly) WKBrowsingContextController *browsingContextController;

@property (nonatomic, assign) id <WKContentViewDelegate> delegate;

@property (nonatomic, readonly) WKPageRef _pageRef;
@property (nonatomic, readonly) BOOL isAssistingNode;

- (instancetype)initWithFrame:(CGRect)frame context:(WebKit::WebContext&)context configuration:(WebKit::WebPageConfiguration)webPageConfiguration;

- (void)setMinimumSize:(CGSize)size;
- (void)setMinimumLayoutSize:(CGSize)size;

- (void)didFinishScrollTo:(CGPoint)contentOffset;
- (void)didScrollTo:(CGPoint)contentOffset;
- (void)didZoomToScale:(CGFloat)scale;
- (void)willStartZoomOrScroll;
- (void)willStartUserTriggeredZoom;

@end
