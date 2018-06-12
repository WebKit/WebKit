/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if PLATFORM(WATCHOS)

#import "WKQuickboardListViewController.h"

@class WKSelectMenuListViewController;

@protocol WKSelectMenuListViewControllerDelegate <WKQuickboardViewControllerDelegate>

- (void)selectMenu:(WKSelectMenuListViewController *)selectMenu didSelectItemAtIndex:(NSUInteger)index;
- (void)selectMenu:(WKSelectMenuListViewController *)selectMenu didCheckItemAtIndex:(NSUInteger)index checked:(BOOL)checked;

- (BOOL)selectMenu:(WKSelectMenuListViewController *)selectMenu hasSelectedOptionAtIndex:(NSUInteger)index;
- (NSUInteger)numberOfItemsInSelectMenu:(WKSelectMenuListViewController *)selectMenu;
- (NSString *)selectMenu:(WKSelectMenuListViewController *)selectMenu displayTextForItemAtIndex:(NSUInteger)index;
- (BOOL)selectMenuUsesMultipleSelection:(WKSelectMenuListViewController *)selectMenu;

@end

@interface WKSelectMenuListViewController : WKQuickboardListViewController

- (instancetype)initWithDelegate:(id <WKSelectMenuListViewControllerDelegate>)delegate NS_DESIGNATED_INITIALIZER;

@property (nonatomic, weak) id <WKSelectMenuListViewControllerDelegate> delegate;

@end

@interface WKSelectMenuListViewController (Testing)

- (void)selectItemAtIndex:(NSInteger)index;

@end

#endif
