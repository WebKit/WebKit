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

#if PLATFORM(IOS_FAMILY)

#import "GestureTypes.h"
#import "WKActionSheetAssistant.h"
#import "WKContentViewInteraction.h"
#import "_WKActivatedElementInfoInternal.h"
#import <WebCore/LocalizedStrings.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

#if HAVE(SAFARI_SERVICES_FRAMEWORK)
#import <SafariServices/SSReadingList.h>
SOFT_LINK_FRAMEWORK(SafariServices);
SOFT_LINK_CLASS(SafariServices, SSReadingList);
#endif

typedef void (^WKElementActionHandlerInternal)(WKActionSheetAssistant *, _WKActivatedElementInfo *);

@implementation _WKElementAction  {
    RetainPtr<NSString> _title;
    WKElementActionHandlerInternal _actionHandler;
    WKElementActionDismissalHandler _dismissalHandler;
    WeakObjCPtr<WKActionSheetAssistant> _defaultActionSheetAssistant;
}

- (id)_initWithTitle:(NSString *)title actionHandler:(WKElementActionHandlerInternal)handler type:(_WKElementActionType)type assistant:(WKActionSheetAssistant *)assistant
{
    if (!(self = [super init]))
        return nil;

    _title = adoptNS([title copy]);
    _type = type;
    _actionHandler = [handler copy];
    _defaultActionSheetAssistant = assistant;
    return self;
}

- (void)dealloc
{
    [_actionHandler release];
    [_dismissalHandler release];

    [super dealloc];
}

+ (instancetype)elementActionWithTitle:(NSString *)title actionHandler:(WKElementActionHandler)handler
{
    return [[[self alloc] _initWithTitle:title actionHandler:^(WKActionSheetAssistant *, _WKActivatedElementInfo *actionInfo) { handler(actionInfo); }
        type:_WKElementActionTypeCustom assistant:nil] autorelease];
}

#if HAVE(SAFARI_SERVICES_FRAMEWORK)
static void addToReadingList(NSURL *targetURL, NSString *title)
{
    if (!title || [title length] == 0)
        title = [targetURL absoluteString];

    [[getSSReadingListClass() defaultReadingList] addReadingListItemWithURL:targetURL title:title previewText:nil error:nil];
}
#endif

+ (instancetype)_elementActionWithType:(_WKElementActionType)type title:(NSString *)title actionHandler:(WKElementActionHandler)actionHandler
{
    WKElementActionHandlerInternal handler = ^(WKActionSheetAssistant *, _WKActivatedElementInfo *actionInfo) { actionHandler(actionInfo); };
    return [[[self alloc] _initWithTitle:title actionHandler:handler type:type assistant:nil] autorelease];
}

+ (instancetype)_elementActionWithType:(_WKElementActionType)type customTitle:(NSString *)customTitle assistant:(WKActionSheetAssistant *)assistant
{
    NSString *title;
    WKElementActionHandlerInternal handler;
    switch (type) {
    case _WKElementActionTypeCopy:
        title = WEB_UI_STRING_KEY("Copy", "Copy (ActionSheet)", "Title for Copy Link or Image action button");
        handler = ^(WKActionSheetAssistant *assistant, _WKActivatedElementInfo *actionInfo) {
            [assistant.delegate actionSheetAssistant:assistant performAction:WebKit::SheetAction::Copy];
        };
        break;
    case _WKElementActionTypeOpen:
        title = WEB_UI_STRING("Open", "Title for Open Link action button");
        handler = ^(WKActionSheetAssistant *assistant, _WKActivatedElementInfo *actionInfo) {
            [assistant.delegate actionSheetAssistant:assistant openElementAtLocation:actionInfo._interactionLocation];
        };
        break;
    case _WKElementActionTypeSaveImage:
        title = WEB_UI_STRING("Save Image", "Title for Save Image action button");
        handler = ^(WKActionSheetAssistant *assistant, _WKActivatedElementInfo *actionInfo) {
            [assistant.delegate actionSheetAssistant:assistant performAction:WebKit::SheetAction::SaveImage];
        };
        break;
#if HAVE(SAFARI_SERVICES_FRAMEWORK)
    case _WKElementActionTypeAddToReadingList:
        title = WEB_UI_STRING("Add to Reading List", "Title for Add to Reading List action button");
        handler = ^(WKActionSheetAssistant *, _WKActivatedElementInfo *actionInfo) {
            addToReadingList(actionInfo.URL, actionInfo.title);
        };
        break;
#endif
    case _WKElementActionTypeShare:
        title = WEB_UI_STRING("Share…", "Title for Share action button");
        handler = ^(WKActionSheetAssistant *assistant, _WKActivatedElementInfo *actionInfo) {
            [assistant.delegate actionSheetAssistant:assistant shareElementWithURL:actionInfo.URL rect:actionInfo.boundingRect];
        };
        break;
    default:
        [NSException raise:NSInvalidArgumentException format:@"There is no standard web element action of type %ld.", (long)type];
        return nil;
    }

    return [[[self alloc] _initWithTitle:(customTitle ? customTitle : title) actionHandler:handler type:type assistant:assistant] autorelease];
}

+ (instancetype)_elementActionWithType:(_WKElementActionType)type assistant:(WKActionSheetAssistant *)assistant
{
    return [self _elementActionWithType:type customTitle:nil assistant:assistant];
}

+ (instancetype)elementActionWithType:(_WKElementActionType)type customTitle:(NSString *)customTitle
{
    return [self _elementActionWithType:type customTitle:customTitle assistant:nil];
}

+ (instancetype)elementActionWithType:(_WKElementActionType)type
{
    return [self elementActionWithType:type customTitle:nil];
}

- (NSString *)title
{
    return _title.get();
}

- (void)_runActionWithElementInfo:(_WKActivatedElementInfo *)info forActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    _actionHandler(assistant, info);
}

- (void)runActionWithElementInfo:(_WKActivatedElementInfo *)info
{
    [self _runActionWithElementInfo:info forActionSheetAssistant:_defaultActionSheetAssistant.get().get()];
}

@end

#endif // PLATFORM(IOS_FAMILY)
