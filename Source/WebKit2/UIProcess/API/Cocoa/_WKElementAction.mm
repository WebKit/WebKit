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
#import "_WKElementActionInternal.h"

#if WK_API_ENABLED

#if PLATFORM(IOS)

#import "_WKActivatedElementInfoInternal.h"
#import "WKContentViewInteraction.h"
#import "WKGestureTypes.h"
#import <SafariServices/SSReadingList.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SoftLinking.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

SOFT_LINK_FRAMEWORK(SafariServices);
SOFT_LINK_CLASS(SafariServices, SSReadingList);

typedef void (^WKElementActionHandlerInternal)(WKContentView *, _WKActivatedElementInfo *);

@implementation _WKElementAction  {
    RetainPtr<NSString> _title;
    WKElementActionHandlerInternal _actionHandler;
}

- (id)_initWithTitle:(NSString *)title actionHandler:(WKElementActionHandlerInternal)handler type:(_WKElementActionType)type
{
    if (!(self = [super init]))
        return nil;

    _title = adoptNS([title copy]);
    _type = type;
    _actionHandler = [handler copy];
    return self;
}

- (void)dealloc
{
    [_actionHandler release];

    [super dealloc];
}

+ (instancetype)elementActionWithTitle:(NSString *)title actionHandler:(WKElementActionHandler)handler
{
    return [[[self alloc] _initWithTitle:title actionHandler:^(WKContentView *view, _WKActivatedElementInfo *actionInfo) { handler(actionInfo); }
       type:_WKElementActionTypeCustom] autorelease];
}

static void copyElement(WKContentView *view)
{
    [view _performAction:WebKit::WKSheetActionCopy];
}

static void saveImage(WKContentView *view)
{
    [view _performAction:WebKit::WKSheetActionSaveImage];
}

static void addToReadingList(NSURL *targetURL, NSString *title)
{
    if (!title || [title length] == 0)
        title = [targetURL absoluteString];

    [[getSSReadingListClass() defaultReadingList] addReadingListItemWithURL:targetURL title:title previewText:nil error:nil];
}

+ (instancetype)elementActionWithType:(_WKElementActionType)type customTitle:(NSString *)customTitle
{
    NSString *title;
    WKElementActionHandlerInternal handler;
    switch (type) {
    case _WKElementActionTypeCopy:
        title = WEB_UI_STRING_KEY("Copy", "Copy ActionSheet Link", "Title for Copy Link or Image action button");
        handler = ^(WKContentView *view, _WKActivatedElementInfo *actionInfo) {
            copyElement(view);
        };
        break;
    case _WKElementActionTypeOpen:
        title = WEB_UI_STRING_KEY("Open", "Open ActionSheet Link", "Title for Open Link action button");
        handler = ^(WKContentView *view, _WKActivatedElementInfo *actionInfo) {
            [view _attemptClickAtLocation:actionInfo._interactionLocation];
        };
        break;
    case _WKElementActionTypeSaveImage:
        title = WEB_UI_STRING_KEY("Save Image", "Save Image", "Title for Save Image action button");
        handler = ^(WKContentView *view, _WKActivatedElementInfo *actionInfo) {
            saveImage(view);
        };
        break;
    case _WKElementActionTypeAddToReadingList:
        title = WEB_UI_STRING("Add to Reading List", "Title for Add to Reading List action button");
        handler = ^(WKContentView *view, _WKActivatedElementInfo *actionInfo) {
            addToReadingList(actionInfo.URL, actionInfo.title);
        };
        break;
    default:
        [NSException raise:NSInvalidArgumentException format:@"There is no standard web element action of type %ld.", (long)type];
        return nil;
    }

    return [[[self alloc] _initWithTitle:(customTitle ? customTitle : title) actionHandler:handler type:type] autorelease];
}

+ (instancetype)elementActionWithType:(_WKElementActionType)type
{
    return [self elementActionWithType:type customTitle:nil];
}

- (NSString *)title
{
    return _title.get();
}

- (void)_runActionWithElementInfo:(_WKActivatedElementInfo *)info view:(WKContentView *)view
{
    _actionHandler(view, info);
}

@end

#endif // PLATFORM(IOS)

#endif // WK_API_ENABLED
