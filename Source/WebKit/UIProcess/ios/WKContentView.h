/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#import "WKApplicationStateTrackingView.h"
#import "WKBase.h"
#import "WKBrowsingContextController.h"
#import "WKProcessGroup.h"
#import <WebCore/InspectorOverlay.h>
#import <wtf/NakedRef.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

@class WKContentView;
@class WKWebView;

namespace API {
class PageConfiguration;
}

namespace WebCore {
class FloatRect;
}

namespace WebKit {
class DrawingAreaProxy;
class RemoteLayerTreeTransaction;
class WebFrameProxy;
class WebPageProxy;
class WebProcessProxy;
class WebProcessPool;
enum class ViewStabilityFlag : uint8_t;
}

@interface WKContentView : WKApplicationStateTrackingView {
@package
    RefPtr<WebKit::WebPageProxy> _page;
    WeakObjCPtr<WKWebView> _webView;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
@property (nonatomic, readonly) WKBrowsingContextController *browsingContextController;
ALLOW_DEPRECATED_DECLARATIONS_END

@property (nonatomic, readonly) WebKit::WebPageProxy* page;
@property (nonatomic, readonly) BOOL isFocusingElement;
@property (nonatomic, getter=isShowingInspectorIndication) BOOL showingInspectorIndication;
@property (nonatomic, readonly, getter=isResigningFirstResponder) BOOL resigningFirstResponder;
@property (nonatomic) BOOL sizeChangedSinceLastVisibleContentRectUpdate;
@property (nonatomic, readonly) UIInterfaceOrientation interfaceOrientation;
@property (nonatomic, readonly) NSUndoManager *undoManagerForWebView;

#if HAVE(SPATIAL_TRACKING_LABEL)
@property (nonatomic, readonly) const String& spatialTrackingLabel;
#endif

- (instancetype)initWithFrame:(CGRect)frame processPool:(NakedRef<WebKit::WebProcessPool>)processPool configuration:(Ref<API::PageConfiguration>&&)configuration webView:(WKWebView *)webView;

- (void)didUpdateVisibleRect:(CGRect)visibleRect
    unobscuredRect:(CGRect)unobscuredRect
    contentInsets:(UIEdgeInsets)contentInsets
    unobscuredRectInScrollViewCoordinates:(CGRect)unobscuredRectInScrollViewCoordinates
    obscuredInsets:(UIEdgeInsets)obscuredInsets
    unobscuredSafeAreaInsets:(UIEdgeInsets)unobscuredSafeAreaInsets
    inputViewBounds:(CGRect)inputViewBounds
    scale:(CGFloat)scale minimumScale:(CGFloat)minimumScale
    viewStability:(OptionSet<WebKit::ViewStabilityFlag>)viewStability
    enclosedInScrollableAncestorView:(BOOL)enclosedInScrollableAncestorView
    sendEvenIfUnchanged:(BOOL)sendEvenIfUnchanged;

- (void)didFinishScrolling;
- (void)didInterruptScrolling;
- (void)didZoomToScale:(CGFloat)scale;
- (void)willStartZoomOrScroll;
- (BOOL)screenIsBeingCaptured;

- (void)_webViewDestroyed;

- (WKWebView *)webView;
- (UIView *)rootContentView;

- (Ref<WebKit::DrawingAreaProxy>)_createDrawingAreaProxy:(WebKit::WebProcessProxy&)webProcessProxy;
- (void)_processDidExit;
#if ENABLE(GPU_PROCESS)
- (void)_gpuProcessDidExit;
#endif
#if ENABLE(MODEL_PROCESS)
- (void)_modelProcessDidExit;
#endif
- (void)_processWillSwap;
- (void)_didRelaunchProcess;

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
- (void)_webProcessDidCreateContextForVisibilityPropagation;
#if ENABLE(GPU_PROCESS)
- (void)_gpuProcessDidCreateContextForVisibilityPropagation;
#endif // ENABLE(GPU_PROCESS)
#if ENABLE(MODEL_PROCESS)
- (void)_modelProcessDidCreateContextForVisibilityPropagation;
#endif // ENABLE(MODEL_PROCESS)
#if USE(EXTENSIONKIT)
- (UIView *)_createVisibilityPropagationView;
#endif
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

- (void)_setAcceleratedCompositingRootView:(UIView *)rootView;
- (void)_removeTemporaryDirectoriesWhenDeallocated:(Vector<RetainPtr<NSURL>>&&)urls;

- (void)_showInspectorHighlight:(const WebCore::InspectorOverlay::Highlight&)highlight;
- (void)_hideInspectorHighlight;

- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction;
- (void)_layerTreeCommitComplete;

- (void)_setAccessibilityWebProcessToken:(NSData *)data;

- (BOOL)_scrollToRect:(CGRect)targetRect withOrigin:(CGPoint)origin minimumScrollDistance:(CGFloat)minimumScrollDistance;
- (void)_zoomToFocusRect:(CGRect)rectToFocus selectionRect:(CGRect)selectionRect fontSize:(float)fontSize minimumScale:(double)minimumScale maximumScale:(double)maximumScale allowScaling:(BOOL)allowScaling forceScroll:(BOOL)forceScroll;
- (BOOL)_zoomToRect:(CGRect)targetRect withOrigin:(CGPoint)origin fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale minimumScrollDistance:(CGFloat)minimumScrollDistance;
- (void)_zoomOutWithOrigin:(CGPoint)origin;
- (void)_zoomToInitialScaleWithOrigin:(CGPoint)origin;
- (double)_initialScaleFactor;
- (double)_contentZoomScale;
- (double)_targetContentZoomScaleForRect:(const WebCore::FloatRect&)targetRect currentScale:(double)currentScale fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale;

@end
