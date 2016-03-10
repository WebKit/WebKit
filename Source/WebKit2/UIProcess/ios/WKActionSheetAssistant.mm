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
#import "WKActionSheetAssistant.h"

#if PLATFORM(IOS)

#import "APIUIClient.h"
#import "SandboxUtilities.h"
#import "TCCSPI.h"
#import "UIKitSPI.h"
#import "WKActionSheet.h"
#import "WKContentViewInteraction.h"
#import "WKNSURLExtras.h"
#import "WeakObjCPtr.h"
#import "WebPageProxy.h"
#import "_WKActivatedElementInfoInternal.h"
#import "_WKElementActionInternal.h"
#import <UIKit/UIView.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/WebCoreNSURLExtras.h>
#import <wtf/text/WTFString.h>

#if HAVE(APP_LINKS)
#import <WebCore/LaunchServicesSPI.h>
#endif

#if HAVE(SAFARI_SERVICES_FRAMEWORK)
#import <SafariServices/SSReadingList.h>
SOFT_LINK_FRAMEWORK(SafariServices)
SOFT_LINK_CLASS(SafariServices, SSReadingList)
#endif

SOFT_LINK_PRIVATE_FRAMEWORK(TCC)
SOFT_LINK(TCC, TCCAccessPreflight, TCCAccessPreflightResult, (CFStringRef service, CFDictionaryRef options), (service, options))
SOFT_LINK_CONSTANT(TCC, kTCCServicePhotos, CFStringRef)

@interface DDDetectionController (StagingToRemove)
- (NSArray *)actionsForURL:(NSURL *)url identifier:(NSString *)identifier selectedText:(NSString *)selectedText results:(NSArray *)results context:(NSDictionary *)context;
@end

using namespace WebKit;

#if HAVE(APP_LINKS)
static bool applicationHasAppLinkEntitlements()
{
    static bool hasEntitlement = processHasEntitlement(@"com.apple.private.canGetAppLinkInfo") && processHasEntitlement(@"com.apple.private.canModifyAppLinkPermissions");
    return hasEntitlement;
}

static LSAppLink *appLinkForURL(NSURL *url)
{
    __block LSAppLink *syncAppLink = nil;

    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    [LSAppLink getAppLinkWithURL:url completionHandler:^(LSAppLink *appLink, NSError *error) {
        syncAppLink = [appLink retain];
        dispatch_semaphore_signal(semaphore);
    }];
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

    return [syncAppLink autorelease];
}
#endif

@implementation WKActionSheetAssistant {
    WeakObjCPtr<id <WKActionSheetAssistantDelegate>> _delegate;
    RetainPtr<WKActionSheet> _interactionSheet;
    RetainPtr<_WKActivatedElementInfo> _elementInfo;
    UIView *_view;
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
    [super dealloc];
}

- (UIView *)superviewForSheet
{
    UIView *view = [_view window];

    // FIXME: WebKit has a delegate to retrieve the superview for the image sheet (superviewForImageSheetForWebView)
    // Do we need it in WK2?

    // Find the top most view with a view controller
    UIViewController *controller = nil;
    UIView *currentView = _view;
    while (currentView) {
        UIViewController *aController = [UIViewController viewControllerForView:currentView];
        if (aController)
            controller = aController;

        currentView = [currentView superview];
    }
    if (controller)
        view = controller.view;

    return view;
}

- (CGRect)_presentationRectForSheetGivenPoint:(CGPoint)point inHostView:(UIView *)hostView
{
    CGPoint presentationPoint = [hostView convertPoint:point fromView:_view];
    CGRect presentationRect = CGRectMake(presentationPoint.x, presentationPoint.y, 1.0, 1.0);

    return CGRectInset(presentationRect, -22.0, -22.0);
}

- (UIView *)hostViewForSheet
{
    return [self superviewForSheet];
}

- (CGRect)initialPresentationRectInHostViewForSheet
{
    UIView *view = [self superviewForSheet];
    auto delegate = _delegate.get();
    if (!view || !delegate)
        return CGRectZero;

    return [self _presentationRectForSheetGivenPoint:[delegate positionInformationForActionSheetAssistant:self].point inHostView:view];
}

- (CGRect)presentationRectInHostViewForSheet
{
    UIView *view = [self superviewForSheet];
    auto delegate = _delegate.get();
    if (!view || !delegate)
        return CGRectZero;

    const auto& positionInformation = [delegate positionInformationForActionSheetAssistant:self];

    CGRect boundingRect = positionInformation.bounds;
    CGPoint fromPoint = positionInformation.point;

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
    if (UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone) {
        presentationRect = [self initialPresentationRectInHostViewForSheet];
        if (CGRectIsEmpty(presentationRect))
            return NO;
    }

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

- (void)_createSheetWithElementActions:(NSArray *)actions showLinkTitle:(BOOL)showLinkTitle
{
    auto delegate = _delegate.get();
    if (!delegate)
        return;

    const auto& positionInformation = [delegate positionInformationForActionSheetAssistant:self];

    NSURL *targetURL = [NSURL URLWithString:positionInformation.url];
    NSString *urlScheme = [targetURL scheme];
    BOOL isJavaScriptURL = [urlScheme length] && [urlScheme caseInsensitiveCompare:@"javascript"] == NSOrderedSame;
    // FIXME: We should check if Javascript is enabled in the preferences.

    _interactionSheet = adoptNS([[WKActionSheet alloc] init]);
    _interactionSheet.get().sheetDelegate = self;
    _interactionSheet.get().preferredStyle = UIAlertControllerStyleActionSheet;

    NSString *titleString = nil;
    BOOL titleIsURL = NO;
    if (showLinkTitle && [[targetURL absoluteString] length]) {
        if (isJavaScriptURL)
            titleString = WEB_UI_STRING_KEY("JavaScript", "JavaScript Action Sheet Title", "Title for action sheet for JavaScript link");
        else {
            titleString = WebCore::userVisibleString(targetURL);
            titleIsURL = YES;
        }
    } else
        titleString = positionInformation.title;

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

    const auto& positionInformation = [delegate positionInformationForActionSheetAssistant:self];

    NSURL *targetURL = [NSURL _web_URLWithWTFString:positionInformation.url];
    auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeImage URL:targetURL location:positionInformation.point title:positionInformation.title rect:positionInformation.bounds image:positionInformation.image.get()]);
    if ([delegate respondsToSelector:@selector(actionSheetAssistant:showCustomSheetForElement:)] && [delegate actionSheetAssistant:self showCustomSheetForElement:elementInfo.get()])
        return;
    auto defaultActions = [self defaultActionsForImageSheet:elementInfo.get()];

    RetainPtr<NSArray> actions = [delegate actionSheetAssistant:self decideActionsForElement:elementInfo.get() defaultActions:WTFMove(defaultActions)];

    if (![actions count])
        return;

    [self _createSheetWithElementActions:actions.get() showLinkTitle:YES];
    if (!_interactionSheet)
        return;

    _elementInfo = WTFMove(elementInfo);

    if (![_interactionSheet presentSheet])
        [self cleanupSheet];
}

- (void)_appendOpenActionsForURL:(NSURL *)url actions:(NSMutableArray *)defaultActions elementInfo:(_WKActivatedElementInfo *)elementInfo
{
#if HAVE(APP_LINKS)
    ASSERT(_delegate);
    if (applicationHasAppLinkEntitlements() && [_delegate.get() actionSheetAssistant:self shouldIncludeAppLinkActionsForElement:elementInfo]) {
        LSAppLink *appLink = appLinkForURL(url);
        if (appLink) {
            NSString *title = WEB_UI_STRING("Open in Safari", "Title for Open in Safari Link action button");
            _WKElementAction *openInDefaultBrowserAction = [_WKElementAction _elementActionWithType:_WKElementActionTypeOpenInDefaultBrowser title:title actionHandler:^(_WKActivatedElementInfo *) {
                [appLink openInWebBrowser:YES setAppropriateOpenStrategyAndWebBrowserState:nil completionHandler:^(BOOL success, NSError *error) { }];
            }];
            [defaultActions addObject:openInDefaultBrowserAction];

            NSString *externalApplicationName = [appLink.targetApplicationProxy localizedNameForContext:nil];
            if (externalApplicationName) {
                NSString *title = [NSString stringWithFormat:WEB_UI_STRING("Open in “%@”", "Title for Open in External Application Link action button"), externalApplicationName];
                _WKElementAction *openInExternalApplicationAction = [_WKElementAction _elementActionWithType:_WKElementActionTypeOpenInExternalApplication title:title actionHandler:^(_WKActivatedElementInfo *) {
                    [appLink openInWebBrowser:NO setAppropriateOpenStrategyAndWebBrowserState:nil completionHandler:^(BOOL success, NSError *error) { }];
                }];
                [defaultActions addObject:openInExternalApplicationAction];
            }
        } else
            [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeOpen assistant:self]];
    } else
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeOpen assistant:self]];
#else
    [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeOpen assistant:self]];
#endif
}

- (RetainPtr<NSArray>)defaultActionsForLinkSheet:(_WKActivatedElementInfo *)elementInfo
{
    auto delegate = _delegate.get();
    if (!delegate)
        return nil;

    const auto& positionInformation = [delegate positionInformationForActionSheetAssistant:self];

    NSURL *targetURL = [NSURL URLWithString:positionInformation.url];
    if (!targetURL)
        return nil;

    auto defaultActions = adoptNS([[NSMutableArray alloc] init]);
    [self _appendOpenActionsForURL:targetURL actions:defaultActions.get() elementInfo:elementInfo];

#if HAVE(SAFARI_SERVICES_FRAMEWORK)
    if ([getSSReadingListClass() supportsURL:targetURL])
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeAddToReadingList assistant:self]];
#endif
    if (![[targetURL scheme] length] || [[targetURL scheme] caseInsensitiveCompare:@"javascript"] != NSOrderedSame) {
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeCopy assistant:self]];
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeShare assistant:self]];
    }

    return defaultActions;
}

- (RetainPtr<NSArray>)defaultActionsForImageSheet:(_WKActivatedElementInfo *)elementInfo
{
    auto delegate = _delegate.get();
    if (!delegate)
        return nil;

    const auto& positionInformation = [delegate positionInformationForActionSheetAssistant:self];
    NSURL *targetURL = [NSURL _web_URLWithWTFString:positionInformation.url];

    auto defaultActions = adoptNS([[NSMutableArray alloc] init]);
    if (!positionInformation.url.isEmpty())
        [self _appendOpenActionsForURL:targetURL actions:defaultActions.get() elementInfo:elementInfo];

#if HAVE(SAFARI_SERVICES_FRAMEWORK)
    if ([getSSReadingListClass() supportsURL:targetURL])
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeAddToReadingList assistant:self]];
#endif
    if (TCCAccessPreflight(getkTCCServicePhotos(), NULL) != kTCCAccessPreflightDenied)
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeSaveImage assistant:self]];
    if (!targetURL.scheme.length || [targetURL.scheme caseInsensitiveCompare:@"javascript"] != NSOrderedSame)
        [defaultActions addObject:[_WKElementAction _elementActionWithType:_WKElementActionTypeCopy assistant:self]];

    return defaultActions;
}

- (void)showLinkSheet
{
    ASSERT(!_elementInfo);

    auto delegate = _delegate.get();
    if (!delegate)
        return;

    const auto& positionInformation = [delegate positionInformationForActionSheetAssistant:self];

    NSURL *targetURL = [NSURL _web_URLWithWTFString:positionInformation.url];
    if (!targetURL)
        return;

    auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeLink URL:targetURL location:positionInformation.point title:positionInformation.title rect:positionInformation.bounds image:positionInformation.image.get()]);
    if ([delegate respondsToSelector:@selector(actionSheetAssistant:showCustomSheetForElement:)] && [delegate actionSheetAssistant:self showCustomSheetForElement:elementInfo.get()])
        return;

    auto defaultActions = [self defaultActionsForLinkSheet:elementInfo.get()];

    RetainPtr<NSArray> actions = [delegate actionSheetAssistant:self decideActionsForElement:elementInfo.get() defaultActions:WTFMove(defaultActions)];

    if (![actions count])
        return;

    [self _createSheetWithElementActions:actions.get() showLinkTitle:YES];
    if (!_interactionSheet)
        return;

    _elementInfo = WTFMove(elementInfo);

    if (![_interactionSheet presentSheet])
        [self cleanupSheet];
}

- (void)showDataDetectorsSheet
{
    auto delegate = _delegate.get();
    if (!delegate)
        return;

    const WebKit::InteractionInformationAtPosition& positionInformation = [delegate positionInformationForActionSheetAssistant:self];
    NSURL *targetURL = [NSURL _web_URLWithWTFString:positionInformation.url];
    if (!targetURL)
        return;

    if (![[getDDDetectionControllerClass() tapAndHoldSchemes] containsObject:[targetURL scheme]])
        return;

    DDDetectionController *controller = [getDDDetectionControllerClass() sharedController];
    NSArray *dataDetectorsActions = nil;
    if ([controller respondsToSelector:@selector(actionsForURL:identifier:selectedText:results:context:)]) {
        NSDictionary *context = nil;
        NSString *textAtSelection = nil;
        RetainPtr<NSMutableDictionary> extendedContext;

        if ([delegate respondsToSelector:@selector(dataDetectionContextForActionSheetAssistant:)])
            context = [delegate dataDetectionContextForActionSheetAssistant:self];
        if ([delegate respondsToSelector:@selector(selectedTextForActionSheetAssistant:)])
            textAtSelection = [delegate selectedTextForActionSheetAssistant:self];
        if (!positionInformation.textBefore.isEmpty() || !positionInformation.textAfter.isEmpty()) {
            extendedContext = adoptNS([@{
                getkDataDetectorsLeadingText() : positionInformation.textBefore,
                getkDataDetectorsTrailingText() : positionInformation.textAfter,
            } mutableCopy]);
            
            if (context)
                [extendedContext addEntriesFromDictionary:context];
            context = extendedContext.get();
        }
        dataDetectorsActions = [controller actionsForURL:targetURL identifier:positionInformation.dataDetectorIdentifier selectedText:textAtSelection results:positionInformation.dataDetectorResults.get() context:context];
    } else
        dataDetectorsActions = [controller actionsForAnchor:nil url:targetURL forFrame:nil];
    if ([dataDetectorsActions count] == 0)
        return;

    NSMutableArray *elementActions = [NSMutableArray array];
    for (NSUInteger actionNumber = 0; actionNumber < [dataDetectorsActions count]; actionNumber++) {
        DDAction *action = [dataDetectorsActions objectAtIndex:actionNumber];
        _WKElementAction *elementAction = [_WKElementAction elementActionWithTitle:[action localizedName] actionHandler:^(_WKActivatedElementInfo *actionInfo) {
            [[getDDDetectionControllerClass() sharedController] performAction:action
                                                          fromAlertController:_interactionSheet.get()
                                                          interactionDelegate:self];
        }];
        elementAction.dismissalHandler = ^{
            return (BOOL)!action.hasUserInterface;
        };
        [elementActions addObject:elementAction];
    }

    [self _createSheetWithElementActions:elementActions showLinkTitle:NO];
    if (!_interactionSheet)
        return;

    if (elementActions.count <= 1)
        _interactionSheet.get().arrowDirections = UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;

    if (![_interactionSheet presentSheet])
        [self cleanupSheet];
}

- (void)cleanupSheet
{
    auto delegate = _delegate.get();
    if ([delegate respondsToSelector:@selector(actionSheetAssistantDidStopInteraction:)])
        [delegate actionSheetAssistantDidStopInteraction:self];

    [_interactionSheet doneWithSheet];
    [_interactionSheet setSheetDelegate:nil];
    _interactionSheet = nil;
    _elementInfo = nil;
}

@end

#endif // PLATFORM(IOS)
