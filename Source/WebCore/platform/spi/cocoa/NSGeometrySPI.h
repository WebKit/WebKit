/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <CoreGraphics/CGBase.h>
#import <CoreGraphics/CGGeometry.h>
#import <Foundation/Foundation.h>

#if PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)
#import <Foundation/NSGeometry.h>
#else
typedef CGPoint NSPoint;
typedef CGRect NSRect;
typedef CGSize NSSize;
typedef NSUInteger NSRectEdge;

@interface NSValue (NSGeometryDetails)
+ (NSValue *)valueWithPoint:(NSPoint)point;
+ (NSValue *)valueWithRect:(NSRect)rect;
+ (NSValue *)valueWithSize:(NSSize)size;
@property (readonly) NSPoint pointValue;
@property (readonly) NSRect rectValue;
@property (readonly) NSSize sizeValue;
@end

#define NSMinXEdge CGRectMinXEdge
#define NSMinYEdge CGRectMinYEdge
#define NSMaxXEdge CGRectMaxXEdge
#define NSMaxYEdge CGRectMaxYEdge

#define NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES 1

ALWAYS_INLINE NSPoint NSMakePoint(CGFloat x, CGFloat y)
{
    NSPoint point;
    point.x = x;
    point.y = y;
    return point;
}

ALWAYS_INLINE NSSize NSMakeSize(CGFloat width, CGFloat height)
{
    NSSize size;
    size.width = width;
    size.height = height;
    return size;
}

ALWAYS_INLINE NSRect NSMakeRect(CGFloat x, CGFloat y, CGFloat width, CGFloat height)
{
    NSRect rect;
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width = width;
    rect.size.height = height;
    return rect;
}

ALWAYS_INLINE CGFloat NSMaxX(NSRect rect)
{
    return rect.origin.x + rect.size.width;
}

ALWAYS_INLINE CGFloat NSMaxY(NSRect rect)
{
    return rect.origin.y + rect.size.height;
}

ALWAYS_INLINE CGFloat NSMinX(NSRect rect)
{
    return rect.origin.x;
}

ALWAYS_INLINE CGFloat NSMinY(NSRect rect)
{
    return rect.origin.y;
}
#endif // PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)

extern const NSPoint NSZeroPoint;
extern const NSRect NSZeroRect;
extern const NSSize NSZeroSize;

extern BOOL NSEqualPoints(NSPoint, NSPoint);
extern BOOL NSEqualRects(NSRect, NSRect);
extern BOOL NSEqualSizes(NSSize, NSSize);
extern NSRect NSInsetRect(NSRect, CGFloat dX, CGFloat dY);
extern NSRect NSIntersectionRect(NSRect, NSRect);
extern BOOL NSIsEmptyRect(NSRect);
extern BOOL NSPointInRect(NSPoint, NSRect);
