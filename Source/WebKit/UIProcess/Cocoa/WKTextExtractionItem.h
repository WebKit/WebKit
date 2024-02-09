/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, WKTextExtractionContainer) {
    WKTextExtractionContainerRoot,
    WKTextExtractionContainerViewportConstrained,
    WKTextExtractionContainerLink,
    WKTextExtractionContainerList,
    WKTextExtractionContainerListItem,
    WKTextExtractionContainerBlockQuote,
    WKTextExtractionContainerArticle,
    WKTextExtractionContainerSection,
    WKTextExtractionContainerNav
};

@interface WKTextExtractionItem : NSObject
@property (nonatomic, strong) NSArray<WKTextExtractionItem *> *children;
@end

@interface WKTextExtractionContainerItem : WKTextExtractionItem
- (instancetype)initWithContainer:(WKTextExtractionContainer)container rectInRootView:(CGRect)rectInRootView;
@end

@interface WKTextExtractionTextItem : WKTextExtractionItem
- (instancetype)initWithContent:(NSString *)content rectInRootView:(CGRect)rectInRootView;
@end

@interface WKTextExtractionScrollableItem : WKTextExtractionItem
- (instancetype)initWithContentSize:(CGSize)contentSize rectInRootView:(CGRect)rectInRootView;
@end

@interface WKTextExtractionEditableItem : WKTextExtractionItem
- (instancetype)initWithIsFocused:(BOOL)isFocused rectInRootView:(CGRect)rectInRootView;
@end

@interface WKTextExtractionInteractiveItem : WKTextExtractionItem
- (instancetype)initWithIsEnabled:(BOOL)isEnabled rectInRootView:(CGRect)rectInRootView;
@end

@interface WKTextExtractionImageItem : WKTextExtractionItem
- (instancetype)initWithName:(NSString *)name altText:(NSString *)altText rectInRootView:(CGRect)rectInRootView;
@end
