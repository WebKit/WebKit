/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#import "WKViewPrivate.h"

#import "RemoteLayerTreeTransaction.h"
#import "ViewGestureController.h"
#import "WebPageProxy.h"
#import "WKBrowsingContextGroupPrivate.h"
#import "WKContentView.h"
#import "WKProcessGroupPrivate.h"
#import "WKScrollView.h"
#import "WKAPICast.h"
#import <UIKit/UIImage_Private.h>
#import <UIKit/UIPeripheralHost_Private.h>
#import <UIKit/UIScreen.h>
#import <UIKit/UIScrollView_Private.h>
#import <UIKit/UIWindow_Private.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

@interface WKView () <UIScrollViewDelegate, WKContentViewDelegate>
- (void)_setDocumentScale:(CGFloat)newScale;
@end

@interface UIScrollView (UIScrollViewInternal)
- (void)_adjustForAutomaticKeyboardInfo:(NSDictionary*)info animated:(BOOL)animated lastAdjustment:(CGFloat*)lastAdjustment;
@end

@implementation WKView {
    RetainPtr<WKScrollView> _scrollView;
    RetainPtr<WKContentView> _contentView;

    BOOL _isWaitingForNewLayerTreeAfterDidCommitLoad;
    std::unique_ptr<ViewGestureController> _gestureController;
    
    BOOL _allowsBackForwardNavigationGestures;

    BOOL _hasStaticMinimumLayoutSize;
    CGSize _minimumLayoutSizeOverride;

    UIEdgeInsets _obscuredInsets;
    bool _isChangingObscuredInsetsInteractively;
    CGFloat _lastAdjustmentForScroller;
}

- (id)initWithCoder:(NSCoder *)coder
{
    // FIXME: Implement.
    [self release];
    return nil;
}

- (id)initWithFrame:(CGRect)frame processGroup:(WKProcessGroup *)processGroup browsingContextGroup:(WKBrowsingContextGroup *)browsingContextGroup
{
    return [self initWithFrame:frame processGroup:processGroup browsingContextGroup:browsingContextGroup relatedToView:nil];
}

- (id)initWithFrame:(CGRect)frame processGroup:(WKProcessGroup *)processGroup browsingContextGroup:(WKBrowsingContextGroup *)browsingContextGroup relatedToView:(WKView *)relatedView
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    [self _commonInitializationWithContextRef:processGroup._contextRef pageGroupRef:browsingContextGroup._pageGroupRef relatedToPage:relatedView ? [relatedView pageRef] : nullptr];
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)setFrame:(CGRect)frame
{
    CGRect oldFrame = [self frame];
    [super setFrame:frame];

    if (!CGSizeEqualToSize(oldFrame.size, frame.size))
        [self _frameOrBoundsChanged];
}

- (void)setBounds:(CGRect)bounds
{
    CGRect oldBounds = [self bounds];
    [super setBounds:bounds];
    
    if (!CGSizeEqualToSize(oldBounds.size, bounds.size))
        [self _frameOrBoundsChanged];
}

- (UIScrollView *)scrollView
{
    return _scrollView.get();
}

- (WKBrowsingContextController *)browsingContextController
{
    return [_contentView browsingContextController];
}

- (void)setAllowsBackForwardNavigationGestures:(BOOL)allowsBackForwardNavigationGestures
{
    _allowsBackForwardNavigationGestures = allowsBackForwardNavigationGestures;
    
    WebPageProxy *webPageProxy = toImpl([_contentView _pageRef]);
    
    if (allowsBackForwardNavigationGestures && !_gestureController) {
        _gestureController = std::make_unique<ViewGestureController>(*webPageProxy);
        _gestureController->installSwipeHandler(self, [self scrollView]);
    } else
        _gestureController = nullptr;
    
    webPageProxy->setShouldRecordNavigationSnapshots(allowsBackForwardNavigationGestures);
}

- (BOOL)allowsBackForwardNavigationGestures
{
    return _allowsBackForwardNavigationGestures;
}

#pragma mark WKContentViewDelegate

- (void)contentViewDidCommitLoadForMainFrame:(WKContentView *)contentView
{
    _isWaitingForNewLayerTreeAfterDidCommitLoad = YES;
}

- (void)contentView:(WKContentView *)contentView didCommitLayerTree:(const RemoteLayerTreeTransaction&)layerTreeTransaction
{
    [_scrollView setMinimumZoomScale:layerTreeTransaction.minimumScaleFactor()];
    [_scrollView setMaximumZoomScale:layerTreeTransaction.maximumScaleFactor()];
    [_scrollView setZoomEnabled:layerTreeTransaction.allowsUserScaling()];
    if (![_scrollView isZooming] && ![_scrollView isZoomBouncing])
        [_scrollView setZoomScale:layerTreeTransaction.pageScaleFactor()];
    
    if (_gestureController)
        _gestureController->setRenderTreeSize(layerTreeTransaction.renderTreeSize());

    if (_isWaitingForNewLayerTreeAfterDidCommitLoad) {
        UIEdgeInsets inset = [_scrollView contentInset];
        [_scrollView setContentOffset:CGPointMake(-inset.left, -inset.top)];
        _isWaitingForNewLayerTreeAfterDidCommitLoad = NO;
    }

}

#pragma mark - UIScrollViewDelegate

- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView
{
    ASSERT(_scrollView == scrollView);
    return _contentView.get();
}

- (void)scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view
{
    if (scrollView.pinchGestureRecognizer.state == UIGestureRecognizerStateBegan)
        [_contentView willStartUserTriggeredZoom];
    [_contentView willStartZoomOrScroll];
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
    [_contentView willStartZoomOrScroll];
}

- (void)_didFinishScroll
{
    CGPoint position = [_scrollView convertPoint:[_scrollView contentOffset] toView:_contentView.get()];
    [_contentView didFinishScrollTo:position];
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
    // If we're decelerating, scroll offset will be updated when scrollViewDidFinishDecelerating: is called.
    if (!decelerate)
        [self _didFinishScroll];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
    [self _didFinishScroll];
}

- (void)scrollViewDidScrollToTop:(UIScrollView *)scrollView
{
    [self _didFinishScroll];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    CGPoint position = [_scrollView convertPoint:[_scrollView contentOffset] toView:_contentView.get()];
    [_contentView didScrollTo:position];
}

- (void)scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale
{
    ASSERT(scrollView == _scrollView);
    [_contentView didZoomToScale:scale];
}

#pragma mark Internal

- (void)_commonInitializationWithContextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef relatedToPage:(WKPageRef)relatedPage
{
    ASSERT(!_scrollView);
    ASSERT(!_contentView);

    CGRect bounds = self.bounds;

    _scrollView = adoptNS([[WKScrollView alloc] initWithFrame:bounds]);
    [_scrollView setInternalDelegate:self];
    [_scrollView setBouncesZoom:YES];

    [self addSubview:_scrollView.get()];

    WebKit::WebPageConfiguration webPageConfiguration;
    webPageConfiguration.pageGroup = toImpl(pageGroupRef);
    webPageConfiguration.relatedPage = toImpl(relatedPage);

    _contentView = adoptNS([[WKContentView alloc] initWithFrame:bounds context:*toImpl(contextRef) configuration:std::move(webPageConfiguration)]);

    [_contentView setDelegate:self];
    [[_contentView layer] setAnchorPoint:CGPointZero];
    [_contentView setFrame:bounds];
    [_scrollView addSubview:_contentView.get()];

    [self _frameOrBoundsChanged];

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(_keyboardWillChangeFrame:) name:UIKeyboardWillChangeFrameNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardDidChangeFrame:) name:UIKeyboardDidChangeFrameNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardWillShow:) name:UIKeyboardWillShowNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardWillHide:) name:UIKeyboardWillHideNotification object:nil];
}

- (void)_frameOrBoundsChanged
{
    CGRect bounds = [self bounds];
    if (!_hasStaticMinimumLayoutSize)
        [_contentView setMinimumLayoutSize:bounds.size];
    [_scrollView setFrame:bounds];
    [_contentView setMinimumSize:bounds.size];
}

- (void)_setDocumentScale:(CGFloat)newScale
{
    CGPoint contentOffsetInDocumentCoordinates = [_scrollView convertPoint:[_scrollView contentOffset] toView:_contentView.get()];

    [_scrollView setZoomScale:newScale];
    [_contentView didZoomToScale:newScale];

    CGPoint contentOffset = [_scrollView convertPoint:contentOffsetInDocumentCoordinates fromView:_contentView.get()];
    [_scrollView setContentOffset:contentOffset];
}


- (RetainPtr<CGImageRef>)takeViewSnapshotForContentView:(WKContentView *)contentView
{
    // FIXME: We should be able to use acquire an IOSurface directly, instead of going to CGImage here and back in ViewSnapshotStore.
    UIGraphicsBeginImageContextWithOptions(self.bounds.size, YES, self.window.screen.scale);
    [self drawViewHierarchyInRect:[self bounds] afterScreenUpdates:NO];
    UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return image.CGImage;
}

- (void)_keyboardChangedWithInfo:(NSDictionary *)keyboardInfo adjustScrollView:(BOOL)adjustScrollView
{
    // FIXME: We will also need to adjust the unobscured rect by taking into account the keyboard rect and the obscured insets.
    if (adjustScrollView)
        [_scrollView _adjustForAutomaticKeyboardInfo:keyboardInfo animated:YES lastAdjustment:&_lastAdjustmentForScroller];
}

- (void)_keyboardWillChangeFrame:(NSNotification *)notification
{
    if ([_contentView isAssistingNode])
        [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:YES];
}

- (void)_keyboardDidChangeFrame:(NSNotification *)notification
{
    [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:NO];
}

- (void)_keyboardWillShow:(NSNotification *)notification
{
    if ([_contentView isAssistingNode])
        [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:YES];
}

- (void)_keyboardWillHide:(NSNotification *)notification
{
    // Ignore keyboard will hide notifications sent during rotation. They're just there for
    // backwards compatibility reasons and processing the will hide notification would
    // temporarily screw up the the unobscured view area.
    if ([[UIPeripheralHost sharedInstance] rotationState])
        return;

    [self _keyboardChangedWithInfo:notification.userInfo adjustScrollView:YES];
}

@end

@implementation WKView (Private)

- (WKPageRef)pageRef
{
    return [_contentView _pageRef];
}

- (id)initWithFrame:(CGRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef
{
    return [self initWithFrame:frame contextRef:contextRef pageGroupRef:pageGroupRef relatedToPage:nil];
}

- (id)initWithFrame:(CGRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef relatedToPage:(WKPageRef)relatedPage
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    [self _commonInitializationWithContextRef:contextRef pageGroupRef:pageGroupRef relatedToPage:relatedPage];
    return self;
}

- (CGSize)minimumLayoutSizeOverride
{
    ASSERT(_hasStaticMinimumLayoutSize);
    return _minimumLayoutSizeOverride;
}

- (void)setMinimumLayoutSizeOverride:(CGSize)minimumLayoutSizeOverride
{
    _hasStaticMinimumLayoutSize = YES;
    _minimumLayoutSizeOverride = minimumLayoutSizeOverride;
    [_contentView setMinimumLayoutSize:minimumLayoutSizeOverride];
}

- (UIEdgeInsets)_obscuredInsets
{
    return _obscuredInsets;
}

- (void)_setObscuredInsets:(UIEdgeInsets)obscuredInsets
{
    ASSERT(obscuredInsets.top >= 0);
    ASSERT(obscuredInsets.left >= 0);
    ASSERT(obscuredInsets.bottom >= 0);
    ASSERT(obscuredInsets.right >= 0);
    _obscuredInsets = obscuredInsets;
}

- (void)_beginInteractiveObscuredInsetsChange
{
    ASSERT(!_isChangingObscuredInsetsInteractively);
    _isChangingObscuredInsetsInteractively = YES;
}

- (void)_endInteractiveObscuredInsetsChange
{
    ASSERT(_isChangingObscuredInsetsInteractively);
    _isChangingObscuredInsetsInteractively = NO;
}

- (UIColor *)_pageExtendedBackgroundColor
{
    WebPageProxy* webPageProxy = toImpl([_contentView _pageRef]);
    WebCore::Color color = webPageProxy->pageExtendedBackgroundColor();
    if (!color.isValid())
        return nil;

    return [UIColor colorWithRed:(color.red() / 255.0) green:(color.green() / 255.0) blue:(color.blue() / 255.0) alpha:(color.alpha() / 255.0)];
}

- (void)_setBackgroundExtendsBeyondPage:(BOOL)backgroundExtends
{
    WebPageProxy* webPageProxy = toImpl([_contentView _pageRef]);
    webPageProxy->setBackgroundExtendsBeyondPage(backgroundExtends);
}

- (BOOL)_backgroundExtendsBeyondPage
{
    WebPageProxy* webPageProxy = toImpl([_contentView _pageRef]);
    return webPageProxy->backgroundExtendsBeyondPage();
}

@end
