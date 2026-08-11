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

#import <wtf/Platform.h>

#if HAVE(PDFKIT)

#import <Foundation/Foundation.h>
#import <PDFKit/PDFKit.h>

#if PLATFORM(IOS_FAMILY)
@class UIColor;
typedef UIColor CocoaColor;
#else
@class NSColor;
typedef NSColor CocoaColor;
#endif

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_UI_ACTOR
@interface TestPDFAnnotation : NSObject

@property (nonatomic, readonly) BOOL isLink;

@property (nonatomic, readonly) CGRect bounds;

@property (nonatomic, readonly, nullable) NSURL *linkURL;

- (instancetype)initWithPDFAnnotation:(PDFAnnotation *)annotation;

@end

NS_SWIFT_UI_ACTOR
@interface TestPDFPage : NSObject

@property (nonatomic, readonly) CGRect bounds;

@property (nonatomic, readonly) NSArray<TestPDFAnnotation *> *annotations;

@property (nonatomic, readonly) NSString *text;

@property (nonatomic, readonly) NSInteger characterCount;

- (instancetype)initWithPDFPage:(PDFPage *)page;

- (CGRect)rectForCharacterAtIndex:(NSInteger)index;

- (NSInteger)characterIndexAtPoint:(CGPoint)point;

- (CocoaColor *)colorAtPoint:(CGPoint)point;

@end

NS_SWIFT_UI_ACTOR
@interface TestPDFDocument : NSObject

@property (nonatomic, readonly) NSInteger pageCount;

- (nullable TestPDFPage *)pageAtIndex:(NSInteger)index;

- (instancetype)initFromData:(NSData *)data;

@end

NS_ASSUME_NONNULL_END

#endif // HAVE(PDFKIT)
