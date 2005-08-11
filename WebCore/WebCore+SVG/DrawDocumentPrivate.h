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


#import <WebCore+SVG/DrawDocument.h>

@class DrawView;
@class DrawCanvasItem;

@interface DrawDocument (PrivateMethods)

// register as "primary" view (zoom/pan notifications)
- (DrawView *)primaryView;
- (void)setPrimaryView:(DrawView *)view;

// register/unregister w/ DOM, etc.
- (void)registerView:(DrawView *)view;
- (void)unregisterView:(DrawView *)view;

@end

@interface DrawDocument (SuperPrivateSPIForDavid)
- (void)drawRect:(NSRect)dirtyRect inCGContext:(CGContextRef)context;
@end

@interface DrawDocument (DrawMouseEvents)
- (BOOL)documentListensForMouseMovedEvents;
- (BOOL)documentListensForMouseDownEvents;
- (BOOL)documentListensForMouseUpEvents;

- (void)propagateMouseUpEvent:(NSEvent *)theEvent fromView:(DrawView *)view;
- (void)propagateMouseDownEvent:(NSEvent *)theEvent fromView:(DrawView *)view;
- (NSCursor *)cursorAfterPropagatingMouseMovedEvent:(NSEvent *)theEvent fromView:(DrawView *)view;

@end

/* This should probably break out into another file/class */
@interface DrawDocument (KCanvasManipulation)

- (NSSize)canvasSize;
- (void)sizeCanvasToFitContent;
- (NSString *)renderTreeAsExternalRepresentation;

/*
 This currently (hackishly) manipulates the render tree.
 This "design" decision was made because the dom hit testing
 was not working properly in ksvg2 at the time.
 Anyone writing a real editor should to hit testing against
 the DOM, using Obj-C DOM interfaces, instead of this
 very hackish DrawCanvasItem Obj-C RenderObject wrapper.
*/
- (DrawCanvasItem *)canvasItemAtPoint:(NSPoint)canvasHitPoint;
- (void)removeItemFromDOM:(DrawCanvasItem *)item;
- (DrawCanvasItem *)createItemForTool:(int)tool atPoint:(NSPoint)canvasPoint;

@end
