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
#import "_WKCaptionStyleMenuControllerAVKit.h"

#if PLATFORM(IOS_FAMILY) && HAVE(AVLEGIBLEMEDIAOPTIONSMENUCONTROLLER)

#import "_WKCaptionStyleMenuControllerInternal.h"

#if __has_include(<AVKit/AVLegibleMediaOptionsMenuController.h>)
#import <AVKit/AVLegibleMediaOptionsMenuController.h>
#endif

#import <UIKit/UIKit.h>
#import <WebCore/CaptionUserPreferencesMediaAF.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/WTFString.h>

#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVLegibleMediaOptionsMenuController)

using namespace WebCore;
using namespace WTF;

@interface _WKCaptionStyleMenuControllerAVKit () <AVLegibleMediaOptionsMenuControllerDelegate> {
    RetainPtr<AVLegibleMediaOptionsMenuController> _menuController;
}
@end

@implementation _WKCaptionStyleMenuControllerAVKit

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
    self.menu = [_menuController menuWithContents:AVLegibleMediaOptionsMenuContentsCaptionAppearance];
}

#pragma mark - AVLegibleMediaOptionsMenuControllerDelegate

- (void)legibleMenuController:(AVLegibleMediaOptionsMenuController *)menuController didRequestCaptionPreviewForProfileID:(NSString *)profileID
{
    [self setPreviewProfileID:profileID];
    [self rebuildMenu];

    // UIMenu does not have the ability to notify clients when a submenu opens or closes.
    // Provide a similar functionality for previewing subtitle changes by triggering the
    // preview of subtitle styles when the first profile menu item is selected.
    [self notifyMenuWillOpen];

    if (auto delegate = self.delegate; delegate && [delegate respondsToSelector:@selector(captionStyleMenu:didSelectProfile:)])
        [delegate captionStyleMenu:self.menu didSelectProfile:profileID];
}

- (void)legibleMenuControllerDidRequestStoppingSubtitleCaptionPreview:(AVLegibleMediaOptionsMenuController *)menuController
{
    [self setPreviewProfileID:nil];
}
@end

#endif

