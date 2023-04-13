/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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
#import "WKActionSheetAssistant.h"

#if PLATFORM(IOS_FAMILY)

#import "APIUIClient.h"
#import "ImageAnalysisUtilities.h"
#import "UIKitSPI.h"
#import "WKActionSheet.h"
#import "WKContentViewInteraction.h"
#import "WKNSURLExtras.h"
#import "WebPageProxy.h"
#import "_WKActivatedElementInfoInternal.h"
#import "_WKElementActionInternal.h"
#import <UIKit/UIView.h>
#import <WebCore/DataDetection.h>
#import <WebCore/FloatRect.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/PathUtilities.h>
#import <wtf/CompletionHandler.h>
#import <wtf/SoftLinking.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>
#import <wtf/threads/BinarySemaphore.h>

#if HAVE(APP_LINKS)
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#endif

#if HAVE(SAFARI_SERVICES_FRAMEWORK)
#import "SafariServicesSPI.h"
SOFT_LINK_FRAMEWORK(SafariServices)
SOFT_LINK_CLASS(SafariServices, SSReadingList)
#endif

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
SOFT_LINK_LIBRARY_OPTIONAL(libAccessibility)
SOFT_LINK_OPTIONAL(libAccessibility, _AXSReduceMotionAutoplayAnimatedImagesEnabled, Boolean, (), ());
#endif

#import "TCCSoftLink.h"

OBJC_CLASS DDAction;

#if HAVE(APP_LINKS)
static bool applicationHasAppLinkEntitlements()
{
    static bool hasEntitlement = WTF::processHasEntitlement("com.apple.private.canGetAppLinkInfo"_s) && WTF::processHasEntitlement("com.apple.private.canModifyAppLinkPermissions"_s);
    return hasEntitlement;
}

static LSAppLink *appLinkForURL(NSURL *url)
{
#if HAVE(APP_LINKS_WITH_ISENABLED)
    NSArray<LSAppLink *> *appLinks = [LSAppLink appLinksWithURL:url limit:1 error:nil];
    return appLinks.firstObject;
#else
    BinarySemaphore semaphore;
    RetainPtr<LSAppLink> syncAppLink;
    [LSAppLink getAppLinkWithURL:url completionHandler:[&semaphore, &syncAppLink](LSAppLink *appLink, NSError *error) {
        syncAppLink = retainPtr(appLink);
        semaphore.signal();
    }];
    semaphore.wait();

    return syncAppLink.autorelease();
#endif
}
#endif

@implementation WKActionSheetAssistant {
    WeakObjCPtr<id <WKActionSheetAssistantDelegate>> _delegate;
    RetainPtr<WKActionSheet> _interactionSheet;
    RetainPtr<_WKActivatedElementInfo> _elementInfo;
    std::optional<WebKit::InteractionInformationAtPosition> _positionInformation;
#if USE(UICONTEXTMENU)
#if ENABLE(DATA_DETECTION)
    RetainPtr<UIContextMenuInteraction> _dataDetectorContextMenuInteraction;
#endif // ENABLE(DATA_DETECTION)
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    RetainPtr<UIContextMenuInteraction> _mediaControlsContextMenuInteraction;
    RetainPtr<UIMenu> _mediaControlsContextMenu;
    WebCore::FloatRect _mediaControlsContextMenuTargetFrame;
    CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)> _mediaControlsContextMenuCallback;
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
#endif // USE(UICONTEXTMENU)
    WeakObjCPtr<UIView> _view;
    BOOL _needsLinkIndicator;
    BOOL _isPresentingDDUserInterface;
    BOOL _hasPendingActionSheet;
}

- (id <WKActionSheetAssistantDelegate>)delegate
{
    return _delegate.getAutoreleased();
}

- (void)setDelegate:(id <WKActionSheetAssistantDelegate>)delegate
{
    _delegate = delegate;
}

- (id)initWithView:(UIView *)view
{
    _view = view;
    return self;
}

- (void)dealloc
{
    [self cleanupSheet];
#if USE(UICONTEXTMENU)
#if ENABLE(DATA_DETECTION)
    [self _removeDataDetectorContextMenuInteraction];
#endif // ENABLE(DATA_DETECTION)
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    [self _removeMediaControlsContextMenuInteraction];
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
#endif // USE(UICONTEXTMENU)
    [super dealloc];
}

- (BOOL)synchronouslyRetrievePositionInformation
{
    auto delegate = _delegate.get();
    if (!delegate)
        return NO;

    // FIXME: This should be asynchronous, since we control the presentation of the action sheet.
    _positionInformation = [delegate positionInformationForActionSheetAssistant:self];
    return !!_positionInformation;
}

- (UIView *)superviewForSheet
{
    UIView *view = _view.getAutoreleased();
    UIView *superview = [view window];

    // FIXME: WebKit has a delegate to retrieve the superview for the image sheet (superviewForImageSheetForWebView)
    // Do we need it in WK2?

    // Find the top most view with a view controller
    UIViewController *controller = nil;
    UIView *currentView = view;
    while (currentView) {
        UIViewController *aController = [UIViewController viewControllerForView:currentView];
        if (aController)
            controller = aController;

        currentView = [currentView superview];
    }
    if (controller)
        superview = controller.view;

    return superview;
}

- (CGRect)_presentationRectForSheetGivenPoint:(CGPoint)point inHostView:(UIView *)hostView
{
    CGPoint presentationPoint = [hostView convertPoint:point fromView:_view.getAutoreleased()];
    CGRect presentationRect = CGRectMake(presentationPoint.x, presentationPoint.y, 1.0, 1.0);

    return CGRectInset(presentationRect, -22.0, -22.0);
}

- (UIView *)hostViewForSheet
{
    return [self superviewForSheet];
}

static const CGFloat presentationElementRectPadding = 15;

- (CGRect)presentationRectForElementUsingClosestIndicatedRect
{
    UIView *view = [self superviewForSheet];
    auto delegate = _delegate.get();
    if (!view || !delegate || !_positionInformation)
        return CGRectZero;

    auto indicator = _positionInformation->linkIndicator;
    if (indicator.textRectsInBoundingRectCoordinates.isEmpty())
        return CGRectZero;

    WebCore::FloatPoint touchLocation = _positionInformation->request.point;
    WebCore::FloatPoint linkElementLocation = indicator.textBoundingRectInRootViewCoordinates.location();
    auto indicatedRects = indicator.textRectsInBoundingRectCoordinates.map([&](auto rect) {
        rect.inflate(2);
        rect.moveBy(linkElementLocation);
        return rect;
    });

    for (const auto& path : WebCore::PathUtilities::pathsWithShrinkWrappedRects(indicatedRects, 0)) {
        auto boundingRect = path.fastBoundingRect();
        if (boundingRect.contains(touchLocation))
            return CGRectInset([view convertRect:(CGRect)boundingRect fromView:_view.getAutoreleased()], -presentationElementRectPadding, -presentationElementRectPadding);
    }

    return CGRectZero;
}

- (CGRect)presentationRectForIndicatedElement
{
    UIView *view = [self superviewForSheet];
    auto delegate = _delegate.get();
    if (!view || !delegate || !_positionInformation)
        return CGRectZero;

    auto elementBounds = _positionInformation->bounds;
    return CGRectInset([view convertRect:elementBounds fromView:_view.getAutoreleased()], -presentationElementRectPadding, -presentationElementRectPadding);
}

- (CGRect)initialPresentationRectInHostViewForSheet
{
    UIView *view = [self superviewForSheet];
    auto delegate = _delegate.get();
    if (!view || !delegate || !_positionInformation)
        return CGRectZero;

    return [self _presentationRectForSheetGivenPoint:_positionInformation->request.point inHostView:view];
}

- (CGRect)presentationRectInHostViewForSheet
{
    UIView *view = [self superviewForSheet];
    auto delegate = _delegate.get();
    if (!view || !delegate || !_positionInformation)
        return CGRectZero;

    CGRect boundingRect = _positionInformation->bounds;
    CGPoint fromPoint = _positionInformation->request.point;

    // FIXME: We must adjust our presentation point to take into account a change in document scale.

    // Test to see if we are still within the target node as it may have moved after rotation.
    if (!CGRectContainsPoint(boundingRect, fromPoint))
        fromPoint = CGPointMake(CGRectGetMidX(boundingRect), CGRectGetMidY(boundingRect));

    return [self _presentationRectForSheetGivenPoint:fromPoint inHostView:view];
}

- (void)updatePositionInformation
{
    auto delegate = _delegate.get();
    if ([delegate respondsToSelector:@selector(updatePositionInformationForActionSheetAssistant:)])
        [delegate updatePositionInformationForActionSheetAssistant:self];
}

- (BOOL)presentSheet
{
    // Calculate the presentation rect just before showing.
    CGRect presentationRect = CGRectZero;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone) {
        presentationRect = [self initialPresentationRectInHostViewForSheet];
        if (CGRectIsEmpty(presentationRect))
            return NO;
    }
    ALLOW_DEPRECATED_DECLARATIONS_END

    return [_interactionSheet presentSheetFromRect:presentationRect];
}

- (void)updateSheetPosition
{
    [_interactionSheet updateSheetPosition];
}

- (BOOL)isShowingSheet
{
    return _interactionSheet != nil;
}

- (void)interactionDidStartWithPositionInformation:(const WebKit::InteractionInformationAtPosition&)information
{
#if ENABLE(DATA_DETECTION)
    if (!_delegate)
        return;

    if (!WebCore::DataDetection::canBePresentedByDataDetectors(information.url))
        return;

    NSURL *targetURL = information.url;
    if (!targetURL)
        return;

    auto *controller = [getDDDetectionControllerClass() sharedController];
    if ([controller respondsToSelector:@selector(interactionDidStartForURL:)])
        [controller interactionDidStartForURL:targetURL];
#endif
}

- (std::optional<WebKit::InteractionInformationAtPosition>)currentPositionInformation
{
    return _positionInformation;
}

static bool isJavaScriptURL(NSURL *url)
{
    auto scheme = url.scheme;
    return scheme && [scheme caseInsensitiveCompare:@"javascript"] == NSOrderedSame;
}

- (void)_createSheetWithElementActions:(NSArray *)actions defaultTitle:(NSString *)defaultTitle showLinkTitle:(BOOL)showLinkTitle
{
    auto delegate = _delegate.get();
    if (!delegate)
        return;

    if (!_positionInformation)
        return;

    NSURL *targetURL = _positionInformation->url;
    // FIXME: We should check if Javascript is enabled in the preferences.

    _interactionSheet = adoptNS([[WKActionSheet alloc] init]);
    _interactionSheet.get().sheetDelegate = self;
    _interactionSheet.get().preferredStyle = UIAlertControllerStyleActionSheet;

    NSString *titleString = nil;
    if (showLinkTitle && [[targetURL absoluteString] length]) {
        if (isJavaScriptURL(targetURL))
            titleString = WEB_UI_STRING_KEY("JavaScript", "JavaScript Action Sheet Title", "Title for action sheet for JavaScript link");
        else
            titleString = WTF::userVisibleString(targetURL);
    } else if (defaultTitle)
        titleString = defaultTitle;
    else
        titleString = _positionInformation->title;

    if ([titleString length]) {
        [_interactionSheet setTitle:titleString];
        // We should configure the text field's line breaking mode correctly here, based on whether
        // the title is an URL or not, but the appropriate UIAlertController SPIs are not available yet.
        // The code that used to do this in the UIActionSheet world has been saved for reference in
        // <rdar://problem/17049781> Configure the UIAlertController's title appropriately.
    }

    for (_WKElementAction *action in actions) {
        [_interactionSheet _addActionWithTitle:[action title] style:UIAlertActionStyleDefault handler:^{
            [action _runActionWithElementInfo:_elementInfo.get() forActionSheetAssistant:self];
            [self cleanupSheet];
        } shouldDismissHandler:^{
            return (BOOL)(!action.dismissalHandler || action.dismissalHandler());
        }];
    }

    [_interactionSheet addAction:[UIAlertAction actionWithTitle:WEB_UI_STRING_KEY("Cancel", "Cancel button label in button bar", "Title for Cancel button label in button bar")
                                                          style:UIAlertActionStyleCancel
                                                        handler:^(UIAlertAction *action) {
                                                            [self cleanupSheet];
                                                        }]];

    if ([delegate respondsToSelector:@selector(actionSheetAssistant:willStartInteractionWithElement:)])
        [delegate actionSheetAssistant:self willStartInteractionWithElement:_elementInfo.get()];
}

- (void)showImageSheet
{
    ASSERT(!_elementInfo);

    auto delegate = _delegate.get();
    if (!delegate)
        return;

    if (![self synchronouslyRetrievePositionInformation])
        return;

    void (^showImageSheetWithAlternateURLBlock)(NSURL*, NSDictionary *userInfo) = ^(NSURL *alternateURL, NSDictionary *userInfo) {
        NSURL *targetURL = _positionInformation->url;
        NSURL *imageURL = _positionInformation->imageURL;
        if (!targetURL)
            targetURL = alternateURL;
        auto elementBounds = _positionInformation->bounds;
        auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeImage URL:targetURL imageURL:imageURL location:_positionInformation->request.point title:_positionInformation->title ID:_positionInformation->idAttribute rect:elementBounds image:_positionInformation->image.get() imageMIMEType:_positionInformation->imageMIMEType userInfo:userInfo]);
        if ([delegate respondsToSelector:@selector(actionSheetAssistant:showCustomSheetForElement:)] && [delegate actionSheetAssistant:self showCustomSheetForElement:elementInfo.get()])
            return;
        auto defaultActions = [self defaultActionsForImageSheet:elementInfo.get()];

        RetainPtr<NSArray> actions = [delegate actionSheetAssistant:self decideActionsForElement:elementInfo.get() defaultActions:WTFMove(defaultActions)];

        if (![actions count])
            return;

        if (!alternateURL && userInfo) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [UIApp _cancelAllTouches];
ALLOW_DEPRECATED_DECLARATIONS_END
            return;
        }

        [self _createSheetWithElementActions:actions.get() defaultTitle:nil showLinkTitle:YES];
        if (!_interactionSheet)
            return;

        _elementInfo = WTFMove(elementInfo);

        if (![_interactionSheet presentSheet:[self _presentationStyleForPositionInfo:_positionInformation.value() elementInfo:_elementInfo.get()]])
            [self cleanupSheet];
    };

    if (_positionInformation->url.isEmpty() && _positionInformation->image && [delegate respondsToSelector:@selector(actionSheetAssistant:getAlternateURLForImage:completion:)]) {
        RetainPtr<UIImage> uiImage = adoptNS([[UIImage alloc] initWithCGImage:_positionInformation->image->makeCGImageCopy().get()]);

        _hasPendingActionSheet = YES;
        RetainPtr<WKActionSheetAssistant> retainedSelf(self);
        [delegate actionSheetAssistant:self getAlternateURLForImage:uiImage.get() completion:^(NSURL *alternateURL, NSDictionary *userInfo) {
            if (!retainedSelf->_hasPendingActionSheet)
                return;

            retainedSelf->_hasPendingActionSheet = NO;
            showImageSheetWithAlternateURLBlock(alternateURL, userInfo);
        }];
        return;
    }

    showImageSheetWithAlternateURLBlock(nil, nil);
}

- (WKActionSheetPresentationStyle)_presentationStyleForPositionInfo:(const WebKit::InteractionInformationAtPosition&)positionInfo elementInfo:(_WKActivatedElementInfo *)elementInfo
{
    auto apparentElementRect = [_view convertRect:positionInfo.bounds toView:[_view window]];
    if (CGRectIsEmpty(apparentElementRect))
        return WKActionSheetPresentAtTouchLocation;

    CGRect visibleRect;
    auto delegate = _delegate.get();
    if ([delegate respondsToSelector:@selector(unoccludedWindowBoundsForActionSheetAssistant:)])
        visibleRect = [delegate unoccludedWindowBoundsForActionSheetAssistant:self];
    else
        visibleRect = [[_view window] bounds];

    apparentElementRect = CGRectIntersection(apparentElementRect, visibleRect);
    auto leftInset = CGRectGetMinX(apparentElementRect) - CGRectGetMinX(visibleRect);
    auto topInset = CGRectGetMinY(apparentElementRect) - CGRectGetMinY(visibleRect);
    auto rightInset = CGRectGetMaxX(visibleRect) - CGRectGetMaxX(apparentElementRect);
    auto bottomInset = CGRectGetMaxY(visibleRect) - CGRectGetMaxY(apparentElementRect);

    // If at least this much of the window is available for the popover to draw in, then target the element rect when presenting the action menu popover.
    // Otherwise, there is not enough space to position the popover around the element, so revert to using the touch location instead.
    static const CGFloat minimumAvailableWidthOrHeightRatio = 0.4;
    if (std::max(leftInset, rightInset) <= minimumAvailableWidthOrHeightRatio * CGRectGetWidth(visibleRect) && std::max(topInset, bottomInset) <= minimumAvailableWidthOrHeightRatio * CGRectGetHeight(visibleRect))
        return WKActionSheetPresentAtTouchLocation;

    if (elementInfo.type == _WKActivatedElementTypeLink && positionInfo.linkIndicator.textRectsInBoundingRectCoordinates.size())
        return WKActionSheetPresentAtClosestIndicatorRect;

    return WKActionSheetPresentAtElementRect;
}

#if HAVE(APP_LINKS)
- (BOOL)_appendAppLinkOpenActionsForURL:(NSURL *)url actions:(NSMutableArray *)defaultActions elementInfo:(_WKActivatedElementInfo *)elementInfo
{
    ASSERT(_delegate);

    if (!applicationHasAppLinkEntitlements() || ![_delegate.get() actionSheetAssistant:self shouldIncludeAppLinkActionsForElement:elementInfo])
        return NO;

    LSAppLink *appLink = appLinkForURL(url);
    if (!appLink)
        return NO;

    NSString *openInDefaultBrowserTitle = WEB_UI_STRING("Open in Safari", "Title for Open in Safari Link action button");
    _WKElementAction *openInDefaultBrowserAction = [_WKElementAction _elementActionWithType:_WKElementActionTypeOpenInDefaultBrowser title:openInDefaultBrowserTitle actionHandler:^(_WKActivatedElementInfo *) {
#if HAVE(APP_LINKS_WITH_ISENABLED)
        appLink.enabled = NO;
        [appLink openWithCompletionHandler:nil];
#else
        [appLink openInWebBrowser:YES setAppropriateOpenStrategyAndWebBrowserState:nil completionHandler:^(BOOL success, NSError *error) { }];
#endif
    }];
    [defaultActions addObject:openInDefaultBrowserAction];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSString *externalApplicationName = appLink.targetApplicationProxy.localizedName;
ALLOW_DEPRECATED_DECLARATIONS_END
    if (!externalApplicationName)
        return YES;

    NSString *openInExternalApplicationTitle = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Open in “%@”", "Title for Open in External Application Link action button"), externalApplicationName];
    _WKElementAction *openInExternalApplicationAction = [_WKElementAction _elementActionWithType:_WKElementActionTypeOpenInExternalApplication title:openInExternalApplicationTitle actionHandler:^(_WKActivatedElementInfo *) {
#if HAVE(APP_LINKS_WITH_ISENABLED)
        appLink.enabled = YES;
        [appLink openWithCompletionHandler:nil];
#else
        [appLink openInWebBrowser:NO setAppropriateOpenStrategyAndWebBrowserState:nil completionHandler:^(BOOL success, NSError *error) { }];
#endif
    }];
    [defaultActions addObject:openInExternalApplicationAction];

    return YES;
}
#endif

- (void)_appendOpenActionsForURL:(NSURL *)url actions:(NSMutableArray *)defaultActions elementInfo:(_WKActivatedElementInfo *)elementInfo
{
#if HAVE(APP_LINKS)
    if ([self _appendAppLinkOpenActionsForURL:url actions:defaultActions elementInfo:elementInfo])
        return;
#endif

    [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeOpen info:elementInfo assistant:self]];
}

- (RetainPtr<NSArray<_WKElementAction *>>)defaultActionsForLinkSheet:(_WKActivatedElementInfo *)elementInfo
{
    NSURL *targetURL = [elementInfo URL];
    if (!targetURL)
        return nil;

    auto defaultActions = adoptNS([[NSMutableArray alloc] init]);
    [self _appendOpenActionsForURL:targetURL actions:defaultActions.get() elementInfo:elementInfo];

#if HAVE(SAFARI_SERVICES_FRAMEWORK)
    if ([getSSReadingListClass() supportsURL:targetURL])
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeAddToReadingList info:elementInfo assistant:self]];
#endif

    if ([elementInfo imageURL]) {
        if (TCCAccessPreflight(WebKit::get_TCC_kTCCServicePhotos(), NULL) != kTCCAccessPreflightDenied)
            [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeSaveImage info:elementInfo assistant:self]];
    }

    if (!isJavaScriptURL(targetURL)) {
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeCopy info:elementInfo assistant:self]];
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeShare info:elementInfo assistant:self]];
    }

    if (elementInfo.type == _WKActivatedElementTypeImage || elementInfo._isImage) {
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
        if ([_delegate respondsToSelector:@selector(actionSheetAssistantShouldIncludeCopySubjectAction:)] && [_delegate actionSheetAssistantShouldIncludeCopySubjectAction:self])
            [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeCopyCroppedImage info:elementInfo assistant:self]];
#endif
#if ENABLE(IMAGE_ANALYSIS)
        if ([_delegate respondsToSelector:@selector(actionSheetAssistant:shouldIncludeShowTextActionForElement:)] && [_delegate actionSheetAssistant:self shouldIncludeShowTextActionForElement:elementInfo])
            [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeImageExtraction info:elementInfo assistant:self]];
        if ([_delegate respondsToSelector:@selector(actionSheetAssistant:shouldIncludeLookUpImageActionForElement:)] && [_delegate actionSheetAssistant:self shouldIncludeLookUpImageActionForElement:elementInfo])
            [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeRevealImage info:elementInfo assistant:self]];
#endif
    }

    return defaultActions;
}

- (RetainPtr<NSArray<_WKElementAction *>>)defaultActionsForImageSheet:(_WKActivatedElementInfo *)elementInfo
{
    NSURL *targetURL = [elementInfo URL];

    auto defaultActions = adoptNS([[NSMutableArray alloc] init]);
    if (targetURL) {
        [self _appendOpenActionsForURL:targetURL actions:defaultActions.get() elementInfo:elementInfo];
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeShare info:elementInfo assistant:self]];
    } else if ([elementInfo imageURL])
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeShare info:elementInfo assistant:self]];

#if HAVE(SAFARI_SERVICES_FRAMEWORK)
    if ([getSSReadingListClass() supportsURL:targetURL])
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeAddToReadingList info:elementInfo assistant:self]];
#endif
    if (TCCAccessPreflight(WebKit::get_TCC_kTCCServicePhotos(), NULL) != kTCCAccessPreflightDenied)
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeSaveImage info:elementInfo assistant:self]];

    [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeCopy info:elementInfo assistant:self]];
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if ([_delegate respondsToSelector:@selector(actionSheetAssistantShouldIncludeCopySubjectAction:)] && [_delegate actionSheetAssistantShouldIncludeCopySubjectAction:self])
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeCopyCroppedImage info:elementInfo assistant:self]];
#endif

#if ENABLE(IMAGE_ANALYSIS)
    if ([_delegate respondsToSelector:@selector(actionSheetAssistant:shouldIncludeShowTextActionForElement:)] && [_delegate actionSheetAssistant:self shouldIncludeShowTextActionForElement:elementInfo])
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeImageExtraction info:elementInfo assistant:self]];
    if ([_delegate respondsToSelector:@selector(actionSheetAssistant:shouldIncludeLookUpImageActionForElement:)] && [_delegate actionSheetAssistant:self shouldIncludeLookUpImageActionForElement:elementInfo])
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeRevealImage info:elementInfo assistant:self]];
#endif

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    if (elementInfo.isAnimatedImage) {
        auto* autoplayAnimatedImagesFunction = _AXSReduceMotionAutoplayAnimatedImagesEnabledPtr();
        // Only show these controls if autoplay of animated images has been disabled.
        if (autoplayAnimatedImagesFunction && !autoplayAnimatedImagesFunction() && elementInfo.canShowAnimationControls) {
            if (elementInfo.isAnimating)
                [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionPauseAnimation info:elementInfo assistant:self]];
            else
                [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionPlayAnimation info:elementInfo assistant:self]];

        }
    }
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

    return defaultActions;
}

- (BOOL)needsLinkIndicator
{
    return _needsLinkIndicator;
}

- (void)showLinkSheet
{
    ASSERT(!_elementInfo);
    if (!_delegate)
        return;

    _needsLinkIndicator = YES;
    if (![self synchronouslyRetrievePositionInformation])
        return;

    NSURL *targetURL = _positionInformation->url;
    if (!targetURL) {
        _needsLinkIndicator = NO;
        return;
    }

    auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeLink URL:targetURL imageURL:(NSURL*)_positionInformation->imageURL location:_positionInformation->request.point title:_positionInformation->title ID:_positionInformation->idAttribute rect:_positionInformation->bounds image:_positionInformation->image.get() imageMIMEType:_positionInformation->imageMIMEType]);
    if ([_delegate respondsToSelector:@selector(actionSheetAssistant:showCustomSheetForElement:)] && [_delegate actionSheetAssistant:self showCustomSheetForElement:elementInfo.get()]) {
        _needsLinkIndicator = NO;
        return;
    }

    auto defaultActions = [self defaultActionsForLinkSheet:elementInfo.get()];

    RetainPtr<NSArray> actions = [_delegate actionSheetAssistant:self decideActionsForElement:elementInfo.get() defaultActions:WTFMove(defaultActions)];

    if (![actions count]) {
        _needsLinkIndicator = NO;
        return;
    }

    [self _createSheetWithElementActions:actions.get() defaultTitle:nil showLinkTitle:YES];
    if (!_interactionSheet) {
        _needsLinkIndicator = NO;
        return;
    }

    _elementInfo = WTFMove(elementInfo);

    if (![_interactionSheet presentSheet:[self _presentationStyleForPositionInfo:_positionInformation.value() elementInfo:_elementInfo.get()]])
        [self cleanupSheet];
}

#if USE(UICONTEXTMENU)

#if ENABLE(DATA_DETECTION)

- (UIContextMenuInteraction *)_ensureDataDetectorContextMenuInteraction
{
    if (!_dataDetectorContextMenuInteraction) {
        _dataDetectorContextMenuInteraction = adoptNS([[UIContextMenuInteraction alloc] initWithDelegate:self]);
        [_view addInteraction:_dataDetectorContextMenuInteraction.get()];
    }
    return _dataDetectorContextMenuInteraction.get();
}


- (void)_removeDataDetectorContextMenuInteraction
{
    if (!_dataDetectorContextMenuInteraction)
        return;

    [_view removeInteraction:_dataDetectorContextMenuInteraction.get()];
    _dataDetectorContextMenuInteraction = nil;

    if ([_delegate respondsToSelector:@selector(removeContextMenuViewIfPossibleForActionSheetAssistant:)])
        [_delegate removeContextMenuViewIfPossibleForActionSheetAssistant:self];
}

#endif // ENABLE(DATA_DETECTION)

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

- (UIContextMenuInteraction *)_ensureMediaControlsContextMenuInteraction
{
    if (!_mediaControlsContextMenuInteraction) {
        _mediaControlsContextMenuInteraction = adoptNS([[UIContextMenuInteraction alloc] initWithDelegate:self]);
        [_view addInteraction:_mediaControlsContextMenuInteraction.get()];
    }
    return _mediaControlsContextMenuInteraction.get();
}


- (void)_removeMediaControlsContextMenuInteraction
{
    if (!_mediaControlsContextMenuInteraction)
        return;

    [_view removeInteraction:_mediaControlsContextMenuInteraction.get()];
    _mediaControlsContextMenuInteraction = nil;

    _mediaControlsContextMenu = nil;
    _mediaControlsContextMenuTargetFrame = { };
    if (auto mediaControlsContextMenuCallback = std::exchange(_mediaControlsContextMenuCallback, { }))
        mediaControlsContextMenuCallback(WebCore::MediaControlsContextMenuItem::invalidID);

    if ([_delegate respondsToSelector:@selector(removeContextMenuViewIfPossibleForActionSheetAssistant:)])
        [_delegate removeContextMenuViewIfPossibleForActionSheetAssistant:self];
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

- (BOOL)hasContextMenuInteraction
{
#if ENABLE(DATA_DETECTION)
    if (_dataDetectorContextMenuInteraction)
        return YES;
#endif // ENABLE(DATA_DETECTION)

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (_mediaControlsContextMenuInteraction)
        return YES;
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

    return NO;
}

#endif // USE(UICONTEXTMENU)

#if ENABLE(DATA_DETECTION)

- (_WKElementAction *)_elementActionForDDAction:(DDAction *)action
{
    auto retainedSelf = retainPtr(self);
    _WKElementAction *elementAction = [_WKElementAction elementActionWithTitle:action.localizedName actionHandler:^(_WKActivatedElementInfo *actionInfo) {
        retainedSelf->_isPresentingDDUserInterface = action.hasUserInterface;
        [[getDDDetectionControllerClass() sharedController] performAction:action fromAlertController:retainedSelf->_interactionSheet.get() interactionDelegate:retainedSelf.get()];
    }];
    elementAction.dismissalHandler = ^BOOL {
        return !action.hasUserInterface;
    };
    return elementAction;
}

#endif // ENABLE(DATA_DETECTION)

- (void)showDataDetectorsUIForPositionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation
{
#if ENABLE(DATA_DETECTION)
    if (!_delegate)
        return;

    _positionInformation = positionInformation;

    if (!WebCore::DataDetection::canBePresentedByDataDetectors(_positionInformation->url))
        return;

    NSURL *targetURL = _positionInformation->url;
    if (!targetURL)
        return;

    DDDetectionController *controller = [getDDDetectionControllerClass() sharedController];
    NSDictionary *context = nil;
    NSString *textAtSelection = nil;

    if ([_delegate respondsToSelector:@selector(dataDetectionContextForActionSheetAssistant:positionInformation:)])
        context = [_delegate dataDetectionContextForActionSheetAssistant:self positionInformation:*_positionInformation];
    if ([_delegate respondsToSelector:@selector(selectedTextForActionSheetAssistant:)])
        textAtSelection = [_delegate selectedTextForActionSheetAssistant:self];

    if ([controller respondsToSelector:@selector(shouldImmediatelyLaunchDefaultActionForURL:)] && [controller shouldImmediatelyLaunchDefaultActionForURL:targetURL]) {
        auto action = [controller defaultActionForURL:targetURL results:_positionInformation->dataDetectorResults.get() context:context];
        auto *elementAction = [self _elementActionForDDAction:action];
        [elementAction _runActionWithElementInfo:_elementInfo.get() forActionSheetAssistant:self];
        return;
    }

    NSArray *dataDetectorsActions = [controller actionsForURL:targetURL identifier:_positionInformation->dataDetectorIdentifier selectedText:textAtSelection results:_positionInformation->dataDetectorResults.get() context:context];
    if ([dataDetectorsActions count] == 0)
        return;
    
#if USE(UICONTEXTMENU) && HAVE(UICONTEXTMENU_LOCATION)
    if ([_view window])
        [self._ensureDataDetectorContextMenuInteraction _presentMenuAtLocation:_positionInformation->request.point];
#else
    NSMutableArray *elementActions = [NSMutableArray array];
    for (NSUInteger actionNumber = 0; actionNumber < [dataDetectorsActions count]; actionNumber++) {
        DDAction *action = [dataDetectorsActions objectAtIndex:actionNumber];
        auto *elementAction = [self _elementActionForDDAction:action];
        [elementActions addObject:elementAction];
    }

    NSString *title = [controller titleForURL:targetURL results:_positionInformation->dataDetectorResults.get() context:context];
    [self _createSheetWithElementActions:elementActions defaultTitle:title showLinkTitle:NO];
    if (!_interactionSheet)
        return;

    if (elementActions.count <= 1)
        _interactionSheet.get().arrowDirections = UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;

    if (![_interactionSheet presentSheet:WKActionSheetPresentAtTouchLocation])
        [self cleanupSheet];
#endif
#endif // ENABLE(DATA_DETECTION)
}

#if USE(UICONTEXTMENU)

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

- (NSArray<UIMenuElement *> *)_uiMenuElementsForMediaControlContextMenuItems:(Vector<WebCore::MediaControlsContextMenuItem>&&)items
{
    return createNSArray(items, [&] (WebCore::MediaControlsContextMenuItem& item) -> UIMenuElement * {
        if (item.id == WebCore::MediaControlsContextMenuItem::invalidID && item.title.isEmpty() && item.icon.isEmpty() && item.children.isEmpty())
            return [UIMenu menuWithTitle:@"" image:nil identifier:nil options:UIMenuOptionsDisplayInline children:@[]];

        UIImage *image = !item.icon.isEmpty() ? [UIImage systemImageNamed:WTFMove(item.icon)] : nil;

        if (!item.children.isEmpty())
            return [UIMenu menuWithTitle:WTFMove(item.title) image:image identifier:nil options:0 children:[self _uiMenuElementsForMediaControlContextMenuItems:WTFMove(item.children)]];

        auto selectedItemID = item.id;
        UIAction *action = [UIAction actionWithTitle:WTFMove(item.title) image:image identifier:nil handler:[weakSelf = WeakObjCPtr<WKActionSheetAssistant>(self), selectedItemID] (UIAction *) {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            strongSelf->_mediaControlsContextMenuCallback(selectedItemID);
        }];
        if (item.checked)
            action.state = UIMenuElementStateOn;
        return action;
    }).autorelease();
}

- (void)showMediaControlsContextMenu:(WebCore::FloatRect&&)targetFrame items:(Vector<WebCore::MediaControlsContextMenuItem>&&)items completionHandler:(CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&&)completionHandler
{
    ASSERT(!_mediaControlsContextMenuInteraction);
    ASSERT(!_mediaControlsContextMenu);
    ASSERT(!_mediaControlsContextMenuCallback);

    String menuTitle;
    Vector<WebCore::MediaControlsContextMenuItem> itemsToPresent;
    if (items.size() == 1) {
        menuTitle = WTFMove(items[0].title);
        itemsToPresent = WTFMove(items[0].children);
    } else
        itemsToPresent = WTFMove(items);

    if (![_view window] || itemsToPresent.isEmpty()) {
        completionHandler(WebCore::MediaControlsContextMenuItem::invalidID);
        return;
    }

    _mediaControlsContextMenu = [UIMenu menuWithTitle:WTFMove(menuTitle) children:[self _uiMenuElementsForMediaControlContextMenuItems:WTFMove(itemsToPresent)]];
    _mediaControlsContextMenuTargetFrame = WTFMove(targetFrame);
    _mediaControlsContextMenuCallback = WTFMove(completionHandler);

    [self._ensureMediaControlsContextMenuInteraction _presentMenuAtLocation:_mediaControlsContextMenuTargetFrame.center()];
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

static NSMutableArray<UIMenuElement *> *menuElementsFromDefaultActions(RetainPtr<NSArray> defaultElementActions, RetainPtr<_WKActivatedElementInfo> elementInfo)
{
    if (![defaultElementActions count])
        return nil;

    auto actions = [NSMutableArray arrayWithCapacity:[defaultElementActions count]];
    for (_WKElementAction *elementAction in defaultElementActions.get())
        [actions addObject:[elementAction uiActionForElementInfo:elementInfo.get()]];

    return actions;
}

- (NSMutableArray<UIMenuElement *> *)suggestedActionsForContextMenuWithPositionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation
{
    auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithInteractionInformationAtPosition:positionInformation isUsingAlternateURLForImage:NO userInfo:nil]);
    RetainPtr<NSArray<_WKElementAction *>> defaultActionsFromAssistant = positionInformation.isLink ? [self defaultActionsForLinkSheet:elementInfo.get()] : [self defaultActionsForImageSheet:elementInfo.get()];
    return menuElementsFromDefaultActions(defaultActionsFromAssistant, elementInfo);
}


- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location
{
#if ENABLE(DATA_DETECTION)
    if (interaction == _dataDetectorContextMenuInteraction) {
        DDDetectionController *controller = [getDDDetectionControllerClass() sharedController];
        NSDictionary *context = nil;
        NSString *textAtSelection = nil;

        if ([_delegate respondsToSelector:@selector(dataDetectionContextForActionSheetAssistant:positionInformation:)])
            context = [_delegate dataDetectionContextForActionSheetAssistant:self positionInformation:*_positionInformation];
        if ([_delegate respondsToSelector:@selector(selectedTextForActionSheetAssistant:)])
            textAtSelection = [_delegate selectedTextForActionSheetAssistant:self];

        NSDictionary *newContext = nil;
        DDResultRef ddResult = [controller resultForURL:_positionInformation->url identifier:_positionInformation->dataDetectorIdentifier selectedText:textAtSelection results:_positionInformation->dataDetectorResults.get() context:context extendedContext:&newContext];

        CGRect sourceRect;
        if (_positionInformation->isLink)
            sourceRect = _positionInformation->linkIndicator.textBoundingRectInRootViewCoordinates;
        else
            sourceRect = _positionInformation->bounds;

        auto ddContextMenuActionClass = getDDContextMenuActionClass();
        auto finalContext = [ddContextMenuActionClass updateContext:newContext withSourceRect:sourceRect];

        if (ddResult)
            return [ddContextMenuActionClass contextMenuConfigurationWithResult:ddResult inView:_view.getAutoreleased() context:finalContext menuIdentifier:nil];
        return [ddContextMenuActionClass contextMenuConfigurationWithURL:_positionInformation->url inView:_view.getAutoreleased() context:finalContext menuIdentifier:nil];
    }
#endif // ENABLE(DATA_DETECTION)

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (interaction == _mediaControlsContextMenuInteraction) {
        return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:[weakSelf = WeakObjCPtr<WKActionSheetAssistant>(self)] (NSArray<UIMenuElement *> *suggestedActions) -> UIMenu * {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return nil;

            return strongSelf->_mediaControlsContextMenu.get();
        }];
    }
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

    return nil;
}

#if HAVE(UI_CONTEXT_MENU_PREVIEW_ITEM_IDENTIFIER)
- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configuration:(UIContextMenuConfiguration *)configuration highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier
#else
- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForHighlightingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
#endif
{
#if ENABLE(DATA_DETECTION)
    if (interaction == _dataDetectorContextMenuInteraction) {
        auto delegate = _delegate.get();
        CGPoint center = _positionInformation->request.point;

        if ([delegate respondsToSelector:@selector(createTargetedContextMenuHintForActionSheetAssistant:)])
            return [delegate createTargetedContextMenuHintForActionSheetAssistant:self];

        RetainPtr<UIPreviewParameters> unusedPreviewParameters = adoptNS([[UIPreviewParameters alloc] init]);
        RetainPtr<UIPreviewTarget> previewTarget = adoptNS([[UIPreviewTarget alloc] initWithContainer:_view.getAutoreleased() center:center]);
        RetainPtr<UITargetedPreview> preview = adoptNS([[UITargetedPreview alloc] initWithView:_view.getAutoreleased() parameters:unusedPreviewParameters.get() target:previewTarget.get()]);
        return preview.autorelease();
    }
#endif // ENABLE(DATA_DETECTION)

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (interaction == _mediaControlsContextMenuInteraction) {
        auto emptyView = adoptNS([[UIView alloc] initWithFrame:_mediaControlsContextMenuTargetFrame]);
        auto previewParameters = adoptNS([[UIPreviewParameters alloc] init]);
        auto previewTarget = adoptNS([[UIPreviewTarget alloc] initWithContainer:_view.getAutoreleased() center:_mediaControlsContextMenuTargetFrame.center()]);
        auto preview = adoptNS([[UITargetedPreview alloc] initWithView:emptyView.get() parameters:previewParameters.get() target:previewTarget.get()]);
        return preview.autorelease();
    }
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

    return nil;
}

- (_UIContextMenuStyle *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction styleForMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    _UIContextMenuStyle *style = [_UIContextMenuStyle defaultStyle];
    style.preferredLayout = _UIContextMenuLayoutCompactMenu;
    return style;
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willDisplayMenuForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id <UIContextMenuInteractionAnimating>)animator
{
    [animator addCompletion:[weakSelf = WeakObjCPtr<WKActionSheetAssistant>(self)] {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        if ([strongSelf->_delegate respondsToSelector:@selector(actionSheetAssistantDidShowContextMenu:)])
            [strongSelf->_delegate actionSheetAssistantDidShowContextMenu:strongSelf.get()];
    }];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id<UIContextMenuInteractionAnimating>)animator
{
#if ENABLE(DATA_DETECTION)
    if (interaction == _dataDetectorContextMenuInteraction)
        [self _removeDataDetectorContextMenuInteraction];
#endif // ENABLE(DATA_DETECTION)

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (interaction == _mediaControlsContextMenuInteraction)
        [self _removeMediaControlsContextMenuInteraction];
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

    [animator addCompletion:[weakSelf = WeakObjCPtr<WKActionSheetAssistant>(self)] {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        if ([strongSelf->_delegate respondsToSelector:@selector(actionSheetAssistantDidDismissContextMenu:)])
            [strongSelf->_delegate actionSheetAssistantDidDismissContextMenu:strongSelf.get()];
    }];
}

- (NSArray<UIMenuElement *> *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction overrideSuggestedActionsForConfiguration:(UIContextMenuConfiguration *)configuration
{
#if ENABLE(DATA_DETECTION)
    if (interaction == _dataDetectorContextMenuInteraction) {
        if (!_positionInformation)
            return nil;
        return [self suggestedActionsForContextMenuWithPositionInformation:*_positionInformation];
    }
#endif // ENABLE(DATA_DETECTION)

    return nil;
}

#endif // USE(UICONTEXTMENU)

- (void)handleElementActionWithType:(_WKElementActionType)type element:(_WKActivatedElementInfo *)element needsInteraction:(BOOL)needsInteraction
{
    auto delegate = _delegate.get();

    if (needsInteraction && [delegate respondsToSelector:@selector(actionSheetAssistant:willStartInteractionWithElement:)])
        [delegate actionSheetAssistant:self willStartInteractionWithElement:element];

    switch (type) {
    case _WKElementActionTypeCopy:
        [delegate actionSheetAssistant:self performAction:WebKit::SheetAction::Copy];
        break;
    case _WKElementActionTypeOpen:
        if (element._isUsingAlternateURLForImage)
            [[UIApplication sharedApplication] openURL:element.URL options:@{ } completionHandler:nil];
        else
            [delegate actionSheetAssistant:self openElementAtLocation:element._interactionLocation];
        break;
    case _WKElementActionTypeSaveImage:
        [delegate actionSheetAssistant:self performAction:WebKit::SheetAction::SaveImage];
        break;
    case _WKElementActionTypeShare:
        if (URL(element.imageURL).protocolIsData() && element.image && [delegate respondsToSelector:@selector(actionSheetAssistant:shareElementWithImage:rect:)])
            [delegate actionSheetAssistant:self shareElementWithImage:element.image rect:element.boundingRect];
        else {
            auto urlToShare = element.URL && !isJavaScriptURL(element.URL) ? element.URL : element.imageURL;
            [delegate actionSheetAssistant:self shareElementWithURL:urlToShare rect:element.boundingRect];
        }
        break;
    case _WKElementActionTypeImageExtraction:
#if ENABLE(IMAGE_ANALYSIS)
        [delegate actionSheetAssistant:self showTextForImage:element.image imageURL:element.imageURL title:element.title imageBounds:element.boundingRect];
#endif
        break;
    case _WKElementActionTypeRevealImage:
#if ENABLE(IMAGE_ANALYSIS)
        [delegate actionSheetAssistant:self lookUpImage:element.image imageURL:element.imageURL title:element.title imageBounds:element.boundingRect];
#endif
        break;
    case _WKElementActionTypeCopyCroppedImage:
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
        [delegate actionSheetAssistant:self copySubject:element.image sourceMIMEType:element.imageMIMEType];
#endif
        break;
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    case _WKElementActionPlayAnimation:
        [delegate actionSheetAssistant:self performAction:WebKit::SheetAction::PlayAnimation];
        break;
    case _WKElementActionPauseAnimation:
        [delegate actionSheetAssistant:self performAction:WebKit::SheetAction::PauseAnimation];
        break;
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (needsInteraction && [delegate respondsToSelector:@selector(actionSheetAssistantDidStopInteraction:)])
        [delegate actionSheetAssistantDidStopInteraction:self];
}

- (void)cleanupSheet
{
    auto delegate = _delegate.get();
    if ([delegate respondsToSelector:@selector(actionSheetAssistantDidStopInteraction:)])
        [delegate actionSheetAssistantDidStopInteraction:self];

    [_interactionSheet doneWithSheet:!_isPresentingDDUserInterface];
    [_interactionSheet setSheetDelegate:nil];
    _interactionSheet = nil;
    _elementInfo = nil;
    _positionInformation = std::nullopt;
    _needsLinkIndicator = NO;
    _isPresentingDDUserInterface = NO;
    _hasPendingActionSheet = NO;
}

#pragma mark - WKTesting

- (NSArray *)currentlyAvailableActionTitles
{
    if (!_interactionSheet)
        return @[];

    NSMutableArray *array = [NSMutableArray arrayWithCapacity:_interactionSheet.get().actions.count];

    for (UIAlertAction *action in _interactionSheet.get().actions)
        [array addObject:action.title];

    return array;
}

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

- (NSDictionary *)_contentsOfContextMenuItem:(UIMenuElement *)item
{
    NSMutableDictionary* result = NSMutableDictionary.dictionary;

    if (item.title.length)
        result[@"title"] = item.title;

    if ([item isKindOfClass:UIMenu.class]) {
        UIMenu *menu = (UIMenu *)item;

        NSMutableArray *children = [NSMutableArray arrayWithCapacity:menu.children.count];
        for (UIMenuElement *child in menu.children)
            [children addObject:[self _contentsOfContextMenuItem:child]];
        result[@"children"] = children;
    }

    if ([item isKindOfClass:UIAction.class]) {
        UIAction *action = (UIAction *)item;

        if (action.state == UIMenuElementStateOn)
            result[@"checked"] = @YES;
    }

    return result;
}

- (NSArray *)currentlyAvailableMediaControlsContextMenuItems
{
    NSMutableArray *result = NSMutableArray.array;
    if (_mediaControlsContextMenu)
        [result addObject:[self _contentsOfContextMenuItem:_mediaControlsContextMenu.get()]];
    return result;
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

@end

#endif // PLATFORM(IOS_FAMILY)
