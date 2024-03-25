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

#import <WebKit/_WKRectEdge.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, _WKTargetedElementPosition) {
    _WKTargetedElementPositionStatic,
    _WKTargetedElementPositionRelative,
    _WKTargetedElementPositionAbsolute,
    _WKTargetedElementPositionSticky,
    _WKTargetedElementPositionFixed
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

@class _WKFrameTreeNode;

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKTargetedElementInfo : NSObject

@property (nonatomic, readonly) _WKTargetedElementPosition positionType;
@property (nonatomic, readonly) CGRect bounds;
@property (nonatomic, readonly, getter=isUnderPoint) BOOL underPoint;

@property (nonatomic, readonly, copy) NSArray<NSString *> *selectors;
@property (nonatomic, readonly, copy) NSString *renderedText;
@property (nonatomic, readonly) _WKRectEdge offsetEdges;

- (BOOL)isSameElement:(_WKTargetedElementInfo *)other;

- (void)getChildFrames:(void(^)(NSArray<_WKFrameTreeNode *> *))completionHandler;

@end

NS_ASSUME_NONNULL_END
