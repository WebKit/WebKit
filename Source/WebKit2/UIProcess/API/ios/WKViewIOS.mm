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

#import "WKBrowsingContextGroupPrivate.h"
#import "WKContentView.h"
#import "WKProcessGroupPrivate.h"
#import "WKScrollView.h"
#import <UIKit/UIScreen.h>
#import <UIKit/UIScrollView_Private.h>
#import <WebKit2/RemoteLayerTreeTransaction.h>
#import <wtf/RetainPtr.h>

@interface WKView () <UIScrollViewDelegate, WKContentViewDelegate>
- (void)_setDocumentScale:(CGFloat)newScale;
@end

@implementation WKView {
    RetainPtr<WKScrollView> _scrollView;
    RetainPtr<WKContentView> _contentView;

    BOOL _isWaitingForNewLayerTreeAfterDidCommitLoad;
    BOOL _hasStaticMinimumLayoutSize;
    CGSize _minimumLayoutSizeOverride;
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

#pragma mark WKContentViewDelegate

- (void)contentViewDidCommitLoadForMainFrame:(WKContentView *)contentView
{
    _isWaitingForNewLayerTreeAfterDidCommitLoad = YES;
}

- (void)contentView:(WKContentView *)contentView didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    [_scrollView setMinimumZoomScale:layerTreeTransaction.minimumScaleFactor()];
    [_scrollView setMaximumZoomScale:layerTreeTransaction.maximumScaleFactor()];
    [_scrollView setZoomEnabled:layerTreeTransaction.allowsUserScaling()];
    if (![_scrollView isZooming] && ![_scrollView isZoomBouncing])
        [_scrollView setZoomScale:layerTreeTransaction.pageScaleFactor()];

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

    _contentView = adoptNS([[WKContentView alloc] initWithFrame:bounds contextRef:contextRef pageGroupRef:pageGroupRef relatedToPage:relatedPage]);
    [_contentView setDelegate:self];
    [[_contentView layer] setAnchorPoint:CGPointZero];
    [_contentView setFrame:bounds];
    [_scrollView addSubview:_contentView.get()];

    [self _frameOrBoundsChanged];
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

@end
