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

#import "config.h"
#import "WKPreviewPopoverAnimationController.h"

#import "WKPagePreviewViewController.h"
#import "WKViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/FloatRect.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/NSPopoverSPI.h>

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

using namespace WebCore;
using namespace WebKit;

@interface WKPreviewPopoverAnimationController () <WKPagePreviewViewControllerDelegate, NSPopoverDelegate>
@end

@implementation WKPreviewPopoverAnimationController

+ (bool)_shouldImmediatelyShowPreview
{
    static bool shouldImmediatelyShowPreview;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shouldImmediatelyShowPreview = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitShouldImmediatelyShowPreview"];
    });
    return shouldImmediatelyShowPreview;
}

- (instancetype)initWithURL:(NSURL *)url view:(WKView *)wkView page:(WebPageProxy&)page originRect:(NSRect)originRect eventLocationInView:(NSPoint)eventLocationInView
{
    self = [super init];

    if (!self)
        return nil;

    _url = url;
    _wkView = wkView;
    _page = &page;
    _popoverOriginRect = originRect;

    // FIXME: We should be able to get this from the recognizer.
    _eventLocationInView = eventLocationInView;

    return self;
}

- (void)close
{
    [self _hidePreview];

    _wkView = nil;
    _page = nullptr;
}

- (void)setPreviewLoading:(BOOL)loading
{
    [_previewViewController setLoading:loading];
    _hasFinishedLoading = !loading;
    if (!loading && _shouldShowPreviewWhenLoaded)
        [self _showPreview];
}

- (void)setPreviewOverrideImage:(NSImage *)image
{
    NSSize imageSize = image.size;
    NSSize scaledImageSize;
    if ([_previewPopover isShown]) {
        NSSize currentPopoverContentSize = [_previewPopover contentSize];
        float scale = std::min(std::min(currentPopoverContentSize.width / imageSize.width, 1.0), std::min(currentPopoverContentSize.height / imageSize.height, 1.0));
        scaledImageSize = NSMakeSize(imageSize.width * scale, imageSize.height * scale);
    } else
        scaledImageSize = [self _preferredPopoverSizeWithPreviewPadding:[WKPagePreviewViewController previewPadding] forTargetSize:imageSize];

    [_previewPopover setContentSize:scaledImageSize];
    [_previewViewController replacePreviewWithImage:image atSize:scaledImageSize];
}

- (void)_hidePreview
{
    if (_page)
        _page->clearTextIndicator();

    if (_previewViewController) {
        _previewViewController->_delegate = nil;
        [_wkView _finishPreviewingURL:_previewViewController->_url.get() withPreviewView:_previewViewController->_previewView.get()];
        _previewViewController = nil;
    }

    [_previewPopover close];
    [_previewPopover setDelegate:nil];
    _previewPopover = nil;
}

- (void)setPreviewTitle:(NSString *)previewTitle
{
    [_previewViewController setPreviewTitle:previewTitle];
}

- (void)_createPreviewPopover
{
    NSSize previewPadding = [WKPagePreviewViewController previewPadding];
    NSSize popoverSize = [self _preferredPopoverSizeWithPreviewPadding:previewPadding forTargetSize:[self _targetSizeForPagePreview]];
    CGFloat actualPopoverToViewScale = popoverSize.width / NSWidth(_wkView.bounds);
    popoverSize.width += previewPadding.width;
    popoverSize.height += previewPadding.height;

    _previewViewController = adoptNS([[WKPagePreviewViewController alloc] initWithPageURL:_url.get() mainViewSize:_wkView.bounds.size popoverToViewScale:actualPopoverToViewScale]);
    _previewViewController->_delegate = self;
    [_previewViewController setLoading:YES];
    [_previewViewController loadView];

    _previewPopover = adoptNS([[NSPopover alloc] init]);
    [_previewPopover setBehavior:NSPopoverBehaviorTransient];
    [_previewPopover setContentSize:popoverSize];
    [_previewPopover setContentViewController:_previewViewController.get()];
    [_previewPopover setDelegate:self];
    [_previewPopover setPositioningOptions:NSPopoverPositioningOptionKeepTopStable];
}

- (void)_showPreview
{
    _shouldShowPreviewWhenLoaded = false;
    [_previewWatchdogTimer invalidate];
    _previewWatchdogTimer = nil;

    Class nsPopoverAnimationControllerClass = NSClassFromString(@"NSPopoverAnimationController");
    if (nsPopoverAnimationControllerClass) {
        _popoverAnimationController = [nsPopoverAnimationControllerClass popoverAnimationControllerWithPopover:_previewPopover.get()];
        [_popoverAnimationController setPreferredEdge:NSMaxYEdge];
        [_popoverAnimationController setAnchorView:_wkView];
        [_popoverAnimationController setPositioningRect:_popoverOriginRect];

        [_popoverAnimationController recognizerWillBeginAnimation:_recognizer];

        if (_didCompleteAnimation)
            [_popoverAnimationController recognizerDidCompleteAnimation:_recognizer];
        else
            [_popoverAnimationController recognizerDidUpdateAnimation:_recognizer];
    }
}

static bool targetSizeFitsInAvailableSpace(NSSize targetSize, NSSize availableSpace)
{
    return targetSize.width <= availableSpace.width && targetSize.height <= availableSpace.height;
}

- (NSSize)largestPopoverSize
{
    NSSize screenSize = _wkView.window.screen.frame.size;

    if (screenSize.width == 1280 && screenSize.height == 800)
        return NSMakeSize(1240, 674);

    if (screenSize.width == 1366 && screenSize.height == 768)
        return NSMakeSize(1264, 642);

    if (screenSize.width == 1440 && screenSize.height == 900)
        return NSMakeSize(1264, 760);

    if (screenSize.width == 1680 && screenSize.height == 1050)
        return NSMakeSize(1324, 910);

    return NSMakeSize(1324, 940);
}

- (NSSize)_targetSizeForPagePreview
{
    static const CGFloat preferredPopoverToViewScale = 0.75;
    NSRect wkViewBounds = _wkView.bounds;
    return NSMakeSize(NSWidth(wkViewBounds) * preferredPopoverToViewScale, NSHeight(wkViewBounds) * preferredPopoverToViewScale);
}

- (NSSize)_preferredPopoverSizeWithPreviewPadding:(NSSize)previewPadding forTargetSize:(NSSize)targetSize
{
    static const NSSize screenPadding = NSMakeSize(40, 40);
    static const NSSize smallestPopoverSize = NSMakeSize(500, 500);

    const NSSize effectivePadding = NSMakeSize(screenPadding.width + previewPadding.width, screenPadding.height + previewPadding.height);

    NSWindow *window = _wkView.window;
    NSRect originScreenRect = [window convertRectToScreen:[_wkView convertRect:_popoverOriginRect toView:nil]];
    NSRect screenFrame = window.screen.visibleFrame;

    NSSize largestPopoverSize = [self largestPopoverSize];

    CGFloat availableSpaceAbove = NSMaxY(screenFrame) - NSMaxY(originScreenRect);
    CGFloat availableSpaceBelow = NSMinY(originScreenRect) - NSMinY(screenFrame);
    CGFloat maxAvailableVerticalSpace = fmax(availableSpaceAbove, availableSpaceBelow) - effectivePadding.height;
    NSSize maxSpaceAvailableOnYEdge = NSMakeSize(screenFrame.size.width - effectivePadding.height, maxAvailableVerticalSpace);
    if (targetSizeFitsInAvailableSpace(targetSize, maxSpaceAvailableOnYEdge) && targetSizeFitsInAvailableSpace(targetSize, largestPopoverSize))
        return targetSize;

    CGFloat availableSpaceAtLeft = NSMinX(originScreenRect) - NSMinX(screenFrame);
    CGFloat availableSpaceAtRight = NSMaxX(screenFrame) - NSMaxX(originScreenRect);
    CGFloat maxAvailableHorizontalSpace = fmax(availableSpaceAtLeft, availableSpaceAtRight) - effectivePadding.width;
    NSSize maxSpaceAvailableOnXEdge = NSMakeSize(maxAvailableHorizontalSpace, screenFrame.size.height - effectivePadding.width);
    if (targetSizeFitsInAvailableSpace(targetSize, maxSpaceAvailableOnXEdge) && targetSizeFitsInAvailableSpace(targetSize, largestPopoverSize))
        return targetSize;

    // Adjust the maximum space available if it is larger than the largest popover size.
    if (maxSpaceAvailableOnYEdge.width > largestPopoverSize.width && maxSpaceAvailableOnYEdge.height > largestPopoverSize.height)
        maxSpaceAvailableOnYEdge = largestPopoverSize;
    if (maxSpaceAvailableOnXEdge.width > largestPopoverSize.width && maxSpaceAvailableOnXEdge.height > largestPopoverSize.height)
        maxSpaceAvailableOnXEdge = largestPopoverSize;

    // If the target size doesn't fit anywhere, we'll find the largest rect that does fit that also maintains the original view's aspect ratio.
    CGFloat aspectRatio = targetSize.width / targetSize.height;
    FloatRect maxVerticalTargetSizePreservingAspectRatioRect = largestRectWithAspectRatioInsideRect(aspectRatio, FloatRect(FloatPoint(), FloatSize(maxSpaceAvailableOnYEdge)));
    FloatRect maxHorizontalTargetSizePreservingAspectRatioRect = largestRectWithAspectRatioInsideRect(aspectRatio, FloatRect(FloatPoint(), FloatSize(maxSpaceAvailableOnXEdge)));

    NSSize maxVerticalTargetSizePreservingAspectRatio = NSMakeSize(maxVerticalTargetSizePreservingAspectRatioRect.width(), maxVerticalTargetSizePreservingAspectRatioRect.height());
    NSSize maxHortizontalTargetSizePreservingAspectRatio = NSMakeSize(maxHorizontalTargetSizePreservingAspectRatioRect.width(), maxHorizontalTargetSizePreservingAspectRatioRect.height());

    NSSize computedTargetSize;
    if ((maxVerticalTargetSizePreservingAspectRatio.width * maxVerticalTargetSizePreservingAspectRatio.height) > (maxHortizontalTargetSizePreservingAspectRatio.width * maxHortizontalTargetSizePreservingAspectRatio.height))
        computedTargetSize = maxVerticalTargetSizePreservingAspectRatio;
    else
        computedTargetSize = maxHortizontalTargetSizePreservingAspectRatio;

    // Now make sure what we've computed isn't too small.
    if (computedTargetSize.width < smallestPopoverSize.width && computedTargetSize.height < smallestPopoverSize.height) {
        float limitWidth = smallestPopoverSize.width > computedTargetSize.width ? smallestPopoverSize.width : computedTargetSize.width;
        float limitHeight = smallestPopoverSize.height > computedTargetSize.height ? smallestPopoverSize.height : computedTargetSize.height;
        FloatRect targetRectLargerThanMinSize = largestRectWithAspectRatioInsideRect(aspectRatio, FloatRect(0, 0, limitWidth, limitHeight));
        computedTargetSize = NSMakeSize(targetRectLargerThanMinSize.size().width(), targetRectLargerThanMinSize.size().height());

        // If our orignal computedTargetSize was so small that we had to get here and make a new computedTargetSize that is
        // larger than the minimum, then the elementBoundingBox of the _hitTestResult is probably huge. So we should use
        // the event origin as the popover origin in this case and not worry about obscuring the _hitTestResult.
        _popoverOriginRect.origin = _eventLocationInView;
        _popoverOriginRect.size = NSMakeSize(1, 1);
    }
    
    return computedTargetSize;
}

- (void)_previewWatchdogTimerFired:(NSTimer *)timer
{
    [self _showPreview];
    _previewWatchdogTimer = nil;
}

#pragma mark NSImmediateActionAnimationController

- (void)recognizerWillBeginAnimation:(NSImmediateActionGestureRecognizer *)recognizer
{
    _recognizer = recognizer;
    _didCompleteAnimation = false;
    [self _createPreviewPopover];

    if ([self.class _shouldImmediatelyShowPreview])
        [self _showPreview];
    else {
        _shouldShowPreviewWhenLoaded = true;
        _previewWatchdogTimer = [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(_previewWatchdogTimerFired:) userInfo:nil repeats:NO];
    }
}

- (void)recognizerDidUpdateAnimation:(NSImmediateActionGestureRecognizer *)recognizer
{
    [_popoverAnimationController recognizerDidUpdateAnimation:recognizer];
}

- (void)recognizerDidCancelAnimation:(NSImmediateActionGestureRecognizer *)recognizer
{
    [self _hidePreview];

    [_popoverAnimationController recognizerDidCancelAnimation:recognizer];
}

- (void)recognizerDidCompleteAnimation:(NSImmediateActionGestureRecognizer *)recognizer
{
    _didCompleteAnimation = true;
    [_popoverAnimationController recognizerDidCompleteAnimation:recognizer];
}

- (void)recognizerDidDismissAnimation:(NSImmediateActionGestureRecognizer *)recognizer
{
    [self _hidePreview];

    [_popoverAnimationController recognizerDidDismissAnimation:recognizer];
}

#pragma mark WKPagePreviewViewControllerDelegate

- (NSView *)pagePreviewViewController:(WKPagePreviewViewController *)pagePreviewViewController viewForPreviewingURL:(NSURL *)url initialFrameSize:(NSSize)initialFrameSize
{
    return [_wkView _viewForPreviewingURL:url initialFrameSize:initialFrameSize];
}

- (NSString *)pagePreviewViewController:(WKPagePreviewViewController *)pagePreviewViewController titleForPreviewOfURL:(NSURL *)url
{
    return [_wkView _titleForPreviewOfURL:url];
}

- (void)pagePreviewViewControllerWasClicked:(WKPagePreviewViewController *)pagePreviewViewController
{
    if (NSURL *url = pagePreviewViewController->_url.get())
        [_wkView _handleClickInPreviewView:pagePreviewViewController->_previewView.get() URL:url];
}

#pragma mark NSPopoverDelegate

- (void)popoverWillClose:(NSNotification *)notification
{
    [self _hidePreview];
}

@end


#endif // PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
