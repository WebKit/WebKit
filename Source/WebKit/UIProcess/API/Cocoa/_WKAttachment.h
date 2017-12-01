/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, _WKAttachmentDisplayMode) {
    _WKAttachmentDisplayModeAuto = 1,
    _WKAttachmentDisplayModeInPlace,
    _WKAttachmentDisplayModeAsIcon
} WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));

WK_CLASS_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKAttachmentDisplayOptions : NSObject
@property (nonatomic) _WKAttachmentDisplayMode mode;
@end

WK_CLASS_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKAttachment : NSObject
- (void)requestData:(void(^)(NSData * _Nullable, NSError * _Nullable))completionHandler;
- (void)setDisplayOptions:(_WKAttachmentDisplayOptions *)options completion:(void(^ _Nullable)(NSError * _Nullable))completionHandler;
- (void)setData:(NSData *)data newContentType:(nullable NSString *)newContentType newFilename:(nullable NSString *)newFilename completion:(void(^ _Nullable)(NSError * _Nullable))completionHandler;
@end

NS_ASSUME_NONNULL_END

#endif
