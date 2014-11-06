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

#ifndef WKActionMenuController_h
#define WKActionMenuController_h

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

#import "ActionMenuHitTestResult.h"
#import "WKActionMenuItemTypes.h"
#import "WKActionMenuTypes.h"
#import <AppKit/NSSharingService.h>
#import <wtf/RetainPtr.h>

namespace WebKit {
class WebPageProxy;

enum class ActionMenuState {
    None = 0,
    Pending,
    Ready
};
}

@class WKView;

@interface WKActionMenuController : NSObject <NSMenuDelegate> {
@private
    WebKit::WebPageProxy *_page;
    WKView *_wkView;

    WebKit::ActionMenuState _state;
    WebKit::ActionMenuHitTestResult _hitTestResult;
    RefPtr<API::Object> _userData;
    _WKActionMenuType _type;
    RetainPtr<NSSharingServicePicker> _sharingServicePicker;

    BOOL _isShowingTextIndicator;
}

- (instancetype)initWithPage:(WebKit::WebPageProxy&)page view:(WKView *)wkView;
- (void)willDestroyView:(WKView *)view;

- (void)prepareForMenu:(NSMenu *)menu withEvent:(NSEvent *)event;
- (void)willOpenMenu:(NSMenu *)menu withEvent:(NSEvent *)event;
- (void)didCloseMenu:(NSMenu *)menu withEvent:(NSEvent *)event;

- (void)didPerformActionMenuHitTest:(const WebKit::ActionMenuHitTestResult&)hitTestResult userData:(API::Object*)userData;

- (void)dismissActionMenuDataDetectorPopovers;

@end

#endif // PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

#endif // WKActionMenuController_h
