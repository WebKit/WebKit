/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef WAKAppKitStubs_h
#define WAKAppKitStubs_h

#if TARGET_OS_IPHONE

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#ifndef NSClipView
#define NSClipView WAKClipView
#endif
#ifndef NSView
#define NSView WAKView
#endif
#ifndef NSScrollView
#define NSScrollView WAKScrollView
#endif
#ifndef WebDynamicScrollBarsView
#define WebDynamicScrollBarsView WAKScrollView
#endif
#ifndef NSWindow
#define NSWindow WAKWindow
#endif
#ifndef NSResponder
#define NSResponder WAKResponder
#endif

// FIXME: <rdar://problem/6669434> Switch from using NSGeometry methods to CGGeometry methods
//
// We explicitly use __has_include() instead of the macro define USE_APPLE_INTERNAL_SDK as
// the condition for including the header Foundation/NSGeometry.h to support internal Apple
// clients that build without header wtf/Platform.h.
#if __has_include(<Foundation/NSGeometry.h>)

#import <Foundation/NSGeometry.h>

#else

typedef CGPoint NSPoint;
typedef CGRect NSRect;
typedef CGSize NSSize;
typedef NSUInteger NSRectEdge;

#ifndef NSZeroPoint
#define NSZeroPoint CGPointZero
#endif
#ifndef NSZeroRect
#define NSZeroRect CGRectZero
#endif
#ifndef NSZeroSize
#define NSZeroSize CGSizeZero
#endif
#ifndef NSMakePoint
#define NSMakePoint CGPointMake
#endif
#ifndef NSMakeRect
#define NSMakeRect CGRectMake
#endif
#ifndef NSMakeSize
#define NSMakeSize CGSizeMake
#endif
#ifndef NSEqualPoints
#define NSEqualPoints CGPointEqualToPoint
#endif
#ifndef NSEqualRects
#define NSEqualRects CGRectEqualToRect
#endif
#ifndef NSEqualSizes
#define NSEqualSizes CGSizeEqualToSize
#endif
#ifndef NSInsetRect
#define NSInsetRect CGRectInset
#endif
#ifndef NSIntersectionRect
#define NSIntersectionRect CGRectIntersection
#endif
#ifndef NSIsEmptyRect
#define NSIsEmptyRect CGRectIsEmpty
#endif
#ifndef NSContainsRect
#define NSContainsRect CGRectContainsRect
#endif
#ifndef NSPointInRect
#define NSPointInRect(x, y) CGRectContainsPoint((y), (x))
#endif
#ifndef NSMinX
#define NSMinX CGRectGetMinX
#endif
#ifndef NSMinY
#define NSMinY CGRectGetMinY
#endif
#ifndef NSMaxX
#define NSMaxX CGRectGetMaxX
#endif
#ifndef NSMaxY
#define NSMaxY CGRectGetMaxY
#endif
#ifndef NSMinXEdge
#define NSMinXEdge CGRectMinXEdge
#endif
#ifndef NSMinYEdge
#define NSMinYEdge CGRectMinYEdge
#endif
#ifndef NSMaxXEdge
#define NSMaxXEdge CGRectMaxXEdge
#endif
#ifndef NSMaxYEdge
#define NSMaxYEdge CGRectMaxYEdge
#endif

@interface NSValue (NSGeometryDetails)
+ (NSValue *)valueWithPoint:(NSPoint)point;
+ (NSValue *)valueWithRect:(NSRect)rect;
+ (NSValue *)valueWithSize:(NSSize)size;
@property (readonly) NSPoint pointValue;
@property (readonly) NSRect rectValue;
@property (readonly) NSSize sizeValue;
@end

#endif // __has_include(<Foundation/NSGeometry.h>)

@class WAKClipView;
@class WAKResponder;
@class WAKScrollView;
@class WAKView;
@class WAKWindow;

/* Device-independent bits found in event modifier flags */
enum {
    NSEventModifierFlagCapsLock = 1 << 16,
    NSEventModifierFlagShift = 1 << 17,
    NSEventModifierFlagControl = 1 << 18,
    NSEventModifierFlagOption = 1 << 19,
    NSEventModifierFlagCommand = 1 << 20,
    NSEventModifierFlagNumericPad = 1 << 21,
    NSEventModifierFlagHelp = 1 << 22,
    NSEventModifierFlagFunction = 1 << 23,
    NSEventModifierFlagDeviceIndependentFlagsMask = 0xffff0000U
};

typedef enum _WKWritingDirection {
    WKWritingDirectionNatural     = -1, /* Determines direction using the Unicode Bidi Algorithm rules P2 and P3 */
    WKWritingDirectionLeftToRight = 0,  /* Left to right writing direction */
    WKWritingDirectionRightToLeft       /* Right to left writing direction */
} WKWritingDirection;

typedef enum _NSSelectionAffinity {
    NSSelectionAffinityUpstream = 0,
    NSSelectionAffinityDownstream = 1
} NSSelectionAffinity;

typedef enum _NSCellState {
    NSControlStateValueMixed = -1,
    NSControlStateValueOff   =  0,
    NSControlStateValueOn    =  1
} NSControlStateValue;

typedef enum _NSCompositingOperation {
    NSCompositeClear           = 0,
    NSCompositeCopy            = 1,
    NSCompositeSourceOver      = 2,
    NSCompositeSourceIn        = 3,
    NSCompositeSourceOut       = 4,
    NSCompositeSourceAtop      = 5,
    NSCompositeDestinationOver = 6,
    NSCompositeDestinationIn   = 7,
    NSCompositeDestinationOut  = 8,
    NSCompositeDestinationAtop = 9,
    NSCompositeXOR             = 10,
    NSCompositePlusDarker      = 11,
    NSCompositeHighlight       = 12,
    NSCompositePlusLighter     = 13
} NSCompositingOperation;

typedef enum _NSSelectionDirection {
    NSDirectSelection = 0,
    NSSelectingNext,
    NSSelectingPrevious
} NSSelectionDirection;

#endif // TARGET_OS_IPHONE

#endif // WAKAppKitStubs_h
