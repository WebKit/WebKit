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

#import <wtf/Platform.h>

#if PLATFORM(IOS_FAMILY) || HAVE(NSTEXTPLACEHOLDER_RECTS)

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#else
#import <pal/spi/mac/NSTextInputContextSPI.h>
#endif

namespace WebCore {
class FloatQuad;
class SelectionGeometry;
}

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@class WKTextSelectionRect;

@protocol WKTextSelectionRectDelegate
- (CGFloat)scaleFactorForSelectionRect:(WKTextSelectionRect *)rect;
- (WebCore::FloatQuad)selectionRect:(WKTextSelectionRect *)rect convertQuadToSelectionContainer:(const WebCore::FloatQuad&)quad;
@end

#if PLATFORM(IOS_FAMILY)
@interface WKTextSelectionRect : UITextSelectionRect
#else
@interface WKTextSelectionRect : NSTextSelectionRect
#endif

- (instancetype)initWithCGRect:(CGRect)rect;
- (instancetype)initWithSelectionGeometry:(const WebCore::SelectionGeometry&)selectionGeometry delegate:(nullable id<WKTextSelectionRectDelegate>)delegate;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // PLATFORM(IOS_FAMILY) || HAVE(NSTEXTPLACEHOLDER_RECTS)
