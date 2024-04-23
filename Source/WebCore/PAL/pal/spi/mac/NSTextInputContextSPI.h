/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#if USE(APPLE_INTERNAL_SDK)

#import <AppKit/NSTextInputContext_Private.h>
#import <AppKit/NSTextPlaceholder_Private.h>

#if HAVE(NSTEXTPLACEHOLDER_RECTS)
// Staging for rdar://126696059
@interface NSTextSelectionRect : NSObject
@property (nonatomic, readonly) NSRect rect;
@property (nonatomic, readonly) NSWritingDirection writingDirection;
@property (nonatomic, readonly) BOOL isVertical;
@property (nonatomic, readonly) NSAffineTransform *transform;
@end

@interface NSTextPlaceholder (staging_126696059)
@property (nonatomic, readonly) NSArray<NSTextSelectionRect *> *rects;
@end

@protocol NSTextInputClient_Async_staging_126696059
@optional
- (void)insertTextPlaceholderWithSize:(CGSize)size completionHandler:(void (^)(NSTextPlaceholder *))completionHandler;
- (void)removeTextPlaceholder:(NSTextPlaceholder *)placeholder willInsertText:(BOOL)willInsertText completionHandler:(void (^)(void))completionHandler;
@end
#endif // HAVE(NSTEXTPLACEHOLDER_RECTS)

#else // !USE(APPLE_INTERNAL_SDK)

@interface NSTextSelectionRect : NSObject
@property (nonatomic, readonly) NSRect rect;
@property (nonatomic, readonly) NSWritingDirection writingDirection;
@property (nonatomic, readonly) BOOL isVertical;
@property (nonatomic, readonly) NSAffineTransform *transform;
@end

@interface NSTextPlaceholder : NSObject
@property (nonatomic, readonly) NSArray<NSTextSelectionRect *> *rects;
@end

@interface NSTextInputContext ()
- (void)handleEvent:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler;
- (void)handleEventByInputMethod:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler;
- (BOOL)handleEventByKeyboardLayout:(NSEvent *)event;

#if HAVE(REDESIGNED_TEXT_CURSOR)
@property BOOL showsCursorAccessories;
#endif
@end

#endif // USE(APPLE_INTERNAL_SDK)

APPKIT_EXTERN NSString *NSTextInsertionUndoableAttributeName;
APPKIT_EXTERN NSString *NSTextInputReplacementRangeAttributeName;

#endif // PLATFORM(MAC)
