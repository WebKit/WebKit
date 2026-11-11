/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import <WebKit/WKFoundation.h>

#if TARGET_OS_IPHONE

#import <Foundation/Foundation.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

/*! @enum _WKViewportFitEdge
 @abstract How the page's content relates to the safe area at one edge of the viewport.
 @constant _WKViewportFitEdgeContain The page lays out within the safe area at this edge. This is
 the case when the page does not specify viewport-fit, or specifies auto or contain.
 @constant _WKViewportFitEdgeImplicitCover The page extends underneath the safe area at this
 edge because it specified viewport-fit=cover, without specifying a value for this edge in
 particular.
 @constant _WKViewportFitEdgeExplicitCover The page extends underneath the safe area at this
 edge because it asked for this edge specifically, e.g. with viewport-fit-top=cover.
 */
typedef NS_ENUM(NSInteger, _WKViewportFitEdge) {
    _WKViewportFitEdgeContain,
    _WKViewportFitEdgeImplicitCover,
    _WKViewportFitEdgeExplicitCover,
} WK_API_AVAILABLE(ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

/*! @abstract The viewport-fit the main frame's document asked for at each edge of the viewport,
 as specified in its <meta name="viewport"> tag.
 */
NS_SWIFT_SENDABLE
WK_CLASS_AVAILABLE(ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface _WKViewportFitEdges : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@property (nonatomic, readonly) _WKViewportFitEdge top;
@property (nonatomic, readonly) _WKViewportFitEdge left;
@property (nonatomic, readonly) _WKViewportFitEdge bottom;
@property (nonatomic, readonly) _WKViewportFitEdge right;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // TARGET_OS_IPHONE
