/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#if TARGET_OS_IPHONE

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <WebKit/WebFrameIOS.h>

@interface WebSelectionRect : NSObject <NSCopying> {
    CGRect m_rect;
    WKWritingDirection m_writingDirection;
    BOOL m_isLineBreak;
    BOOL m_isFirstOnLine;
    BOOL m_isLastOnLine;
    BOOL m_containsStart;
    BOOL m_containsEnd;
    BOOL m_isInFixedPosition;
    BOOL m_isHorizontal;
}

@property (nonatomic, assign) CGRect rect;
@property (nonatomic, assign) WKWritingDirection writingDirection;
@property (nonatomic, assign) BOOL isLineBreak;
@property (nonatomic, assign) BOOL isFirstOnLine;
@property (nonatomic, assign) BOOL isLastOnLine;
@property (nonatomic, assign) BOOL containsStart;
@property (nonatomic, assign) BOOL containsEnd;
@property (nonatomic, assign) BOOL isInFixedPosition;
@property (nonatomic, assign) BOOL isHorizontal;

+ (WebSelectionRect *)selectionRect;

+ (CGRect)startEdge:(NSArray *)rects;
+ (CGRect)endEdge:(NSArray *)rects;

@end

#endif // TARGET_OS_IPHONE
