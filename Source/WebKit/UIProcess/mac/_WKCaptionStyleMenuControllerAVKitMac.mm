/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "_WKCaptionStyleMenuControllerAVKitMac.h"

#if PLATFORM(MAC) && HAVE(AVLEGIBLEMEDIAOPTIONSMENUCONTROLLER)

#import "_WKCaptionStyleMenuControllerInternal.h"

#if __has_include(<AVKit/AVLegibleMediaOptionsMenuController.h>)
#import <AVKit/AVLegibleMediaOptionsMenuController.h>
#endif

#import <AppKit/AppKit.h>
#import <WebCore/CaptionUserPreferencesMediaAF.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/WTFString.h>

#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVLegibleMediaOptionsMenuController)

// CLEANUP(rdar://164667890)
@interface AVLegibleMediaOptionsMenuController (WebKitSPI)
- (nullable NSMenu *)buildMenuOfType:(NSInteger)type;
- (nullable NSMenu *)menuWithContents:(NSInteger)contents;
@end

using namespace WebCore;
using namespace WTF;

@interface _WKCaptionStyleMenuControllerAVKitMac () <AVLegibleMediaOptionsMenuControllerDelegate> {
    RetainPtr<AVLegibleMediaOptionsMenuController> _menuController;
}
@end

@implementation _WKCaptionStyleMenuControllerAVKitMac

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    if (!AVKitLibrary())
        return nil;

    _menuController = [allocAVLegibleMediaOptionsMenuControllerInstance() initWithPlayer:nil];
    [_menuController setDelegate:self];
    [self rebuildMenu];

    return self;
}

- (void)rebuildMenu
{
    if ([_menuController respondsToSelector:@selector(menuWithContents:)])
        self.menu = [_menuController menuWithContents:AVLegibleMediaOptionsMenuContentsCaptionAppearance];
    else
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        self.menu = [_menuController buildMenuOfType:AVLegibleMediaOptionsMenuTypeCaptionAppearance];
ALLOW_DEPRECATED_DECLARATIONS_END
}

- (NSMenu *)captionStyleMenu
{
    return self.menu;
}

#pragma mark - AVLegibleMediaOptionsMenuControllerDelegate

- (void)legibleMenuController:(AVLegibleMediaOptionsMenuController *)menuController didRequestCaptionPreviewForProfileID:(NSString *)profileID
{
    [self setPreviewProfileID:profileID];
}

- (void)legibleMenuControllerDidRequestStoppingSubtitleCaptionPreview:(AVLegibleMediaOptionsMenuController *)menuController
{
    [self setPreviewProfileID:nil];
}

@end

#endif
