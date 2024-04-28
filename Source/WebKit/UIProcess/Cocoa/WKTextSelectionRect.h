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

#if PLATFORM(COCOA)

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#else
#import <pal/spi/mac/NSTextInputContextSPI.h>
#endif

namespace WebCore {
class SelectionGeometry;
}

#if PLATFORM(IOS_FAMILY)
@interface WKTextSelectionRect : UITextSelectionRect
#else
@interface WKTextSelectionRect : NSObject // FIXME: Change to NSTextSelectionRect after rdar://126379463 lands
#endif

- (instancetype)initWithCGRect:(CGRect)rect;
- (instancetype)initWithSelectionGeometry:(const WebCore::SelectionGeometry&)selectionGeometry scaleFactor:(CGFloat)scaleFactor;

#if PLATFORM(MAC)
@property (nonatomic, readonly) NSRect rect;
@property (nonatomic, readonly) NSWritingDirection writingDirection;
@property (nonatomic, readonly) BOOL isVertical;
@property (nonatomic, readonly) NSAffineTransform *transform;
#endif

@end

#endif // PLATFORM(COCOA)
