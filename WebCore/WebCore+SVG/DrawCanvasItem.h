/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 WARNING: These files are temporary and exist solely for the purpose
 of allowing greater testing of WebCore+SVG prior to full DOM integration.
 Do NOT depend on these SPIs or files as they will soon be gone.
*/

#import <Cocoa/Cocoa.h>

/*
 This is an Obj-C wrapper object for KCanvasItem
 This exists mostly because we don't have Obj-C
 bindings for the SVG DOM yet.

 In a real editor, you would not want to make changes to only the render
 tree, but rather to the DOM directly, and let the engine automatically
 reflect those in the render tree.
*/

@class DrawCanvasItemPrivate;

@interface DrawCanvasItem : NSObject {
    DrawCanvasItemPrivate *_private;
}

// convenience
+ (NSArray *)controlPointsForRect:(NSRect)bbox;
+ (NSPoint)dragAnchorPointForControlPointIndex:(int)controlPointIndex fromRectControlPoints:(NSArray *)controlPoints;

/*
   These were all originally intended to allow more than just the four corner
   knobs to be displayed for selection, editing, but are not finished, and
   instead always return the 4 corner points for any shape, path, etc.
   Why? A classic example is a curved path, where you don't just want 4 selection
   nobs, but rather ones along the path, possibly for curve control points, etc.
*/
- (NSPoint)dragAnchorPointForControlPointIndex:(int)controlPointIndex;
- (NSArray *)controlPoints;
- (NSRect)boundingBox;

// used for moving objects
- (void)translateByOffset:(NSSize)offset;
- (void)resizeWithPoint:(NSPoint)canvasPoint usingControlPoint:(int)controlPointIndex dragAnchorPoint:(NSPoint)canvasDragAnchorPoint;

- (unsigned int)zIndex;
- (void)raise;
- (void)lower;

@end
