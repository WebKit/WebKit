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

#include <WebCore+SVG/DrawView.h>

extern NSArray *DrawViewDragTypes;

@interface DrawView (PrivateMethods)

// For debuging (filter support is partially broken)
+ (void)setFilterSupportEnabled:(BOOL)enabled;
+ (BOOL)isFilterSupportEnabled;
+ (void)setHardwareFilterSupportEnabled:(BOOL)enabled;
+ (BOOL)isHardwareFilterSupportEnabled;

- (void)setBackgroundColor:(NSColor *)color;
- (NSColor *)backgroundColor;

// FIXME: should be Obj-C DOM objects, not render tree objects.
- (NSArray *)selectedCanvasItems;

// for DrawDocument delloc
- (void)_clearDocument;

// Zoom/Pan

- (CGAffineTransform)transformFromViewToCanvas;

- (NSPoint)mapViewPointToCanvas:(NSPoint)viewPoint;
- (NSRect)mapViewRectToCanvas:(NSRect)viewRect;
- (NSSize)mapViewSizeToCanvas:(NSSize)viewSize;
- (NSPoint)mapCanvasPointToView:(NSPoint)canvasPoint;
- (NSRect)mapCanvasRectToView:(NSRect)canvasRect;
- (NSSize)mapCanvasSizeToView:(NSSize)viewSize;

- (NSPoint)canvasVisibleOrigin;
- (void)setCanvasVisibleOrigin:(NSPoint)newOrigin;

- (NSPoint)canvasvisibleCenterPoint;
- (void)panToCanvasCenterPoint:(NSPoint)newCenter;

- (float)canvasZoom;
- (void)setCanvasZoom:(float)newZoom;

@end
