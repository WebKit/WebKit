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
#import "WKImmediateActionController.h"

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

#import "WKNSURLExtras.h"
#import "WKPagePreviewViewController.h"
#import "WKViewInternal.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebPageProxyMessages.h"
#import "WebProcessProxy.h"
#import <WebCore/GeometryUtilities.h>
#import <WebCore/NSImmediateActionGestureRecognizerSPI.h>
#import <WebCore/NSMenuSPI.h>
#import <WebCore/QuickLookMacSPI.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/URL.h>

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(Quartz, QuickLookUI)
SOFT_LINK_CLASS(QuickLookUI, QLPreviewMenuItem)

using namespace WebCore;
using namespace WebKit;

@interface WKImmediateActionController () <NSPopoverDelegate, QLPreviewMenuItemDelegate>
@end

#if WK_API_ENABLED
@interface WKImmediateActionController () <WKPagePreviewViewControllerDelegate>
@end
#endif

@implementation WKImmediateActionController

- (instancetype)initWithPage:(WebPageProxy&)page view:(WKView *)wkView recognizer:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    self = [super init];

    if (!self)
        return nil;

    _page = &page;
    _wkView = wkView;
    _type = kWKImmediateActionNone;
    _immediateActionRecognizer = immediateActionRecognizer;

    return self;
}

- (void)willDestroyView:(WKView *)view
{
    _page = nullptr;
    _wkView = nil;
    _hitTestResult = ActionMenuHitTestResult();
}

- (void)_clearImmediateActionState
{
    _state = ImmediateActionState::None;
    _hitTestResult = ActionMenuHitTestResult();
    _type = kWKImmediateActionNone;
}

- (void)didPerformActionMenuHitTest:(const ActionMenuHitTestResult&)hitTestResult userData:(API::Object*)userData
{
    // FIXME: This needs to use the WebKit2 callback mechanism to avoid out-of-order replies.
    _state = ImmediateActionState::Ready;
    _hitTestResult = hitTestResult;

    [self _updateImmediateActionItem];
}

#pragma mark NSGestureRecognizerDelegate

- (void)immediateActionRecognizerWillPrepare:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    _eventLocationInView = [immediateActionRecognizer locationInView:immediateActionRecognizer.view];
    _page->performActionMenuHitTestAtLocation(_eventLocationInView);

    _state = ImmediateActionState::Pending;
    [self _updateImmediateActionItem];
}

- (void)immediateActionRecognizerWillBeginAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    ASSERT(_state != ImmediateActionState::None);

    // FIXME: We need to be able to cancel this if the gesture recognizer goes away.
    // FIXME: Connection can be null if the process is closed; we should clean up better in that case.
    if (_state == ImmediateActionState::Pending) {
        if (auto* connection = _page->process().connection()) {
            bool receivedReply = connection->waitForAndDispatchImmediately<Messages::WebPageProxy::DidPerformActionMenuHitTest>(_page->pageID(), std::chrono::milliseconds(500));
            if (!receivedReply)
                _state = ImmediateActionState::TimedOut;
        }
    }

    if (_state != ImmediateActionState::Ready)
        [self _updateImmediateActionItem];
}

- (void)immediateActionRecognizerDidCancelAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    [self _clearImmediateActionState];
}

- (void)immediateActionRecognizerDidCompleteAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    // FIXME: Add support for the types of functionality provided in Action menu's willOpenMenu.
}

- (PassRefPtr<WebHitTestResult>)_webHitTestResult
{
    RefPtr<WebHitTestResult> hitTestResult;
    if (_state == ImmediateActionState::Ready)
        hitTestResult = WebHitTestResult::create(_hitTestResult.hitTestResult);
    else
        hitTestResult = _page->lastMouseMoveHitTestResult();

    return hitTestResult.release();
}

#pragma mark Immediate actions

- (void)_updateImmediateActionItem
{
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];

    _type = kWKImmediateActionNone;
    _immediateActionRecognizer.animationController = nil;

    if (!hitTestResult)
        return;

    String absoluteLinkURL = hitTestResult->absoluteLinkURL();
    if (!absoluteLinkURL.isEmpty() && WebCore::protocolIsInHTTPFamily(absoluteLinkURL)) {
        _type = kWKImmediateActionLinkPreview;

        BOOL shouldUseStandardQuickLookPreview = [_wkView _shouldUseStandardQuickLookPreview] && [NSMenuItem respondsToSelector:@selector(standardQuickLookMenuItem)];
        if (shouldUseStandardQuickLookPreview) {
            RetainPtr<NSMenuItem> previewLinkItem;
            RetainPtr<QLPreviewMenuItem> qlPreviewLinkItem;
            if (shouldUseStandardQuickLookPreview) {
                qlPreviewLinkItem = [NSMenuItem standardQuickLookMenuItem];
                [qlPreviewLinkItem setPreviewStyle:QLPreviewStylePopover];
                [qlPreviewLinkItem setDelegate:self];
            }
            _immediateActionRecognizer.animationController = (id<NSImmediateActionAnimationController>)qlPreviewLinkItem.get();
        } else {
#if WK_API_ENABLED
            [self _createPreviewPopoverIfNeededForURL:absoluteLinkURL];
            _immediateActionRecognizer.animationController = (id<NSImmediateActionAnimationController>)_previewPopover.get();
#endif // WK_API_ENABLED
        }

        return;
    }
}

#pragma mark Link Preview action

#if WK_API_ENABLED

- (void)_createPreviewPopoverIfNeededForURL:(String)absoluteLinkURL
{
    if (_previewPopoverOriginalURL == absoluteLinkURL)
        return;

    _previewPopoverOriginalURL = absoluteLinkURL;

    NSURL *url = [NSURL _web_URLWithWTFString:absoluteLinkURL];
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
    _popoverOriginRect = hitTestResult->elementBoundingBox();

    NSSize previewPadding = [WKPagePreviewViewController previewPadding];
    NSSize popoverSize = [self _preferredPopoverSizeWithPreviewPadding:previewPadding];
    CGFloat actualPopoverToViewScale = popoverSize.width / NSWidth(_wkView.bounds);
    popoverSize.width += previewPadding.width;
    popoverSize.height += previewPadding.height;

    _previewViewController = adoptNS([[WKPagePreviewViewController alloc] initWithPageURL:url mainViewSize:_wkView.bounds.size popoverToViewScale:actualPopoverToViewScale]);
    _previewViewController->_delegate = self;
    [_previewViewController loadView];

    _previewPopover = adoptNS([[NSPopover alloc] init]);
    [_previewPopover setBehavior:NSPopoverBehaviorTransient];
    [_previewPopover setContentSize:popoverSize];
    [_previewPopover setContentViewController:_previewViewController.get()];
    [_previewPopover setDelegate:self];
}

- (void)_clearPreviewPopover
{
    if (_previewViewController) {
        _previewViewController->_delegate = nil;
        [_wkView _finishPreviewingURL:_previewViewController->_url.get() withPreviewView:_previewViewController->_previewView.get()];
        _previewViewController = nil;
    }

    [_previewPopover close];
    [_previewPopover setDelegate:nil];
    _previewPopover = nil;
    _previewPopoverOriginalURL = String();
}

- (void)setPreviewTitle:(NSString *)previewTitle
{
    [_previewViewController setPreviewTitle:previewTitle];
}

- (void)popoverWillClose:(NSNotification *)notification
{
    [self _clearPreviewPopover];
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

- (NSSize)_preferredPopoverSizeWithPreviewPadding:(NSSize)previewPadding
{
    static const CGFloat preferredPopoverToViewScale = 0.75;
    static const NSSize screenPadding = {40, 40};
    static const NSSize smallestPopoverSize = NSMakeSize(500, 300);

    const NSSize effectivePadding = NSMakeSize(screenPadding.width + previewPadding.width, screenPadding.height + previewPadding.height);

    NSWindow *window = _wkView.window;
    NSRect originScreenRect = [window convertRectToScreen:[_wkView convertRect:_popoverOriginRect toView:nil]];
    NSRect screenFrame = window.screen.visibleFrame;

    NSRect wkViewBounds = _wkView.bounds;
    NSSize targetSize = NSMakeSize(NSWidth(wkViewBounds) * preferredPopoverToViewScale, NSHeight(wkViewBounds) * preferredPopoverToViewScale);
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
    CGFloat aspectRatio = wkViewBounds.size.width / wkViewBounds.size.height;
    FloatRect maxVerticalTargetSizePreservingAspectRatioRect = largestRectWithAspectRatioInsideRect(aspectRatio, FloatRect(0, 0, maxSpaceAvailableOnYEdge.width, maxSpaceAvailableOnYEdge.height));
    FloatRect maxHorizontalTargetSizePreservingAspectRatioRect = largestRectWithAspectRatioInsideRect(aspectRatio, FloatRect(0, 0, maxSpaceAvailableOnXEdge.width, maxSpaceAvailableOnXEdge.height));

    NSSize maxVerticalTargetSizePreservingAspectRatio = NSMakeSize(maxVerticalTargetSizePreservingAspectRatioRect.width(), maxVerticalTargetSizePreservingAspectRatioRect.height());
    NSSize maxHortizontalTargetSizePreservingAspectRatio = NSMakeSize(maxHorizontalTargetSizePreservingAspectRatioRect.width(), maxHorizontalTargetSizePreservingAspectRatioRect.height());

    NSSize computedTargetSize;
    if ((maxVerticalTargetSizePreservingAspectRatio.width * maxVerticalTargetSizePreservingAspectRatio.height) > (maxHortizontalTargetSizePreservingAspectRatio.width * maxHortizontalTargetSizePreservingAspectRatio.height))
        computedTargetSize = maxVerticalTargetSizePreservingAspectRatio;
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

#endif // WK_API_ENABLED

#pragma mark QLPreviewMenuItemDelegate implementation

- (NSView *)menuItem:(NSMenuItem *)menuItem viewAtScreenPoint:(NSPoint)screenPoint
{
    return _wkView;
}

- (id<QLPreviewItem>)menuItem:(NSMenuItem *)menuItem previewItemAtPoint:(NSPoint)point
{
    if (!_wkView)
        return nil;

    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
    return [NSURL _web_URLWithWTFString:hitTestResult->absoluteLinkURL()];
}

- (NSRectEdge)menuItem:(NSMenuItem *)menuItem preferredEdgeForPoint:(NSPoint)point
{
    return NSMaxYEdge;
}

@end

#endif // PLATFORM(MAC)
