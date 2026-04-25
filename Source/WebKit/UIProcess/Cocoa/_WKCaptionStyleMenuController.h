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

#pragma once

#import <WebKit/WKFoundation.h>

#if TARGET_OS_OSX
@class NSMenu;
typedef NSMenu PlatformMenu;
#else
@class UIContextMenuInteraction;
@class UIMenu;
typedef UIMenu PlatformMenu;
#endif

NS_ASSUME_NONNULL_BEGIN

@protocol WKCaptionStyleMenuControllerDelegate <NSObject>
- (void)captionStyleMenuWillOpen:(PlatformMenu *)menu;
- (void)captionStyleMenuDidClose:(PlatformMenu *)menu;
@optional
- (void)captionStyleMenu:(PlatformMenu *)menu setPreviewProfileID:(NSString *)profileID;
- (void)captionStyleMenu:(PlatformMenu *)menu didSelectProfile:(NSString *)profileID;
@end

WK_EXTERN
@interface WKCaptionStyleMenuController : NSObject

+ (instancetype)menuController;

@property (weak, nullable, nonatomic) id<WKCaptionStyleMenuControllerDelegate> delegate;
@property (readonly, nonatomic) PlatformMenu *captionStyleMenu;
#if !TARGET_OS_OSX && !TARGET_OS_WATCH
@property (readonly, nullable, nonatomic) UIContextMenuInteraction *contextMenuInteraction;
#endif

- (BOOL)isAncestorOf:(PlatformMenu*)menu;
- (BOOL)hasAncestor:(PlatformMenu*)menu;
@end

NS_ASSUME_NONNULL_END
