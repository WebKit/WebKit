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
    WKTextExtractionContainerList,
    WKTextExtractionContainerListItem,
    WKTextExtractionContainerBlockQuote,
    WKTextExtractionContainerArticle,
    WKTextExtractionContainerSection,
    WKTextExtractionContainerNav,
    WKTextExtractionContainerButton
};

@interface WKTextExtractionItem : NSObject
@property (nonatomic, readonly) NSArray<WKTextExtractionItem *> *children;
@property (nonatomic, readonly) CGRect rectInRootView;
@end

@interface WKTextExtractionContainerItem : WKTextExtractionItem
- (instancetype)initWithContainer:(WKTextExtractionContainer)container rectInRootView:(CGRect)rectInRootView children:(NSArray<WKTextExtractionItem *> *)children;
@property (nonatomic, readonly) WKTextExtractionContainer container;
@end

@interface WKTextExtractionLink : NSObject
- (instancetype)initWithURL:(NSURL *)url range:(NSRange)range;
@property (nonatomic, readonly) NSURL *url;
@property (nonatomic, readonly) NSRange range;
@end

@interface WKTextExtractionEditable : NSObject
- (instancetype)initWithLabel:(NSString *)label placeholder:(NSString *)placeholder isSecure:(BOOL)isSecure isFocused:(BOOL)isFocused;
@property (nonatomic, readonly) NSString *label;
@property (nonatomic, readonly) NSString *placeholder;
@property (nonatomic, readonly, getter=isSecure) BOOL secure;
@property (nonatomic, readonly, getter=isFocused) BOOL focused;
@end

@interface WKTextExtractionTextItem : WKTextExtractionItem
- (instancetype)initWithContent:(NSString *)content selectedRange:(NSRange)selectedRange links:(NSArray<WKTextExtractionLink *> *)links editable:(WKTextExtractionEditable *)editable rectInRootView:(CGRect)rectInRootView children:(NSArray<WKTextExtractionItem *> *)children;
@property (nonatomic, readonly) NSString *content;
@property (nonatomic, readonly) NSRange selectedRange;
@property (nonatomic, readonly) NSArray<WKTextExtractionLink *> *links;
@property (nonatomic, readonly) WKTextExtractionEditable *editable;
@end

@interface WKTextExtractionScrollableItem : WKTextExtractionItem
- (instancetype)initWithContentSize:(CGSize)contentSize rectInRootView:(CGRect)rectInRootView children:(NSArray<WKTextExtractionItem *> *)children;
@property (nonatomic, readonly) CGSize contentSize;
@end

@interface WKTextExtractionImageItem : WKTextExtractionItem
- (instancetype)initWithName:(NSString *)name altText:(NSString *)altText rectInRootView:(CGRect)rectInRootView children:(NSArray<WKTextExtractionItem *> *)children;
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) NSString *altText;
@end
