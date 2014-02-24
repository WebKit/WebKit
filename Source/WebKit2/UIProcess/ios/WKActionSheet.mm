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
#import "WKActionSheet.h"

#import "WKGestureTypes.h"
#import "WKInteractionView.h"
#import <SafariServices/SSReadingList.h>
#import <UIKit/UIActionSheet_Private.h>
#import <UIKit/UIView.h>
#import <UIKit/UIWindow_Private.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SoftLinking.h>
#import <wtf/text/WTFString.h>
#import <wtf/RetainPtr.h>

SOFT_LINK_FRAMEWORK(SafariServices);
SOFT_LINK_CLASS(SafariServices, SSReadingList);

@interface WKElementAction(Private)
- (void)_runActionWithElement:(NSURL *)targetURL documentView:(WKInteractionView *)view interactionLocation:(CGPoint)interactionLocation;
@end

@implementation WKActionSheet {
    id<WKActionSheetDelegate> _sheetDelegate;
    id<UIActionSheetDelegate> _delegateWhileRotating;
    WKInteractionView *_view;
    UIPopoverArrowDirection _arrowDirections;
    BOOL _isRotating;
}

- (id)initWithView:(WKInteractionView *)view
{
    self = [super init];
    if (!self)
        return nil;

    _arrowDirections = UIPopoverArrowDirectionAny;
    _view = view;

    if (UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone) {
        // Only iPads support popovers that rotate. UIActionSheets actually block rotation on iPhone/iPod Touch
        NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
        [center addObserver:self selector:@selector(willRotate) name:UIWindowWillRotateNotification object:nil];
        [center addObserver:self selector:@selector(didRotate) name:UIWindowDidRotateNotification object:nil];
    }

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [NSObject cancelPreviousPerformRequestsWithTarget:self];

    [super dealloc];
}

#pragma mark - Sheet presentation code

- (BOOL)presentSheet
{
    // Calculate the presentation rect just before showing.
    CGRect presentationRect = CGRectZero;
    if (UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone) {
        presentationRect = [_sheetDelegate initialPresentationRectInHostViewForSheet];
        if (CGRectIsEmpty(presentationRect))
            return NO;
    }

    return [self presentSheetFromRect:presentationRect];
}

- (BOOL)presentSheetFromRect:(CGRect)presentationRect
{
    UIView *view = [_sheetDelegate hostViewForSheet];
    if (!view)
        return NO;

    if (UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone) {
        [self presentFromRect:presentationRect
                       inView:view
                    direction:_arrowDirections
    allowInteractionWithViews:nil
              backgroundStyle:UIPopoverBackgroundStyleDefault
                     animated:YES];
    } else
        [self showInView:view]; 

    return YES;
}

- (void)doneWithSheet
{
    _delegateWhileRotating = nil;

    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(didRotate) object:nil];
}

#pragma mark - Rotation handling code

- (void)willRotate
{
    ASSERT(UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone);

    if (_isRotating)
        return;

    // Clear the delegate so that the method:
    // - (void)actionSheet:(UIActionSheet *)actionSheet didDismissWithButtonIndex:(NSInteger)buttonIndex
    // is not called when the ActionSheet is dismissed below. Delegate is re-instated after rotation.
    _delegateWhileRotating = [self delegate];
    self.delegate = nil;
    _isRotating = YES;

    [self dismissWithClickedButtonIndex:[self cancelButtonIndex] animated:NO];
}

- (void)updateSheetPosition
{
    ASSERT(UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone);

    if (!_isRotating)
        return;

    _isRotating = NO;
    
    CGRect presentationRect = [_sheetDelegate presentationRectInHostViewForSheet];
    if (!CGRectIsEmpty(presentationRect)) {
        // Re-present the popover only if we are still pointing to content onscreen.
        CGRect intersection = CGRectIntersection([[_sheetDelegate hostViewForSheet] bounds], presentationRect);
        if (!CGRectIsEmpty(intersection)) {
            self.delegate = _delegateWhileRotating;
            [self presentSheetFromRect:intersection];
            return;
        }
    }
    
    // Cancel the sheet as there is either no view or the content has now moved off screen.
    [_delegateWhileRotating actionSheet:self clickedButtonAtIndex:[self cancelButtonIndex]];
}

- (void)didRotate
{
    ASSERT(UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone);

    [_view _updatePositionInformation];
}

@end

@implementation WKElementActionInfo  {
    CGPoint _interactionLocation;
    NSURL *_url;
    NSString *_title;
    CGRect _boundingRect;
    CGImageRef _snapshot;
}

- (WKElementActionInfo *) initWithInfo:(NSURL *)url location:(CGPoint)location title:(NSString *)title rect:(CGRect)rect
{
    _url = [url copy];
    _interactionLocation = location;
    _title = [title copy];
    _boundingRect = rect;

    return self;
}

- (void)dealloc
{
    [_title release];
    [_url release];
    
    [super dealloc];
}

@end

@implementation WKElementAction  {
    NSString *_title;
    WKElementActionHandlerInternal _actionHandler;
    WKElementActionType _type;
}

- (id)initWithTitle:(NSString *)title actionHandler:(WKElementActionHandlerInternal)handler type:(WKElementActionType)type
{
    if (!(self = [super init]))
        return nil;
    _title = [title copy];
    _type = type;
    _actionHandler = Block_copy(handler);
    return self;
}

- (void)dealloc
{
    [_title release];
    [_actionHandler release];
    [super dealloc];
}

+ (WKElementAction *)customElementActionWithTitle:(NSString *)title actionHandler:(WKElementActionHandler)handler
{
    return [[[self alloc] initWithTitle:title
                          actionHandler:^(WKInteractionView *view, WKElementActionInfo *actionInfo) { handler(actionInfo); }
                                   type:WKElementActionTypeCustom] autorelease];
}

static void copyElement(WKInteractionView *view)
{
    [view _performAction:WebKit::WKSheetActionCopy];
}

static void saveImage(WKInteractionView *view)
{
    [view _performAction:WebKit::WKSheetActionSaveImage];
}

static void addToReadingList(NSURL *targetURL, NSString *title)
{
    if (!title || [title length] == 0)
        title = [targetURL absoluteString];

    [[getSSReadingListClass() defaultReadingList] addReadingListItemWithURL:targetURL title:title previewText:nil error:nil];
}

+ (WKElementAction *)standardElementActionWithType:(WKElementActionType)type customTitle:(NSString *)customTitle
{
    NSString *title;
    WKElementActionHandlerInternal handler;
    switch (type) {
    case WKElementActionTypeCopy:
        title = WEB_UI_STRING_KEY("Copy", "Copy ActionSheet Link", "Title for Copy Link or Image action button");
        handler = ^(WKInteractionView *view, WKElementActionInfo *actionInfo) {
            copyElement(view);
        };
        break;
    case WKElementActionTypeOpen:
        title = WEB_UI_STRING_KEY("Open", "Open ActionSheet Link", "Title for Open Link action button");
        handler = ^(WKInteractionView *view, WKElementActionInfo *actionInfo) {
            [view _attemptClickAtLocation:actionInfo.interactionLocation];
        };
        break;
    case WKElementActionTypeSaveImage:
        title = WEB_UI_STRING_KEY("Save Image", "Save Image", "Title for Save Image action button");
        handler = ^(WKInteractionView *view, WKElementActionInfo *actionInfo) {
            saveImage(view);
        };
        break;
    case WKElementActionTypeAddToReadingList:
        title = WEB_UI_STRING("Add to Reading List", "Title for Add to Reading List action button");
        handler = ^(WKInteractionView *view, WKElementActionInfo *actionInfo) {
            addToReadingList(actionInfo.url, actionInfo.title);
        };
        break;
    default:
        [NSException raise:NSInvalidArgumentException format:@"There is no standard web element action of type %d.", type];
        return nil;
    }
    return [[[WKElementAction alloc] initWithTitle:(customTitle ? customTitle : title) actionHandler:handler type:type] autorelease];
}

+ (WKElementAction *)standardElementActionWithType:(WKElementActionType)type
{
    return [self standardElementActionWithType:type customTitle:nil];
}

- (void)runActionWithElementInfo:(WKElementActionInfo *)info view:(WKInteractionView *)view
{
    _actionHandler(view, info);
}
@end


